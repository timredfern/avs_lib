// Tests for effects using color.h - verifying correct channel handling
#include <catch2/catch_test_macros.hpp>
#include "core/color.h"

using namespace avs;

// Test the color interpolation formula used by ring, oscstar, rotstar, dot_grid
// Formula: ((c1_channel * (63 - r)) + (c2_channel * r)) / 64
TEST_CASE("Color interpolation formula", "[color]") {
    SECTION("Red to Blue interpolation at midpoint") {
        uint32_t c1 = 0xFFFF0000;  // Red
        uint32_t c2 = 0xFF0000FF;  // Blue
        int r = 32;  // Midpoint (0-63 range)

        int red = ((color::red(c1) * (63 - r)) + (color::red(c2) * r)) / 64;
        int grn = ((color::green(c1) * (63 - r)) + (color::green(c2) * r)) / 64;
        int blu = ((color::blue(c1) * (63 - r)) + (color::blue(c2) * r)) / 64;

        uint32_t result = color::make(red, grn, blu);

        // At midpoint, red and blue should be roughly equal (~127)
        INFO("red=" << red << " grn=" << grn << " blu=" << blu);
        REQUIRE(color::red(result) > 100);
        REQUIRE(color::red(result) < 160);
        REQUIRE(color::blue(result) > 100);
        REQUIRE(color::blue(result) < 160);
        REQUIRE(color::green(result) == 0);
    }

    SECTION("Yellow interpolation preserves both R and G") {
        uint32_t c1 = 0xFFFFFF00;  // Yellow
        uint32_t c2 = 0xFFFFFF00;  // Yellow
        int r = 32;

        int red = ((color::red(c1) * (63 - r)) + (color::red(c2) * r)) / 64;
        int grn = ((color::green(c1) * (63 - r)) + (color::green(c2) * r)) / 64;
        int blu = ((color::blue(c1) * (63 - r)) + (color::blue(c2) * r)) / 64;

        // Yellow should stay yellow
        REQUIRE(red > 250);
        REQUIRE(grn > 250);
        REQUIRE(blu == 0);
    }
}

// Test the bilinear interpolation formula used by ddm.cpp
TEST_CASE("DDM bilinear interpolation", "[ddm][color]") {
    SECTION("Corner pixels average correctly") {
        // Four corner pixels: Red, Green, Blue, White
        uint32_t p00 = 0xFFFF0000;  // Red (top-left)
        uint32_t p01 = 0xFF00FF00;  // Green (top-right)
        uint32_t p10 = 0xFF0000FF;  // Blue (bottom-left)
        uint32_t p11 = 0xFFFFFFFF;  // White (bottom-right)

        // Center point (xpart=128, ypart=128 means 50% in both directions)
        int xpart = 128;
        int ypart = 128;

        int r = (((color::red(p00) * (256 - xpart) + color::red(p01) * xpart) >> 8) * (256 - ypart) +
                 ((color::red(p10) * (256 - xpart) + color::red(p11) * xpart) >> 8) * ypart) >> 8;
        int g = (((color::green(p00) * (256 - xpart) + color::green(p01) * xpart) >> 8) * (256 - ypart) +
                 ((color::green(p10) * (256 - xpart) + color::green(p11) * xpart) >> 8) * ypart) >> 8;
        int b = (((color::blue(p00) * (256 - xpart) + color::blue(p01) * xpart) >> 8) * (256 - ypart) +
                 ((color::blue(p10) * (256 - xpart) + color::blue(p11) * xpart) >> 8) * ypart) >> 8;

        uint32_t result = color::make(r, g, b);

        INFO("r=" << r << " g=" << g << " b=" << b);
        // At center of Red(255,0,0), Green(0,255,0), Blue(0,0,255), White(255,255,255):
        // Each channel averages to ~127
        REQUIRE(color::red(result) > 100);
        REQUIRE(color::red(result) < 160);
        REQUIRE(color::green(result) > 100);
        REQUIRE(color::green(result) < 160);
        REQUIRE(color::blue(result) > 100);
        REQUIRE(color::blue(result) < 160);
    }
}

// Test the starfield color blending formula
TEST_CASE("Starfield color blending", "[starfield][color]") {
    SECTION("Red target tints gray toward red") {
        uint32_t target_color = 0xFFFF0000;  // Pure red
        uint8_t c = 128;  // Intensity

        int divisor = c >> 4;  // 128 >> 4 = 8
        if (divisor > 15) divisor = 15;

        uint32_t gray = color::make(c, c, c);
        int r = (color::red(gray) * (16 - divisor) + color::red(target_color) * divisor) >> 4;
        int g = (color::green(gray) * (16 - divisor) + color::green(target_color) * divisor) >> 4;
        int b = (color::blue(gray) * (16 - divisor) + color::blue(target_color) * divisor) >> 4;

        uint32_t result = color::make(r, g, b);

        // Red channel should be boosted, green and blue reduced
        REQUIRE(color::red(result) > color::green(result));
        REQUIRE(color::red(result) > color::blue(result));
    }
}

// Test picture.cpp RGBA to ARGB conversion
TEST_CASE("Picture RGBA to ARGB conversion", "[picture][color]") {
    SECTION("Pure red converts correctly") {
        uint8_t r = 255, g = 0, b = 0, a = 255;
        uint32_t result = color::make(r, g, b, a);

        REQUIRE(color::red(result) == 255);
        REQUIRE(color::green(result) == 0);
        REQUIRE(color::blue(result) == 0);
        REQUIRE(color::alpha(result) == 255);
    }

    SECTION("Cyan with alpha converts correctly") {
        uint8_t r = 0, g = 255, b = 255, a = 128;
        uint32_t result = color::make(r, g, b, a);

        REQUIRE(color::red(result) == 0);
        REQUIRE(color::green(result) == 255);
        REQUIRE(color::blue(result) == 255);
        REQUIRE(color::alpha(result) == 128);
    }
}
