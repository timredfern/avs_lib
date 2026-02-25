// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "bump.h"
#include "core/binary_reader.h"
#include "core/plugin_manager.h"
#include "core/blend.h"
#include "core/color.h"
#include "core/global_buffer.h"
#include "core/parallel.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>

// Temporary debug - set to 1 to enable
#define BUMP_DEBUG 0
static int g_bump_debug_frame = 0;  // Global to reset on load

namespace avs {

BumpEffect::BumpEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
    // Default oldstyle to 1 (0-100 percent coordinates) for compatibility
    // with old presets. New presets will override via load_parameters.
    parameters().set_int("oldstyle", 1);
    this_depth_ = parameters().get_int("depth");
    compile_scripts();
}

void BumpEffect::compile_scripts() {
    init_compiled_ = script_engine_.compile(parameters().get_string("init_script"));
    frame_compiled_ = script_engine_.compile(parameters().get_string("frame_script"));
    beat_compiled_ = script_engine_.compile(parameters().get_string("beat_script"));
}

void BumpEffect::on_parameter_changed(const std::string& param_name) {
    if (param_name == "init_script" || param_name == "frame_script" ||
        param_name == "beat_script") {
        compile_scripts();
    }
    if (param_name == "init_script") {
        inited_ = false;
    }
}

void BumpEffect::load_parameters(const std::vector<uint8_t>& data) {
#if BUMP_DEBUG
    fprintf(stderr, "BUMP load_parameters called with %zu bytes\n", data.size());
#endif
    if (data.size() < 4) return;

    BinaryReader reader(data);

    // Binary format from r_bump.cpp:
    // enabled, onbeat, durFrames, depth, depth2, blend, blendavg
    // code1 (frame), code2 (beat), code3 (init)
    // showlight, invert, oldstyle, buffern
    int enabled = reader.read_u32();
    parameters().set_bool("enabled", enabled != 0);

    if (reader.remaining() >= 4) {
        int onbeat = reader.read_u32();
        parameters().set_bool("onbeat", onbeat != 0);
    }
    if (reader.remaining() >= 4) {
        int durFrames = reader.read_u32();
        parameters().set_int("beat_duration", durFrames);
    }
    if (reader.remaining() >= 4) {
        int depth = reader.read_u32();
        parameters().set_int("depth", depth);
    }
    if (reader.remaining() >= 4) {
        int depth2 = reader.read_u32();
        parameters().set_int("depth2", depth2);
    }

    // blend and blendavg are separate flags in original
    int blend = 0, blendavg = 0;
    if (reader.remaining() >= 4) {
        blend = reader.read_u32();
    }
    if (reader.remaining() >= 4) {
        blendavg = reader.read_u32();
    }
    // Map to blend_mode: 0=replace, 1=additive, 2=50/50
    int blend_mode = 0;
    if (blend) blend_mode = 1;
    else if (blendavg) blend_mode = 2;
    parameters().set_int("blend_mode", blend_mode);

    // Scripts: code1=frame, code2=beat, code3=init (length-prefixed)
    std::string frame_script = reader.read_length_prefixed_string();
    std::string beat_script = reader.read_length_prefixed_string();
    std::string init_script = reader.read_length_prefixed_string();
    parameters().set_string("frame_script", frame_script);
    parameters().set_string("beat_script", beat_script);
    parameters().set_string("init_script", init_script);

    if (reader.remaining() >= 4) {
        int showlight = reader.read_u32();
        parameters().set_bool("showlight", showlight != 0);
    }
    if (reader.remaining() >= 4) {
        int invert = reader.read_u32();
        parameters().set_bool("invert", invert != 0);
    }
    // oldstyle: coordinate interpretation (see OLDSTYLE.md)
    // 0 = new (0-1 normalized), 1 = old (0-100 percent)
    // Default to 1 for very old presets that don't have this field
    int oldstyle;
    if (reader.remaining() >= 4) {
        oldstyle = reader.read_u32();
    } else {
        oldstyle = 1;
    }
    parameters().set_int("oldstyle", oldstyle);
#if BUMP_DEBUG
    fprintf(stderr, "BUMP load_parameters: oldstyle=%d (remaining=%zu)\n", oldstyle, reader.remaining());
    fprintf(stderr, "  init_script: \"%s\"\n", parameters().get_string("init_script").c_str());
    fprintf(stderr, "  frame_script: \"%s\"\n", parameters().get_string("frame_script").c_str());
    g_bump_debug_frame = 0;  // Reset debug output on load
#endif
    if (reader.remaining() >= 4) {
        int buffern = reader.read_u32();
        parameters().set_int("depth_buffer", buffern);
#if BUMP_DEBUG
        fprintf(stderr, "BUMP load_parameters: depth_buffer=%d\n", buffern);
#endif
    } else {
#if BUMP_DEBUG
        fprintf(stderr, "BUMP load_parameters: no depth_buffer field (remaining=%zu)\n", reader.remaining());
#endif
    }

    // Initialize state from loaded depth
    this_depth_ = parameters().get_int("depth");
    nF_ = 0;
    inited_ = false;
    compile_scripts();
}

