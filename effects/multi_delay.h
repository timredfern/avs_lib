// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include "core/effect_base.h"
#include "core/ui.h"
#include <vector>
#include <cstring>

namespace avs {

// Global shared state for Multi Delay buffers
// Original AVS uses 6 global buffers shared across all instances
struct MultiDelayGlobal {
    static constexpr int NUM_BUFFERS = 6;

    // Per-buffer state
    bool use_beats[NUM_BUFFERS] = {};
    int delay[NUM_BUFFERS] = {};
    int frame_delay[NUM_BUFFERS] = {};

    std::vector<uint32_t> buffer[NUM_BUFFERS];
    size_t in_pos[NUM_BUFFERS] = {};
    size_t out_pos[NUM_BUFFERS] = {};
    size_t virtual_size[NUM_BUFFERS] = {};  // in pixels
    size_t old_virtual_size[NUM_BUFFERS] = {};

    // Beat tracking
    int frames_since_beat = 0;
    int frames_per_beat = 0;

    // Frame tracking
    int frame_mem = 0;  // w*h of current frame
    int old_frame_mem = 0;

    // Instance tracking
    unsigned int num_instances = 0;
    unsigned int render_id = 0;

    static MultiDelayGlobal& instance() {
        static MultiDelayGlobal g;
        return g;
    }

private:
    MultiDelayGlobal() {
        for (int i = 0; i < NUM_BUFFERS; i++) {
            buffer[i].resize(1);
            virtual_size[i] = 1;
            old_virtual_size[i] = 1;
        }
    }
};

class MultiDelayEffect : public EffectBase {
public:
    MultiDelayEffect();
    virtual ~MultiDelayEffect();

    int render(AudioData visdata, int isBeat,
               uint32_t* framebuffer, uint32_t* fbout,
               int w, int h) override;

    const PluginInfo& get_plugin_info() const override;
    void load_parameters(const std::vector<uint8_t>& data) override;
    void on_parameter_changed(const std::string& name) override;

    static const PluginInfo effect_info;

private:
    unsigned int creation_id_;
};

} // namespace avs
