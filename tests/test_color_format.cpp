// avs_lib - Portable Advanced Visualization Studio library
// Test color format conversions and pixel storage
//
// COLOR FORMAT REFERENCE:
// =======================
// Legacy Windows COLORREF: 0x00BBGGRR
//   - R in bits 0-7
//   - G in bits 8-15
//   - B in bits 16-23
//   - This is what AVS binary presets store
//
// Internal ARGB: 0xAARRGGBB
//   - A in bits 24-31
//   - R in bits 16-23
//   - G in bits 8-15
//   - B in bits 0-7
//   - This is what avs_lib uses internally (required for OpenGL/OpenFrameworks)
//
// swap_rb() converts between these by swapping R and B positions.
// This happens ONCE when loading presets - nowhere else.

#include <catch2/catch_test_macros.hpp>
#include "core/color.h"
#include "core/binary_reader.h"
#include "core/preset.h"
#include "core/builtin_effects.h"
#include "effects/effect_list_root.h"
#include <cstdio>
#include <fstream>
#include <cmath>

using namespace avs;

// Ensure effects are registered
static void ensure_effects_registered() {
    static bool registered = false;
    if (!registered) {
        avs::register_builtin_effects();
        registered = true;
    }
}

// =============================================================================
// COLORREF format tests - what AVS presets store
// =============================================================================

TEST_CASE("COLORREF format - Orange", "[color]") {
    // Orange = RGB(255, 128, 0)
    // COLORREF = 0x00BBGGRR = (B<<16) | (G<<8) | R
    // Orange: (0<<16) | (128<<8) | 255 = 0x000080FF

    uint32_t colorref_orange = 0x000080FF;

    // Extract from COLORREF (R in bits 0-7, B in bits 16-23)
    uint8_t r = colorref_orange & 0xFF;
    uint8_t g = (colorref_orange >> 8) & 0xFF;
    uint8_t b = (colorref_orange >> 16) & 0xFF;

    REQUIRE(r == 255);  // High red
    REQUIRE(g == 128);  // Medium green
    REQUIRE(b == 0);    // No blue
}

TEST_CASE("COLORREF format - Sky Blue", "[color]") {
    // Sky blue = RGB(0, 128, 255)
    // COLORREF: (255<<16) | (128<<8) | 0 = 0x00FF8000

    uint32_t colorref_skyblue = 0x00FF8000;

    uint8_t r = colorref_skyblue & 0xFF;
    uint8_t g = (colorref_skyblue >> 8) & 0xFF;
    uint8_t b = (colorref_skyblue >> 16) & 0xFF;

    REQUIRE(r == 0);    // No red
    REQUIRE(g == 128);  // Medium green
    REQUIRE(b == 255);  // High blue
}

// =============================================================================
// Internal ARGB format tests - what avs_lib uses
// =============================================================================

TEST_CASE("Internal ARGB format - color extraction", "[color]") {
    // Orange in ARGB: 0xFFFF8000 (A=FF, R=FF, G=80, B=00)
    uint32_t argb_orange = 0xFFFF8000;

    // MUST use color:: functions for extraction
    REQUIRE(color::alpha(argb_orange) == 0xFF);
    REQUIRE(color::red(argb_orange) == 0xFF);    // 255
    REQUIRE(color::green(argb_orange) == 0x80);  // 128
    REQUIRE(color::blue(argb_orange) == 0x00);   // 0
}

TEST_CASE("Internal ARGB format - Sky Blue", "[color]") {
    // Sky blue in ARGB: 0xFF0080FF (A=FF, R=00, G=80, B=FF)
    uint32_t argb_skyblue = 0xFF0080FF;

    REQUIRE(color::alpha(argb_skyblue) == 0xFF);
    REQUIRE(color::red(argb_skyblue) == 0x00);
    REQUIRE(color::green(argb_skyblue) == 0x80);
    REQUIRE(color::blue(argb_skyblue) == 0xFF);
}

TEST_CASE("color::make builds ARGB from components", "[color]") {
    // Orange: R=255, G=128, B=0
    uint32_t orange = color::make(255, 128, 0, 255);

    REQUIRE(orange == 0xFFFF8000);
    REQUIRE(color::red(orange) == 255);
    REQUIRE(color::green(orange) == 128);
    REQUIRE(color::blue(orange) == 0);
    REQUIRE(color::alpha(orange) == 255);
}

