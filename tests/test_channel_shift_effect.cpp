// Test ChannelShiftEffect color channel operations
// Uses asymmetric RGB values to catch any R<->B swapping bugs

#include <catch2/catch_test_macros.hpp>
#include "effects/channel_shift.h"
#include "core/color.h"

using namespace avs;

TEST_CASE("Channel Shift Effect - color channels", "[channel_shift][color]") {
    ChannelShiftEffect effect;
    AudioData dummy_audio = {};

    // Asymmetric test color: R=255, G=128, B=64
    // If R and B get swapped, we'll see 64 where we expect 255
    const uint32_t test_color = 0xFFFF8040;  // ARGB: A=255, R=255, G=128, B=64

    REQUIRE(color::red(test_color) == 255);
    REQUIRE(color::green(test_color) == 128);
    REQUIRE(color::blue(test_color) == 64);

    const int w = 2, h = 2;
    std::vector<uint32_t> framebuffer(w * h, test_color);
    std::vector<uint32_t> fbout(w * h, 0);

    SECTION("MODE_RGB - no change") {
        effect.parameters().set_int("mode", 0);
        effect.parameters().set_int("onbeat", 0);
        effect.render(dummy_audio, 0, framebuffer.data(), fbout.data(), w, h);

        uint32_t result = framebuffer[0];
        REQUIRE(color::red(result) == 255);
        REQUIRE(color::green(result) == 128);
        REQUIRE(color::blue(result) == 64);
    }

    SECTION("MODE_RBG - swap G and B: output (R, B, G)") {
        effect.parameters().set_int("mode", 1);
        effect.parameters().set_int("onbeat", 0);
        effect.render(dummy_audio, 0, framebuffer.data(), fbout.data(), w, h);

        uint32_t result = framebuffer[0];
        // Output R = input R (255)
        // Output G = input B (64)
        // Output B = input G (128)
        REQUIRE(color::red(result) == 255);
        REQUIRE(color::green(result) == 64);
        REQUIRE(color::blue(result) == 128);
    }

    SECTION("MODE_GRB - swap R and G: output (G, R, B)") {
        effect.parameters().set_int("mode", 2);
        effect.parameters().set_int("onbeat", 0);
        effect.render(dummy_audio, 0, framebuffer.data(), fbout.data(), w, h);

        uint32_t result = framebuffer[0];
        // Output R = input G (128)
        // Output G = input R (255)
        // Output B = input B (64)
        REQUIRE(color::red(result) == 128);
        REQUIRE(color::green(result) == 255);
        REQUIRE(color::blue(result) == 64);
    }

    SECTION("MODE_GBR - rotate: output (G, B, R)") {
        effect.parameters().set_int("mode", 3);
        effect.parameters().set_int("onbeat", 0);
        effect.render(dummy_audio, 0, framebuffer.data(), fbout.data(), w, h);

        uint32_t result = framebuffer[0];
        // Output R = input G (128)
        // Output G = input B (64)
        // Output B = input R (255)
        REQUIRE(color::red(result) == 128);
        REQUIRE(color::green(result) == 64);
        REQUIRE(color::blue(result) == 255);
    }

    SECTION("MODE_BRG - rotate: output (B, R, G)") {
        effect.parameters().set_int("mode", 4);
        effect.parameters().set_int("onbeat", 0);
        effect.render(dummy_audio, 0, framebuffer.data(), fbout.data(), w, h);

        uint32_t result = framebuffer[0];
        // Output R = input B (64)
        // Output G = input R (255)
        // Output B = input G (128)
        REQUIRE(color::red(result) == 64);
        REQUIRE(color::green(result) == 255);
        REQUIRE(color::blue(result) == 128);
    }

    SECTION("MODE_BGR - swap R and B: output (B, G, R)") {
        effect.parameters().set_int("mode", 5);
        effect.parameters().set_int("onbeat", 0);
        effect.render(dummy_audio, 0, framebuffer.data(), fbout.data(), w, h);

        uint32_t result = framebuffer[0];
        // Output R = input B (64)
        // Output G = input G (128)
        // Output B = input R (255)
        REQUIRE(color::red(result) == 64);
        REQUIRE(color::green(result) == 128);
        REQUIRE(color::blue(result) == 255);
    }
}
