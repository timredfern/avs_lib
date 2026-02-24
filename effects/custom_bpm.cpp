// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "custom_bpm.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"
#include <chrono>

namespace avs {

CustomBpmEffect::CustomBpmEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
    arb_last_tc_ = get_tick_count();
}

uint64_t CustomBpmEffect::get_tick_count() {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

void CustomBpmEffect::step_indicator(int& slide, int& inc) {
    slide += inc;
    if (slide <= 0 || slide >= 8) {
        inc *= -1;
    }
}

int CustomBpmEffect::render(AudioData visdata, int isBeat,
                            uint32_t* framebuffer, uint32_t* fbout,
                            int w, int h) {
    if (!is_enabled()) return 0;
    if (isBeat & 0x80000000) return 0;

    int arbitrary = parameters().get_bool("arbitrary") ? 1 : 0;
    int skip = parameters().get_bool("skip") ? 1 : 0;
    int invert = parameters().get_bool("invert") ? 1 : 0;
    int arb_val = parameters().get_int("arb_val");
    int skip_val = parameters().get_int("skip_val");
    int skipfirst = parameters().get_int("skipfirst");

    // Step input indicator on incoming beat
    if (isBeat) {
        step_indicator(in_slide_, in_inc_);
        parameters().set_int("in_indicator", in_slide_);
        count_++;
    }

    // Skip first N beats
    if (skipfirst != 0 && count_ <= skipfirst) {
        return isBeat ? RENDER_CLR_BEAT : 0;
    }

    // Mode: Arbitrary (fixed BPM)
    if (arbitrary) {
        uint64_t tc_now = get_tick_count();
        if (tc_now > arb_last_tc_ + arb_val) {
            arb_last_tc_ = tc_now;
            step_indicator(out_slide_, out_inc_);
            parameters().set_int("out_indicator", out_slide_);
            return RENDER_SET_BEAT;
        }
        return RENDER_CLR_BEAT;
    }

    // Mode: Skip beats
    if (skip) {
        if (isBeat && ++skip_count_ >= skip_val + 1) {
            skip_count_ = 0;
            step_indicator(out_slide_, out_inc_);
            parameters().set_int("out_indicator", out_slide_);
            return RENDER_SET_BEAT;
        }
        return RENDER_CLR_BEAT;
    }

    // Mode: Invert beats
    if (invert) {
        if (isBeat) {
            return RENDER_CLR_BEAT;
        } else {
            step_indicator(out_slide_, out_inc_);
            parameters().set_int("out_indicator", out_slide_);
            return RENDER_SET_BEAT;
        }
    }

    return 0;
}

void CustomBpmEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;

    BinaryReader reader(data);

    // Binary format from r_bpm.cpp:
    // enabled (int32)
    // arbitrary (int32) - fixed BPM mode
    // skip (int32) - skip beats mode
    // invert (int32) - invert mode
    // arbVal (int32) - milliseconds between beats (200-10000)
    // skipVal (int32) - beats to skip (1-16)
    // skipfirst (int32) - skip first N beats (0-64)

    if (reader.remaining() >= 4) {
        parameters().set_bool("enabled", reader.read_u32() != 0);
    }
    if (reader.remaining() >= 4) {
        parameters().set_bool("arbitrary", reader.read_u32() != 0);
    }
    if (reader.remaining() >= 4) {
        parameters().set_bool("skip", reader.read_u32() != 0);
    }
    if (reader.remaining() >= 4) {
        parameters().set_bool("invert", reader.read_u32() != 0);
    }
    if (reader.remaining() >= 4) {
        parameters().set_int("arb_val", reader.read_u32());
    }
    if (reader.remaining() >= 4) {
        parameters().set_int("skip_val", reader.read_u32());
    }
    if (reader.remaining() >= 4) {
        parameters().set_int("skipfirst", reader.read_u32());
    }

    arb_last_tc_ = get_tick_count();
}

// Static member definition
const PluginInfo CustomBpmEffect::effect_info {
    .name = "Custom BPM",
    .category = "Misc",
    .description = "Override automatic BPM detection",
    .author = "",
    .version = 1,
    .legacy_index = 33,  // R_Bpm
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<CustomBpmEffect>();
    },
    .ui_layout = {
        {
            // Original UI from res.rc IDD_CFG_BPM (dialog 137x137)
            {
                .id = "enabled",
                .text = "Enable BPM Customizer",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 0, .w = 137, .h = 10,
                .default_val = 1
            },
            {
                .id = "arbitrary",
                .text = "Arbitrary",
                .type = ControlType::CHECKBOX,  // Original uses radio
                .x = 0, .y = 14, .w = 41, .h = 10,
                .default_val = 1
            },
            {
                .id = "arb_val",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 38, .y = 14, .w = 65, .h = 11,
                .range = {200, 10000, 100},
                .default_val = 500
            },
            {
                .id = "skip",
                .text = "Skip",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 24, .w = 30, .h = 10,
                .default_val = 0
            },
            {
                .id = "skip_val",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 38, .y = 24, .w = 65, .h = 11,
                .range = {1, 16, 1},
                .default_val = 1
            },
            {
                .id = "invert",
                .text = "Reverse",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 34, .w = 43, .h = 10,
                .default_val = 0
            },
            {
                .id = "firstskip_label",
                .text = "First skip",
                .type = ControlType::LABEL,
                .x = 0, .y = 45, .w = 28, .h = 8
            },
            {
                .id = "skipfirst",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 38, .y = 45, .w = 65, .h = 11,
                .range = {0, 64, 1},
                .default_val = 0
            },
            // Beat activity indicators (visual feedback, not user-adjustable)
            // "In" shows beats received from AVS, "Out" shows beats output by this effect
            // They animate 0-8, bouncing to visualize beat activity
            {
                .id = "in_label",
                .text = "In",
                .type = ControlType::LABEL,
                .x = 0, .y = 57, .w = 8, .h = 8
            },
            {
                .id = "in_indicator",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 13, .y = 56, .w = 124, .h = 11,
                .range = {0, 8, 1},
                .default_val = 0,
                .enabled = false  // Read-only visual indicator
            },
            {
                .id = "out_label",
                .text = "Out",
                .type = ControlType::LABEL,
                .x = 0, .y = 67, .w = 12, .h = 8
            },
            {
                .id = "out_indicator",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 13, .y = 66, .w = 124, .h = 11,
                .range = {0, 8, 1},
                .default_val = 0,
                .enabled = false  // Read-only visual indicator
            }
        }
    }
};

// Register effect at startup
static bool register_custom_bpm = []() {
    PluginManager::instance().register_effect(CustomBpmEffect::effect_info);
    return true;
}();

} // namespace avs