// =============================================================================
// Format conversion tests - COLORREF <-> ARGB
// =============================================================================

TEST_CASE("swap_rb converts COLORREF to ARGB", "[color]") {
    // Orange in COLORREF: 0x000080FF (R=FF at bits 0-7, B=00 at bits 16-23)
    // Orange in ARGB:     0x00FF8000 (R=FF at bits 16-23, B=00 at bits 0-7)
    // (swap_rb doesn't touch alpha, bgra_to_rgba adds 0xFF)

    uint32_t colorref_orange = 0x000080FF;
    uint32_t argb_orange = color::swap_rb(colorref_orange) | 0xFF000000;

    REQUIRE(argb_orange == 0xFFFF8000);
    REQUIRE(color::red(argb_orange) == 255);
    REQUIRE(color::green(argb_orange) == 128);
    REQUIRE(color::blue(argb_orange) == 0);
}

TEST_CASE("swap_rb converts sky blue correctly", "[color]") {
    // Sky blue in COLORREF: 0x00FF8000 (R=00 at bits 0-7, B=FF at bits 16-23)
    // Sky blue in ARGB:     0xFF0080FF (R=00 at bits 16-23, B=FF at bits 0-7)

    uint32_t colorref_skyblue = 0x00FF8000;
    uint32_t argb_skyblue = color::swap_rb(colorref_skyblue) | 0xFF000000;

    REQUIRE(argb_skyblue == 0xFF0080FF);
    REQUIRE(color::red(argb_skyblue) == 0);
    REQUIRE(color::green(argb_skyblue) == 128);
    REQUIRE(color::blue(argb_skyblue) == 255);
}

TEST_CASE("bgra_to_rgba is swap_rb with alpha", "[color]") {
    uint32_t colorref_orange = 0x000080FF;

    uint32_t via_bgra = color::bgra_to_rgba(colorref_orange);
    uint32_t via_swap = color::swap_rb(colorref_orange) | 0xFF000000;

    REQUIRE(via_bgra == via_swap);
    REQUIRE(via_bgra == 0xFFFF8000);
}

TEST_CASE("swap_rb round-trip", "[color]") {
    // swap_rb is its own inverse
    uint32_t colors[] = {0x00000000, 0xFFFFFFFF, 0x00FF8000, 0x000080FF, 0x12345678};

    for (uint32_t c : colors) {
        REQUIRE(color::swap_rb(color::swap_rb(c)) == c);
    }
}

// =============================================================================
// Bouncing colorfade preset - golden values
// =============================================================================

TEST_CASE("Bouncing colorfade preset colors - COLORREF values", "[color][preset]") {
    // From hex dump of "justin - bouncing colorfade.avs":
    // The oscilloscope has 3 colors stored in COLORREF format:
    //   0x00FF8000 = sky blue   (R=0, G=128, B=255)
    //   0x0000FF00 = green      (R=0, G=255, B=0)
    //   0x008000FF = magenta    (R=255, G=0, B=128)

    SECTION("Color 0: Sky blue") {
        uint32_t colorref = 0x00FF8000;
        uint8_t r = colorref & 0xFF;
        uint8_t g = (colorref >> 8) & 0xFF;
        uint8_t b = (colorref >> 16) & 0xFF;

        REQUIRE(r == 0);
        REQUIRE(g == 128);
        REQUIRE(b == 255);
    }

    SECTION("Color 1: Green") {
        uint32_t colorref = 0x0000FF00;
        uint8_t r = colorref & 0xFF;
        uint8_t g = (colorref >> 8) & 0xFF;
        uint8_t b = (colorref >> 16) & 0xFF;

        REQUIRE(r == 0);
        REQUIRE(g == 255);
        REQUIRE(b == 0);
    }

    SECTION("Color 2: Magenta") {
        uint32_t colorref = 0x008000FF;
        uint8_t r = colorref & 0xFF;
        uint8_t g = (colorref >> 8) & 0xFF;
        uint8_t b = (colorref >> 16) & 0xFF;

        REQUIRE(r == 255);
        REQUIRE(g == 0);
        REQUIRE(b == 128);
    }
}

