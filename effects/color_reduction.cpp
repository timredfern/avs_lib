// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "color_reduction.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"

namespace avs {

ColorReductionEffect::ColorReductionEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
}

int ColorReductionEffect::render(AudioData visdata, int isBeat,
                                  uint32_t* framebuffer, uint32_t* fbout,
                                  int w, int h) {
    if (isBeat & 0x80000000) return 0;

    int levels = parameters().get_int("levels");

    // Build mask based on levels (1-8)
    // levels=8 means 256 colors per channel (full), levels=1 means 2 colors
    int shift = 8 - levels;
    uint32_t mask = 0xFF;
    for (int i = 0; i < shift; i++) {
        mask = (mask << 1) & 0xFF;
    }
    // Apply to all channels
    mask = mask | (mask << 8) | (mask << 16);

    int count = w * h;
    for (int i = 0; i < count; i++) {
        framebuffer[i] = (framebuffer[i] & 0xFF000000) | (framebuffer[i] & mask);
    }

    return 0;
}

void ColorReductionEffect::load_parameters(const std::vector<uint8_t>& data) {
    // The original uses an apeconfig struct with MAX_PATH fname followed by int levels
    // fname is 260 bytes on Windows, so levels starts at offset 260
    if (data.size() < 264) {
        // If not enough data, default to 7
        parameters().set_int("levels", 7);
        return;
    }

    // Skip the filename (260 bytes) and read levels
    int levels = data[260] | (data[261] << 8) | (data[262] << 16) | (data[263] << 24);
    parameters().set_int("levels", levels);
}

// Static member definition - UI layout from res.rc IDD_CFG_COLORREDUCTION
const PluginInfo ColorReductionEffect::effect_info {
    .name = "Color Reduction",
    .category = "Trans",
    .description = "Reduces the number of color levels per channel",
    .author = "",
    .version = 1,
    .legacy_index = -1,  // APE plugin, uses string ID "Color Reduction"
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<ColorReductionEffect>();
    },
    .ui_layout = {
        {
            // Original UI from res.rc IDD_CFG_COLORREDUCTION (dialog 231x216)
            {
                .id = "levels_group",
                .text = "Color levels",
                .type = ControlType::GROUPBOX,
                .x = 5, .y = 5, .w = 220, .h = 35
            },
            {
                .id = "levels",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 10, .y = 15, .w = 185, .h = 20,
                .range = {1, 8, 1},
                .default_val = 7
            },
            {
                .id = "levels_label",
                .text = "Static",
                .type = ControlType::LABEL,
                .x = 200, .y = 17, .w = 20, .h = 10
            }
        }
    }
};

// Register effect at startup
static bool register_color_reduction = []() {
    PluginManager::instance().register_effect(ColorReductionEffect::effect_info);
    return true;
}();

} // namespace avs
