// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "color_modifier.h"
#include "core/binary_reader.h"
#include "core/plugin_manager.h"
#include "core/ui.h"
#include <algorithm>
#include <cmath>

namespace avs {

// Preset definitions from original AVS
struct ColorModPreset {
    const char* name;
    const char* init;
    const char* level;
    const char* frame;
    const char* beat;
    int recompute;
};

static const ColorModPreset presets[] = {
    {"4x Red Brightness, 2x Green, 1x Blue",
     "", "red=4*red; green=2*green;", "", "", 0},
    {"Solarization",
     "", "red=(min(1,red*2)-red)*2;\r\ngreen=red; blue=red;", "", "", 0},
    {"Double Solarization",
     "", "red=(min(1,red*2)-red)*2;\r\nred=(min(1,red*2)-red)*2;\r\ngreen=red; blue=red;", "", "", 0},
    {"Inverse Solarization (Soft)",
     "", "red=abs(red - .5) * 1.5;\r\ngreen=red; blue=red;", "", "", 0},
    {"Big Brightness on Beat",
     "scale=1.0", "red=red*scale;\r\ngreen=red; blue=red;", "scale=0.07 + (scale*0.93)", "scale=16", 1},
    {"Big Brightness on Beat (Interpolative)",
     "c = 200; f = 0;", "red = red * t;\r\ngreen=red;blue=red;", "f = f + 1;\r\nt = (1.025 - (f / c)) * 5;", "c = f;f = 0;", 1},
    {"Pulsing Brightness (Beat Interpolative)",
     "c = 200; f = 0;", "red = red * st;\r\ngreen=red;blue=red;", "f = f + 1;\r\nt = (f * 2 * $PI) / c;\r\nst = sin(t) + 1;", "c = f;f = 0;", 1},
    {"Rolling Solarization (Beat Interpolative)",
     "c = 200; f = 0;", "red=(min(1,red*st)-red)*st;\r\nred=(min(1,red*2)-red)*2;\r\ngreen=red; blue=red;", "f = f + 1;\r\nt = (f * 2 * $PI) / c;\r\nst = ( sin(t) * .75 ) + 2;", "c = f;f = 0;", 1},
    {"Rolling Tone (Beat Interpolative)",
     "c = 200; f = 0;", "red = red * st;\r\ngreen = green * ct;\r\nblue = (blue * 4 * ti) - red - green;", "f = f + 1;\r\nt = (f * 2 * $PI) / c;\r\nti = (f / c);\r\nst = sin(t) + 1.5;\r\nct = cos(t) + 1.5;", "c = f;f = 0;", 1},
    {"Random Inverse Tone (Switch on Beat)",
     "", "dd = red * 1.5;\r\nred = pow(dd, dr);\r\ngreen = pow(dd, dg);\r\nblue = pow(dd, db);", "", "token = rand(99) % 3;\r\ndr = if (equal(token, 0), -1, 1);\r\ndg = if (equal(token, 1), -1, 1);\r\ndb = if (equal(token, 2), -1, 1);", 1},
};

static constexpr int NUM_PRESETS = sizeof(presets) / sizeof(presets[0]);

ColorModifierEffect::ColorModifierEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
    table_.fill(0);
    compile_scripts();
}

void ColorModifierEffect::compile_scripts() {
    init_compiled_ = engine_.compile(parameters().get_string("init_script"));
    frame_compiled_ = engine_.compile(parameters().get_string("frame_script"));
    beat_compiled_ = engine_.compile(parameters().get_string("beat_script"));
    level_compiled_ = engine_.compile(parameters().get_string("level_script"));
}

