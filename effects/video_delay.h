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

class VideoDelayEffect : public EffectBase {
public:
    VideoDelayEffect();
    virtual ~VideoDelayEffect() = default;

    int render(AudioData visdata, int isBeat,
               uint32_t* framebuffer, uint32_t* fbout,
               int w, int h) override;

    const PluginInfo& get_plugin_info() const override;
    void load_parameters(const std::vector<uint8_t>& data) override;
    void on_parameter_changed(const std::string& name) override;

    static const PluginInfo effect_info;

private:
    std::vector<uint32_t> buffer_;
    size_t buffer_size_ = 1;      // Allocated size in pixels
    size_t virtual_size_ = 1;     // Used size in pixels
    size_t old_virtual_size_ = 1;
    size_t in_out_pos_ = 0;       // Current position in circular buffer

    int frames_since_beat_ = 0;
    int frame_delay_ = 10;        // Actual delay in frames
    int frame_mem_ = 0;           // w*h of current frame
    int old_frame_mem_ = 0;
};

} // namespace avs
