// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License - see LICENSE file in repository root

#include "effect_list.h"
#include "core/plugin_manager.h"
#include "core/ui.h"
#include "core/blend.h"
#include "core/color.h"
#include "core/global_buffer.h"
#include <algorithm>
#include <memory>
#include <cstring>
#include <chrono>

namespace avs {

// Helper function to get depth from a pixel for buffer blending
static inline int depthof(uint32_t c, int invert) {
    int depth = std::max({color::blue(c), color::green(c), color::red(c)});
    return invert ? 255 - depth : depth;
}

EffectList::EffectList() {
    init_parameters_from_layout(effect_info.ui_layout);
    compile_scripts();
}

void EffectList::on_parameter_changed(const std::string& param_name) {
    if (param_name == "init_script" || param_name == "frame_script") {
        compile_scripts();
        if (param_name == "init_script") {
            inited_ = false;  // Re-run init script
        }
    }
}

void EffectList::compile_scripts() {
    std::string init_script = parameters().get_string("init_script");
    std::string frame_script = parameters().get_string("frame_script");

    init_compiled_ = engine_.compile(init_script);
    frame_compiled_ = engine_.compile(frame_script);
}

void EffectList::blend_buffers(uint32_t* dest, const uint32_t* src, int count,
                                int blend_mode, int blend_val, int buffer_index, bool invert) {
    switch (blend_mode) {
        case 0:  // Ignore - do nothing
            break;

        case 1:  // Replace
            std::memcpy(dest, src, count * sizeof(uint32_t));
            break;

        case 2:  // 50/50
            for (int i = 0; i < count; i++) {
                dest[i] = BLEND_AVG(dest[i], src[i]);
            }
            break;

        case 3:  // Maximum
            for (int i = 0; i < count; i++) {
                dest[i] = BLEND_MAX(dest[i], src[i]);
            }
            break;

        case 4:  // Additive
            for (int i = 0; i < count; i++) {
                dest[i] = BLEND(dest[i], src[i]);
            }
            break;

        case 5:  // Subtractive 1 (dest - src)
            for (int i = 0; i < count; i++) {
                dest[i] = BLEND_SUB(dest[i], src[i]);
            }
            break;

        case 6:  // Subtractive 2 (src - dest)
            for (int i = 0; i < count; i++) {
                dest[i] = BLEND_SUB(src[i], dest[i]);
            }
            break;

        case 7:  // Every other line
            {
                // Assumes dest buffer has same stride as src
                // We need w to know line length - estimate from count
                // For now, copy every other line
                // Note: this needs width info - we'll handle in render()
            }
            break;

        case 8:  // Every other pixel (checkerboard)
            {
                // Checkerboard pattern - needs width for row alternation
                // Handle in render() where we have dimensions
            }
            break;

        case 9:  // XOR
            for (int i = 0; i < count; i++) {
                dest[i] = dest[i] ^ src[i];
            }
            break;

        case 10: // Adjustable (alpha blend)
            if (blend_val >= 255) {
                // Full source - same as replace
                std::memcpy(dest, src, count * sizeof(uint32_t));
            } else if (blend_val > 0) {
                for (int i = 0; i < count; i++) {
                    dest[i] = BLEND_ADJ(src[i], dest[i], blend_val);
                }
            }
            // blend_val == 0: keep dest unchanged
            break;

        case 11: // Multiply
            for (int i = 0; i < count; i++) {
                dest[i] = BLEND_MUL(dest[i], src[i]);
            }
            break;

        case 12: // Buffer (use global buffer as alpha mask)
            {
                // This requires access to global buffers - handle in render()
            }
            break;

        case 13: // Minimum
            for (int i = 0; i < count; i++) {
                dest[i] = BLEND_MIN(dest[i], src[i]);
            }
            break;

        default:
            break;
    }
}

int EffectList::render(AudioData visdata, int isBeat,
                       uint32_t* framebuffer, uint32_t* fbout,
                       int w, int h) {
    // Handle beat-gated rendering
    int beat_render = parameters().get_int("beat_render");
    if (isBeat && beat_render) {
        fake_enabled_ = parameters().get_int("beat_render_frames");
    }

    // Get initial values from parameters
    bool use_enabled = is_enabled() || (fake_enabled_ > 0);
    bool use_clear = parameters().get_bool("clear_each_frame");
    int inblendval = parameters().get_int("inblendval");
    int outblendval = parameters().get_int("outblendval");

    // Script execution (evaluation override)
    bool use_code = parameters().get_bool("use_code");
    if (use_code) {
        // Set up script variables
        engine_.set_variable("beat", (isBeat && !(isBeat & 0x80000000)) ? 1.0 : 0.0);
        engine_.set_variable("enabled", use_enabled ? 1.0 : 0.0);
        engine_.set_variable("clear", use_clear ? 1.0 : 0.0);
        engine_.set_variable("alphain", inblendval / 255.0);
        engine_.set_variable("alphaout", outblendval / 255.0);
        engine_.set_variable("w", static_cast<double>(w));
        engine_.set_variable("h", static_cast<double>(h));

        // Run init script (once)
        if (!inited_ && init_compiled_.is_valid() && !init_compiled_.is_empty()) {
            engine_.execute(init_compiled_);
            inited_ = true;
        }

        // Run frame script
        if (frame_compiled_.is_valid() && !frame_compiled_.is_empty()) {
            engine_.execute(frame_compiled_);
        }

        // Read back modified values
        if (!(isBeat & 0x80000000)) {
            double beat_val = engine_.get_variable("beat");
            isBeat = (beat_val > 0.1 || beat_val < -0.1) ? 1 : 0;
        }

        double enabled_val = engine_.get_variable("enabled");
        use_enabled = (enabled_val > 0.1 || enabled_val < -0.1);

        double clear_val = engine_.get_variable("clear");
        use_clear = (clear_val > 0.1 || clear_val < -0.1);

        double alphain_val = engine_.get_variable("alphain");
        inblendval = static_cast<int>(alphain_val * 255.0);
        if (inblendval < 0) inblendval = 0;
        if (inblendval > 255) inblendval = 255;

        double alphaout_val = engine_.get_variable("alphaout");
        outblendval = static_cast<int>(alphaout_val * 255.0);
        if (outblendval < 0) outblendval = 0;
        if (outblendval > 255) outblendval = 255;
    }

    // Check if enabled (after script may have modified it)
    if (!use_enabled) {
        return 0;
    }

    // Get blend modes (not modifiable by script in original)
    int blendin = parameters().get_int("blendin");
    int blendout = parameters().get_int("blendout");

    // Get buffer indices for buffer blend mode
    int bufferin = parameters().get_int("bufferin");
    int bufferout = parameters().get_int("bufferout");
    bool ininvert = parameters().get_int("ininvert") != 0;
    bool outinvert = parameters().get_int("outinvert") != 0;

    // Save and reset line blend mode (as per original AVS r_list.cpp)
    int line_blend_mode_save = g_line_blend_mode;
    g_line_blend_mode = 0;

    int pixel_count = w * h;
    int result = 0;

    // Optimization: if both input and output are Replace mode, render directly to parent buffer
    // (This is the "root" path in original AVS)
    if (blendin == 1 && blendout == 1) {
        // Clear framebuffer if requested
        if (use_clear) {
            std::memset(framebuffer, 0, pixel_count * sizeof(uint32_t));
        }

        // Render children directly to framebuffer
        uint32_t* current_in = framebuffer;
        uint32_t* current_out = fbout;

        for (size_t i = 0; i < children_.size(); i++) {
            EffectBase* child = children_[i].get();
            if (!child) continue;

            auto start = std::chrono::high_resolution_clock::now();
            int child_result = child->render(visdata, isBeat, current_in, current_out, w, h);
            auto end = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(end - start).count();
            child->set_last_render_time_us(us);

            // Handle beat signal modification
            if (child_result & RENDER_SET_BEAT) isBeat = 1;
            if (child_result & RENDER_CLR_BEAT) isBeat = 0;

            // Swap buffers if child wrote to fbout
            if (child_result & RENDER_SWAP_BUFFER) {
                std::swap(current_in, current_out);
            }
        }

        // If we ended with current_in != framebuffer, copy back
        if (current_in != framebuffer) {
            std::memcpy(framebuffer, current_in, pixel_count * sizeof(uint32_t));
        }

        g_line_blend_mode = line_blend_mode_save;
        fake_enabled_--;
        return result;
    }

    // Full compositing path: use internal buffer
    // Allocate or resize internal buffer if needed
    if (buffer_w_ != w || buffer_h_ != h || internal_buffer_.empty()) {
        internal_buffer_.resize(pixel_count);
        buffer_w_ = w;
        buffer_h_ = h;
        // Clear on resize unless we're preserving
        if (use_clear) {
            std::memset(internal_buffer_.data(), 0, pixel_count * sizeof(uint32_t));
        }
    }

    // Clear internal buffer if requested
    if (use_clear) {
        std::memset(internal_buffer_.data(), 0, pixel_count * sizeof(uint32_t));
    }

    // Input blending: blend parent framebuffer into internal buffer
    // Optimization: if adjustable mode with val >= 255, treat as Replace
    int use_blendin = blendin;
    if (use_blendin == 10 && inblendval >= 255) {
        use_blendin = 1;
    }

    switch (use_blendin) {
        case 0:  // Ignore - keep internal buffer as-is
            break;

        case 1:  // Replace
            std::memcpy(internal_buffer_.data(), framebuffer, pixel_count * sizeof(uint32_t));
            break;

        case 2:  // 50/50
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND_AVG(internal_buffer_[i], framebuffer[i]);
            }
            break;

        case 3:  // Maximum
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND_MAX(internal_buffer_[i], framebuffer[i]);
            }
            break;

