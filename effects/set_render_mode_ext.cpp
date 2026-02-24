// avs_lib - Portable Advanced Visualization Studio library
// Extended Set Render Mode - NOT part of original AVS
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "set_render_mode_ext.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"
#include "core/blend.h"
#include "core/blend_ext.h"
#include <algorithm>
#include <cmath>

namespace avs {

SetRenderModeExtEffect::SetRenderModeExtEffect() {
    init_parameters_from_layout(effect_info.ui_layout);

    // Bind script variables to UI parameters
    engine_.bind_int_param("lw", "line_width", 1, 255);
    engine_.bind_int_param("bm", "blend_mode", 0, 9);
    engine_.bind_int_param("a", "alpha", 0, 255);

    compile_scripts();
}

void SetRenderModeExtEffect::compile_scripts() {
    init_compiled_ = engine_.compile(parameters().get_string("init_script"));
    frame_compiled_ = engine_.compile(parameters().get_string("frame_script"));
    beat_compiled_ = engine_.compile(parameters().get_string("beat_script"));
}

int SetRenderModeExtEffect::render(AudioData visdata, int isBeat,
                                   uint32_t* framebuffer, uint32_t* fbout,
                                   int w, int h) {
    if (isBeat & 0x80000000) return 0;

    if (!parameters().get_bool("enabled")) return 0;

    // Line style flags
    int line_style = 0;
    if (parameters().get_bool("anti_aliased")) line_style |= LINE_STYLE_AA;
    if (parameters().get_bool("angle_corrected")) line_style |= LINE_STYLE_ANGLE_CORRECT;
    if (parameters().get_bool("rounded_ends")) line_style |= LINE_STYLE_ROUNDED;
    if (parameters().get_bool("point_size")) line_style |= LINE_STYLE_POINTSIZE;

    // Run scripts if any are defined
    bool use_scripts = !init_compiled_.is_empty() || !frame_compiled_.is_empty() || !beat_compiled_.is_empty();

    if (use_scripts) {
        // Sync bound parameters to script variables
        engine_.sync_from_params(parameters());

        // Set up additional engine variables
        engine_.set_variable("w", static_cast<double>(w));
        engine_.set_variable("h", static_cast<double>(h));
        engine_.set_variable("b", isBeat ? 1.0 : 0.0);

        // Run init script once
        if (!inited_) {
            if (!init_compiled_.is_empty()) {
                engine_.execute(init_compiled_);
            }
            inited_ = true;
        }

        // Run frame script
        if (!frame_compiled_.is_empty()) {
            engine_.execute(frame_compiled_);
        }

        // Run beat script
        if (isBeat && !beat_compiled_.is_empty()) {
            engine_.execute(beat_compiled_);
        }

        // Sync script variables back to parameters (updates UI if changed)
        engine_.sync_to_params(parameters());
    }

    // Get final values from parameters
    int line_width = parameters().get_int("line_width");
    int blend_mode = parameters().get_int("blend_mode");
    int alpha = parameters().get_int("alpha");

    // Format: 0x80000000 | (line_style << 24) | (line_width << 16) | (alpha << 8) | blend_mode
    g_line_blend_mode = 0x80000000 | (line_style << 24) |
                       ((line_width & 0xFF) << 16) |
                       ((alpha & 0xFF) << 8) | (blend_mode & 0xFF);

    return 0;  // No framebuffer modification
}

void SetRenderModeExtEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    BinaryReader reader(data);

    // Binary format from r_linemode.cpp:
    // newmode (int32) - packed value:
    //   bit 31: enabled
    //   bits 0-7: blend mode
    //   bits 8-15: alpha (for adjustable blend)
    //   bits 16-23: line width

    if (reader.remaining() >= 4) {
        int newmode = reader.read_u32();

        parameters().set_bool("enabled", (newmode & 0x80000000) != 0);
        parameters().set_int("blend_mode", newmode & 0xFF);
        parameters().set_int("alpha", (newmode >> 8) & 0xFF);
        parameters().set_int("line_width", (newmode >> 16) & 0xFF);
    }

    // Reset init state when loading
    compile_scripts();
    inited_ = false;
}

void SetRenderModeExtEffect::on_parameter_changed(const std::string& param_name) {
    // Re-compile when any script changes
    if (param_name == "init_script" || param_name == "frame_script" ||
        param_name == "beat_script") {
        compile_scripts();
    }
    if (param_name == "init_script") {
        inited_ = false;
    }
}

