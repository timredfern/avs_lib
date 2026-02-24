// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "channel_shift.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"
#include "core/color.h"
#include <cstdlib>
#include <ctime>

namespace avs {

ChannelShiftEffect::ChannelShiftEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

int ChannelShiftEffect::render(AudioData visdata, int isBeat,
                                uint32_t* framebuffer, uint32_t* fbout,
                                int w, int h) {
    if (isBeat & 0x80000000) return 0;

    int mode = parameters().get_int("mode");
    int onbeat = parameters().get_int("onbeat");

    // On beat, randomly select a new mode
    if (isBeat && onbeat) {
        mode = std::rand() % 6;
        parameters().set_int("mode", mode);
    }

    int count = w * h;

    // ARGB format: A=bits 24-31, R=bits 16-23, G=bits 8-15, B=bits 0-7
    using avs::color::red;
    using avs::color::green;
    using avs::color::blue;

    switch (mode) {
        case MODE_RGB:  // No change
            break;

        case MODE_RBG:  // Swap G and B: output (R, B, G)
            for (int i = 0; i < count; i++) {
                uint32_t pix = framebuffer[i];
                uint8_t r = red(pix);
                uint8_t g = green(pix);
                uint8_t b = blue(pix);
                framebuffer[i] = (pix & 0xFF000000) | (r << 16) | (b << 8) | g;
            }
            break;

        case MODE_GRB:  // Swap R and G: output (G, R, B)
            for (int i = 0; i < count; i++) {
                uint32_t pix = framebuffer[i];
                uint8_t r = red(pix);
                uint8_t g = green(pix);
                uint8_t b = blue(pix);
                framebuffer[i] = (pix & 0xFF000000) | (g << 16) | (r << 8) | b;
            }
            break;

        case MODE_GBR:  // Rotate: R->B, G->R, B->G: output (G, B, R)
            for (int i = 0; i < count; i++) {
                uint32_t pix = framebuffer[i];
                uint8_t r = red(pix);
                uint8_t g = green(pix);
                uint8_t b = blue(pix);
                framebuffer[i] = (pix & 0xFF000000) | (g << 16) | (b << 8) | r;
            }
            break;

        case MODE_BRG:  // Rotate: R->G, G->B, B->R: output (B, R, G)
            for (int i = 0; i < count; i++) {
                uint32_t pix = framebuffer[i];
                uint8_t r = red(pix);
                uint8_t g = green(pix);
                uint8_t b = blue(pix);
                framebuffer[i] = (pix & 0xFF000000) | (b << 16) | (r << 8) | g;
            }
            break;

        case MODE_BGR:  // Swap R and B: output (B, G, R)
            for (int i = 0; i < count; i++) {
                uint32_t pix = framebuffer[i];
                uint8_t r = red(pix);
                uint8_t b = blue(pix);
                framebuffer[i] = (pix & 0xFF00FF00) | (b << 16) | r;
            }
            break;
    }

    return 0;
}

void ChannelShiftEffect::load_parameters(const std::vector<uint8_t>& data) {
    // Original uses apeconfig struct: { int mode; int onbeat; }
    if (data.size() < 4) return;

    BinaryReader reader(data);
    int mode = reader.read_u32();
    parameters().set_int("mode", mode);

    if (reader.remaining() >= 4) {
        int onbeat = reader.read_u32();
        parameters().set_int("onbeat", onbeat);
    }
}

// Static member definition - UI layout from res.rc IDD_CFG_CHANSHIFT
const PluginInfo ChannelShiftEffect::effect_info {
    .name = "Channel Shift",
    .category = "Trans",
    .description = "Swaps or rotates color channels",
    .author = "",
    .version = 1,
    .legacy_index = -1,  // APE plugin, uses string ID "Channel Shift"
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<ChannelShiftEffect>();
    },
    .ui_layout = {
        {
            {
                .id = "mode",
                .text = "",
                .type = ControlType::RADIO_GROUP,
                .x = 0, .y = 0, .w = 100, .h = 90,
                .default_val = 1,  // Default to RBG
                .radio_options = {
                    {"RGB (no change)", 0, 0, 90, 10},
                    {"RBG", 0, 14, 90, 10},
                    {"GRB", 0, 28, 90, 10},
                    {"GBR", 0, 42, 90, 10},
                    {"BRG", 0, 56, 90, 10},
                    {"BGR", 0, 70, 90, 10}
                }
            },
            {
                .id = "onbeat",
                .text = "On beat random",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 95, .w = 100, .h = 10,
                .default_val = 1
            }
        }
    }
};

// Register effect at startup
static bool register_channel_shift = []() {
    PluginManager::instance().register_effect(ChannelShiftEffect::effect_info);
    return true;
}();

} // namespace avs
