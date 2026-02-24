// Test color loading in load_parameters
// Verifies that binary AVS color values are correctly loaded into effect parameters
//
// COLOR FORMAT REFERENCE:
// =======================
// Internal ARGB: 0xAARRGGBB (what avs_lib uses)
//
// Binary preset colors come in TWO formats depending on the effect:
// - 0x00RRGGBB: Same as ARGB (no swap needed) - used by Oscilloscope, SuperScope, etc.
// - COLORREF (0x00BBGGRR): Windows GDI format (swap needed) - used by DotFountain, etc.
//
// See EFFECTS.md for which effects use which format.
// Always use color::red/green/blue() to extract components - NEVER inline bit ops.

#include <catch2/catch_test_macros.hpp>
#include "core/color.h"
#include "core/binary_reader.h"
#include "core/preset.h"
#include "core/plugin_manager.h"
#include "core/builtin_effects.h"
#include "effects/oscilloscope.h"
#include "effects/effect_list_root.h"
#include <vector>
#include <cstdint>

using namespace avs;

// Helper to append a little-endian uint32_t
static void append_u32(std::vector<uint8_t>& data, uint32_t val) {
    data.push_back(val & 0xFF);
    data.push_back((val >> 8) & 0xFF);
    data.push_back((val >> 16) & 0xFF);
    data.push_back((val >> 24) & 0xFF);
}

// Ensure effects are registered
static void ensure_effects_registered() {
    static bool registered = false;
    if (!registered) {
        avs::register_builtin_effects();
        registered = true;
    }
}

// =============================================================================
// swap_rb / bgra_to_rgba function tests
// =============================================================================

TEST_CASE("swap_rb color conversion", "[color][binary]") {
    SECTION("Orange - COLORREF to ARGB") {
        // Orange = RGB(255, 128, 0)
        // COLORREF: 0x000080FF (R=FF at bits 0-7, B=00 at bits 16-23)
        // ARGB:     0xFFFF8000 (R=FF at bits 16-23, B=00 at bits 0-7)

        uint32_t colorref_orange = 0x000080FF;
        uint32_t argb_orange = color::swap_rb(colorref_orange) | 0xFF000000;

        REQUIRE(argb_orange == 0xFFFF8000);
        REQUIRE(color::red(argb_orange) == 255);
        REQUIRE(color::green(argb_orange) == 128);
        REQUIRE(color::blue(argb_orange) == 0);
    }

    SECTION("Sky Blue - COLORREF to ARGB") {
        // Sky blue = RGB(0, 128, 255)
        // COLORREF: 0x00FF8000 (R=00 at bits 0-7, B=FF at bits 16-23)
        // ARGB:     0xFF0080FF (R=00 at bits 16-23, B=FF at bits 0-7)

        uint32_t colorref_skyblue = 0x00FF8000;
        uint32_t argb_skyblue = color::swap_rb(colorref_skyblue) | 0xFF000000;

        REQUIRE(argb_skyblue == 0xFF0080FF);
        REQUIRE(color::red(argb_skyblue) == 0);
        REQUIRE(color::green(argb_skyblue) == 128);
        REQUIRE(color::blue(argb_skyblue) == 255);
    }

    SECTION("Green stays green") {
        // Green = RGB(0, 255, 0)
        // COLORREF: 0x0000FF00 (R=00, G=FF, B=00)
        // ARGB:     0xFF00FF00 (same R and B, just swap is no-op)

        uint32_t colorref_green = 0x0000FF00;
        uint32_t argb_green = color::swap_rb(colorref_green) | 0xFF000000;

        REQUIRE(argb_green == 0xFF00FF00);
        REQUIRE(color::red(argb_green) == 0);
        REQUIRE(color::green(argb_green) == 255);
        REQUIRE(color::blue(argb_green) == 0);
    }

    SECTION("Magenta") {
        // Magenta = RGB(255, 0, 128)
        // COLORREF: 0x008000FF (R=FF at bits 0-7, G=00, B=80 at bits 16-23)
        // ARGB:     0xFFFF0080 (R=FF at bits 16-23, G=00, B=80 at bits 0-7)

        uint32_t colorref_magenta = 0x008000FF;
        uint32_t argb_magenta = color::swap_rb(colorref_magenta) | 0xFF000000;

        REQUIRE(argb_magenta == 0xFFFF0080);
        REQUIRE(color::red(argb_magenta) == 255);
        REQUIRE(color::green(argb_magenta) == 0);
        REQUIRE(color::blue(argb_magenta) == 128);
    }

    SECTION("Round-trip: swap_rb is its own inverse") {
        uint32_t test_colors[] = {0x00000000, 0xFFFFFFFF, 0x00FF8000, 0x000080FF, 0x12345678};
        for (uint32_t c : test_colors) {
            REQUIRE(color::swap_rb(color::swap_rb(c)) == c);
        }
    }
}