inline int BumpEffect::depthof(int c, bool invert) {
    // Depth is max of RGB channels
    int depth = std::max({color::blue(c), color::green(c), color::red(c)});
    return invert ? 255 - depth : depth;
}

inline int BumpEffect::setdepth(int l, int c) {
    int b = std::min(color::blue(c) + l, 254);
    int g = std::min(color::green(c) + l, 254);
    int r = std::min(color::red(c) + l, 254);
    return color::make(r, g, b, color::alpha(c));
}

inline int BumpEffect::setdepth0(int c) {
    int b = std::min(static_cast<int>(color::blue(c)), 254);
    int g = std::min(static_cast<int>(color::green(c)), 254);
    int r = std::min(static_cast<int>(color::red(c)), 254);
    return color::make(r, g, b, color::alpha(c));
}

int BumpEffect::render(AudioData visdata, int isBeat,
                       uint32_t* framebuffer, uint32_t* fbout,
                       int w, int h) {
    if (!is_enabled()) return 0;
    if (isBeat & 0x80000000) return 0;

    // Get parameters
    int depth = parameters().get_int("depth");
    int depth2 = parameters().get_int("depth2");
    int durFrames = parameters().get_int("beat_duration");
    bool onbeat = parameters().get_bool("onbeat");
    int blend_mode = parameters().get_int("blend_mode");
    bool blend = (blend_mode == 1);     // Additive
    bool blendavg = (blend_mode == 2);  // 50/50
    bool showlight = parameters().get_bool("showlight");
    bool invert = parameters().get_bool("invert");
    int buffern = parameters().get_int("depth_buffer");

    // Set up script context
    script_engine_.set_audio_context(visdata, isBeat);

    // Initialize bi to 1.0 (original does this at variable registration)
    script_engine_.set_variable("bi", 1.0);

    // Execute init script once
    if (!inited_) {
        if (!init_compiled_.is_empty()) {
            script_engine_.execute(init_compiled_);
        }
        inited_ = true;
    }

    // Execute frame script
    if (!frame_compiled_.is_empty()) {
        script_engine_.execute(frame_compiled_);
    }

    // Execute beat script on beat
    if (isBeat && !beat_compiled_.is_empty()) {
        script_engine_.execute(beat_compiled_);
    }

    // Set beat variables AFTER script execution (original behavior)
    // Scripts see previous frame's beat state, not current
    script_engine_.set_variable("isbeat", isBeat ? -1.0 : 1.0);
    script_engine_.set_variable("islbeat", nF_ ? -1.0 : 1.0);

    // Handle on-beat depth transition
    if (onbeat && isBeat) {
        this_depth_ = depth2;
        nF_ = durFrames;
    } else if (!nF_) {
        this_depth_ = depth;
    }

    // Get depth buffer - use framebuffer or global buffer based on buffern
    uint32_t* depthbuffer = buffern ? get_global_buffer(w, h, buffern - 1, false) : framebuffer;
    if (!depthbuffer) return 0;
    bool curbuf = (depthbuffer == framebuffer);

    // Clear output buffer (original does this)
    std::memset(fbout, 0, w * h * sizeof(uint32_t));

    // Get light position from script variables
    double var_x = script_engine_.get_variable("x");
    double var_y = script_engine_.get_variable("y");

    // Convert based on coordinate style (see OLDSTYLE.md)
    int oldstyle = parameters().get_int("oldstyle");
    int cx, cy;
    if (oldstyle) {
        // Old style: 0-100 percent
        cx = static_cast<int>(var_x / 100.0 * w);
        cy = static_cast<int>(var_y / 100.0 * h);
    } else {
        // New style: 0-1 normalized
        cx = static_cast<int>(var_x * w);
        cy = static_cast<int>(var_y * h);
    }
    // Original clamps to w/h (not w-1/h-1) - match exactly for compatibility
    cx = std::max(0, std::min(w, cx));
    cy = std::max(0, std::min(h, cy));

    // Show light position dot
    if (showlight) {
        fbout[cx + cy * w] = 0xFFFFFF;
    }

    // Apply bi (bump intensity) - always clamp and multiply (original behavior)
    // bi defaults to 1.0 (set before script execution), script can override
    double bi = script_engine_.get_variable("bi");
    bi = std::min(std::max(bi, 0.0), 1.0);
    this_depth_ = static_cast<int>(this_depth_ * bi);

    int thisDepth_scaled = (this_depth_ << 8) / 100;

#if BUMP_DEBUG
    if (g_bump_debug_frame++ < 5) {
        // Sample pixels from various positions
        uint32_t center_in = framebuffer[(h/2) * w + (w/2)];
        uint32_t corner_in = framebuffer[w + 1];
        fprintf(stderr, "BUMP frame %d: oldstyle=%d buffern=%d x=%.2f y=%.2f cx=%d cy=%d depth=%d thisDepth_scaled=%d\n",
                g_bump_debug_frame, oldstyle, buffern, var_x, var_y, cx, cy, this_depth_, thisDepth_scaled);
        fprintf(stderr, "  INPUT center: 0x%08X (R=%d G=%d B=%d)\n",
                center_in, center_in & 0xFF, (center_in >> 8) & 0xFF, (center_in >> 16) & 0xFF);
        fprintf(stderr, "  INPUT corner: 0x%08X (R=%d G=%d B=%d)\n",
                corner_in, corner_in & 0xFF, (corner_in >> 8) & 0xFF, (corner_in >> 16) & 0xFF);
    }
#endif

    // Process pixels (skip 1-pixel border) — parallelized
    {
        uint32_t* base_depth = depthbuffer;
        uint32_t* base_fb = framebuffer;
        uint32_t* base_out = fbout;

        // Hoist blend mode outside inner loop
        auto do_rows = [&](auto blend_fn) {
            parallel_for_rows(h - 2, [&](int ys, int ye) {
                for (int y = ys + 1; y < ye + 1; y++) {
                    uint32_t* db = base_depth + y * w + 1;
                    uint32_t* fb = base_fb + y * w + 1;
                    uint32_t* out = base_out + y * w + 1;
                    int ly = y - cy;

                    for (int x = 1; x < w - 1; x++) {
                        int m1 = db[-1];
                        int p1 = db[1];
                        int mw = db[-w];
                        int pw = db[w];

                        if (!curbuf || (m1 || p1 || mw || pw)) {
                            int lx = x - cx;
                            int coul1 = depthof(p1, invert) - depthof(m1, invert) - lx;
                            int coul2 = depthof(pw, invert) - depthof(mw, invert) - ly;
                            coul1 = 127 - std::abs(coul1);
                            coul2 = 127 - std::abs(coul2);

                            int pixel;
                            if (coul1 <= 0 || coul2 <= 0) {
                                pixel = setdepth0(fb[0]);
                            } else {
                                pixel = setdepth((coul1 * coul2 * thisDepth_scaled) >> (8 + 6), fb[0]);
                            }

                            blend_fn(out, fb[0], pixel);
                        }

                        db++; fb++; out++;
                    }
                }
            });
        };

        if (blend) {
            do_rows([](uint32_t* out, uint32_t fb, int pixel) {
                *out = BLEND(fb, pixel);
            });
        } else if (blendavg) {
            do_rows([](uint32_t* out, uint32_t fb, int pixel) {
                *out = BLEND_AVG(fb, pixel);
            });
        } else {
            do_rows([](uint32_t* out, uint32_t, int pixel) {
                *out = pixel;
            });
        }
    }

    // Decay on-beat transition
    if (nF_) {
        nF_--;
        if (nF_) {
            int a = std::abs(depth - depth2) / durFrames;
            this_depth_ += a * (depth2 > depth ? -1 : 1);
        }
    }

    return 1;  // Output in fbout
}

