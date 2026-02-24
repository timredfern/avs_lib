// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "builtin_effects.h"
#include "plugin_manager.h"
#include "effects/clear.h"
#include "effects/oscilloscope.h"
#include "effects/superscope.h"
#include "effects/blur.h"
#include "effects/brightness.h"
#include "effects/movement.h"
#include "effects/dynamic_movement.h"
#include "effects/onbeat_clear.h"
#include "effects/dot_grid.h"
#include "effects/color_fade.h"
#include "effects/ddm.h"
#include "effects/fadeout.h"
#include "effects/bump.h"
#include "effects/effect_list.h"
#include "effects/rotoblitter.h"
#include "effects/custom_bpm.h"
#include "effects/set_render_mode.h"
#include "effects/mirror.h"
#include "effects/shift.h"
#include "effects/scatter.h"
#include "effects/dot_fountain.h"
#include "effects/water.h"
#include "effects/moving_particle.h"
#include "effects/interferences.h"
#include "effects/timescope.h"
#include "effects/picture.h"
#include "effects/oscstar.h"
#include "effects/ring.h"
#include "effects/rotstar.h"
#include "effects/invert.h"
#include "effects/fast_brightness.h"
#include "effects/color_reduction.h"
#include "effects/multiplier.h"
#include "effects/channel_shift.h"
#include "effects/unique_tone.h"
#include "effects/grain.h"
#include "effects/interleave.h"
#include "effects/bass_spin.h"
#include "effects/blitter_feedback.h"
#include "effects/comment.h"
#include "effects/dot_plane.h"
#include "effects/starfield.h"
#include "effects/mosaic.h"
#include "effects/buffer_save.h"
#include "effects/color_modifier.h"
#include "effects/color_clip.h"
#include "effects/water_bump.h"
#include "effects/multi_delay.h"
#include "effects/video_delay.h"
#include "effects/dynamic_movement_ext.h"
#include "effects/set_render_mode_ext.h"
#include "effects/starfield_ext.h"
#include "effects/superscope_ext.h"

namespace avs {

static bool effects_registered = false;

void register_builtin_effects() {
    if (effects_registered) return;
    effects_registered = true;

    auto& pm = PluginManager::instance();

    // Explicitly register all built-in effects
    // This is needed because static initialization in static libraries
    // may not trigger if the translation unit appears unused to the linker
    pm.register_effect(ClearEffect::effect_info);
    pm.register_effect(OscilloscopeEffect::effect_info);
    pm.register_effect(SuperScopeEffect::effect_info);
    pm.register_effect(BlurEffect::effect_info);
    pm.register_effect(BrightnessEffect::effect_info);
    pm.register_effect(MovementEffect::effect_info);
    pm.register_effect(DynamicMovementEffect::effect_info);
    pm.register_effect(OnBeatClearEffect::effect_info);
    pm.register_effect(DotGridEffect::effect_info);
    pm.register_effect(ColorFadeEffect::effect_info);
    pm.register_effect(DDMEffect::effect_info);
    pm.register_effect(FadeoutEffect::effect_info);
    pm.register_effect(BumpEffect::effect_info);
    pm.register_effect(EffectList::effect_info);
    pm.register_effect(RotoBlitterEffect::effect_info);
    pm.register_effect(CustomBpmEffect::effect_info);
    pm.register_effect(SetRenderModeEffect::effect_info);
    pm.register_effect(MirrorEffect::effect_info);
    pm.register_effect(ShiftEffect::effect_info);
    pm.register_effect(ScatterEffect::effect_info);
    pm.register_effect(DotFountainEffect::effect_info);
    pm.register_effect(WaterEffect::effect_info);
    pm.register_effect(MovingParticleEffect::effect_info);
    pm.register_effect(InterferencesEffect::effect_info);
    pm.register_effect(TimescopeEffect::effect_info);
    pm.register_effect(PictureEffect::effect_info);
    pm.register_effect(OscStarEffect::effect_info);
    pm.register_effect(RingEffect::effect_info);
    pm.register_effect(RotStarEffect::effect_info);
    pm.register_effect(InvertEffect::effect_info);
    pm.register_effect(FastBrightnessEffect::effect_info);
    pm.register_effect(ColorReductionEffect::effect_info);
    pm.register_effect(MultiplierEffect::effect_info);
    pm.register_effect(ChannelShiftEffect::effect_info);
    pm.register_effect(UniqueToneEffect::effect_info);
    pm.register_effect(GrainEffect::effect_info);
    pm.register_effect(InterleaveEffect::effect_info);
    pm.register_effect(BassSpinEffect::effect_info);
    pm.register_effect(BlitterFeedbackEffect::effect_info);
    pm.register_effect(CommentEffect::effect_info);
    pm.register_effect(DotPlaneEffect::effect_info);
    pm.register_effect(StarfieldEffect::effect_info);
    pm.register_effect(MosaicEffect::effect_info);
    pm.register_effect(BufferSaveEffect::effect_info);
    pm.register_effect(ColorModifierEffect::effect_info);
    pm.register_effect(ColorClipEffect::effect_info);
    pm.register_effect(WaterBumpEffect::effect_info);
    pm.register_effect(MultiDelayEffect::effect_info);
    pm.register_effect(VideoDelayEffect::effect_info);
    pm.register_effect(DynamicMovementExtEffect::effect_info);
    pm.register_effect(SetRenderModeExtEffect::effect_info);
    pm.register_effect(StarfieldExtEffect::effect_info);
    pm.register_effect(SuperScopeExtEffect::effect_info);
}

} // namespace avs