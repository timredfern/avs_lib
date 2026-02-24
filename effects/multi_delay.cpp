// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "multi_delay.h"
#include "core/binary_reader.h"
#include "core/plugin_manager.h"
#include "core/ui.h"
#include <algorithm>

namespace avs {

MultiDelayEffect::MultiDelayEffect() {
    auto& g = MultiDelayGlobal::instance();
    g.num_instances++;
    creation_id_ = g.num_instances;

    if (creation_id_ == 1) {
        // First instance - initialize global state
        g.render_id = 0;
        g.frames_since_beat = 0;
        g.frames_per_beat = 0;
        g.frame_mem = 1;
        g.old_frame_mem = 1;

        for (int i = 0; i < MultiDelayGlobal::NUM_BUFFERS; i++) {
            g.use_beats[i] = false;
            g.delay[i] = 0;
            g.frame_delay[i] = 0;
            g.buffer[i].resize(1);
            g.buffer[i][0] = 0;
            g.in_pos[i] = 0;
            g.out_pos[i] = 0;
            g.virtual_size[i] = 1;
            g.old_virtual_size[i] = 1;
        }
    }

    init_parameters_from_layout(effect_info.ui_layout);
}

MultiDelayEffect::~MultiDelayEffect() {
    auto& g = MultiDelayGlobal::instance();
    g.num_instances--;

    if (g.num_instances == 0) {
        // Last instance - clear buffers
        for (int i = 0; i < MultiDelayGlobal::NUM_BUFFERS; i++) {
            g.buffer[i].clear();
            g.buffer[i].shrink_to_fit();
        }
    }
}

int MultiDelayEffect::render(AudioData visdata, int isBeat,
                             uint32_t* framebuffer, uint32_t* fbout,
                             int w, int h) {
    (void)visdata;
    (void)fbout;

    if (isBeat & 0x80000000) return 0;

    auto& g = MultiDelayGlobal::instance();

    // Track render order across instances
    if (g.render_id == g.num_instances) g.render_id = 0;
    g.render_id++;

    int mode = parameters().get_int("mode");
    int active_buffer = parameters().get_int("active_buffer");
    size_t frame_pixels = static_cast<size_t>(w) * h;

    // First instance per frame handles global updates
    if (g.render_id == 1) {
        g.frame_mem = static_cast<int>(frame_pixels);

        // Beat tracking
        if (isBeat) {
            g.frames_per_beat = g.frames_since_beat;
            for (int i = 0; i < MultiDelayGlobal::NUM_BUFFERS; i++) {
                if (g.use_beats[i]) {
                    g.frame_delay[i] = g.frames_per_beat + 1;
                }
            }
            g.frames_since_beat = 0;
        }
        g.frames_since_beat++;

        // Update buffers
        for (int i = 0; i < MultiDelayGlobal::NUM_BUFFERS; i++) {
            if (g.frame_delay[i] > 1) {
                size_t new_virtual_size = static_cast<size_t>(g.frame_delay[i]) * frame_pixels;

                if (g.frame_mem == g.old_frame_mem) {
                    // Frame size unchanged - handle virtual size changes
                    if (new_virtual_size != g.old_virtual_size[i]) {
                        if (new_virtual_size > g.old_virtual_size[i]) {
                            // Growing buffer
                            if (new_virtual_size > g.buffer[i].size()) {
                                // Need to allocate more
                                size_t new_size = g.use_beats[i] ? new_virtual_size * 2 : new_virtual_size;
                                try {
                                    g.buffer[i].resize(new_size, 0);
                                } catch (...) {
                                    // Allocation failed
                                    g.frame_delay[i] = 0;
                                    if (g.use_beats[i]) {
                                        g.frames_per_beat = 0;
                                        g.frames_since_beat = 0;
                                        g.delay[i] = 0;
                                    }
                                    continue;
                                }
                            }
                            // Fill gap with copies of last frame
                            size_t old_end = g.old_virtual_size[i];
                            for (size_t pos = old_end; pos < new_virtual_size; pos += frame_pixels) {
                                size_t copy_size = std::min(frame_pixels, new_virtual_size - pos);
                                std::copy_n(g.buffer[i].begin(), copy_size,
                                           g.buffer[i].begin() + pos);
                            }
                        }
                        // For shrinking, we just use less of the buffer
                        g.virtual_size[i] = new_virtual_size;
                        g.old_virtual_size[i] = new_virtual_size;

                        // Reset positions to be safe
                        g.out_pos[i] = 0;
                        g.in_pos[i] = (new_virtual_size > frame_pixels) ?
                                      new_virtual_size - frame_pixels : 0;
                    }
                } else {
                    // Frame size changed - reallocate
                    size_t new_size = g.use_beats[i] ? new_virtual_size * 2 : new_virtual_size;
                    try {
                        g.buffer[i].resize(new_size, 0);
                    } catch (...) {
                        g.frame_delay[i] = 0;
                        if (g.use_beats[i]) {
                            g.frames_per_beat = 0;
                            g.frames_since_beat = 0;
                            g.delay[i] = 0;
                        }
                        continue;
                    }
                    g.virtual_size[i] = new_virtual_size;
                    g.old_virtual_size[i] = new_virtual_size;
                    g.out_pos[i] = 0;
                    g.in_pos[i] = (new_virtual_size > frame_pixels) ?
                                  new_virtual_size - frame_pixels : 0;
                }
            }
        }
        g.old_frame_mem = g.frame_mem;
    }

    // Apply effect based on mode
    if (mode != 0 && g.frame_delay[active_buffer] > 1 &&
        g.buffer[active_buffer].size() >= frame_pixels) {

        if (mode == 2) {
            // Output mode - copy from buffer to framebuffer
            size_t out = g.out_pos[active_buffer];
            if (out + frame_pixels <= g.buffer[active_buffer].size()) {
                std::memcpy(framebuffer, &g.buffer[active_buffer][out],
                           frame_pixels * sizeof(uint32_t));
            }
        } else if (mode == 1) {
            // Input mode - copy from framebuffer to buffer
            size_t in = g.in_pos[active_buffer];
            if (in + frame_pixels <= g.buffer[active_buffer].size()) {
                std::memcpy(&g.buffer[active_buffer][in], framebuffer,
                           frame_pixels * sizeof(uint32_t));
            }
        }
    }

    // Last instance per frame advances buffer positions
    if (g.render_id == g.num_instances) {
        for (int i = 0; i < MultiDelayGlobal::NUM_BUFFERS; i++) {
            if (g.virtual_size[i] > 0 && frame_pixels > 0) {
                g.in_pos[i] += frame_pixels;
                g.out_pos[i] += frame_pixels;
                if (g.in_pos[i] >= g.virtual_size[i]) g.in_pos[i] = 0;
                if (g.out_pos[i] >= g.virtual_size[i]) g.out_pos[i] = 0;
            }
        }
    }

    return 0;
}

void MultiDelayEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return;
    BinaryReader reader(data);

