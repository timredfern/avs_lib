// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "dynamic_movement_ext.h"
#include "core/plugin_manager.h"
#include <cmath>
#include <cstring>

namespace avs {

struct DMovExtPreset {
    const char* name;
    bool rect;      // rectangular coordinates
    bool wrap;
    int grid_w;
    int grid_h;
    const char* init;
    const char* pixel;
    const char* frame;
    const char* beat;
};

static const DMovExtPreset presets[] = {
    // Index 0: placeholder for current/custom
    {"(current)", false, false, 16, 16, "", "d=d*0.9", "", ""},

    // Original AVS presets from r_dmove.cpp (indices 1-8)
    {"Random Rotate", false, true, 2, 2, "", "r = r + dr;", "", "dr = (rand(100) / 100) * $PI;\r\nd = d * .95;"},
    {"Random Direction", true, true, 2, 2, "speed=.05;dr = (rand(200) / 100) * $PI;", "x = x + dx;\r\ny = y + dy;", "dx = cos(dr) * speed;\r\ndy = sin(dr) * speed;", "dr = (rand(200) / 100) * $PI;"},
    {"In and Out", false, true, 2, 2, "speed=.2;c=0;", "d = d * dd;", "", "c = c + ($PI/2);\r\ndd = 1 - (sin(c) * speed);"},
    {"Unspun Kaleida", false, true, 33, 33, "c=200;f=0;dt=0;dl=0;beatdiv=8", "r=cos(r*dr);", "f = f + 1;\r\nt = ((f * $pi * 2)/c)/beatdiv;\r\ndt = dl + t;\r\ndr = 4+(cos(dt)*2);", "c=f;f=0;dl=dt"},
    {"Roiling Gridley", true, true, 32, 32, "c=200;f=0;dt=0;dl=0;beatdiv=8", "x=x+(sin(y*dx) * .03);\r\ny=y-(cos(x*dy) * .03);", "f = f + 1;\r\nt = ((f * $pi * 2)/c)/beatdiv;\r\ndt = dl + t;\r\ndx = 14+(cos(dt)*8);\r\ndy = 10+(sin(dt*2)*4);", "c=f;f=0;dl=dt"},
    {"6-Way Outswirl", false, false, 32, 32, "c=200;f=0;dt=0;dl=0;beatdiv=8", "d=d*(1+(cos(r*6) * .05));\r\nr=r-(sin(d*dr) * .05);\r\nd = d * .98;", "f = f + 1;\r\nt = ((f * $pi * 2)/c)/beatdiv;\r\ndt = dl + t;\r\ndr = 18+(cos(dt)*12);", "c=f;f=0;dl=dt"},
    {"Wavy", true, true, 6, 6, "c=200;f=0;dx=0;dl=0;beatdiv=16;speed=.05", "y = y + ((sin((x+dx) * $PI))*speed);\r\nx = x + .025", "f = f + 1;\r\nt = ( (f * 2 * 3.1415) / c ) / beatdiv;\r\ndx = dl + t;", "c = f;\r\nf = 0;\r\ndl = dx;"},
    {"Smooth Rotoblitter", false, true, 2, 2, "c=200;f=0;dt=0;dl=0;beatdiv=4;speed=.15", "r = r + dr;\r\nd = d * dd;", "f = f + 1;\r\nt = ((f * $pi * 2)/c)/beatdiv;\r\ndt = dl + t;\r\ndr = cos(dt)*speed*2;\r\ndd = 1 - (sin(dt)*speed);", "c=f;f=0;dl=dt"},

    // Extended presets (indices 9+)
    {"Fisheye Lens", false, true, 16, 16, "", "d=d*0.97+sqrt(d)*0.03", "", ""},
    {"Tunnel Zoom", false, true, 16, 16, "", "d=d*0.98+d*d*0.02", "", ""},
    {"Inverse Tunnel", false, true, 16, 16, "", "d=d*0.97+pow(d,0.7)*0.03", "", ""},
    {"Barrel Distortion", false, true, 24, 24, "", "d=d+d*d*d*0.01", "", ""},
    {"Pincushion", false, true, 24, 24, "", "d=d-d*d*d*0.01", "", ""},
    {"Spiral Inward", false, true, 24, 24, "", "r=r+d*0.02", "", ""},
    {"Spiral Outward", false, true, 24, 24, "", "r=r+(1-d)*0.02", "", ""},
    {"Vortex", false, true, 32, 32, "", "r=r+(1-d)*(1-d)*0.05", "", ""},
    {"Tight Spiral", false, true, 32, 32, "", "r=r+d*d*0.08", "", ""},
    {"Angle Stretch", false, true, 24, 24, "", "r=r*1.01", "", ""},
    {"Angle Compress", false, true, 24, 24, "", "r=r*0.99", "", ""},
    {"Kaleidoscope 4", false, true, 32, 32, "", "r=r*0.98+abs(sin(r*2))*$PI*0.02", "", ""},
    {"Kaleidoscope 6", false, true, 32, 32, "", "r=r*0.98+abs(sin(r*3))*$PI*0.02", "", ""},
    {"Radial Waves", false, true, 48, 48, "", "d=d+sin(d*20)*0.003", "", ""},
    {"Angular Waves", false, true, 48, 48, "", "r=r+sin(d*15)*0.01", "", ""},
    {"Ripple Pool", false, true, 48, 48, "", "d=d+sin(d*30)*0.002;r=r+cos(d*30)*0.005", "", ""},
    {"Flower Petals", false, true, 32, 32, "", "d=d+sin(r*5)*0.01*d", "", ""},
    {"Twist", false, true, 32, 32, "", "r=r+r*(d-0.5)*0.02", "", ""},
    {"Swirl", false, true, 32, 32, "", "r=r+d*d*0.05", "", ""},
    {"Whirlpool", false, true, 32, 32, "", "r=r+(1/(d+0.3)-1)*0.01", "", ""},
    {"Liquid Glass", true, true, 32, 32, "", "x=x+sin(y*10)*0.003;y=y+sin(x*10)*0.003", "", ""},
    {"Heat Shimmer", true, true, 32, 32, "", "x=x+sin(y*20)*0.002", "", ""},
    {"Wobble", false, true, 32, 32, "", "d=d+sin(r*8)*0.005;r=r+sin(d*10)*0.01", "", ""},
    {"Starburst", false, true, 32, 32, "", "r=r+sin(r*6)*d*0.02", "", ""},
    {"Black Hole", false, true, 48, 48, "", "d=d*0.99+d*d*d*0.01;r=r+(1-d)*0.02", "", ""},
};

static constexpr int NUM_PRESETS = sizeof(presets) / sizeof(presets[0]);

DynamicMovementExtEffect::DynamicMovementExtEffect()
{
    init_parameters_from_layout(effect_info.ui_layout);

    // Bind script variables to UI parameters
    engine_.bind_int_param("gridx", "grid_width", 1, 256);
    engine_.bind_int_param("gridy", "grid_height", 1, 256);

    compile_scripts();
}

void DynamicMovementExtEffect::compile_scripts() {
    frame_compiled_ = engine_.compile(parameters().get_string("frame_script"));
    beat_compiled_ = engine_.compile(parameters().get_string("beat_script"));
}

void DynamicMovementExtEffect::apply_preset(int preset_index)
{
    if (preset_index < 0 || preset_index >= NUM_PRESETS) return;

    const auto& preset = presets[preset_index];
    parameters().set_string("init_script", preset.init);
    parameters().set_string("frame_script", preset.frame);
    parameters().set_string("beat_script", preset.beat);
    parameters().set_string("pixel_script", preset.pixel);
    parameters().set_int("grid_width", preset.grid_w);
    parameters().set_int("grid_height", preset.grid_h);
    parameters().set_bool("rectangular", preset.rect);
    parameters().set_bool("wrap", preset.wrap);

    // Run init script and compile frame/beat
    if (preset.init[0] != '\0') {
        engine_.evaluate(preset.init);
    }
    compile_scripts();
}

void DynamicMovementExtEffect::on_parameter_changed(const std::string& param_name) {
    if (param_name == "init_script") {
        engine_.evaluate(parameters().get_string("init_script"));
    }

    // Compile frame/beat scripts when they change
    if (param_name == "frame_script" || param_name == "beat_script") {
        compile_scripts();
    }

    if (param_name == "example") {
        int preset_idx = parameters().get_int("example");
        if (preset_idx > 0) {
            apply_preset(preset_idx);
            // Reset to (current) after loading
            parameters().set_int("example", 0);
        }
    }
}

int DynamicMovementExtEffect::render(AudioData visdata, int isBeat,
                                     uint32_t* framebuffer, uint32_t* fbout,
                                     int w, int h) {
    if (!is_enabled()) return 0;

    if (parameters().get_bool("no_movement")) {
        memcpy(fbout, framebuffer, w * h * sizeof(uint32_t));
        return 1;
    }

    // Sync bound parameters to script variables
    engine_.sync_from_params(parameters());

    engine_.set_audio_context(visdata, isBeat);
    if (!frame_compiled_.is_empty()) {
        engine_.execute(frame_compiled_);
    }
    if (isBeat && !beat_compiled_.is_empty()) {
        engine_.execute(beat_compiled_);
    }

    // Sync script variables back to parameters (updates UI if changed)
    engine_.sync_to_params(parameters());

    CoordMode mode = parameters().get_bool("rectangular") ? CoordMode::RECTANGULAR : CoordMode::POLAR;
    grid_.generate(engine_,
                   parameters().get_int("grid_width"),
                   parameters().get_int("grid_height"),
                   w, h,
                   parameters().get_string("pixel_script"),
                   mode, visdata);

    grid_.apply(framebuffer, fbout, w, h,
                parameters().get_bool("bilinear"),
                parameters().get_bool("wrap"),
                parameters().get_bool("blend"),
                parameters().get_bool("precise"),
                parameters().get_bool("parallel"));

    return 1;
}

// Static member definition
const PluginInfo DynamicMovementExtEffect::effect_info {
    .name = "Dynamic Movement (Extended)",
    .category = "Trans",
    .description = "Non-linear transformation presets",
    .author = "",
    .version = 1,
    .legacy_index = -1,  // New effect, no legacy index
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<DynamicMovementExtEffect>();
    },
    .ui_layout = {
        {
            // Labels for script boxes (from res.rc IDD_CFG_DMOVE)
            {
                .id = "init_label",
                .text = "init",
                .type = ControlType::LABEL,
                .x = 0, .y = 3, .w = 10, .h = 8
            },
            {
                .id = "frame_label",
                .text = "frame",
                .type = ControlType::LABEL,
                .x = 0, .y = 36, .w = 18, .h = 8
            },
            {
                .id = "beat_label",
                .text = "beat",
                .type = ControlType::LABEL,
                .x = 0, .y = 89, .w = 15, .h = 8
            },
            {
                .id = "pixel_label",
                .text = "pixel",
                .type = ControlType::LABEL,
                .x = 0, .y = 142, .w = 15, .h = 8
            },
            // Script editors
            {
                .id = "init_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 0, .w = 208, .h = 14
            },
            {
                .id = "frame_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 14, .w = 208, .h = 53
            },
            {
                .id = "beat_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 67, .w = 208, .h = 53
            },
            {
                .id = "pixel_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 120, .w = 208, .h = 53,
                .default_val = "d=d*0.9"
            },
            // Example preset dropdown - "source" label with dropdown
            {
                .id = "example_label",
                .text = "source",
                .type = ControlType::LABEL,
                .x = 0, .y = 177, .w = 22, .h = 8
            },
            {
                .id = "example",
                .text = "",
                .type = ControlType::DROPDOWN,
                .x = 25, .y = 175, .w = 85, .h = 56,
                .default_val = 0,
                .options = {
                    "(current)",
                    // Original AVS presets
                    "Random Rotate", "Random Direction", "In and Out",
                    "Unspun Kaleida", "Roiling Gridley", "6-Way Outswirl", "Wavy",
                    "Smooth Rotoblitter",
                    // Extended presets
                    "Fisheye Lens", "Tunnel Zoom", "Inverse Tunnel",
                    "Barrel Distortion", "Pincushion",
                    "Spiral Inward", "Spiral Outward", "Vortex", "Tight Spiral",
                    "Angle Stretch", "Angle Compress", "Kaleidoscope 4", "Kaleidoscope 6",
                    "Radial Waves", "Angular Waves", "Ripple Pool", "Flower Petals",
                    "Twist", "Swirl", "Whirlpool",
                    "Liquid Glass", "Heat Shimmer", "Wobble", "Starburst", "Black Hole"
                }
            },
            // Grid size labels
            {
                .id = "gridsize_label",
                .text = "Grid size:",
                .type = ControlType::LABEL,
                .x = 78, .y = 192, .w = 30, .h = 8
            },
            {
                .id = "gridx_label",
                .text = "x",
                .type = ControlType::LABEL,
                .x = 128, .y = 191, .w = 8, .h = 8
            },
            // Grid size inputs
            {
                .id = "grid_width",
                .text = "",
                .type = ControlType::TEXT_INPUT,
                .x = 108, .y = 190, .w = 18, .h = 12,
                .range = {1, 256},
                .default_val = 16
            },
            {
                .id = "grid_height",
                .text = "",
                .type = ControlType::TEXT_INPUT,
                .x = 136, .y = 190, .w = 18, .h = 12,
                .range = {1, 256},
                .default_val = 16
            },
            // Checkboxes - bottom row
            {
                .id = "blend",
                .text = "Blend",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 190, .w = 34, .h = 10,
                .default_val = 0
            },
            {
                .id = "wrap",
                .text = "Wrap",
                .type = ControlType::CHECKBOX,
                .x = 35, .y = 190, .w = 33, .h = 10,
                .default_val = 0
            },
            {
                .id = "no_movement",
                .text = "No movement (just blend)",
                .type = ControlType::CHECKBOX,
                .x = 113, .y = 176, .w = 100, .h = 10,
                .default_val = 0
            },
            {
                .id = "rectangular",
                .text = "Rectangular coords",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 204, .w = 90, .h = 10,
                .default_val = 0
            },
            {
                .id = "bilinear",
                .text = "Bilinear",
                .type = ControlType::CHECKBOX,
                .x = 94, .y = 204, .w = 45, .h = 10,
                .default_val = 1
            },
            {
                .id = "precise",
                .text = "Precise",
                .type = ControlType::CHECKBOX,
                .x = 140, .y = 204, .w = 45, .h = 10,
                .default_val = 0
            },
            {
                .id = "parallel",
                .text = "Parallel",
                .type = ControlType::CHECKBOX,
                .x = 186, .y = 204, .w = 45, .h = 10,
                .default_val = 1
            }
        }
    },
    .help_text =
        "Dynamic Movement (Extended)\n"
        "\n"
        "Variables (polar mode):\n"
        "d = distance from center (0=center, 1=edge)\n"
        "r = angle in radians\n"
        "\n"
        "Variables (rectangular mode):\n"
        "x, y = coordinates (-1 to 1)\n"
        "\n"
        "Common variables:\n"
        "w, h = width, height (in pixels)\n"
        "b = isBeat (1 if beat, 0 otherwise)\n"
        "alpha = blend amount (0.0-1.0)\n"
        "\n"
        "Grid variables (init/frame/beat):\n"
        "gridx, gridy = grid dimensions (1-256)\n"
        "  Can be modified by scripts to change resolution\n"
        "\n"
        "Scripts:\n"
        "init: runs once at start\n"
        "frame: runs once per frame\n"
        "beat: runs on beat\n"
        "pixel: runs for each grid point\n"
        "\n"
        "Options:\n"
        "Bilinear: smooth pixel sampling\n"
        "Precise: per-pixel grid interpolation\n"
        "  (slower but has unique visual character)\n"
};

// Register effect at startup
static bool register_dynamic_movement_ext = []() {
    PluginManager::instance().register_effect(DynamicMovementExtEffect::effect_info);
    return true;
}();

} // namespace avs
