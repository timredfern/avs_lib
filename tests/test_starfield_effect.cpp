// Tests for StarfieldEffect color blending
#include <catch2/catch_test_macros.hpp>
#include "effects/starfield.h"
#include "core/plugin_manager.h"
#include "core/color.h"
#include <vector>

using namespace avs;

TEST_CASE("Starfield Effect", "[starfield]") {
    StarfieldEffect effect;
    AudioData dummy_audio = {};

    SECTION("Effect initialization") {
        REQUIRE(effect.get_plugin_info().name == "Starfield");

        auto& params = effect.parameters();
        REQUIRE(params.get_int("enabled") == 1);
        REQUIRE(params.get_int("warpSpeed") == 60);
        REQUIRE(params.get_int("maxStars") == 350);
    }

    SECTION("Color parameter uses ARGB format") {
        auto& params = effect.parameters();

        // Set a red color (ARGB format: 0xFFFF0000)
        params.set_color("color", 0xFFFF0000);
        uint32_t retrieved = params.get_color("color");

        // Verify it's stored correctly
        REQUIRE(color::red(retrieved) == 255);
        REQUIRE(color::green(retrieved) == 0);
        REQUIRE(color::blue(retrieved) == 0);
    }

    SECTION("Disabled effect returns 0") {
        auto& params = effect.parameters();
        params.set_int("enabled", 0);

        const int w = 64, h = 64;
        std::vector<uint32_t> framebuffer(w * h, 0);
        std::vector<uint32_t> fbout(w * h, 0);

        int result = effect.render(dummy_audio, 0, framebuffer.data(), fbout.data(), w, h);
        REQUIRE(result == 0);
    }
}

