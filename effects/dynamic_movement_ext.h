// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include "../core/effect_base.h"
#include "../core/coordinate_grid.h"
#include "../core/script/script_engine.h"
#include <string>

namespace avs {

/**
 * Dynamic Movement Extended - Preset-based non-linear transformations
 *
 * This effect provides 25 curated non-linear transformation presets
 * that showcase the grid-based coordinate system. Unlike uniform
 * rotation or zoom (which are linear), these transforms produce
 * visible grid artifacts that vary with grid resolution.
 *
 * All presets are non-linear, meaning they cannot be exactly
 * represented by bilinear interpolation - higher grid resolution
 * produces smoother results.
 */
class DynamicMovementExtEffect : public EffectBase {
public:
    DynamicMovementExtEffect();
    ~DynamicMovementExtEffect() = default;

    // EffectBase interface
    int render(AudioData visdata, int isBeat,
               uint32_t* framebuffer, uint32_t* fbout,
               int w, int h) override;

    const PluginInfo& get_plugin_info() const override { return effect_info; }

    void on_parameter_changed(const std::string& param_name) override;

    static const PluginInfo effect_info;

private:
    void compile_scripts();
    void apply_preset(int preset_index);

    CoordinateGrid grid_;
    ScriptEngine engine_;

    // Compiled scripts for frame/beat execution
    CompiledScript frame_compiled_;
    CompiledScript beat_compiled_;
};

} // namespace avs
