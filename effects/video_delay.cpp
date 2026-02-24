// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "video_delay.h"
#include "core/binary_reader.h"
#include "core/plugin_manager.h"
#include "core/ui.h"
#include <algorithm>

namespace avs {

VideoDelayEffect::VideoDelayEffect() {
    buffer_.resize(1);
    init_parameters_from_layout(effect_info.ui_layout);
}

int VideoDelayEffect::render(AudioData visdata, int isBeat,
                             uint32_t* framebuffer, uint32_t* fbout,
                             int w, int h) {
    (void)visdata;

    if (isBeat & 0x80000000) return 0;

    bool enabled = parameters().get_bool("enabled");
    bool use_beats = (parameters().get_int("timing") == 0);
    int delay = parameters().get_int("delay");

    frame_mem_ = w * h;

    // Beat tracking for beat-synced mode
    if (use_beats) {
        if (isBeat) {
            // Original: framedelay = framessincebeat * delay (capped at 400)
            frame_delay_ = frames_since_beat_ * delay;
            if (frame_delay_ > 400) frame_delay_ = 400;
            frames_since_beat_ = 0;
        }
        frames_since_beat_++;
    } else {
        frame_delay_ = delay;
    }

    if (!enabled || frame_delay_ == 0) {
        return 0;
    }

    size_t frame_pixels = static_cast<size_t>(frame_mem_);
    virtual_size_ = static_cast<size_t>(frame_delay_) * frame_pixels;

    if (frame_mem_ == old_frame_mem_) {
        // Frame size unchanged - handle virtual size changes
        if (virtual_size_ != old_virtual_size_) {
            if (virtual_size_ > old_virtual_size_) {
                // Growing buffer
                if (virtual_size_ > buffer_size_) {
                    // Need more memory
                    size_t new_size = use_beats ? virtual_size_ * 2 : virtual_size_;
                    // Cap at 400 frames worth
                    size_t max_size = frame_pixels * 400;
                    if (new_size > max_size) new_size = max_size;

                    try {
                        buffer_.resize(new_size, 0);
                        buffer_size_ = new_size;
                    } catch (...) {
                        frame_delay_ = 0;
                        frames_since_beat_ = 0;
                        return 0;
                    }
                }
                // Fill gap with copies of current content
                size_t old_end = old_virtual_size_;
                for (size_t pos = old_end; pos < virtual_size_; pos += frame_pixels) {
                    size_t copy_size = std::min(frame_pixels, virtual_size_ - pos);
                    if (in_out_pos_ + copy_size <= buffer_.size()) {
                        std::copy_n(buffer_.begin() + in_out_pos_, copy_size,
                                   buffer_.begin() + pos);
                    }
                }
            }
            // For shrinking, adjust position if needed
            if (in_out_pos_ >= virtual_size_) {
                in_out_pos_ = 0;
            }
            old_virtual_size_ = virtual_size_;
        }
    } else {
        // Frame size changed - reallocate
        size_t new_size = use_beats ? virtual_size_ * 2 : virtual_size_;
        size_t max_size = frame_pixels * 400;
        if (new_size > max_size) new_size = max_size;

        try {
            buffer_.resize(new_size, 0);
            buffer_size_ = new_size;
        } catch (...) {
            frame_delay_ = 0;
            frames_since_beat_ = 0;
            return 0;
        }
        in_out_pos_ = 0;
        old_virtual_size_ = virtual_size_;
    }
    old_frame_mem_ = frame_mem_;

    // Perform the delay operation
    if (buffer_.size() >= frame_pixels && virtual_size_ >= frame_pixels) {
        // Copy delayed frame to output
        if (in_out_pos_ + frame_pixels <= buffer_.size()) {
            std::memcpy(fbout, &buffer_[in_out_pos_], frame_pixels * sizeof(uint32_t));
        }
        // Copy current frame to buffer
        if (in_out_pos_ + frame_pixels <= buffer_.size()) {
            std::memcpy(&buffer_[in_out_pos_], framebuffer, frame_pixels * sizeof(uint32_t));
        }

        // Advance position
        in_out_pos_ += frame_pixels;
        if (in_out_pos_ >= virtual_size_) {
            in_out_pos_ = 0;
        }

        return 1;  // Output is in fbout
    }

    return 0;
}

void VideoDelayEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.size() < 12) return;
    BinaryReader reader(data);

    bool enabled = (reader.read_u32() == 1);
    parameters().set_bool("enabled", enabled);

    bool use_beats = (reader.read_u32() == 1);
    parameters().set_int("timing", use_beats ? 0 : 1);

    int delay = static_cast<int>(reader.read_u32());
    // Apply original limits
    if (use_beats && delay > 16) delay = 16;
    if (!use_beats && delay > 200) delay = 200;
    parameters().set_int("delay", delay);

    // Initialize frame_delay
    frame_delay_ = use_beats ? 0 : delay;
}

void VideoDelayEffect::on_parameter_changed(const std::string& name) {
    if (name == "timing" || name == "delay") {
        bool use_beats = (parameters().get_int("timing") == 0);
        int delay = parameters().get_int("delay");

        // Enforce limits based on mode
        if (use_beats && delay > 16) {
            parameters().set_int("delay", 16);
            delay = 16;
        } else if (!use_beats && delay > 200) {
            parameters().set_int("delay", 200);
        }

        // Update frame_delay for frames mode
        if (!use_beats) {
            frame_delay_ = delay;
        } else {
            // Beat mode - will update on next beat
            frame_delay_ = 0;
            frames_since_beat_ = 0;
        }
    }
}

const PluginInfo& VideoDelayEffect::get_plugin_info() const {
    return effect_info;
}

const PluginInfo VideoDelayEffect::effect_info = {
    .name = "Video Delay",
    .category = "Trans",
    .description = "Frame delay/echo effect",
    .author = "Tom Holden",
    .version = 1,
    .legacy_index = 47,
    .factory = []() -> std::unique_ptr<EffectBase> {
        return std::make_unique<VideoDelayEffect>();
    },
    .ui_layout = {
        {
            // Enabled checkbox
            {.id = "enabled", .text = "Enabled", .type = ControlType::CHECKBOX,
             .x = 12, .y = 14, .w = 84, .h = 16, .default_val = 1},

            // Delay value input
            {.id = "delay", .text = "", .type = ControlType::TEXT_INPUT,
             .x = 12, .y = 36, .w = 86, .h = 24, .range = {0, 200}, .default_val = 10},

            // Timing mode (Beats vs Frames)
            {.id = "timing", .text = "Timing", .type = ControlType::RADIO_GROUP,
             .x = 108, .y = 36, .w = 100, .h = 24,
             .default_val = 1,  // Default to Frames
             .radio_options = {
                 {"Beats", 0, 0, 50, 24},
                 {"Frames", 52, 0, 56, 24}
             }},

            // Copyright notice
            {.id = "copyright", .text = "(c) Tom Holden, 2002", .type = ControlType::LABEL,
             .x = 12, .y = 72, .w = 240, .h = 16},
        }
    }
};

} // namespace avs