TEST_CASE("Starfield Color Blending Logic", "[starfield]") {
    // Test the color blending formula used in starfield
    // The effect blends from grayscale toward a target color based on intensity

    SECTION("Pure white target produces grayscale") {
        // When color is white (0x00FFFFFF without alpha check),
        // the code path uses simple grayscale: c | (c << 8) | (c << 16)

        uint8_t c = 128;  // Mid intensity
        uint32_t expected_gray = color::make(c, c, c);

        REQUIRE(color::red(expected_gray) == 128);
        REQUIRE(color::green(expected_gray) == 128);
        REQUIRE(color::blue(expected_gray) == 128);
    }

    SECTION("Red target blends correctly") {
        // Simulate the blending formula from starfield render
        uint32_t target_color = 0xFFFF0000;  // Pure red
        uint8_t c = 128;  // Intensity

        int divisor = c >> 4;  // 128 >> 4 = 8
        if (divisor > 15) divisor = 15;

        uint32_t gray = color::make(c, c, c);

        // Blend each channel: (gray * (16-divisor) + color * divisor) >> 4
        int r = (color::red(gray) * (16 - divisor) + color::red(target_color) * divisor) >> 4;
        int g = (color::green(gray) * (16 - divisor) + color::green(target_color) * divisor) >> 4;
        int b = (color::blue(gray) * (16 - divisor) + color::blue(target_color) * divisor) >> 4;

        uint32_t result = color::make(r, g, b);

        INFO("divisor=" << divisor);
        INFO("gray R=" << (int)color::red(gray) << " G=" << (int)color::green(gray) << " B=" << (int)color::blue(gray));
        INFO("result R=" << r << " G=" << g << " B=" << b);

        // With divisor=8, we're halfway between gray and target
        // R: (128 * 8 + 255 * 8) >> 4 = (1024 + 2040) >> 4 = 191
        // G: (128 * 8 + 0 * 8) >> 4 = 1024 >> 4 = 64
        // B: (128 * 8 + 0 * 8) >> 4 = 1024 >> 4 = 64
        REQUIRE(r == 191);  // Boosted toward red
        REQUIRE(g == 64);   // Reduced (gray toward 0)
        REQUIRE(b == 64);   // Reduced (gray toward 0)

        // Verify red channel is highest (red tint)
        REQUIRE(color::red(result) > color::green(result));
        REQUIRE(color::red(result) > color::blue(result));
    }

    SECTION("Blue target blends correctly") {
        uint32_t target_color = 0xFF0000FF;  // Pure blue
        uint8_t c = 128;

        int divisor = c >> 4;
        if (divisor > 15) divisor = 15;

        uint32_t gray = color::make(c, c, c);

        int r = (color::red(gray) * (16 - divisor) + color::red(target_color) * divisor) >> 4;
        int g = (color::green(gray) * (16 - divisor) + color::green(target_color) * divisor) >> 4;
        int b = (color::blue(gray) * (16 - divisor) + color::blue(target_color) * divisor) >> 4;

        uint32_t result = color::make(r, g, b);

        INFO("result R=" << r << " G=" << g << " B=" << b);

        // R: (128 * 8 + 0 * 8) >> 4 = 64
        // G: (128 * 8 + 0 * 8) >> 4 = 64
        // B: (128 * 8 + 255 * 8) >> 4 = 191
        REQUIRE(r == 64);
        REQUIRE(g == 64);
        REQUIRE(b == 191);

        // Verify blue channel is highest (blue tint)
        REQUIRE(color::blue(result) > color::red(result));
        REQUIRE(color::blue(result) > color::green(result));
    }

    SECTION("Yellow target blends correctly") {
        uint32_t target_color = 0xFFFFFF00;  // Yellow (R=255, G=255, B=0)
        uint8_t c = 128;

        int divisor = c >> 4;
        if (divisor > 15) divisor = 15;

        uint32_t gray = color::make(c, c, c);

        int r = (color::red(gray) * (16 - divisor) + color::red(target_color) * divisor) >> 4;
        int g = (color::green(gray) * (16 - divisor) + color::green(target_color) * divisor) >> 4;
        int b = (color::blue(gray) * (16 - divisor) + color::blue(target_color) * divisor) >> 4;

        uint32_t result = color::make(r, g, b);

        INFO("result R=" << r << " G=" << g << " B=" << b);

        // R: (128 * 8 + 255 * 8) >> 4 = 191
        // G: (128 * 8 + 255 * 8) >> 4 = 191
        // B: (128 * 8 + 0 * 8) >> 4 = 64
        REQUIRE(r == 191);
        REQUIRE(g == 191);
        REQUIRE(b == 64);

        // Yellow should have R=G and both higher than B
        REQUIRE(color::red(result) == color::green(result));
        REQUIRE(color::red(result) > color::blue(result));
    }

    SECTION("Low intensity gives more gray") {
        uint32_t target_color = 0xFFFF0000;  // Red
        uint8_t c = 32;  // Low intensity

        int divisor = c >> 4;  // 32 >> 4 = 2
        if (divisor > 15) divisor = 15;

        uint32_t gray = color::make(c, c, c);

        int r = (color::red(gray) * (16 - divisor) + color::red(target_color) * divisor) >> 4;
        int g = (color::green(gray) * (16 - divisor) + color::green(target_color) * divisor) >> 4;
        int b = (color::blue(gray) * (16 - divisor) + color::blue(target_color) * divisor) >> 4;

        INFO("divisor=" << divisor);
        INFO("result R=" << r << " G=" << g << " B=" << b);

        // With divisor=2, mostly gray (14/16 gray, 2/16 color)
        // R: (32 * 14 + 255 * 2) >> 4 = (448 + 510) >> 4 = 59
        // G: (32 * 14 + 0 * 2) >> 4 = 448 >> 4 = 28
        // B: (32 * 14 + 0 * 2) >> 4 = 448 >> 4 = 28
        REQUIRE(r == 59);
        REQUIRE(g == 28);
        REQUIRE(b == 28);
    }

    SECTION("High intensity gives more color") {
        uint32_t target_color = 0xFFFF0000;  // Red
        uint8_t c = 255;  // Max intensity

        int divisor = c >> 4;  // 255 >> 4 = 15
        if (divisor > 15) divisor = 15;

        uint32_t gray = color::make(c, c, c);

        int r = (color::red(gray) * (16 - divisor) + color::red(target_color) * divisor) >> 4;
        int g = (color::green(gray) * (16 - divisor) + color::green(target_color) * divisor) >> 4;
        int b = (color::blue(gray) * (16 - divisor) + color::blue(target_color) * divisor) >> 4;

        INFO("divisor=" << divisor);
        INFO("result R=" << r << " G=" << g << " B=" << b);

        // With divisor=15, mostly color (1/16 gray, 15/16 color)
        // R: (255 * 1 + 255 * 15) >> 4 = (255 + 3825) >> 4 = 255
        // G: (255 * 1 + 0 * 15) >> 4 = 255 >> 4 = 15
        // B: (255 * 1 + 0 * 15) >> 4 = 255 >> 4 = 15
        REQUIRE(r == 255);
        REQUIRE(g == 15);
        REQUIRE(b == 15);
    }
}
