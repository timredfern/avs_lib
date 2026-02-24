// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "water_bump.h"
#include "core/binary_reader.h"
#include "core/plugin_manager.h"
#include "core/ui.h"
#include <cmath>
#include <cstdlib>

namespace avs {

WaterBumpEffect::WaterBumpEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
}

void WaterBumpEffect::sine_blob(int x, int y, int radius, int height, int page) {
    int cx, cy;
    int left, top, right, bottom;
    int square;
    double dist;
    int radsquare = radius * radius;
    double length = (1024.0 / static_cast<double>(radius)) * (1024.0 / static_cast<double>(radius));

    // Random position if x or y is negative
    if (x < 0) x = 1 + radius + rand() % (buffer_w_ - 2 * radius - 1);
    if (y < 0) y = 1 + radius + rand() % (buffer_h_ - 2 * radius - 1);

    left = -radius;
    right = radius;
    top = -radius;
    bottom = radius;

    // Edge clipping
    if (x - radius < 1) left -= (x - radius - 1);
    if (y - radius < 1) top -= (y - radius - 1);
    if (x + radius > buffer_w_ - 1) right -= (x + radius - buffer_w_ + 1);
    if (y + radius > buffer_h_ - 1) bottom -= (y + radius - buffer_h_ + 1);

    for (cy = top; cy < bottom; cy++) {
        for (cx = left; cx < right; cx++) {
            square = cy * cy + cx * cx;
            if (square < radsquare) {
                dist = sqrt(square * length);
                buffers_[page][buffer_w_ * (cy + y) + cx + x] +=
                    static_cast<int>((cos(dist) + 0xffff) * height) >> 19;
            }
        }
    }
}

void WaterBumpEffect::calc_water(int npage, int density) {
    int newh;
    int count = buffer_w_ + 1;

    int* newptr = buffers_[npage].data();
    int* oldptr = buffers_[!npage].data();

    int x, y;

    for (y = (buffer_h_ - 1) * buffer_w_; count < y; count += 2) {
        for (x = count + buffer_w_ - 2; count < x; count++) {
            // Eight-pixel neighbor averaging method
            newh = ((oldptr[count + buffer_w_]
                   + oldptr[count - buffer_w_]
                   + oldptr[count + 1]
                   + oldptr[count - 1]
                   + oldptr[count - buffer_w_ - 1]
                   + oldptr[count - buffer_w_ + 1]
                   + oldptr[count + buffer_w_ - 1]
                   + oldptr[count + buffer_w_ + 1]
                   ) >> 2)
                   - newptr[count];

            newptr[count] = newh - (newh >> density);
        }
    }
}

int WaterBumpEffect::render(AudioData visdata, int isBeat,
                             uint32_t* framebuffer, uint32_t* fbout,
                             int w, int h) {
    (void)visdata;

    if (!parameters().get_bool("enabled")) return 0;

    int len = w * h;

    // Reallocate buffers if size changed
    if (buffer_w_ != w || buffer_h_ != h) {
        buffers_[0].clear();
        buffers_[1].clear();
    }

    if (buffers_[0].empty()) {
        buffers_[0].resize(len, 0);
        buffers_[1].resize(len, 0);
        buffer_w_ = w;
        buffer_h_ = h;
    }

    if (isBeat & 0x80000000) return 0;

    // Create ripple on beat
    if (isBeat) {
        int depth = parameters().get_int("depth");
        int drop_radius = parameters().get_int("drop_radius");
        bool random_drop = parameters().get_bool("random_drop");

        if (random_drop) {
            int max_dim = (h > w) ? h : w;
            sine_blob(-1, -1, drop_radius * max_dim / 100, -depth, page_);
        } else {
            int drop_x = parameters().get_int("drop_position_x");
            int drop_y = parameters().get_int("drop_position_y");

            int x, y;
            switch (drop_x) {
                case 0: x = w / 4; break;
                case 1: x = w / 2; break;
                case 2: x = w * 3 / 4; break;
                default: x = w / 2; break;
            }
            switch (drop_y) {
                case 0: y = h / 4; break;
                case 1: y = h / 2; break;
                case 2: y = h * 3 / 4; break;
                default: y = h / 2; break;
            }
            sine_blob(x, y, drop_radius, -depth, page_);
        }
    }

    // Render water displacement
    {
        int dx, dy;
        int x, y;
        int ofs;
        int offset = buffer_w_ + 1;

        int* ptr = buffers_[page_].data();

        for (y = (buffer_h_ - 1) * buffer_w_; offset < y; offset += 2) {
            for (x = offset + buffer_w_ - 2; offset < x; offset++) {
                dx = ptr[offset] - ptr[offset + 1];
                dy = ptr[offset] - ptr[offset + buffer_w_];
                ofs = offset + buffer_w_ * (dy >> 3) + (dx >> 3);
                if (ofs < len && ofs > -1)
                    fbout[offset] = framebuffer[ofs];
                else
                    fbout[offset] = framebuffer[offset];

                offset++;
                dx = ptr[offset] - ptr[offset + 1];
                dy = ptr[offset] - ptr[offset + buffer_w_];
                ofs = offset + buffer_w_ * (dy >> 3) + (dx >> 3);
                if (ofs < len && ofs > -1)
                    fbout[offset] = framebuffer[ofs];
                else
                    fbout[offset] = framebuffer[offset];
            }
        }
    }

    // Propagate water simulation
    int density = parameters().get_int("density");
    calc_water(!page_, density);

    page_ = !page_;

    return 1;  // Output is in fbout
}

void WaterBumpEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;

    BinaryReader reader(data);

    // enabled
    if (reader.remaining() >= 4) {
        parameters().set_bool("enabled", reader.read_u32() != 0);
    }
    // density
    if (reader.remaining() >= 4) {
        parameters().set_int("density", static_cast<int>(reader.read_u32()));
    }
    // depth
    if (reader.remaining() >= 4) {
        parameters().set_int("depth", static_cast<int>(reader.read_u32()));
    }
    // random_drop
    if (reader.remaining() >= 4) {
        parameters().set_bool("random_drop", reader.read_u32() != 0);
    }
    // drop_position_x
    if (reader.remaining() >= 4) {
        parameters().set_int("drop_position_x", static_cast<int>(reader.read_u32()));
    }
    // drop_position_y
    if (reader.remaining() >= 4) {
        parameters().set_int("drop_position_y", static_cast<int>(reader.read_u32()));
    }
    // drop_radius
    if (reader.remaining() >= 4) {
        parameters().set_int("drop_radius", static_cast<int>(reader.read_u32()));
    }
    // method (unused, but read for compatibility)
    if (reader.remaining() >= 4) {
        reader.read_u32();
    }
}

// Static member definition
const PluginInfo WaterBumpEffect::effect_info {
    .name = "Water Bump",
    .category = "Trans",
    .description = "Water ripple displacement effect",
    .author = "",
    .version = 1,
    .legacy_index = 31,  // R_WaterBump
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<WaterBumpEffect>();
    },
    .ui_layout = {
        {
            // Enable checkbox
            {
                .id = "enabled",
                .text = "Enable Water bump effect",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 0, .w = 99, .h = 10,
                .default_val = 1
            },
            // Density slider (water damping)
            {
                .id = "density_label",
                .text = "Water density",
                .type = ControlType::LABEL,
                .x = 0, .y = 11, .w = 137, .h = 10
            },
            {
                .id = "density",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 5, .y = 21, .w = 130, .h = 12,
                .range = {2, 10},
                .default_val = 6
            },
            {
                .id = "thicker_label",
                .text = "Thicker",
                .type = ControlType::LABEL,
                .x = 3, .y = 34, .w = 25, .h = 8
            },
            {
                .id = "fluid_label",
                .text = "Fluid",
                .type = ControlType::LABEL,
                .x = 118, .y = 34, .w = 16, .h = 8
            },
            // Random drop checkbox
            {
                .id = "random_drop",
                .text = "Random drop position",
                .type = ControlType::CHECKBOX,
                .x = 2, .y = 51, .w = 85, .h = 10,
                .default_val = 0
            },
            // Drop position X
            {
                .id = "drop_position_label",
                .text = "Drop position",
                .type = ControlType::LABEL,
                .x = 0, .y = 61, .w = 137, .h = 10
            },
            {
                .id = "drop_position_x",
                .type = ControlType::RADIO_GROUP,
                .default_val = 1,  // Center
                .radio_options = {
                    {"Left", 6, 72, 28, 10},
                    {"Center", 46, 72, 37, 10},
                    {"Right", 95, 72, 33, 10}
                }
            },
            // Drop position Y
            {
                .id = "drop_position_y",
                .type = ControlType::RADIO_GROUP,
                .default_val = 1,  // Middle
                .radio_options = {
                    {"Top", 6, 84, 29, 10},
                    {"Middle", 46, 84, 37, 10},
                    {"Bottom", 95, 84, 38, 10}
                }
            },
            // Drop depth slider
            {
                .id = "depth_label",
                .text = "Drop depth",
                .type = ControlType::LABEL,
                .x = 0, .y = 98, .w = 67, .h = 10
            },
            {
                .id = "depth",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 5, .y = 108, .w = 56, .h = 12,
                .range = {100, 2000},
                .default_val = 600
            },
            {
                .id = "less_label",
                .text = "Less",
                .type = ControlType::LABEL,
                .x = 3, .y = 120, .w = 16, .h = 8
            },
            {
                .id = "more_label",
                .text = "More",
                .type = ControlType::LABEL,
                .x = 44, .y = 120, .w = 17, .h = 8
            },
            // Drop radius slider
            {
                .id = "radius_label",
                .text = "Drop radius",
                .type = ControlType::LABEL,
                .x = 70, .y = 98, .w = 67, .h = 10
            },
            {
                .id = "drop_radius",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 74, .y = 108, .w = 58, .h = 12,
                .range = {10, 100},
                .default_val = 40
            },
            {
                .id = "small_label",
                .text = "Small",
                .type = ControlType::LABEL,
                .x = 75, .y = 120, .w = 18, .h = 8
            },
            {
                .id = "big_label",
                .text = "Big",
                .type = ControlType::LABEL,
                .x = 120, .y = 120, .w = 11, .h = 8
            }
        }
    },
    .help_text =
        "Water Bump\n"
        "\n"
        "Creates a water ripple displacement effect.\n"
        "Ripples are created on beat and propagate\n"
        "across the screen.\n"
        "\n"
        "Water density: Controls how quickly ripples\n"
        "fade. Thicker = slower fade, Fluid = faster.\n"
        "\n"
        "Random drop position: Creates ripples at\n"
        "random locations instead of fixed position.\n"
        "\n"
        "Drop position: Where ripples appear when\n"
        "not using random position.\n"
        "\n"
        "Drop depth: Intensity of the ripple effect.\n"
        "\n"
        "Drop radius: Size of ripples (as percentage\n"
        "of screen size).\n"
};

// Register effect at startup
static bool register_water_bump = []() {
    PluginManager::instance().register_effect(WaterBumpEffect::effect_info);
    return true;
}();

} // namespace avs