// =============================================================================
// Oscilloscope load_parameters tests
// =============================================================================

TEST_CASE("Oscilloscope load_parameters - single color", "[color][oscilloscope]") {
    ensure_effects_registered();

    // NOTE: Oscilloscope stores colors in 0x00RRGGBB format (same as ARGB without alpha)
    // This is determined from the original Windows AVS source - the dialog code swaps R↔B
    // before passing to GDI, which means internal format is 0x00RRGGBB.

    SECTION("Orange from binary") {
        avs::OscilloscopeEffect effect;

        // Build binary config:
        // - effect mode (int32)
        // - num_colors (int32) = 1
        // - color[0] (int32) in 0x00RRGGBB format
        //
        // Orange = RGB(255, 128, 0) in 0x00RRGGBB = 0x00FF8000
        std::vector<uint8_t> data;
        append_u32(data, 0);           // effect mode
        append_u32(data, 1);           // num_colors = 1
        append_u32(data, 0x00FF8000);  // Orange in 0x00RRGGBB

        effect.load_parameters(data);

        uint32_t loaded = effect.parameters().get_color("color_0");

        // Verify using color:: functions (internal ARGB format)
        REQUIRE(color::red(loaded) == 255);
        REQUIRE(color::green(loaded) == 128);
        REQUIRE(color::blue(loaded) == 0);
        REQUIRE(color::alpha(loaded) == 255);

        // Verify raw value is correct ARGB
        REQUIRE(loaded == 0xFFFF8000);
    }

    SECTION("Sky blue from binary") {
        avs::OscilloscopeEffect effect;

        // Sky blue = RGB(0, 128, 255) in 0x00RRGGBB = 0x000080FF
        std::vector<uint8_t> data;
        append_u32(data, 0);           // effect mode
        append_u32(data, 1);           // num_colors = 1
        append_u32(data, 0x000080FF);  // Sky blue in 0x00RRGGBB

        effect.load_parameters(data);

        uint32_t loaded = effect.parameters().get_color("color_0");

        REQUIRE(color::red(loaded) == 0);
        REQUIRE(color::green(loaded) == 128);
        REQUIRE(color::blue(loaded) == 255);
        REQUIRE(loaded == 0xFF0080FF);
    }
}

TEST_CASE("Oscilloscope load_parameters - bouncing colorfade colors", "[color][oscilloscope][preset]") {
    ensure_effects_registered();

    // The "bouncing colorfade" preset has these 3 colors in 0x00RRGGBB format:
    //   0x000080FF = sky blue   RGB(0, 128, 255)
    //   0x0000FF00 = green      RGB(0, 255, 0)
    //   0x00FF0080 = magenta    RGB(255, 0, 128)

    avs::OscilloscopeEffect effect;

    std::vector<uint8_t> data;
    append_u32(data, 0x2A);        // effect mode (from preset: 0x2A = line scope, center)
    append_u32(data, 3);           // num_colors = 3
    append_u32(data, 0x000080FF);  // Sky blue in 0x00RRGGBB
    append_u32(data, 0x0000FF00);  // Green in 0x00RRGGBB
    append_u32(data, 0x00FF0080);  // Magenta in 0x00RRGGBB

    effect.load_parameters(data);

    SECTION("Color 0: Sky blue") {
        uint32_t c = effect.parameters().get_color("color_0");
        REQUIRE(color::red(c) == 0);
        REQUIRE(color::green(c) == 128);
        REQUIRE(color::blue(c) == 255);
        REQUIRE(c == 0xFF0080FF);
    }

    SECTION("Color 1: Green") {
        uint32_t c = effect.parameters().get_color("color_1");
        REQUIRE(color::red(c) == 0);
        REQUIRE(color::green(c) == 255);
        REQUIRE(color::blue(c) == 0);
        REQUIRE(c == 0xFF00FF00);
    }

    SECTION("Color 2: Magenta") {
        uint32_t c = effect.parameters().get_color("color_2");
        REQUIRE(color::red(c) == 255);
        REQUIRE(color::green(c) == 0);
        REQUIRE(color::blue(c) == 128);
        REQUIRE(c == 0xFFFF0080);
    }
}

// =============================================================================
// JSON serialization tests
// =============================================================================

