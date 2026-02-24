// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "buffer_save.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"
#include "core/global_buffer.h"
#include "core/blend.h"
#include <cstring>

namespace avs {

BufferSaveEffect::BufferSaveEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
}

int BufferSaveEffect::render(AudioData /*visdata*/, int isBeat,
                              uint32_t* framebuffer, uint32_t* /*fbout*/,
                              int w, int h) {
    if (isBeat & 0x80000000) return 0;

    int dir = parameters().get_int("dir");
    int which = parameters().get_int("which");
    int blend = parameters().get_int("blend");
    int adjblend_val = parameters().get_int("adjblend_val");

    // Get the buffer, allocating if we're saving (dir != 1 means we need to write)
    uint32_t* buffer = get_global_buffer(w, h, which, dir != 1);
    if (!buffer) return 0;

    // Handle clear button
    if (clear_) {
        clear_ = false;
        std::memset(buffer, 0, sizeof(uint32_t) * w * h);
    }

    // Determine direction for this frame
    // dir: 0=save, 1=restore, 2=alternate save/restore, 3=alternate restore/save
    int t_dir;
    if (dir < 2) {
        t_dir = dir;
    } else {
        t_dir = (dir & 1) ^ dir_ch_;
    }
    dir_ch_ ^= 1;

    // Set up source and destination
    // t_dir=0 (Save): copy FROM framebuffer TO buffer
    // t_dir=1 (Restore): copy FROM buffer TO framebuffer
    uint32_t* src = (t_dir == 0) ? framebuffer : buffer;
    uint32_t* dst = (t_dir != 0) ? framebuffer : buffer;

    int num_pixels = w * h;

    switch (blend) {
        case 0:  // Replace
            std::memcpy(dst, src, num_pixels * sizeof(uint32_t));
            break;

        case 1:  // 50/50
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND_AVG(dst[i], src[i]);
            }
            break;

        case 2:  // Additive
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND(dst[i], src[i]);
            }
            break;

        case 3: {  // Every other pixel (checkerboard)
            int r = 0;
            for (int y = 0; y < h; y++) {
                int row_offset = y * w;
                for (int x = r; x < w; x += 2) {
                    dst[row_offset + x] = src[row_offset + x];
                }
                r ^= 1;
            }
            break;
        }

        case 4:  // Subtractive 1 (dst - src)
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND_SUB(dst[i], src[i]);
            }
            break;

        case 5: {  // Every other line
            for (int y = 0; y < h; y += 2) {
                std::memcpy(dst + y * w, src + y * w, w * sizeof(uint32_t));
            }
            break;
        }

        case 6:  // XOR
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = dst[i] ^ src[i];
            }
            break;

        case 7:  // Maximum
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND_MAX(dst[i], src[i]);
            }
            break;

        case 8:  // Minimum
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND_MIN(dst[i], src[i]);
            }
            break;

        case 9:  // Subtractive 2 (src - dst)
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND_SUB(src[i], dst[i]);
            }
            break;

        case 10:  // Multiply
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND_MUL(dst[i], src[i]);
            }
            break;

        case 11:  // Adjustable
            for (int i = 0; i < num_pixels; i++) {
                dst[i] = BLEND_ADJ(src[i], dst[i], adjblend_val);
            }
            break;
    }

    return 0;
}

void BufferSaveEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 16) return;

    BinaryReader reader(data);

    int dir = reader.read_u32();
    int which = reader.read_u32();
    int blend = reader.read_u32();
    int adjblend_val = reader.read_u32();

    // Clamp buffer index
    if (which < 0) which = 0;
    if (which >= NBUF) which = NBUF - 1;

    parameters().set_int("dir", dir);
    parameters().set_int("which", which);
    parameters().set_int("blend", blend);
    parameters().set_int("adjblend_val", adjblend_val);
}

// UI layout from res.rc IDD_CFG_STACK
const PluginInfo BufferSaveEffect::effect_info {
    .name = "Buffer Save",
    .category = "Misc",
    .description = "Saves/restores framebuffer to global buffers with blending options",
    .author = "",
    .version = 1,
    .legacy_index = 18,  // R_Stack from rlib.cpp
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<BufferSaveEffect>();
    },
    .ui_layout = {
        {
            // Original UI from res.rc IDD_CFG_STACK (dialog 233x219)
            {
                .id = "dir",
                .text = "",
                .type = ControlType::RADIO_GROUP,
                .x = 5, .y = 7, .w = 91, .h = 50,
                .default_val = 0,
                .radio_options = {
                    {"Save framebuffer", 0, 0, 70, 10},
                    {"Restore framebuffer", 0, 10, 78, 10},
                    {"Alternate Save/Restore", 0, 21, 91, 10},
                    {"Alternate Restore/Save", 0, 31, 91, 10}
                }
            },
            {
                .id = "buffer_label",
                .text = "Buffer:",
                .type = ControlType::LABEL,
                .x = 8, .y = 55, .w = 22, .h = 8
            },
            {
                .id = "which",
                .text = "",
                .type = ControlType::DROPDOWN,
                .x = 32, .y = 53, .w = 74, .h = 90,
                .default_val = 0,
                .options = {
                    "Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4",
                    "Buffer 5", "Buffer 6", "Buffer 7", "Buffer 8"
                }
            },
            {
                .id = "blending_group",
                .text = "Blending",
                .type = ControlType::GROUPBOX,
                .x = 8, .y = 68, .w = 172, .h = 138
            },
            {
                .id = "blend",
                .text = "",
                .type = ControlType::RADIO_GROUP,
                .x = 12, .y = 79, .w = 164, .h = 120,
                .default_val = 0,
                .radio_options = {
                    {"Replace", 0, 0, 43, 10},
                    {"50/50", 0, 10, 35, 10},
                    {"Additive", 0, 20, 41, 10},
                    {"Every other pixel", 0, 30, 68, 10},
                    {"Every other line", 0, 39, 65, 10},
                    {"Subtractive 1", 0, 49, 58, 10},
                    {"Subtractive 2", 0, 59, 58, 10},
                    {"Xor", 0, 69, 27, 10},
                    {"Maximum", 0, 79, 45, 10},
                    {"Minimum", 0, 89, 43, 10},
                    {"Multiply", 0, 99, 39, 10},
                    {"Adjustable:", 0, 109, 51, 10}
                }
            },
            {
                .id = "adjblend_val",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 62, .y = 188, .w = 111, .h = 11,
                .range = {0, 255},
                .default_val = 128
            }
        }
    },
    .help_text = ""
};

// Register effect at startup
static bool register_buffer_save = []() {
    PluginManager::instance().register_effect(BufferSaveEffect::effect_info);
    return true;
}();

} // namespace avs
