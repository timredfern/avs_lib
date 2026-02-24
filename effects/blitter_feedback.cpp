// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "blitter_feedback.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"
#include "core/blend.h"
#include <algorithm>
#include <cstring>

namespace avs {

BlitterFeedbackEffect::BlitterFeedbackEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
    fpos_ = 30;
}

// Zoom out (scale > 32): map larger source area to smaller dest
int BlitterFeedbackEffect::blitter_out(uint32_t* framebuffer, uint32_t* fbout,
                                        int w, int h, int f_val) {
    const int adj = 7;
    int ds_x = ((f_val + (1 << adj) - 32) << (16 - adj));
    int x_len = ((w << 16) / ds_x) & ~3;
    int y_len = (h << 16) / ds_x;

    if (x_len >= w || y_len >= h) return 0;

    int start_x = (w - x_len) / 2;
    int start_y = (h - y_len) / 2;
    int s_y = 32768;

    int blend_mode = parameters().get_int("blend");

    uint32_t* src_start = fbout + start_y * w;

    for (int y = 0; y < y_len; y++) {
        int s_x = 32768;
        uint32_t* src = framebuffer + (s_y >> 16) * w;
        uint32_t* old_dest = src_start + y * w + start_x;
        s_y += ds_x;

        if (!blend_mode) {
            for (int x = 0; x < x_len; x += 4) {
                old_dest[0] = src[s_x >> 16]; s_x += ds_x;
                old_dest[1] = src[s_x >> 16]; s_x += ds_x;
                old_dest[2] = src[s_x >> 16]; s_x += ds_x;
                old_dest[3] = src[s_x >> 16]; s_x += ds_x;
                old_dest += 4;
            }
        } else {
            uint32_t* s2 = framebuffer + ((y + start_y) * w) + start_x;
            for (int x = 0; x < x_len; x += 4) {
                old_dest[0] = BLEND_AVG(s2[0], src[s_x >> 16]); s_x += ds_x;
                old_dest[1] = BLEND_AVG(s2[1], src[s_x >> 16]); s_x += ds_x;
                old_dest[2] = BLEND_AVG(s2[2], src[s_x >> 16]); s_x += ds_x;
                old_dest[3] = BLEND_AVG(s2[3], src[s_x >> 16]); s_x += ds_x;
                s2 += 4;
                old_dest += 4;
            }
        }
    }

    // Copy result back
    uint32_t* dest = framebuffer + start_y * w + start_x;
    uint32_t* src = fbout + start_y * w + start_x;
    for (int y = 0; y < y_len; y++) {
        std::memcpy(dest, src, x_len * sizeof(uint32_t));
        dest += w;
        src += w;
    }

    return 0;
}

// Zoom in (scale < 32): map source to larger dest
int BlitterFeedbackEffect::blitter_normal(uint32_t* framebuffer, uint32_t* fbout,
                                           int w, int h, int f_val) {
    int ds_x = ((f_val + 32) << 16) / 64;
    int isx = ((w << 16) - (ds_x * w)) / 2;
    int s_y = ((h << 16) - (ds_x * h)) / 2;

    int blend_mode = parameters().get_int("blend");
    int subpixel = parameters().get_int("subpixel");

    if (subpixel) {
        // Bilinear interpolation
        for (int y = 0; y < h; y++) {
            int s_x = isx;
            uint32_t* src = framebuffer + (s_y >> 16) * w;
            int ypart = (s_y >> 8) & 0xff;
            s_y += ds_x;

            if (!blend_mode) {
                for (int x = 0; x < w; x += 4) {
                    fbout[y * w + x] = blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart); s_x += ds_x;
                    fbout[y * w + x + 1] = blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart); s_x += ds_x;
                    fbout[y * w + x + 2] = blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart); s_x += ds_x;
                    fbout[y * w + x + 3] = blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart); s_x += ds_x;
                }
            } else {
                uint32_t* s2 = framebuffer + y * w;
                for (int x = 0; x < w; x += 4) {
                    fbout[y * w + x] = BLEND_AVG(s2[x], blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart)); s_x += ds_x;
                    fbout[y * w + x + 1] = BLEND_AVG(s2[x + 1], blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart)); s_x += ds_x;
                    fbout[y * w + x + 2] = BLEND_AVG(s2[x + 2], blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart)); s_x += ds_x;
                    fbout[y * w + x + 3] = BLEND_AVG(s2[x + 3], blend_bilinear_2x2(src + (s_x >> 16), w, (s_x >> 8) & 0xff, ypart)); s_x += ds_x;
                }
            }
        }
    } else {
        // Nearest neighbor
        for (int y = 0; y < h; y++) {
            int s_x = isx;
            uint32_t* src = framebuffer + (s_y >> 16) * w;
            s_y += ds_x;

            if (!blend_mode) {
                for (int x = 0; x < w; x += 4) {
                    fbout[y * w + x] = src[s_x >> 16]; s_x += ds_x;
                    fbout[y * w + x + 1] = src[s_x >> 16]; s_x += ds_x;
                    fbout[y * w + x + 2] = src[s_x >> 16]; s_x += ds_x;
                    fbout[y * w + x + 3] = src[s_x >> 16]; s_x += ds_x;
                }
            } else {
                uint32_t* s2 = framebuffer + y * w;
                for (int x = 0; x < w; x += 4) {
                    fbout[y * w + x] = BLEND_AVG(s2[x], src[s_x >> 16]); s_x += ds_x;
                    fbout[y * w + x + 1] = BLEND_AVG(s2[x + 1], src[s_x >> 16]); s_x += ds_x;
                    fbout[y * w + x + 2] = BLEND_AVG(s2[x + 2], src[s_x >> 16]); s_x += ds_x;
                    fbout[y * w + x + 3] = BLEND_AVG(s2[x + 3], src[s_x >> 16]); s_x += ds_x;
                }
            }
        }
    }

    return 1;  // Output is in fbout
}