        case 4:  // Additive
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND(internal_buffer_[i], framebuffer[i]);
            }
            break;

        case 5:  // Subtractive 1 (internal - parent)
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND_SUB(internal_buffer_[i], framebuffer[i]);
            }
            break;

        case 6:  // Subtractive 2 (parent - internal)
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND_SUB(framebuffer[i], internal_buffer_[i]);
            }
            break;

        case 7:  // Every other line
            {
                int y = h / 2;
                uint32_t* dest = internal_buffer_.data();
                uint32_t* src = framebuffer;
                while (y-- > 0) {
                    std::memcpy(dest, src, w * sizeof(uint32_t));
                    dest += w * 2;
                    src += w * 2;
                }
            }
            break;

        case 8:  // Every other pixel (checkerboard)
            {
                int row_offset = 0;
                uint32_t* dest = internal_buffer_.data();
                uint32_t* src = framebuffer;
                for (int y = 0; y < h; y++) {
                    for (int x = row_offset; x < w; x += 2) {
                        dest[x] = src[x];
                    }
                    row_offset ^= 1;
                    dest += w;
                    src += w;
                }
            }
            break;

        case 9:  // XOR
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = internal_buffer_[i] ^ framebuffer[i];
            }
            break;

        case 10: // Adjustable
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND_ADJ(framebuffer[i], internal_buffer_[i], inblendval);
            }
            break;

        case 11: // Multiply
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND_MUL(internal_buffer_[i], framebuffer[i]);
            }
            break;

        case 12: // Buffer
            {
                uint32_t* buf = get_global_buffer(w, h, bufferin, false);
                if (buf) {
                    for (int i = 0; i < pixel_count; i++) {
                        int alpha = depthof(buf[i], ininvert);
                        internal_buffer_[i] = BLEND_ADJ(framebuffer[i], internal_buffer_[i], alpha);
                    }
                }
            }
            break;

        case 13: // Minimum
            for (int i = 0; i < pixel_count; i++) {
                internal_buffer_[i] = BLEND_MIN(internal_buffer_[i], framebuffer[i]);
            }
            break;
    }

    // Render children to internal buffer
    uint32_t* current_in = internal_buffer_.data();
    uint32_t* current_out = fbout;

    for (size_t i = 0; i < children_.size(); i++) {
        EffectBase* child = children_[i].get();
        if (!child) continue;

        auto start = std::chrono::high_resolution_clock::now();
        int child_result = child->render(visdata, isBeat, current_in, current_out, w, h);
        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        child->set_last_render_time_us(us);

        // Handle beat signal modification
        if (child_result & RENDER_SET_BEAT) isBeat = 1;
        if (child_result & RENDER_CLR_BEAT) isBeat = 0;

        // Swap buffers if child wrote to fbout
        if (child_result & RENDER_SWAP_BUFFER) {
            std::swap(current_in, current_out);
        }
    }

    // If we ended in fbout, copy back to internal buffer
    if (current_in != internal_buffer_.data()) {
        std::memcpy(internal_buffer_.data(), current_in, pixel_count * sizeof(uint32_t));
    }

    // Output blending: blend internal buffer back to parent framebuffer
    uint32_t* tfb = internal_buffer_.data();

    // Optimization: if adjustable mode with val >= 255, treat as Replace
    int use_blendout = blendout;
    if (use_blendout == 10 && outblendval >= 255) {
        use_blendout = 1;
    }

    switch (use_blendout) {
        case 0:  // Ignore - don't output anything
            break;

        case 1:  // Replace
            std::memcpy(framebuffer, tfb, pixel_count * sizeof(uint32_t));
            break;

        case 2:  // 50/50
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND_AVG(framebuffer[i], tfb[i]);
            }
            break;

        case 3:  // Maximum
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND_MAX(framebuffer[i], tfb[i]);
            }
            break;

        case 4:  // Additive
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND(framebuffer[i], tfb[i]);
            }
            break;

        case 5:  // Subtractive 1 (parent - internal)
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND_SUB(framebuffer[i], tfb[i]);
            }
            break;

        case 6:  // Subtractive 2 (internal - parent)
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND_SUB(tfb[i], framebuffer[i]);
            }
            break;

        case 7:  // Every other line
            {
                int y = h / 2;
                uint32_t* dest = framebuffer;
                uint32_t* src = tfb;
                while (y-- > 0) {
                    std::memcpy(dest, src, w * sizeof(uint32_t));
                    dest += w * 2;
                    src += w * 2;
                }
            }
            break;

        case 8:  // Every other pixel (checkerboard)
            {
                int row_offset = 0;
                uint32_t* dest = framebuffer;
                uint32_t* src = tfb;
                for (int y = 0; y < h; y++) {
                    for (int x = row_offset; x < w; x += 2) {
                        dest[x] = src[x];
                    }
                    row_offset ^= 1;
                    dest += w;
                    src += w;
                }
            }
            break;

        case 9:  // XOR
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = framebuffer[i] ^ tfb[i];
            }
            break;

        case 10: // Adjustable
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND_ADJ(tfb[i], framebuffer[i], outblendval);
            }
            break;

        case 11: // Multiply
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND_MUL(framebuffer[i], tfb[i]);
            }
            break;

        case 12: // Buffer
            {
                uint32_t* buf = get_global_buffer(w, h, bufferout, false);
                if (buf) {
                    for (int i = 0; i < pixel_count; i++) {
                        int alpha = depthof(buf[i], outinvert);
                        framebuffer[i] = BLEND_ADJ(tfb[i], framebuffer[i], alpha);
                    }
                }
            }
            break;

        case 13: // Minimum
            for (int i = 0; i < pixel_count; i++) {
                framebuffer[i] = BLEND_MIN(framebuffer[i], tfb[i]);
            }
            break;
    }

    // Restore line blend mode
    g_line_blend_mode = line_blend_mode_save;
    fake_enabled_--;

    return 0;  // Result is always in framebuffer
}