void ColorModifierEffect::regenerate_table() {
    for (int x = 0; x < 256; x++) {
        // Set input value (0-1 range)
        double input = x / 255.0;
        engine_.set_variable("red", input);
        engine_.set_variable("green", input);
        engine_.set_variable("blue", input);

        // Execute level script
        if (!level_compiled_.is_empty()) {
            engine_.execute(level_compiled_);
        }

        // Get output values and clamp
        int r = static_cast<int>(engine_.get_variable("red") * 255.0 + 0.5);
        int g = static_cast<int>(engine_.get_variable("green") * 255.0 + 0.5);
        int b = static_cast<int>(engine_.get_variable("blue") * 255.0 + 0.5);

        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);

        // Store in lookup table (matches original: blue at 0, green at 256, red at 512)
        table_[x] = static_cast<uint8_t>(b);
        table_[x + 256] = static_cast<uint8_t>(g);
        table_[x + 512] = static_cast<uint8_t>(r);
    }
}

int ColorModifierEffect::render(AudioData visdata, int isBeat,
                                 uint32_t* framebuffer, uint32_t* fbout,
                                 int w, int h) {
    if (!is_enabled()) return 0;
    if (isBeat & 0x80000000) return 0;

    // Set beat variable
    engine_.set_variable("beat", isBeat ? 1.0 : 0.0);

    // Run init script (once)
    if (!inited_ && !init_compiled_.is_empty()) {
        engine_.execute(init_compiled_);
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

    // Regenerate lookup table each frame if recompute enabled
    // (for beat-reactive or animated color modifications)
    if (parameters().get_bool("recompute")) {
        regenerate_table();
    }

    // Apply lookup table to framebuffer
    // Pixel format is ARGB 0xAARRGGBB (little-endian: B at byte 0, G at byte 1, R at byte 2, A at byte 3)
    uint8_t* fb = reinterpret_cast<uint8_t*>(framebuffer);
    int num_pixels = w * h;

    for (int i = 0; i < num_pixels; i++) {
        // ARGB on little-endian: fb[0] = blue, fb[1] = green, fb[2] = red
        fb[0] = table_[fb[0]];        // Blue lookup
        fb[1] = table_[fb[1] + 256];  // Green lookup
        fb[2] = table_[fb[2] + 512];  // Red lookup
        fb += 4;
    }

    return 0;
}

void ColorModifierEffect::on_parameter_changed(const std::string& param_name) {
    // Handle preset selection
    if (param_name == "example_preset") {
        int preset_idx = parameters().get_int("example_preset");
        if (preset_idx > 0 && preset_idx <= NUM_PRESETS) {
            const ColorModPreset& p = presets[preset_idx - 1];
            parameters().set_string("init_script", p.init);
            parameters().set_string("level_script", p.level);
            parameters().set_string("frame_script", p.frame);
            parameters().set_string("beat_script", p.beat);
            parameters().set_bool("recompute", p.recompute != 0);
            parameters().set_int("example_preset", 0);  // Reset selection
            compile_scripts();
            regenerate_table();
            inited_ = false;
        }
        return;
    }

    if (param_name == "init_script" || param_name == "frame_script" ||
        param_name == "beat_script" || param_name == "level_script") {
        compile_scripts();
        regenerate_table();
    }
    if (param_name == "init_script") {
        inited_ = false;
    }
}

void ColorModifierEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    BinaryReader reader(data);

    std::string level_script, frame_script, beat_script, init_script;

    // Check format: new format starts with 1
    if (data[0] == 1) {
        reader.skip(1);
        level_script = reader.read_length_prefixed_string();  // effect_exp[0]
        frame_script = reader.read_length_prefixed_string();  // effect_exp[1]
        beat_script = reader.read_length_prefixed_string();   // effect_exp[2]
        init_script = reader.read_length_prefixed_string();   // effect_exp[3]
    } else {
        // Old format: 4 x 256-byte fixed buffers
        if (data.size() >= 1024) {
            level_script = reader.read_string_fixed(256);
            frame_script = reader.read_string_fixed(256);
            beat_script = reader.read_string_fixed(256);
            init_script = reader.read_string_fixed(256);
        }
    }

    parameters().set_string("level_script", level_script);
    parameters().set_string("frame_script", frame_script);
    parameters().set_string("beat_script", beat_script);
    parameters().set_string("init_script", init_script);

    // m_recompute flag
    if (reader.remaining() >= 4) {
        int recompute = static_cast<int>(reader.read_u32());
        parameters().set_bool("recompute", recompute != 0);
    }

    compile_scripts();
    regenerate_table();
    inited_ = false;
}

