// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "rotoblitter.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"
#include "core/blend.h"
#include "core/parallel.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace avs {

// Original parameter ranges from r_rotblit.cpp
// zoom_scale: 0-256, default 31 (31 = no zoom, zoom = 1.0)
// rot_dir: 0-64, default 31 (32 = no rotation)
// beatch_speed: 0-8

RotoBlitterEffect::RotoBlitterEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
    current_zoom_ = parameters().get_int("zoom_scale");
}

int RotoBlitterEffect::render(AudioData visdata, int isBeat,
                              uint32_t* framebuffer, uint32_t* fbout,
                              int w, int h) {
    // Track size for any dimension-dependent calculations
    if (last_w_ != w || last_h_ != h) {
        last_w_ = w;
        last_h_ = h;
    }

    if (isBeat & 0x80000000) return 0;

    auto* src = framebuffer;
    auto* dest = fbout;
    auto* bdest = framebuffer;  // For 50/50 blend source

    int beatch = parameters().get_bool("beatch") ? 1 : 0;
    int beatch_speed = parameters().get_int("beatch_speed");
    int beatch_scale = parameters().get_bool("beatch_scale") ? 1 : 0;
    int zoom_scale = parameters().get_int("zoom_scale");
    int zoom_scale2 = parameters().get_int("zoom_scale2");
    int rot_dir = parameters().get_int("rot_dir");
    int blend = parameters().get_bool("blend") ? 1 : 0;
    int subpixel = parameters().get_bool("subpixel") ? 1 : 0;

    // Handle beat-triggered direction reversal (original: rot_rev)
    if (isBeat && beatch) {
        direction_ = -direction_;
    }
    if (!beatch) {
        direction_ = 1;
    }

    // Smooth transition to target direction (original: rot_rev_pos)
    current_rotation_ += (1.0 / (1 + beatch_speed * 4)) * (direction_ - current_rotation_);
    if (current_rotation_ > direction_ && direction_ > 0) {
        current_rotation_ = direction_;
    }
    if (current_rotation_ < direction_ && direction_ < 0) {
        current_rotation_ = direction_;
    }

    // Handle beat-triggered zoom (original: scale_fpos)
    if (isBeat && beatch_scale) {
        current_zoom_ = zoom_scale2;
    }

    // Interpolate zoom towards target (original: f_val calculation)
    int f_val;
    if (zoom_scale < zoom_scale2) {
        f_val = std::max((int)current_zoom_, zoom_scale);
        if (current_zoom_ > zoom_scale) {
            current_zoom_ -= 3;
        }
    } else {
        f_val = std::min((int)current_zoom_, zoom_scale);
        if (current_zoom_ < zoom_scale) {
            current_zoom_ += 3;
        }
    }

    // Original zoom formula: zoom = 1.0 + (f_val-31)/31.0
    double zoom = 1.0 + (f_val - 31) / 31.0;

    // Original rotation formula: theta = (rot_dir-32) * rot_rev_pos
    double theta = (rot_dir - 32) * current_rotation_;

    // Calculate rotation transform (fixed-point 16.16)
    double temp = cos(theta * M_PI / 180.0) * zoom;
    int ds_dx = (int)(temp * 65536.0);
    int dt_dy = (int)(temp * 65536.0);
    temp = sin(theta * M_PI / 180.0) * zoom;
    int ds_dy = -(int)(temp * 65536.0);
    int dt_dx = (int)(temp * 65536.0);

    // Starting position (center of output maps to center of input)
    // Use int64_t to avoid overflow at high resolutions (retina 3456x2234)
    int64_t ds64 = int64_t(w - 1) << 16;
    int64_t dt64 = int64_t(h - 1) << 16;

    int64_t sstart64 = -(int64_t((w - 1) / 2) * ds_dx + int64_t((h - 1) / 2) * ds_dy)
                       + int64_t(w - 1) * (32768 + (1 << 20));
    int64_t tstart64 = -(int64_t((w - 1) / 2) * dt_dx + int64_t((h - 1) / 2) * dt_dy)
                       + int64_t(h - 1) * (32768 + (1 << 20));

    // Normalize to [0, ds) and [0, dt) range to fit in int32
    if (ds64) sstart64 = ((sstart64 % ds64) + ds64) % ds64;
    if (dt64) tstart64 = ((tstart64 % dt64) + dt64) % dt64;

    int sstart = static_cast<int>(sstart64);
    int tstart = static_cast<int>(tstart64);
    int ds = static_cast<int>(ds64);
    int dt = static_cast<int>(dt64);

    // Only render if transform stays in bounds (original check)
    if (ds_dx <= -ds || ds_dx >= ds || dt_dx <= -dt || dt_dx >= dt) {
        // Out of bounds - skip rendering
    } else {
        // Pixel-write lambda, templated to eliminate per-pixel branch
        auto do_rows = [&](auto pixel_fn) {
            parallel_for_rows(h, [&](int y_start, int y_end) {
                int ls = sstart + y_start * ds_dy;
                int lt = tstart + y_start * dt_dy;
                uint32_t* rd = dest + y_start * w;
                uint32_t* rb = bdest + y_start * w;

                for (int y = y_start; y < y_end; y++) {
                    if (ds) ls %= ds;
                    if (dt) lt %= dt;
                    if (ls < 0) ls += ds;
                    if (lt < 0) lt += dt;

                    int rs = ls, rt = lt;
                    for (int x = w; x > 0; x--) {
                        if (ds_dx > 0) { if (rs >= ds) rs -= ds; }
                        else { if (rs < 0) rs += ds; }
                        if (dt_dx > 0) { if (rt >= dt) rt -= dt; }
                        else { if (rt < 0) rt += dt; }

                        pixel_fn(src, (rs >> 16) + (rt >> 16) * w,
                                 rs, rt, rd, rb);
                        rd++;
                        rs += ds_dx;
                        rt += dt_dx;
                    }

                    ls += ds_dy;
                    lt += dt_dy;
                }
            });
        };

        if (subpixel && blend) {
            do_rows([&](const uint32_t* s, int idx, int rs, int rt,
                        uint32_t* d, uint32_t*& b) {
                *d = BLEND_AVG(*b++, blend_bilinear_2x2(
                    &s[idx], w, (rs >> 8) & 0xff, (rt >> 8) & 0xff));
            });
        } else if (subpixel) {
            do_rows([&](const uint32_t* s, int idx, int rs, int rt,
                        uint32_t* d, uint32_t*&) {
                *d = blend_bilinear_2x2(
                    &s[idx], w, (rs >> 8) & 0xff, (rt >> 8) & 0xff);
            });
        } else if (blend) {
            do_rows([&](const uint32_t* s, int idx, int, int,
                        uint32_t* d, uint32_t*& b) {
                *d = BLEND_AVG(*b++, s[idx]);
            });
        } else {
            do_rows([&](const uint32_t* s, int idx, int, int,
                        uint32_t* d, uint32_t*&) {
                *d = s[idx];
            });
        }
    }

    return 1;  // Output in fbout
}

void RotoBlitterEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;

    BinaryReader reader(data);

    // Binary format from r_rotblit.cpp:
    // zoom_scale (int32) - 0-256, default 31
    // rot_dir (int32) - 0-64, default 31
    // blend (int32) - 0/1
    // beatch (int32) - 0/1 on beat reverse
    // beatch_speed (int32) - 0-8
    // zoom_scale2 (int32) - 0-256, on beat zoom target
    // beatch_scale (int32) - 0/1 on beat zoom enable
    // subpixel (int32) - 0/1 bilinear filtering

    if (reader.remaining() >= 4) {
        parameters().set_int("zoom_scale", reader.read_u32());
    }
    if (reader.remaining() >= 4) {
        parameters().set_int("rot_dir", reader.read_u32());
    }
    if (reader.remaining() >= 4) {
        parameters().set_bool("blend", reader.read_u32() != 0);
    }
    if (reader.remaining() >= 4) {
        parameters().set_bool("beatch", reader.read_u32() != 0);
    }
    if (reader.remaining() >= 4) {
        parameters().set_int("beatch_speed", reader.read_u32());
    }
    if (reader.remaining() >= 4) {
        parameters().set_int("zoom_scale2", reader.read_u32());
    }
    if (reader.remaining() >= 4) {
        parameters().set_bool("beatch_scale", reader.read_u32() != 0);
    }
    if (reader.remaining() >= 4) {
        parameters().set_bool("subpixel", reader.read_u32() != 0);
    }

    current_zoom_ = parameters().get_int("zoom_scale");
}

