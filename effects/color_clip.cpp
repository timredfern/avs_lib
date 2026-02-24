// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "color_clip.h"
#include "core/binary_reader.h"
#include "core/plugin_manager.h"
#include "core/color.h"
#include "core/ui.h"

namespace avs {

ColorClipEffect::ColorClipEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
}

int ColorClipEffect::render(AudioData visdata, int isBeat,
                             uint32_t* framebuffer, uint32_t* fbout,
                             int w, int h) {
    (void)visdata;
    (void)fbout;

    if (isBeat & 0x80000000) return 0;

    int mode = parameters().get_int("mode");
    if (mode == 0) return 0;  // Off

    uint32_t color_clip = parameters().get_color("color_clip");
    uint32_t color_out = parameters().get_color("color_out");
    int distance = parameters().get_int("distance");

    // Extract threshold RGB
    int clip_r = color::red(color_clip);
    int clip_g = color::green(color_clip);
    int clip_b = color::blue(color_clip);

    // Mask out alpha from output color, preserve input alpha
    uint32_t out_rgb = color_out & 0x00FFFFFF;

    int num_pixels = w * h;

    if (mode == 1) {
        // Below: replace if all components <= threshold
        for (int i = 0; i < num_pixels; i++) {
            uint32_t pixel = framebuffer[i];
            int r = color::red(pixel);
            int g = color::green(pixel);
            int b = color::blue(pixel);

            if (r <= clip_r && g <= clip_g && b <= clip_b) {
                framebuffer[i] = (pixel & 0xFF000000) | out_rgb;
            }
        }
    } else if (mode == 2) {
        // Above: replace if all components >= threshold
        for (int i = 0; i < num_pixels; i++) {
            uint32_t pixel = framebuffer[i];
            int r = color::red(pixel);
            int g = color::green(pixel);
            int b = color::blue(pixel);

            if (r >= clip_r && g >= clip_g && b >= clip_b) {
                framebuffer[i] = (pixel & 0xFF000000) | out_rgb;
            }
        }
    } else {
        // Near: replace if euclidean distance <= threshold
        int dist_sq = distance * 2;
        dist_sq = dist_sq * dist_sq;

        for (int i = 0; i < num_pixels; i++) {
            uint32_t pixel = framebuffer[i];
            int r = color::red(pixel);
            int g = color::green(pixel);
            int b = color::blue(pixel);

            int dr = r - clip_r;
            int dg = g - clip_g;
            int db = b - clip_b;

            if (dr * dr + dg * dg + db * db <= dist_sq) {
                framebuffer[i] = (pixel & 0xFF000000) | out_rgb;
            }
        }
    }

    return 0;
}

void ColorClipEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;

    BinaryReader reader(data);

    // enabled (mode): 0=off, 1=below, 2=above, 3=near
    int mode = static_cast<int>(reader.read_u32());
    parameters().set_int("mode", mode);

    // color_clip (threshold color)
    // Colors stored as 0x00RRGGBB (same as internal ARGB), no swap needed
    if (reader.remaining() >= 4) {
        uint32_t color = reader.read_u32() | 0xFF000000;
        parameters().set_color("color_clip", color);
    }

    // color_clip_out (output color)
    // Colors stored as 0x00RRGGBB (same as internal ARGB), no swap needed
    if (reader.remaining() >= 4) {
        uint32_t color = reader.read_u32() | 0xFF000000;
        parameters().set_color("color_out", color);
    }

    // color_dist (distance for "near" mode)
    if (reader.remaining() >= 4) {
        int dist = static_cast<int>(reader.read_u32());
        parameters().set_int("distance", dist);
    }
}

// Static member definition
const PluginInfo ColorClipEffect::effect_info {
    .name = "Color Clip",
    .category = "Trans",
    .description = "Replace pixels matching color threshold",
    .author = "",
    .version = 1,
    .legacy_index = 12,  // R_ContrastEnhance
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<ColorClipEffect>();
    },
    .ui_layout = {
        {
            // Mode radio buttons
            {
                .id = "mode",
                .type = ControlType::RADIO_GROUP,
                .default_val = 1,  // Below
                .radio_options = {
                    {"Off", 0, 0, 25, 10},
                    {"Below", 0, 10, 35, 10},
                    {"Above", 0, 20, 37, 10},
                    {"Near", 0, 30, 31, 10}
                }
            },
            // Distance slider (for Near mode)
            {
                .id = "distance",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 36, .y = 29, .w = 101, .h = 13,
                .range = {0, 64},
                .default_val = 10
            },
            // Threshold color
            {
                .id = "color_clip",
                .text = "color",
                .type = ControlType::COLOR_BUTTON,
                .x = 0, .y = 46, .w = 26, .h = 10,
                .default_val = static_cast<int>(0xFF202020)  // Dark gray
            },
            // Arrow button label
            {
                .id = "arrow_label",
                .text = "->",
                .type = ControlType::LABEL,
                .x = 28, .y = 46, .w = 17, .h = 10
            },
            // Output color
            {
                .id = "color_out",
                .text = "outcolor",
                .type = ControlType::COLOR_BUTTON,
                .x = 47, .y = 46, .w = 37, .h = 10,
                .default_val = static_cast<int>(0xFF202020)  // Dark gray
            }
        }
    }
};

// Register effect at startup
static bool register_color_clip = []() {
    PluginManager::instance().register_effect(ColorClipEffect::effect_info);
    return true;
}();

} // namespace avs