    int mode = reader.read_u32();
    parameters().set_int("mode", mode);

    int active_buffer = reader.read_u32();
    parameters().set_int("active_buffer", active_buffer);

    // Buffer settings are only saved by first instance
    if (creation_id_ == 1) {
        auto& g = MultiDelayGlobal::instance();
        for (int i = 0; i < MultiDelayGlobal::NUM_BUFFERS && reader.remaining() >= 8; i++) {
            g.use_beats[i] = (reader.read_u32() == 1);
            g.delay[i] = reader.read_u32();
            g.frame_delay[i] = (g.use_beats[i] ? g.frames_per_beat : g.delay[i]) + 1;

            // Set UI parameters
            parameters().set_int("timing_" + std::to_string(i), g.use_beats[i] ? 0 : 1);
            parameters().set_int("delay_" + std::to_string(i), g.delay[i]);
        }
    }
}

void MultiDelayEffect::on_parameter_changed(const std::string& name) {
    auto& g = MultiDelayGlobal::instance();

    // Check for delay_X or timing_X parameter changes
    for (int i = 0; i < MultiDelayGlobal::NUM_BUFFERS; i++) {
        std::string delay_id = "delay_" + std::to_string(i);
        std::string timing_id = "timing_" + std::to_string(i);

        if (name == delay_id || name == timing_id) {
            // Update global state from UI parameters
            g.delay[i] = parameters().get_int(delay_id);
            g.use_beats[i] = (parameters().get_int(timing_id) == 0);  // 0 = Beats, 1 = Frames

            // Recalculate frame_delay
            if (g.use_beats[i]) {
                g.frame_delay[i] = g.frames_per_beat + 1;
            } else {
                g.frame_delay[i] = g.delay[i] + 1;
            }
            break;
        }
    }
}

const PluginInfo& MultiDelayEffect::get_plugin_info() const {
    return effect_info;
}