int BlitterFeedbackEffect::render(AudioData visdata, int isBeat,
                                   uint32_t* framebuffer, uint32_t* fbout,
                                   int w, int h) {
    if (isBeat & 0x80000000) return 0;

    int scale = parameters().get_int("scale");
    int scale2 = parameters().get_int("scale2");
    int beatch = parameters().get_int("beatch");

    // On beat, jump to scale2
    if (isBeat && beatch) {
        fpos_ = scale2;
    }

    // Animate toward target scale
    int f_val;
    if (scale < scale2) {
        f_val = (fpos_ > scale) ? fpos_ : scale;
        fpos_ -= 3;
    } else {
        f_val = (fpos_ < scale) ? fpos_ : scale;
        fpos_ += 3;
    }

    if (f_val < 0) f_val = 0;

    // f_val < 32 = zoom in, f_val > 32 = zoom out, f_val == 32 = no change
    if (f_val < 32) {
        return blitter_normal(framebuffer, fbout, w, h, f_val);
    }
    if (f_val > 32) {
        return blitter_out(framebuffer, fbout, w, h, f_val);
    }

    return 0;
}

void BlitterFeedbackEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;

    BinaryReader reader(data);

    // Binary format from r_blit.cpp:
    // scale, scale2, blend, beatch, subpixel
    int scale = reader.read_u32();
    parameters().set_int("scale", scale);
    fpos_ = scale;

    if (reader.remaining() >= 4) {
        int scale2 = reader.read_u32();
        parameters().set_int("scale2", scale2);
    }

    if (reader.remaining() >= 4) {
        int blend = reader.read_u32();
        parameters().set_int("blend", blend);
    }

    if (reader.remaining() >= 4) {
        int beatch = reader.read_u32();
        parameters().set_int("beatch", beatch);
    }

    if (reader.remaining() >= 4) {
        int subpixel = reader.read_u32();
        parameters().set_int("subpixel", subpixel);
    }
}

// Static member definition - UI layout from res.rc IDD_CFG_BLT
const PluginInfo BlitterFeedbackEffect::effect_info {
    .name = "Blitter Feedback",
    .category = "Trans",
    .description = "Zooms in or out with optional bilinear interpolation",
    .author = "",
    .version = 1,
    .legacy_index = 4,  // R_BlitterFB from rlib.cpp
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<BlitterFeedbackEffect>();
    },
    .ui_layout = {
        {
            // Original UI from res.rc IDD_CFG_BLT (dialog 137x137)
            // Blitter direction group
            {
                .id = "direction_group",
                .text = "Blitter direction",
                .type = ControlType::GROUPBOX,
                .x = 0, .y = 0, .w = 137, .h = 38
            },
            {
                .id = "scale",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 4, .y = 10, .w = 128, .h = 13,
                .range = {0, 64, 1},
                .default_val = 30
            },
            {
                .id = "zoom_in_label",
                .text = "Zooming in",
                .type = ControlType::LABEL,
                .x = 8, .y = 26, .w = 36, .h = 8
            },
            {
                .id = "zoom_out_label",
                .text = "Zooming out",
                .type = ControlType::LABEL,
                .x = 88, .y = 26, .w = 40, .h = 8
            },
            // Blitter direction (onbeat) group
            {
                .id = "onbeat_group",
                .text = "Blitter direction (onbeat)",
                .type = ControlType::GROUPBOX,
                .x = 0, .y = 41, .w = 137, .h = 50
            },
            {
                .id = "beatch",
                .text = "Enable on-beat changes",
                .type = ControlType::CHECKBOX,
                .x = 5, .y = 51, .w = 93, .h = 10,
                .default_val = 0
            },
            {
                .id = "scale2",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 4, .y = 63, .w = 128, .h = 13,
                .range = {0, 64, 1},
                .default_val = 30
            },
            {
                .id = "zoom_in_label2",
                .text = "Zooming in",
                .type = ControlType::LABEL,
                .x = 8, .y = 78, .w = 36, .h = 8
            },
            {
                .id = "zoom_out_label2",
                .text = "Zooming out",
                .type = ControlType::LABEL,
                .x = 88, .y = 78, .w = 40, .h = 8
            },
            // Bottom checkboxes
            {
                .id = "blend",
                .text = "Blend blitter",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 95, .w = 53, .h = 10,
                .default_val = 0
            },
            {
                .id = "subpixel",
                .text = "Bilinear filtering",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 104, .w = 63, .h = 10,
                .default_val = 1
            }
        }
    }
};

// Register effect at startup
static bool register_blitter_feedback = []() {
    PluginManager::instance().register_effect(BlitterFeedbackEffect::effect_info);
    return true;
}();

} // namespace avs