// Blend mode names for UI
static const char* blend_mode_names[] = {
    "Ignore",
    "Replace",
    "50/50",
    "Maximum",
    "Additive",
    "Subtractive 1",
    "Subtractive 2",
    "Every other line",
    "Every other pixel",
    "XOR",
    "Adjustable",
    "Multiply",
    "Buffer",
    "Minimum"
};
static const int num_blend_modes = sizeof(blend_mode_names) / sizeof(blend_mode_names[0]);

// Buffer names for UI
static const char* buffer_names[] = {
    "Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4",
    "Buffer 5", "Buffer 6", "Buffer 7", "Buffer 8"
};

// Static member definition - UI layout for sub-lists
// Root lists use a simpler UI (just "clear each frame")
const PluginInfo EffectList::effect_info {
    .name = "Effect List",
    .category = "Misc",
    .description = "Container for grouping effects with compositing control",
    .author = "",
    .version = 1,
    .legacy_index = LEGACY_INDEX_LIST,  // 0xFFFFFFFE in original
    .factory = []() -> std::unique_ptr<EffectBase> {
        return std::make_unique<EffectList>();
    },
    .ui_layout = {{
        // Row 1: Enabled checkbox (0,2)
        {
            .id = "enabled",
            .text = "Enabled",
            .type = ControlType::CHECKBOX,
            .x = 0, .y = 2, .w = 56, .h = 10,
            .default_val = 1
        },
        // Beat render checkbox and frames input (89,3) and (167,1)
        {
            .id = "beat_render",
            .text = "Enabled OnBeat for",
            .type = ControlType::CHECKBOX,
            .x = 89, .y = 3, .w = 78, .h = 9,
            .default_val = 0
        },
        {
            .id = "beat_render_frames",
            .text = "",
            .type = ControlType::TEXT_INPUT,
            .x = 167, .y = 1, .w = 19, .h = 12,
            .default_val = 1
        },
        {
            .id = "frames_label",
            .text = "frames",
            .type = ControlType::LABEL,
            .x = 190, .y = 3, .w = 22, .h = 8
        },
        // Row 2: Clear every frame (0,13)
        {
            .id = "clear_each_frame",
            .text = "Clear every frame",
            .type = ControlType::CHECKBOX,
            .x = 0, .y = 13, .w = 71, .h = 10,
            .default_val = 0
        },
        // Input blending section - GROUPBOX with stacked controls
        // (Original has slider/buffer/invert at same y, conditionally visible - we stack them)
        {
            .id = "input_group",
            .text = "Input blending",
            .type = ControlType::GROUPBOX,
            .x = 0, .y = 27, .w = 111, .h = 58
        },
        {
            .id = "blendin",
            .text = "",
            .type = ControlType::DROPDOWN,
            .x = 5, .y = 37, .w = 101, .h = 14,
            .default_val = 1,
            .options = {"Ignore", "Replace", "50/50", "Maximum", "Additive",
                       "Subtractive 1", "Subtractive 2", "Every other line",
                       "Every other pixel", "XOR", "Adjustable", "Multiply",
                       "Buffer", "Minimum"}
        },
        {
            .id = "inblendval",
            .text = "",
            .type = ControlType::SLIDER,
            .x = 1, .y = 53, .w = 106, .h = 11,
            .range = {0, 255},
            .default_val = 128
        },
        {
            .id = "bufferin",
            .text = "",
            .type = ControlType::DROPDOWN,
            .x = 5, .y = 66, .w = 62, .h = 14,
            .default_val = 0,
            .options = {"Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4",
                       "Buffer 5", "Buffer 6", "Buffer 7", "Buffer 8"}
        },
        {
            .id = "ininvert",
            .text = "Invert",
            .type = ControlType::CHECKBOX,
            .x = 72, .y = 68, .w = 34, .h = 10,
            .default_val = 0
        },
        // Output blending section - GROUPBOX with stacked controls
        {
            .id = "output_group",
            .text = "Output blending",
            .type = ControlType::GROUPBOX,
            .x = 115, .y = 27, .w = 111, .h = 58
        },
        {
            .id = "blendout",
            .text = "",
            .type = ControlType::DROPDOWN,
            .x = 121, .y = 37, .w = 101, .h = 14,
            .default_val = 1,
            .options = {"Ignore", "Replace", "50/50", "Maximum", "Additive",
                       "Subtractive 1", "Subtractive 2", "Every other line",
                       "Every other pixel", "XOR", "Adjustable", "Multiply",
                       "Buffer", "Minimum"}
        },
        {
            .id = "outblendval",
            .text = "",
            .type = ControlType::SLIDER,
            .x = 117, .y = 53, .w = 106, .h = 11,
            .range = {0, 255},
            .default_val = 128
        },
        {
            .id = "bufferout",
            .text = "",
            .type = ControlType::DROPDOWN,
            .x = 121, .y = 66, .w = 62, .h = 14,
            .default_val = 0,
            .options = {"Buffer 1", "Buffer 2", "Buffer 3", "Buffer 4",
                       "Buffer 5", "Buffer 6", "Buffer 7", "Buffer 8"}
        },
        {
            .id = "outinvert",
            .text = "Invert",
            .type = ControlType::CHECKBOX,
            .x = 187, .y = 68, .w = 34, .h = 10,
            .default_val = 0
        },
        // Evaluation override section (shifted down for taller groupboxes)
        {
            .id = "use_code",
            .text = "Use evaluation override (can change enabled, clear, etc):",
            .type = ControlType::CHECKBOX,
            .x = 0, .y = 90, .w = 197, .h = 10,
            .default_val = 0
        },
        // Init script section
        {
            .id = "init_script",
            .text = "",
            .type = ControlType::EDITTEXT,
            .x = 25, .y = 105, .w = 208, .h = 26,
            .default_val = ""
        },
        {
            .id = "init_label",
            .text = "init",
            .type = ControlType::LABEL,
            .x = 0, .y = 114, .w = 10, .h = 8
        },
        // Frame script section
        {
            .id = "frame_script",
            .text = "",
            .type = ControlType::EDITTEXT,
            .x = 25, .y = 133, .w = 208, .h = 60,
            .default_val = ""
        },
        {
            .id = "frame_label",
            .text = "frame",
            .type = ControlType::LABEL,
            .x = 0, .y = 155, .w = 18, .h = 8
        },
        // Help button
        {
            .id = "help_btn",
            .text = "expression help",
            .type = ControlType::HELP_BUTTON,
            .x = 25, .y = 198, .w = 67, .h = 14
        }
    }},
    .help_text =
        "Effect List\n"
        "\n"
        "Variables:\n"
        "  beat = beat flag (read/write)\n"
        "  enabled = enable/disable effect list (read/write)\n"
        "  clear = clear framebuffer before rendering (read/write)\n"
        "  alphain = input blend alpha 0.0-1.0 (read/write)\n"
        "  alphaout = output blend alpha 0.0-1.0 (read/write)\n"
        "  w = frame width (read)\n"
        "  h = frame height (read)\n"
        "\n"
        "Examples:\n"
        "  Beat gating: beat = beat * (count % 4 == 0); count = count + beat;\n"
        "  Pulsing: enabled = sin(t) > 0; t = t + 0.1;\n"
        "  Beat fade: alphaout = beat ? 1.0 : alphaout * 0.95;\n"
        "  Conditional clear: clear = beat;\n"
};

// Register effect at startup
static bool register_effect_list = []() {
    PluginManager::instance().register_effect(EffectList::effect_info);
    return true;
}();

} // namespace avs