const PluginInfo MultiDelayEffect::effect_info = {
    .name = "Multi Delay",
    .category = "Trans",
    .description = "Multi-buffer frame delay/echo effect",
    .author = "Tom Holden",
    .version = 1,
    .legacy_index = 42,
    .factory = []() -> std::unique_ptr<EffectBase> {
        return std::make_unique<MultiDelayEffect>();
    },
    .ui_layout = {
        {
            // Mode selection (row 0)
            {.id = "mode", .text = "Mode", .type = ControlType::RADIO_GROUP,
             .x = 0, .y = 0, .w = 200, .h = 20, .default_val = 0,
             .radio_options = {
                 {"Disabled", 0, 0, 60, 12},
                 {"Input", 70, 0, 50, 12},
                 {"Output", 130, 0, 60, 12}
             }},

            // Active buffer selection (row 1)
            {.id = "active_buffer", .text = "Active Buffer", .type = ControlType::RADIO_GROUP,
             .x = 0, .y = 30, .w = 200, .h = 20, .default_val = 0,
             .radio_options = {
                 {"A", 0, 0, 25, 12},
                 {"B", 30, 0, 25, 12},
                 {"C", 60, 0, 25, 12},
                 {"D", 90, 0, 25, 12},
                 {"E", 120, 0, 25, 12},
                 {"F", 150, 0, 25, 12}
             }},

            // Buffer delays section header
            {.id = "buffers_label", .text = "Buffer Delays (frames or beats)", .type = ControlType::GROUPBOX,
             .x = 0, .y = 60, .w = 200, .h = 10},

            // Buffer A (row 2)
            {.id = "label_a", .text = "A:", .type = ControlType::LABEL,
             .x = 0, .y = 80, .w = 15, .h = 12},
            {.id = "delay_0", .text = "", .type = ControlType::TEXT_INPUT,
             .x = 20, .y = 80, .w = 40, .h = 12, .range = {0, 60}, .default_val = 0},
            {.id = "timing_0", .text = "", .type = ControlType::RADIO_GROUP,
             .x = 70, .y = 80, .w = 100, .h = 12, .default_val = 1,
             .radio_options = {{"Beats", 0, 0, 45, 12}, {"Frames", 50, 0, 50, 12}}},

            // Buffer B (row 3)
            {.id = "label_b", .text = "B:", .type = ControlType::LABEL,
             .x = 0, .y = 100, .w = 15, .h = 12},
            {.id = "delay_1", .text = "", .type = ControlType::TEXT_INPUT,
             .x = 20, .y = 100, .w = 40, .h = 12, .range = {0, 60}, .default_val = 0},
            {.id = "timing_1", .text = "", .type = ControlType::RADIO_GROUP,
             .x = 70, .y = 100, .w = 100, .h = 12, .default_val = 1,
             .radio_options = {{"Beats", 0, 0, 45, 12}, {"Frames", 50, 0, 50, 12}}},

            // Buffer C (row 4)
            {.id = "label_c", .text = "C:", .type = ControlType::LABEL,
             .x = 0, .y = 120, .w = 15, .h = 12},
            {.id = "delay_2", .text = "", .type = ControlType::TEXT_INPUT,
             .x = 20, .y = 120, .w = 40, .h = 12, .range = {0, 60}, .default_val = 0},
            {.id = "timing_2", .text = "", .type = ControlType::RADIO_GROUP,
             .x = 70, .y = 120, .w = 100, .h = 12, .default_val = 1,
             .radio_options = {{"Beats", 0, 0, 45, 12}, {"Frames", 50, 0, 50, 12}}},

            // Buffer D (row 5)
            {.id = "label_d", .text = "D:", .type = ControlType::LABEL,
             .x = 0, .y = 140, .w = 15, .h = 12},
            {.id = "delay_3", .text = "", .type = ControlType::TEXT_INPUT,
             .x = 20, .y = 140, .w = 40, .h = 12, .range = {0, 60}, .default_val = 0},
            {.id = "timing_3", .text = "", .type = ControlType::RADIO_GROUP,
             .x = 70, .y = 140, .w = 100, .h = 12, .default_val = 1,
             .radio_options = {{"Beats", 0, 0, 45, 12}, {"Frames", 50, 0, 50, 12}}},

            // Buffer E (row 6)
            {.id = "label_e", .text = "E:", .type = ControlType::LABEL,
             .x = 0, .y = 160, .w = 15, .h = 12},
            {.id = "delay_4", .text = "", .type = ControlType::TEXT_INPUT,
             .x = 20, .y = 160, .w = 40, .h = 12, .range = {0, 60}, .default_val = 0},
            {.id = "timing_4", .text = "", .type = ControlType::RADIO_GROUP,
             .x = 70, .y = 160, .w = 100, .h = 12, .default_val = 1,
             .radio_options = {{"Beats", 0, 0, 45, 12}, {"Frames", 50, 0, 50, 12}}},

            // Buffer F (row 7)
            {.id = "label_f", .text = "F:", .type = ControlType::LABEL,
             .x = 0, .y = 180, .w = 15, .h = 12},
            {.id = "delay_5", .text = "", .type = ControlType::TEXT_INPUT,
             .x = 20, .y = 180, .w = 40, .h = 12, .range = {0, 60}, .default_val = 0},
            {.id = "timing_5", .text = "", .type = ControlType::RADIO_GROUP,
             .x = 70, .y = 180, .w = 100, .h = 12, .default_val = 1,
             .radio_options = {{"Beats", 0, 0, 45, 12}, {"Frames", 50, 0, 50, 12}}},
        }
    }
};

} // namespace avs