// Extended UI controls (base + line styles + scripting)
static const std::vector<ControlLayout> ui_controls = {
    // Base controls (from res.rc IDD_CFG_LINEMODE)
    {
        .id = "enabled",
        .text = "Enable mode change",
        .type = ControlType::CHECKBOX,
        .x = 0, .y = 2, .w = 83, .h = 10,
        .default_val = true
    },
    {
        .id = "blend_group",
        .text = "Set blend mode to",
        .type = ControlType::GROUPBOX,
        .x = 0, .y = 15, .w = 136, .h = 37
    },
    {
        .id = "blend_mode",
        .text = "",
        .type = ControlType::DROPDOWN,
        .x = 5, .y = 25, .w = 128, .h = 184,
        .default_val = 0,
        .options = {
            "Replace",
            "Additive",
            "Maximum Blend",
            "50/50 Blend",
            "Subtractive Blend 1",
            "Subtractive Blend 2",
            "Multiply Blend",
            "Adjustable Blend",
            "XOR",
            "Minimum Blend"
        }
    },
    {
        .id = "alpha",
        .text = "",
        .type = ControlType::SLIDER,
        .x = 1, .y = 39, .w = 132, .h = 11,
        .range = {0, 255},
        .default_val = 128
    },
    {
        .id = "line_width",
        .text = "Line/point width",
        .type = ControlType::SLIDER,
        .x = 0, .y = 53, .w = 136, .h = 13,
        .range = {1, 100, 1},
        .default_val = 1
    },
    // Line style options (extension)
    {
        .id = "line_style_group",
        .text = "Line style",
        .type = ControlType::GROUPBOX,
        .x = 0, .y = 68, .w = 136, .h = 45
    },
    {
        .id = "anti_aliased",
        .text = "Anti-aliased",
        .type = ControlType::CHECKBOX,
        .x = 5, .y = 78, .w = 60, .h = 10,
        .default_val = 0
    },
    {
        .id = "angle_corrected",
        .text = "Angle-corrected thickness",
        .type = ControlType::CHECKBOX,
        .x = 5, .y = 88, .w = 100, .h = 10,
        .default_val = 0
    },
    {
        .id = "rounded_ends",
        .text = "Rounded endpoints",
        .type = ControlType::CHECKBOX,
        .x = 5, .y = 98, .w = 80, .h = 10,
        .default_val = 0
    },
    {
        .id = "point_size",
        .text = "Apply size to points",
        .type = ControlType::CHECKBOX,
        .x = 70, .y = 78, .w = 65, .h = 10,
        .default_val = 0
    },
    // Scripting section (extension)
    {
        .id = "script_group",
        .text = "Dynamic scripting",
        .type = ControlType::GROUPBOX,
        .x = 0, .y = 115, .w = 240, .h = 105
    },
    {
        .id = "init_label",
        .text = "init",
        .type = ControlType::LABEL,
        .x = 5, .y = 128, .w = 15, .h = 8
    },
    {
        .id = "init_script",
        .text = "",
        .type = ControlType::EDITTEXT,
        .x = 30, .y = 125, .w = 205, .h = 26
    },
    {
        .id = "frame_label",
        .text = "frame",
        .type = ControlType::LABEL,
        .x = 5, .y = 156, .w = 20, .h = 8
    },
    {
        .id = "frame_script",
        .text = "",
        .type = ControlType::EDITTEXT,
        .x = 30, .y = 153, .w = 205, .h = 26
    },
    {
        .id = "beat_label",
        .text = "beat",
        .type = ControlType::LABEL,
        .x = 5, .y = 184, .w = 15, .h = 8
    },
    {
        .id = "beat_script",
        .text = "",
        .type = ControlType::EDITTEXT,
        .x = 30, .y = 181, .w = 205, .h = 26
    }
};

// Static member definition
const PluginInfo SetRenderModeExtEffect::effect_info {
    .name = "Set Render Mode (extended)",
    .category = "Misc",
    .description = "Control rendering pipeline with scripting",
    .author = "",
    .version = 1,
    .legacy_index = -1,  // No legacy index - this is an extension
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<SetRenderModeExtEffect>();
    },
    .ui_layout = { ui_controls },
    .help_text =
        "Set Render Mode (Extended) - Scripting\n"
        "\n"
        "Variables:\n"
        "  lw   Line width (1-255)\n"
        "  bm   Blend mode (0-9)\n"
        "  a    Alpha for adjustable blend (0-255)\n"
        "  b    Beat (1 on beat, else 0)\n"
        "  w,h  Screen size\n"
        "\n"
        "Blend modes: 0=replace, 1=add, 2=max,\n"
        "3=50/50, 4=sub1, 5=sub2, 6=mul,\n"
        "7=adjustable, 8=xor, 9=min\n"
        "\n"
        "Examples:\n"
        "  init: lw=1; dir=1\n"
        "  frame: lw=lw+dir; if(lw>10,dir=-1,0)\n"
        "  beat: bm=rand(10)\n"
};

// Register effect at startup
static bool register_set_render_mode_ext = []() {
    PluginManager::instance().register_effect(SetRenderModeExtEffect::effect_info);
    return true;
}();

} // namespace avs