// Static member definition - UI layout from res.rc IDD_CFG_BUMP
const PluginInfo BumpEffect::effect_info {
    .name = "Bump",
    .category = "Trans",
    .description = "Bump map lighting effect with scripted light position",
    .author = "",
    .version = 1,
    .legacy_index = 29,  // R_Bump
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<BumpEffect>();
    },
    .ui_layout = {
        {
            // Enable checkbox
            {
                .id = "enabled",
                .text = "Enable Bump",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 0, .w = 56, .h = 10,
                .default_val = 1
            },
            // Invert depth checkbox
            {
                .id = "invert",
                .text = "Invert depth",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 10, .w = 54, .h = 10,
                .default_val = 0
            },
            // Depth slider (Flat/Bumpy)
            {
                .id = "depth",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 58, .y = 11, .w = 79, .h = 11,
                .range = {1, 100, 1},
                .default_val = 30
            },
            // Show Dot checkbox
            {
                .id = "showlight",
                .text = "Show Dot",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 20, .w = 47, .h = 10,
                .default_val = 0
            },
            // Depth slider labels
            {
                .id = "flat_label",
                .text = "Flat",
                .type = ControlType::LABEL,
                .x = 61, .y = 22, .w = 12, .h = 8
            },
            {
                .id = "bumpy_label",
                .text = "Bumpy",
                .type = ControlType::LABEL,
                .x = 115, .y = 22, .w = 22, .h = 8
            },
            // Light position label
            {
                .id = "lightpos_label",
                .text = "Light position:",
                .type = ControlType::LABEL,
                .x = 0, .y = 38, .w = 44, .h = 8
            },
            // Init script
            {
                .id = "init_label",
                .text = "init",
                .type = ControlType::LABEL,
                .x = 0, .y = 63, .w = 10, .h = 8
            },
            {
                .id = "init_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 24, .y = 52, .w = 209, .h = 33,
                .default_val = "t=0;"
            },
            // Frame script
            {
                .id = "frame_label",
                .text = "frame",
                .type = ControlType::LABEL,
                .x = 0, .y = 98, .w = 18, .h = 8
            },
            {
                .id = "frame_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 24, .y = 87, .w = 209, .h = 48,
                .default_val = "x=0.5+cos(t)*0.3;\r\ny=0.5+sin(t)*0.3;\r\nt=t+0.1;"
            },
            // Beat script
            {
                .id = "beat_label",
                .text = "beat",
                .type = ControlType::LABEL,
                .x = 0, .y = 147, .w = 15, .h = 8
            },
            {
                .id = "beat_script",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 24, .y = 136, .w = 209, .h = 40,
                .default_val = ""
            },
            // Depth buffer dropdown
            {
                .id = "buffer_label",
                .text = "Depth buffer:",
                .type = ControlType::LABEL,
                .x = 0, .y = 181, .w = 48, .h = 8
            },
            {
                .id = "depth_buffer",
                .text = "",
                .type = ControlType::DROPDOWN,
                .x = 52, .y = 179, .w = 85, .h = 56,
                .default_val = 0,
                .options = {
                    "Current", "Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4",
                    "Buffer 5", "Buffer 6", "Buffer 7", "Buffer 8"
                }
            },
            // Blend mode radio buttons
            {
                .id = "blend_mode",
                .type = ControlType::RADIO_GROUP,
                .default_val = 0,  // Replace
                .radio_options = {
                    {"Replace", 148, 0, 43, 10},
                    {"Additive blend", 148, 9, 61, 10},
                    {"Blend 50/50", 148, 19, 55, 10}
                }
            },
            // OnBeat section
            {
                .id = "onbeat",
                .text = "OnBeat",
                .type = ControlType::CHECKBOX,
                .x = 0, .y = 198, .w = 40, .h = 10,
                .default_val = 0
            },
            // Beat duration slider (Shorter/Longer)
            {
                .id = "beat_duration",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 44, .y = 196, .w = 67, .h = 11,
                .range = {1, 100, 1},
                .default_val = 15
            },
            // OnBeat depth slider
            {
                .id = "depth2",
                .text = "",
                .type = ControlType::SLIDER,
                .x = 114, .y = 196, .w = 67, .h = 11,
                .range = {1, 100, 1},
                .default_val = 100
            }
        }
    }
};

// Register effect at startup
static bool register_bump = []() {
    PluginManager::instance().register_effect(BumpEffect::effect_info);
    return true;
}();

} // namespace avs
