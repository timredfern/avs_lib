// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "oscilloscope.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"
#include "core/line_draw_ext.h"
#include "core/color.h"
#include "core/ui.h"
#include <algorithm>
#include <cmath>

namespace avs {

OscilloscopeEffect::OscilloscopeEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
}

// Oscilloscope uses the shared draw_line from core/line_draw.h

int OscilloscopeEffect::render(AudioData visdata, int isBeat,
                              uint32_t* framebuffer, uint32_t* fbout,
                              int w, int h) {
    if (!is_enabled()) return 0;
    if (isBeat & 0x80000000) return 0;

    // Get parameters
    auto channel = static_cast<AudioChannel>(parameters().get_int("channel"));
    auto draw_style = static_cast<DrawStyle>(parameters().get_int("draw_style"));
    auto position = static_cast<VerticalPosition>(parameters().get_int("position"));

    // Color cycling - matches original AVS behavior
    int num_colors = parameters().get_int("num_colors");
    if (num_colors < 1) num_colors = 1;
    if (num_colors > 16) num_colors = 16;

    uint32_t color;
    if (num_colors == 1) {
        // Single color - no cycling
        color = parameters().get_color("color_0");
    } else {
        // Multi-color cycling with interpolation
        // Each color gets 64 frames, then interpolates to next
        color_pos_++;
        if (color_pos_ >= num_colors * 64) color_pos_ = 0;

        int p = color_pos_ / 64;      // Current color index
        int r = color_pos_ & 63;      // Interpolation factor (0-63)

        std::string c1_param = "color_" + std::to_string(p);
        std::string c2_param = "color_" + std::to_string((p + 1 < num_colors) ? p + 1 : 0);
        uint32_t c1 = parameters().get_color(c1_param);
        uint32_t c2 = parameters().get_color(c2_param);

        // Linear interpolation: blend = ((c1 * (63-r)) + (c2 * r)) / 64
        int red = ((color::red(c1) * (63 - r)) + (color::red(c2) * r)) / 64;
        int green = ((color::green(c1) * (63 - r)) + (color::green(c2) * r)) / 64;
        int blue = ((color::blue(c1) * (63 - r)) + (color::blue(c2) * r)) / 64;

        color = color::make(red, green, blue);
    }

    // mode: 0 = spectrum, 1 = oscilloscope (waveform)
    int audio_type = (parameters().get_int("mode") == 0) ? AUDIO_SPECTRUM : AUDIO_WAVEFORM;

    // Scale factors matching original AVS (uses 288 samples, scales to screen width)
    const int sample_count = 288;
    float x_scale = (float)sample_count / w;
    float y_scale = h / 2.0f / 256.0f;

    // Calculate vertical position based on position setting
    int y_base;
    if (position == VerticalPosition::TOP) {
        y_base = 0;
    } else if (position == VerticalPosition::BOTTOM) {
        y_base = h / 2;
    } else {  // CENTER
        y_base = h / 4;
    }
    int y_center = y_base + static_cast<int>(y_scale * 128.0f);

    // Choose which channel to display
    char* audio_data;
    static char mixed_data[MAX_AUDIO_SAMPLES];

    if (channel == AudioChannel::RIGHT) {
        audio_data = &visdata[audio_type][AUDIO_RIGHT][0];
    } else if (channel == AudioChannel::CENTER) {
        // Mix both channels
        for (int i = 0; i < MIN_AUDIO_SAMPLES; i++) {
            mixed_data[i] = (visdata[audio_type][AUDIO_LEFT][i] + visdata[audio_type][AUDIO_RIGHT][i]) / 2;
        }
        audio_data = mixed_data;
    } else {  // LEFT (default)
        audio_data = &visdata[audio_type][AUDIO_LEFT][0];
    }

    // Helper to convert audio sample to unsigned
    // Waveform: signed char needs XOR 128 to convert to 0-255
    // Spectrum: already unsigned 0-255, no conversion needed
    auto sample_to_unsigned = [audio_type](char sample) -> int {
        if (audio_type == AUDIO_WAVEFORM) {
            // Waveform: convert signed to unsigned
            return static_cast<unsigned char>(sample) ^ 128;
        } else {
            // Spectrum: already unsigned
            return static_cast<unsigned char>(sample);
        }
    };

    if (draw_style == DrawStyle::SOLID) {
        // Solid scope: draw vertical bars from center to waveform value
        for (int x = 0; x < w; x++) {
            float r = x * x_scale;
            int idx = static_cast<int>(r);
            float frac = r - idx;
            if (idx >= sample_count - 1) idx = sample_count - 2;

            float yr = sample_to_unsigned(audio_data[idx]) * (1.0f - frac) +
                       sample_to_unsigned(audio_data[idx + 1]) * frac;
            int y = y_base + static_cast<int>(yr * y_scale);

            draw_line_styled(framebuffer, x, y_center, x, y, w, h, color);
        }
    } else if (draw_style == DrawStyle::LINES) {
        // Line scope: draw connected line segments
        float xs = 1.0f / x_scale;  // w / 288.0
        int lx = 0;
        int ly = y_base + static_cast<int>(sample_to_unsigned(audio_data[0]) * y_scale);

        for (int i = 1; i < sample_count; i++) {
            int ox = static_cast<int>(i * xs);
            int oy = y_base + static_cast<int>(sample_to_unsigned(audio_data[i]) * y_scale);

            draw_line_styled(framebuffer, lx, ly, ox, oy, w, h, color);

            lx = ox;
            ly = oy;
        }
    } else {  // DOTS
        // Dot scope: draw individual dots with interpolation
        for (int x = 0; x < w; x++) {
            float r = x * x_scale;
            int idx = static_cast<int>(r);
            float frac = r - idx;
            if (idx >= sample_count - 1) idx = sample_count - 2;

            float yr = sample_to_unsigned(audio_data[idx]) * (1.0f - frac) +
                       sample_to_unsigned(audio_data[idx + 1]) * frac;
            int y = y_base + static_cast<int>(yr * y_scale);

            draw_point_styled(framebuffer, x, y, w, h, color);
        }
    }

    return 0;
}

void OscilloscopeEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;

    BinaryReader reader(data);

    // Binary format from r_simple.cpp:
    // effect (int32) - bitfield: bits 0-1=mode, bit 6=dot mode, bits 2-3=channel, bits 4-5=position
    // num_colors (int32)
    // colors[num_colors] (int32 each, BGR format)
    uint32_t effect = reader.read_u32();

    // Decode effect bitfield
    int mode_bits = effect & 0x03;       // 0-3: draw style
    bool dot_mode = (effect >> 6) & 1;   // bit 6: dot mode
    int channel = (effect >> 2) & 0x03;  // bits 2-3: channel
    int position = (effect >> 4) & 0x03; // bits 4-5: position

    // Map to our parameters
    // Original: mode 0=solid analyzer, 1=line analyzer, 2=line scope, 3=solid scope
    // With dot_mode: mode 0=dot analyzer, mode 2=dot scope
    // Our mode: 0=spectrum, 1=oscilloscope
    // Our draw_style: 0=lines, 1=solid, 2=dots
    //
    // Note: The meaning of bit 0 is INVERTED between spectrum and oscilloscope:
    //   Spectrum:     bit 0 = 0 means solid, bit 0 = 1 means lines
    //   Oscilloscope: bit 0 = 0 means lines, bit 0 = 1 means solid
    if (dot_mode) {
        parameters().set_int("draw_style", 2);  // Dots
        parameters().set_int("mode", (mode_bits & 2) ? 1 : 0);  // bit 1 = osc vs spec
    } else {
        // mode_bits: 0=solid analyzer, 1=line analyzer, 2=line scope, 3=solid scope
        bool is_oscilloscope = mode_bits >= 2;
        parameters().set_int("mode", is_oscilloscope ? 1 : 0);
        if (is_oscilloscope) {
            // Oscilloscope: bit 0 = 0 means lines, bit 0 = 1 means solid
            parameters().set_int("draw_style", (mode_bits & 1) ? 1 : 0);
        } else {
            // Spectrum: bit 0 = 0 means solid, bit 0 = 1 means lines
            parameters().set_int("draw_style", (mode_bits & 1) ? 0 : 1);
        }
    }

    parameters().set_int("channel", channel);
    parameters().set_int("position", position);

    // Read colors
    if (reader.remaining() >= 4) {
        int num_colors = reader.read_u32();
        if (num_colors > 16) num_colors = 16;
        if (num_colors < 1) num_colors = 1;
        parameters().set_int("num_colors", num_colors);

        // Colors stored as 0x00RRGGBB (same as internal ARGB), no swap needed
        for (int i = 0; i < num_colors && reader.remaining() >= 4; i++) {
            uint32_t color = reader.read_u32() | 0xFF000000;
            parameters().set_color("color_" + std::to_string(i), color);
        }
    }
}

// Static member definition
const PluginInfo OscilloscopeEffect::effect_info {
    .name = "Oscilloscope",
    .category = "Render",
    .description = "",
    .author = "",
    .version = 1,
    .legacy_index = 0,  // R_SimpleSpectrum ("Simple" in original AVS)
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<OscilloscopeEffect>();
    },
    .ui_layout = {
        {
            // Render mode: Spectrum vs Oscilloscope
            {
                .id = "mode",
                .type = ControlType::RADIO_GROUP,
                .default_val = 1,  // Oscilloscope
                .radio_options = {
                    {"Spectrum", 6, 9, 46, 10},
                    {"Oscilloscope", 53, 9, 53, 10}
                }
            },
            // Draw style: Lines, Solid, Dots
            {
                .id = "draw_style",
                .type = ControlType::RADIO_GROUP,
                .default_val = 0,  // Lines
                .radio_options = {
                    {"Lines", 6, 24, 33, 10},
                    {"Solid", 40, 24, 31, 10},
                    {"Dots", 72, 24, 31, 10}
                }
            },
            // Channel selection
            {
                .id = "channel",
                .type = ControlType::RADIO_GROUP,
                .default_val = 0,  // Left
                .radio_options = {
                    {"Left channel", 8, 68, 56, 10},
                    {"Right channel", 8, 78, 61, 10},
                    {"Center channel", 8, 88, 65, 10}
                }
            },
            // Vertical position
            {
                .id = "position",
                .type = ControlType::RADIO_GROUP,
                .default_val = 2,  // Center
                .radio_options = {
                    {"Top", 86, 68, 29, 10},
                    {"Bottom", 86, 78, 38, 10},
                    {"Center", 86, 88, 37, 10}
                }
            },
            // Color controls
            {
                .id = "num_colors",
                .text = "Colors",
                .type = ControlType::TEXT_INPUT,
                .x = 53, .y = 107, .w = 19, .h = 12,
                .range = {1, 16},
                .default_val = 1
            },
            {
                .id = "colors",
                .text = "",
                .type = ControlType::COLOR_ARRAY,
                .x = 6, .y = 122, .w = 127, .h = 11,
                .default_val = 0xFFFFFFFF,
                .max_items = 16
            }
        }
    }
};

// Register effect at startup
static bool register_oscilloscope = []() {
    PluginManager::instance().register_effect(OscilloscopeEffect::effect_info);
    return true;
}();

} // namespace avs