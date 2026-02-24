// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License - see LICENSE file in repository root

#pragma once

#include "../core/effect_container.h"
#include "../core/script/script_engine.h"
#include <vector>
#include <cstdint>

namespace avs {
struct PluginInfo;
}

namespace avs {

// Effect List - the classic AVS container effect for nested groups
// Provides a compositing container that can blend its contents with the parent framebuffer
class EffectList : public EffectContainer {
public:
    EffectList();
    virtual ~EffectList() = default;

    // Core render function - renders all children in sequence
    int render(AudioData visdata, int isBeat,
               uint32_t* framebuffer, uint32_t* fbout,
               int w, int h) override;

    const PluginInfo& get_plugin_info() const override { return effect_info; }

    // Respond to script changes
    void on_parameter_changed(const std::string& param_name) override;

    static const PluginInfo effect_info;

private:
    // Internal framebuffer for compositing (thisfb in original)
    std::vector<uint32_t> internal_buffer_;
    int buffer_w_ = 0;
    int buffer_h_ = 0;

    // Beat-gated rendering counter
    int fake_enabled_ = 0;

    // Script engine for evaluation override
    ScriptEngine engine_;
    CompiledScript init_compiled_;
    CompiledScript frame_compiled_;
    bool inited_ = false;

    // Blend a source buffer onto a destination buffer using the specified mode
    void blend_buffers(uint32_t* dest, const uint32_t* src, int count,
                       int blend_mode, int blend_val, int buffer_index, bool invert);

    // Compile scripts
    void compile_scripts();
};

} // namespace avs
