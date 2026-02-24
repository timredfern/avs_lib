// avs_lib - Portable Advanced Visualization Studio library
// NOT part of original AVS - this is a new extended effect
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include "../core/effect_base.h"
#include "../core/script/script_engine.h"
#include <array>

namespace avs {
struct PluginInfo;
}

namespace avs {

class SuperScopeExtEffect : public EffectBase {
public:
    SuperScopeExtEffect();
    virtual ~SuperScopeExtEffect() = default;

    int render(AudioData visdata, int isBeat,
              uint32_t* framebuffer, uint32_t* fbout,
              int w, int h) override;

    const PluginInfo& get_plugin_info() const override { return effect_info; }

    // Binary config loading from legacy AVS presets
    void load_parameters(const std::vector<uint8_t>& data) override;

    // Respond to UI parameter changes
    void on_parameter_changed(const std::string& param_name) override;

    static const PluginInfo effect_info;

private:
    ScriptEngine engine_;
    bool inited_ = false;
    int color_pos_ = 0;

    // Cached compiled scripts for performance
    CompiledScript init_compiled_;
    CompiledScript frame_compiled_;
    CompiledScript beat_compiled_;
    CompiledScript point_compiled_;


    // Compile all scripts - called when scripts change
    void compile_scripts();

    // Helper to convert 0-1 color to 0-255
    static int make_color_component(double val) {
        if (val <= 0.0) return 0;
        if (val >= 1.0) return 255;
        return static_cast<int>(val * 255.0);
    }
};

} // namespace avs