TEST_CASE("JSON color output format", "[color][json]") {
    ensure_effects_registered();

    // Load orange from binary, serialize to JSON, verify JSON contains ARGB hex
    avs::OscilloscopeEffect effect;
    std::vector<uint8_t> data;
    append_u32(data, 0);           // mode
    append_u32(data, 1);           // num_colors
    append_u32(data, 0x00FF8000);  // Orange in 0x00RRGGBB
    effect.load_parameters(data);

    avs::EffectListRoot root;
    root.add_child(std::make_unique<avs::OscilloscopeEffect>(effect));
    std::string json = avs::Preset::to_json(root);

    INFO("JSON: " << json);

    // JSON should show the color in some hex format
    // The exact format depends on preset.cpp implementation
    // For now, just verify the effect was serialized
    REQUIRE(json.find("Oscilloscope") != std::string::npos);
    REQUIRE(json.find("color_0") != std::string::npos);
}

TEST_CASE("JSON color round-trip preserves RGB values", "[color][json]") {
    ensure_effects_registered();

    // Create oscilloscope with bouncing colorfade colors
    avs::OscilloscopeEffect effect;
    std::vector<uint8_t> data;
    append_u32(data, 0);           // mode
    append_u32(data, 3);           // num_colors
    append_u32(data, 0x000080FF);  // Sky blue in 0x00RRGGBB
    append_u32(data, 0x0000FF00);  // Green in 0x00RRGGBB
    append_u32(data, 0x00FF0080);  // Magenta in 0x00RRGGBB
    effect.load_parameters(data);

    // Save original RGB values (using color:: functions)
    uint32_t orig_c0 = effect.parameters().get_color("color_0");
    uint32_t orig_c1 = effect.parameters().get_color("color_1");
    uint32_t orig_c2 = effect.parameters().get_color("color_2");

    // Serialize to JSON
    avs::EffectListRoot root;
    root.add_child(std::make_unique<avs::OscilloscopeEffect>(effect));
    std::string json = avs::Preset::to_json(root);

    INFO("JSON: " << json);

    // Load back from JSON
    avs::EffectListRoot root2;
    REQUIRE(avs::Preset::from_json(json, root2) == true);
    REQUIRE(root2.child_count() == 1);

    auto* loaded = root2.get_child(0);
    uint32_t load_c0 = loaded->parameters().get_color("color_0");
    uint32_t load_c1 = loaded->parameters().get_color("color_1");
    uint32_t load_c2 = loaded->parameters().get_color("color_2");

    // RGB values must be preserved through round-trip
    SECTION("Sky blue preserved") {
        REQUIRE(color::red(load_c0) == color::red(orig_c0));
        REQUIRE(color::green(load_c0) == color::green(orig_c0));
        REQUIRE(color::blue(load_c0) == color::blue(orig_c0));
    }

    SECTION("Green preserved") {
        REQUIRE(color::red(load_c1) == color::red(orig_c1));
        REQUIRE(color::green(load_c1) == color::green(orig_c1));
        REQUIRE(color::blue(load_c1) == color::blue(orig_c1));
    }

    SECTION("Magenta preserved") {
        REQUIRE(color::red(load_c2) == color::red(orig_c2));
        REQUIRE(color::green(load_c2) == color::green(orig_c2));
        REQUIRE(color::blue(load_c2) == color::blue(orig_c2));
    }
}

// =============================================================================
// Guards against accidental ABGR/ARGB confusion
// =============================================================================

TEST_CASE("CRITICAL: Never extract colors with inline bit ops", "[color][critical]") {
    // This test documents the WRONG way to extract colors.
    // If you see code like this in the codebase, it's a bug:
    //   uint8_t r = color & 0xFF;  // WRONG - assumes ABGR
    //
    // Always use:
    //   uint8_t r = color::red(color);  // CORRECT - uses defined format

    uint32_t argb_orange = 0xFFFF8000;  // Orange in ARGB

    // CORRECT extraction
    REQUIRE(color::red(argb_orange) == 255);
    REQUIRE(color::green(argb_orange) == 128);
    REQUIRE(color::blue(argb_orange) == 0);

    // WRONG extraction (what ABGR code would do)
    uint8_t wrong_r = argb_orange & 0xFF;          // Gets B, not R!
    uint8_t wrong_b = (argb_orange >> 16) & 0xFF;  // Gets R, not B!

    REQUIRE(wrong_r == 0);    // This gets Blue, not Red!
    REQUIRE(wrong_b == 255);  // This gets Red, not Blue!

    // If you ever see wrong_r == 255 expected to pass, the code assumes ABGR.
    // The ARGB format requires using color::red/green/blue().
}
