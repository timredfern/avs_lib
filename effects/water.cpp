// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "water.h"
#include "core/binary_reader.h"
#include "core/color.h"
#include "core/parallel.h"
#include "core/plugin_manager.h"
#include <algorithm>
#include <cstring>

namespace avs {

// Use in-place channel extraction for efficiency (no shifts in inner loop)
using avs::color::red_i;
using avs::color::green_i;
using avs::color::blue_i;
using avs::color::make_i;
using avs::color::RED_MAX_I;
using avs::color::GREEN_MAX_I;
using avs::color::BLUE_MAX_I;

WaterEffect::WaterEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
}

int WaterEffect::render(AudioData visdata, int isBeat,
                         uint32_t* framebuffer, uint32_t* fbout,
                         int w, int h) {
    if (isBeat & 0x80000000) return 0;
    if (!is_enabled()) return 0;

    // Resize buffer if needed
    if (w != last_w_ || h != last_h_) {
        lastframe_.resize(w * h);
        std::fill(lastframe_.begin(), lastframe_.end(), 0);
        last_w_ = w;
        last_h_ = h;
    }

    uint32_t* f = framebuffer;
    uint32_t* of = fbout;
    uint32_t* lfo = lastframe_.data();

    // Top line
    {
        // Left edge
        {
            int r = red_i(f[1]);
            int g = green_i(f[1]);
            int b = blue_i(f[1]);
            r += red_i(f[w]);
            g += green_i(f[w]);
            b += blue_i(f[w]);
            f++;

            r -= red_i(lfo[0]);
            g -= green_i(lfo[0]);
            b -= blue_i(lfo[0]);
            lfo++;

            r = std::clamp(r, 0, RED_MAX_I);
            g = std::clamp(g, 0, GREEN_MAX_I);
            b = std::clamp(b, 0, BLUE_MAX_I);
            *of++ = make_i(r, g, b);
        }

        // Middle of top line
        for (int x = 0; x < w - 2; x++) {
            int r = red_i(f[1]);
            int g = green_i(f[1]);
            int b = blue_i(f[1]);
            r += red_i(f[-1]);
            g += green_i(f[-1]);
            b += blue_i(f[-1]);
            r += red_i(f[w]);
            g += green_i(f[w]);
            b += blue_i(f[w]);
            f++;

            r /= 2;
            g /= 2;
            b /= 2;

            r -= red_i(lfo[0]);
            g -= green_i(lfo[0]);
            b -= blue_i(lfo[0]);
            lfo++;

            r = std::clamp(r, 0, RED_MAX_I);
            g = std::clamp(g, 0, GREEN_MAX_I);
            b = std::clamp(b, 0, BLUE_MAX_I);
            *of++ = make_i(r, g, b);
        }

        // Right edge
        {
            int r = red_i(f[-1]);
            int g = green_i(f[-1]);
            int b = blue_i(f[-1]);
            r += red_i(f[w]);
            g += green_i(f[w]);
            b += blue_i(f[w]);
            f++;

            r -= red_i(lfo[0]);
            g -= green_i(lfo[0]);
            b -= blue_i(lfo[0]);
            lfo++;

            r = std::clamp(r, 0, RED_MAX_I);
            g = std::clamp(g, 0, GREEN_MAX_I);
            b = std::clamp(b, 0, BLUE_MAX_I);
            *of++ = make_i(r, g, b);
        }
    }

    // Middle rows (parallelized - reads from framebuffer + lastframe_, writes to fbout)
    {
        uint32_t* base_f = framebuffer;
        uint32_t* base_of = fbout;
        uint32_t* base_lfo = lastframe_.data();

        parallel_for_rows(h - 2, [&](int ys, int ye) {
            for (int y = ys + 1; y < ye + 1; y++) {
                uint32_t* f = base_f + y * w;
                uint32_t* of = base_of + y * w;
                uint32_t* lfo = base_lfo + y * w;

                // Left edge
                {
                    int r = red_i(f[1]) + red_i(f[w]) + red_i(f[-w]);
                    int g = green_i(f[1]) + green_i(f[w]) + green_i(f[-w]);
                    int b = blue_i(f[1]) + blue_i(f[w]) + blue_i(f[-w]);
                    r = r / 2 - red_i(lfo[0]);
                    g = g / 2 - green_i(lfo[0]);
                    b = b / 2 - blue_i(lfo[0]);
                    *of++ = make_i(std::clamp(r, 0, RED_MAX_I),
                                   std::clamp(g, 0, GREEN_MAX_I),
                                   std::clamp(b, 0, BLUE_MAX_I));
                    f++; lfo++;
                }

                // Middle pixels
                for (int x = 0; x < w - 2; x++) {
                    int r = red_i(f[1]) + red_i(f[-1]) + red_i(f[w]) + red_i(f[-w]);
                    int g = green_i(f[1]) + green_i(f[-1]) + green_i(f[w]) + green_i(f[-w]);
                    int b = blue_i(f[1]) + blue_i(f[-1]) + blue_i(f[w]) + blue_i(f[-w]);
                    r = r / 2 - red_i(lfo[0]);
                    g = g / 2 - green_i(lfo[0]);
                    b = b / 2 - blue_i(lfo[0]);
                    *of++ = make_i(std::clamp(r, 0, RED_MAX_I),
                                   std::clamp(g, 0, GREEN_MAX_I),
                                   std::clamp(b, 0, BLUE_MAX_I));
                    f++; lfo++;
                }

                // Right edge
                {
                    int r = red_i(f[-1]) + red_i(f[w]) + red_i(f[-w]);
                    int g = green_i(f[-1]) + green_i(f[w]) + green_i(f[-w]);
                    int b = blue_i(f[-1]) + blue_i(f[w]) + blue_i(f[-w]);
                    r = r / 2 - red_i(lfo[0]);
                    g = g / 2 - green_i(lfo[0]);
                    b = b / 2 - blue_i(lfo[0]);
                    *of++ = make_i(std::clamp(r, 0, RED_MAX_I),
                                   std::clamp(g, 0, GREEN_MAX_I),
                                   std::clamp(b, 0, BLUE_MAX_I));
                    f++; lfo++;
                }
            }
        });
        f = base_f + (h - 1) * w;
        of = base_of + (h - 1) * w;
        lfo = base_lfo + (h - 1) * w;
    }

    // Bottom line
    {
        // Left edge
        {
            int r = red_i(f[1]);
            int g = green_i(f[1]);
            int b = blue_i(f[1]);
            r += red_i(f[-w]);
            g += green_i(f[-w]);
            b += blue_i(f[-w]);
            f++;

            r -= red_i(lfo[0]);
            g -= green_i(lfo[0]);
            b -= blue_i(lfo[0]);
            lfo++;

            r = std::clamp(r, 0, RED_MAX_I);
            g = std::clamp(g, 0, GREEN_MAX_I);
            b = std::clamp(b, 0, BLUE_MAX_I);
            *of++ = make_i(r, g, b);
        }

        // Middle of bottom line
        for (int x = 0; x < w - 2; x++) {
            int r = red_i(f[1]);
            int g = green_i(f[1]);
            int b = blue_i(f[1]);
            r += red_i(f[-1]);
            g += green_i(f[-1]);
            b += blue_i(f[-1]);
            r += red_i(f[-w]);
            g += green_i(f[-w]);
            b += blue_i(f[-w]);
            f++;

            r /= 2;
            g /= 2;
            b /= 2;

            r -= red_i(lfo[0]);
            g -= green_i(lfo[0]);
            b -= blue_i(lfo[0]);
            lfo++;

            r = std::clamp(r, 0, RED_MAX_I);
            g = std::clamp(g, 0, GREEN_MAX_I);
            b = std::clamp(b, 0, BLUE_MAX_I);
            *of++ = make_i(r, g, b);
        }

        // Right edge
        {
            int r = red_i(f[-1]);
            int g = green_i(f[-1]);
            int b = blue_i(f[-1]);
            r += red_i(f[-w]);
            g += green_i(f[-w]);
            b += blue_i(f[-w]);
            f++;

            r -= red_i(lfo[0]);
            g -= green_i(lfo[0]);
            b -= blue_i(lfo[0]);
            lfo++;

            r = std::clamp(r, 0, RED_MAX_I);
            g = std::clamp(g, 0, GREEN_MAX_I);
            b = std::clamp(b, 0, BLUE_MAX_I);
            *of++ = make_i(r, g, b);
        }
    }

    // Save current frame for next iteration
    std::memcpy(lastframe_.data(), framebuffer, w * h * sizeof(uint32_t));

    return 1;  // Use fbout
}

void WaterEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    BinaryReader reader(data);

    if (reader.remaining() >= 4) {
        int enabled = static_cast<int>(reader.read_u32());
        parameters().set_bool("enabled", enabled != 0);
    }
}

const PluginInfo WaterEffect::effect_info{
    .name = "Water",
    .category = "Trans",
    .description = "Water ripple effect",
    .author = "",
    .version = 1,
    .legacy_index = 20,
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<WaterEffect>();
    },
    .ui_layout = {
        {
            {
                .id = "enabled",
                .text = "Enable Water effect",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 0, .w = 79, .h = 10,
                .default_val = true
            }
        }
    }
};

// Register effect at startup
static bool register_water = []() {
    PluginManager::instance().register_effect(WaterEffect::effect_info);
    return true;
}();

}  // namespace avs
