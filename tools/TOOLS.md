# AVS Library Tools

Command-line utilities for working with AVS presets.

## Building

```bash
cd libs/avs_lib/tools
mkdir build && cd build
cmake ..
make
```

## avs2json

Converts binary `.avs` presets to JSON format for debugging and inspection.

```bash
./avs2json <input.avs> [output.json]
```

If output file is omitted, prints to stdout.

**Output includes:**
- Effect chain hierarchy with nested effect lists
- All parameter values for each effect
- Unsupported effects marked with their original type name

**Use cases:**
- Debugging preset loading issues
- Understanding preset structure
- Comparing original vs loaded parameters
- Diffing presets to find differences

## avs_render_test

Renders a specific frame from an AVS preset to an image file. Used for regression testing and visual comparison.

```bash
./avs_render_test --preset <file> --frame <N> --output <image.ppm> [options]
```

**Required arguments:**
- `--preset <file>` - AVS preset file (binary .avs or .json)
- `--frame <N>` - Frame number to render (0-based)
- `--output <file>` - Output image file (PPM format)

**Optional arguments:**
- `--width <W>` - Output width (default: 320)
- `--height <H>` - Output height (default: 240)

**Example:**
```bash
# Render frame 30 of a preset at default resolution
./avs_render_test --preset "my_preset.avs" --frame 30 --output frame30.ppm

# Render at higher resolution
./avs_render_test --preset "my_preset.avs" --frame 100 --output frame100.ppm --width 640 --height 480
```

**Notes:**
- Generates synthetic audio (sine wave + fake spectrum) for deterministic output
- Renders all frames 0 to N to reach frame N (state accumulates)
- PPM format is uncompressed RGB, viewable with most image viewers

**Use cases:**
- Regression testing against reference renders
- Visual comparison with other AVS implementations (grandchild port)
- Automated validation of effect output
