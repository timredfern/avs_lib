// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
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

class ColorModifierEffect : public EffectBase {
public:
    ColorModifierEffect();
    virtual ~ColorModifierEffect() = default;

    int render(AudioData visdata, int isBeat,
              uint32_t* framebuffer, uint32_t* fbout,
              int w, int h) override;

    const PluginInfo& get_plugin_info() const override { return effect_info; }

    void load_parameters(const std::vector<uint8_t>& data) override;
    void on_parameter_changed(const std::string& param_name) override;

    static const PluginInfo effect_info;

private:
    void regenerate_table();
    void compile_scripts();

    ScriptEngine engine_;
    bool inited_ = false;

    // Compiled scripts
    CompiledScript init_compiled_;
    CompiledScript frame_compiled_;
    CompiledScript beat_compiled_;
    CompiledScript level_compiled_;

    // Color lookup table: 768 bytes (256 for each of B, G, R)
    // m_tab[0-255] = blue transform, m_tab[256-511] = green, m_tab[512-767] = red
    std::array<uint8_t, 768> table_;
};

} // namespace avs
