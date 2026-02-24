# Color Architecture

This document describes the color format used throughout avs_lib and how legacy preset colors are handled.

## Internal Format: ARGB

All colors in avs_lib use **ARGB format**: `0xAARRGGBB`

| Bits | Component | Mask | Extraction |
|------|-----------|------|------------|
| 24-31 | Alpha | `0xFF000000` | `(color >> 24) & 0xFF` |
| 16-23 | Red | `0x00FF0000` | `(color >> 16) & 0xFF` |
| 8-15 | Green | `0x0000FF00` | `(color >> 8) & 0xFF` |
| 0-7 | Blue | `0x000000FF` | `color & 0xFF` |

On little-endian systems (x86/ARM), this is stored in memory as bytes `[B, G, R, A]`.

**Why ARGB?** OpenFrameworks and OpenGL expect ARGB format for texture uploads. Using ARGB internally means no conversion is needed at render time.

## Legacy Format: COLORREF/ABGR

Original Windows AVS used the Windows COLORREF format: `0x00BBGGRR`

| Bits | Component |
|------|-----------|
| 16-23 | Blue |
| 8-15 | Green |
| 0-7 | Red |

Binary `.avs` preset files store all colors in COLORREF format.

### Format Comparison

The same color in both formats:

| Color | ARGB (internal) | COLORREF (legacy) |
|-------|-----------------|-------------------|
| Sky blue (R=0, G=128, B=255) | `0xFF0080FF` | `0x00FF8000` |
| Orange (R=255, G=128, B=0) | `0xFFFF8000` | `0x000080FF` |
| Pure red | `0xFFFF0000` | `0x000000FF` |
| Pure blue | `0xFF0000FF` | `0x00FF0000` |

Notice that Red and Blue are swapped between formats.

## The One Conversion Point

Color conversion from COLORREF to ARGB happens **exactly once**: in each effect's `load_parameters()` function when loading binary preset data.

```cpp
void MyEffect::load_parameters(const std::vector<uint8_t>& data) {
    BinaryReader reader(data);

    // Convert legacy COLORREF to internal ARGB
    uint32_t color = BinaryReader::bgra_to_rgba(reader.read_u32());
    parameters().set_color("my_color", color);
}
```

After loading, **no further conversions occur anywhere in the codebase**. All effects, blend operations, line drawing, and rendering use ARGB directly.

## Color Utilities

Use the utilities in `core/color.h` instead of inline bit operations:

```cpp
#include "core/color.h"

// Extract components from ARGB color
uint8_t r = avs::color::red(color);    // bits 16-23
uint8_t g = avs::color::green(color);  // bits 8-15
uint8_t b = avs::color::blue(color);   // bits 0-7
uint8_t a = avs::color::alpha(color);  // bits 24-31

// Build ARGB color from components
uint32_t c = avs::color::make(r, g, b);       // alpha defaults to 0xFF
uint32_t c = avs::color::make(r, g, b, a);    // explicit alpha

// Convert between formats (ONLY use in load_parameters)
uint32_t argb = avs::color::swap_rb(colorref);
uint32_t colorref = avs::color::swap_rb(argb);  // swap_rb is its own inverse
```

### Why Use Utilities?

Using `color::red(c)` instead of `(c >> 16) & 0xFF`:

1. **Self-documenting** - Intent is clear
2. **Consistent** - No chance of using wrong bit positions
3. **Searchable** - Easy to find all color extraction in codebase
4. **Testable** - Utilities are unit tested

## Common Mistakes to Avoid

### Wrong: ABGR extraction pattern

```cpp
// WRONG - this is COLORREF/ABGR extraction
int r = pixel & 0xFF;
int g = (pixel >> 8) & 0xFF;
int b = (pixel >> 16) & 0xFF;
```

### Right: ARGB extraction pattern

```cpp
// RIGHT - this is ARGB extraction
int r = (pixel >> 16) & 0xFF;
int g = (pixel >> 8) & 0xFF;
int b = pixel & 0xFF;

// Or better, use the utilities:
int r = avs::color::red(pixel);
int g = avs::color::green(pixel);
int b = avs::color::blue(pixel);
```

### Wrong: Converting colors outside load_parameters

```cpp
// WRONG - colors are already ARGB after loading
void MyEffect::render(...) {
    uint32_t color = swap_rb(parameters().get_color("color"));  // NO!
}
```

### Right: Use colors directly

```cpp
// RIGHT - colors are already ARGB
void MyEffect::render(...) {
    uint32_t color = parameters().get_color("color");  // Already ARGB
}
```

## Byte Access (Little-Endian)

When accessing pixel data as bytes (e.g., for lookup tables), remember that ARGB `0xAARRGGBB` is stored in memory as `[B, G, R, A]` on little-endian systems:

```cpp
uint8_t* pixel = reinterpret_cast<uint8_t*>(&framebuffer[i]);
// pixel[0] = Blue   (bits 0-7)
// pixel[1] = Green  (bits 8-15)
// pixel[2] = Red    (bits 16-23)
// pixel[3] = Alpha  (bits 24-31)
```

## Testing

### Color Format Tests

Located in `tests/test_color_format.cpp`:

```cpp
// Verify extraction from known ARGB value
uint32_t sky_blue = 0xFF0080FF;  // R=0, G=128, B=255
CHECK(color::red(sky_blue) == 0);
CHECK(color::green(sky_blue) == 128);
CHECK(color::blue(sky_blue) == 255);

// Verify construction produces correct ARGB
CHECK(color::make(255, 128, 0) == 0xFFFF8000);  // Orange
```

### Golden Value Tests

Test against known preset colors to catch regressions:

```cpp
// "justin - bouncing colorfade.avs" oscilloscope color 0
// Binary stores COLORREF: 0x00FF8000 (sky blue in BGR format)
// After conversion, ARGB should be: 0xFF0080FF
oscilloscope.load_parameters(preset_data);
uint32_t color = oscilloscope.parameters().get_color("color_0");

CHECK(color::red(color) == 0);      // Not 255!
CHECK(color::green(color) == 128);
CHECK(color::blue(color) == 255);   // Not 0!
```

### Running Color Tests

```bash
cd libs/avs_lib/tests/build
make && ./avs_tests "[color]"
```

## Alpha Channel

Alpha propagates through the effect chain:

- Effects CAN read alpha from the framebuffer
- Effects CAN write alpha for subsequent effects
- BLEND macros preserve alpha
- At final display, the renderer forces alpha to `0xFF`

Original Windows AVS displayed with `BitBlt(SRCCOPY)` which ignores alpha. Our OpenFrameworks port respects alpha, so we force opaque at the output stage only.

## Summary

1. **Internal format is ARGB** (`0xAARRGGBB`)
2. **Legacy presets use COLORREF** (`0x00BBGGRR`)
3. **Convert once in `load_parameters()`** using `swap_rb()` or `bgra_to_rgba()`
4. **Use `core/color.h` utilities** instead of inline bit operations
5. **Never convert colors elsewhere** - they're already ARGB after loading
6. **Test with golden values** from real presets to catch format bugs