// Static member definition
const PluginInfo ColorModifierEffect::effect_info {
    .name = "Color Modifier",
    .category = "Trans",
    .description = "Scripted color channel modification",
    .author = "",
    .version = 1,
    .legacy_index = 45,  // R_DColorMod
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<ColorModifierEffect>();
    },
    .ui_layout = {
        {
            // Labels for script boxes
            {
                .id = "init_label",
                .text = "init",
                .type = ControlType::LABEL,
                .x = 0, .y = 9, .w = 10, .h = 8
            },
            {
                .id = "frame_label",
                .text = "frame",
                .type = ControlType::LABEL,
                .x = 0, .y = 45, .w = 18, .h = 8
            },
            {
                .id = "beat_label",
                .text = "beat",
                .type = ControlType::LABEL,
                .x = 0, .y = 93, .w = 15, .h = 8
            },
            {
                .id = "level_label",
                .text = "level",
                .type = ControlType::LABEL,
                .x = 0, .y = 156, .w = 16, .h = 8
            },
            // Init script - IDC_EDIT4
            {
                .id = "init_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 0, .w = 208, .h = 29
            },
            // Frame script - IDC_EDIT2
            {
                .id = "frame_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 29, .w = 208, .h = 44
            },
            // Beat script - IDC_EDIT3
            {
                .id = "beat_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 73, .w = 208, .h = 53
            },
            // Level script - IDC_EDIT1
            {
                .id = "level_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 25, .y = 126, .w = 208, .h = 69
            },
            // Recompute checkbox - IDC_CHECK1
            {
                .id = "recompute",
                .text = "Recompute every frame",
                .type = ControlType::CHECKBOX,
                .x = 2, .y = 199, .w = 91, .h = 10,
                .default_val = 1
            },
            // Load example button/dropdown - IDC_BUTTON4
            {
                .id = "example_preset",
                .text = "",
                .type = ControlType::DROPDOWN,
                .x = 97, .y = 200, .w = 63, .h = 14,
                .default_val = 0,
                .options = {
                    "(select)",
                    "4x Red, 2x Green, 1x Blue",
                    "Solarization",
                    "Double Solarization",
                    "Inverse Solarization (Soft)",
                    "Big Brightness on Beat",
                    "Big Brightness (Interp)",
                    "Pulsing Brightness (Interp)",
                    "Rolling Solarization (Interp)",
                    "Rolling Tone (Interp)",
                    "Random Inverse Tone (Beat)"
                }
            },
            // Help button - IDC_BUTTON1
            {
                .id = "help_button",
                .text = "?",
                .type = ControlType::HELP_BUTTON,
                .x = 164, .y = 200, .w = 69, .h = 14
            }
        }
    },
    .help_text =
        "Color Modifier\n"
        "\n"
        "Modifies the intensity of each color channel\n"
        "with respect to itself. For example, you could\n"
        "reverse the red channel, double the green\n"
        "channel, or half the blue channel.\n"
        "\n"
        "The code in the 'level' section should adjust\n"
        "the variables 'red', 'green', and 'blue',\n"
        "whose values represent the channel intensity\n"
        "(0 to 1 range).\n"
        "\n"
        "Code in 'frame' or 'level' sections can also\n"
        "use the variable 'beat' to detect if it is\n"
        "currently a beat.\n"
        "\n"
        "Try loading an example via the preset dropdown\n"
        "for examples.\n"
};

// Register effect at startup
static bool register_color_modifier = []() {
    PluginManager::instance().register_effect(ColorModifierEffect::effect_info);
    return true;
}();

} // namespace avs