TEST_CASE("Bouncing colorfade preset colors - after conversion to ARGB", "[color][preset]") {
    // After loading with swap_rb, the colors should be in ARGB format

    SECTION("Color 0: Sky blue converted") {
        uint32_t colorref = 0x00FF8000;
        uint32_t argb = color::swap_rb(colorref) | 0xFF000000;

        REQUIRE(argb == 0xFF0080FF);
        REQUIRE(color::red(argb) == 0);
        REQUIRE(color::green(argb) == 128);
        REQUIRE(color::blue(argb) == 255);
    }

    SECTION("Color 1: Green converted") {
        uint32_t colorref = 0x0000FF00;
        uint32_t argb = color::swap_rb(colorref) | 0xFF000000;

        REQUIRE(argb == 0xFF00FF00);
        REQUIRE(color::red(argb) == 0);
        REQUIRE(color::green(argb) == 255);
        REQUIRE(color::blue(argb) == 0);
    }

    SECTION("Color 2: Magenta converted") {
        uint32_t colorref = 0x008000FF;
        uint32_t argb = color::swap_rb(colorref) | 0xFF000000;

        REQUIRE(argb == 0xFFFF0080);
        REQUIRE(color::red(argb) == 255);
        REQUIRE(color::green(argb) == 0);
        REQUIRE(color::blue(argb) == 128);
    }
}

// =============================================================================
// Memory layout verification
// =============================================================================

TEST_CASE("Memory layout - ARGB on little-endian", "[color]") {
    // On little-endian systems, ARGB (0xAARRGGBB) is stored as bytes [B,G,R,A]
    // This matches OF_PIXELS_RGBA which expects bytes [R,G,B,A]...
    // Wait, that doesn't match!
    //
    // Actually: OF_PIXELS_RGBA expects the FIRST byte to be Red.
    // On little-endian, uint32 0xAARRGGBB stores byte[0]=BB, byte[1]=GG, byte[2]=RR, byte[3]=AA
    // So OF would see R=BB, G=GG, B=RR, A=AA - which is WRONG!
    //
    // This means we need OF_PIXELS_BGRA for our ARGB format, or we need to
    // store as ABGR (0xAABBGGRR) which gives bytes [R,G,B,A] for OF_PIXELS_RGBA.
    //
    // TODO: Verify OpenFrameworks integration is correct

    uint32_t argb_orange = 0xFFFF8000;  // Orange in ARGB
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&argb_orange);

    // Little-endian storage: lowest byte first
    REQUIRE(bytes[0] == 0x00);  // Blue (bits 0-7)
    REQUIRE(bytes[1] == 0x80);  // Green (bits 8-15)
    REQUIRE(bytes[2] == 0xFF);  // Red (bits 16-23)
    REQUIRE(bytes[3] == 0xFF);  // Alpha (bits 24-31)

    INFO("ARGB 0x" << std::hex << argb_orange << " stored as bytes:");
    INFO("  [0]=0x" << std::hex << (int)bytes[0] << " (Blue)");
    INFO("  [1]=0x" << std::hex << (int)bytes[1] << " (Green)");
    INFO("  [2]=0x" << std::hex << (int)bytes[2] << " (Red)");
    INFO("  [3]=0x" << std::hex << (int)bytes[3] << " (Alpha)");
}

// =============================================================================
// JSON serialization (ARGB format for readability)
// =============================================================================

TEST_CASE("abgr_to_argb for JSON output", "[color]") {
    // Note: Despite the function name, this swaps internal ARGB to... ARGB?
    // The function names are confusing. Let's verify what actually happens.

    uint32_t internal = 0xFFFF8000;  // Orange in ARGB
    uint32_t json_format = color::abgr_to_argb(internal);

    // abgr_to_argb calls swap_rb, so it swaps R and B
    // 0xFFFF8000 -> swap R(bits 16-23) and B(bits 0-7) -> 0xFF0080FF

    REQUIRE(json_format == 0xFF0080FF);

    // This is confusing - the function names suggest ABGR<->ARGB but
    // our internal format is already ARGB, not ABGR.
    // TODO: Consider renaming these functions for clarity
}

TEST_CASE("argb_to_abgr for JSON input", "[color]") {
    // Same issue - if internal is ARGB, what does this do?
    uint32_t json_val = 0xFF0080FF;
    uint32_t internal = color::argb_to_abgr(json_val);

    REQUIRE(internal == 0xFFFF8000);  // Swapped back

    // Round-trip
    REQUIRE(color::abgr_to_argb(color::argb_to_abgr(json_val)) == json_val);
}