// Static member definition
const PluginInfo RotoBlitterEffect::effect_info {
    .name = "Roto Blitter",
    .category = "Trans",
    .description = "Rotating bitmap blitter with zoom",
    .author = "",
    .version = 1,
    .legacy_index = 9,  // R_RotBlit
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<RotoBlitterEffect>();
    },
    .ui_layout = {
        {
            // Original UI from res.rc IDD_CFG_ROTBLT (dialog 137x157)
            // Zoom group
            {
                .id = "zoom_group",
                .text = "Zoom",
                .type = ControlType::GROUPBOX,
                .x = 0, .y = 0, .w = 137, .h = 146
            },
            {
                .id = "zoom_scale",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 4, .y = 10, .w = 128, .h = 15,
                .range = {0, 256, 16},
                .default_val = 31
            },
            {
                .id = "beatch_scale",
                .text = "Enable on-beat change",
                .type = ControlType::CHECKBOX,
                .x = 5, .y = 28, .w = 90, .h = 10,
                .default_val = 0
            },
            {
                .id = "zoom_scale2",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 4, .y = 37, .w = 128, .h = 13,
                .range = {0, 256, 16},
                .default_val = 31
            },
            {
                .id = "zoom_in_label",
                .text = "Zooming in",
                .type = ControlType::LABEL,
                .x = 8, .y = 52, .w = 36, .h = 8
            },
            {
                .id = "zoom_out_label",
                .text = "Zooming out",
                .type = ControlType::LABEL,
                .x = 88, .y = 52, .w = 40, .h = 8
            },
            // Rotation group
            {
                .id = "rotation_group",
                .text = "Rotation",
                .type = ControlType::GROUPBOX,
                .x = 0, .y = 69, .w = 137, .h = 77
            },
            {
                .id = "rot_dir",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 4, .y = 78, .w = 128, .h = 15,
                .range = {0, 64, 4},
                .default_val = 32
            },
            {
                .id = "rot_left_label",
                .text = "Rotating Left",
                .type = ControlType::LABEL,
                .x = 7, .y = 96, .w = 42, .h = 8
            },
            {
                .id = "rot_right_label",
                .text = "Rotating Right",
                .type = ControlType::LABEL,
                .x = 83, .y = 96, .w = 46, .h = 8
            },
            {
                .id = "beatch",
                .text = "Enable on-beat reversal",
                .type = ControlType::CHECKBOX,
                .x = 7, .y = 107, .w = 91, .h = 10,
                .default_val = 0
            },
            {
                .id = "beatch_speed",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 4, .y = 115, .w = 128, .h = 15,
                .range = {0, 8, 1},
                .default_val = 0
            },
            {
                .id = "fast_rev_label",
                .text = "Fast reversal",
                .type = ControlType::LABEL,
                .x = 8, .y = 132, .w = 41, .h = 8
            },
            {
                .id = "slow_rev_label",
                .text = "Slow reversal",
                .type = ControlType::LABEL,
                .x = 83, .y = 132, .w = 43, .h = 8
            },
            // Bottom checkboxes
            {
                .id = "blend",
                .text = "Blend blitter",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 147, .w = 53, .h = 10,
                .default_val = 0
            },
            {
                .id = "subpixel",
                .text = "Bilinear filtering",
                .type = ControlType::CHECKBOX,
                .x = 55, .y = 147, .w = 63, .h = 10,
                .default_val = 1
            }
        }
    }
};

// Register effect at startup
static bool register_rotoblitter = []() {
    PluginManager::instance().register_effect(RotoBlitterEffect::effect_info);
    return true;
}();

} // namespace avs
