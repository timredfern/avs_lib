// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "comment.h"
#include "core/plugin_manager.h"
#include "core/binary_reader.h"

namespace avs {

CommentEffect::CommentEffect() {
    init_parameters_from_layout(effect_info.ui_layout);
}

int CommentEffect::render(AudioData visdata, int isBeat,
                          uint32_t* framebuffer, uint32_t* fbout,
                          int w, int h) {
    // Comment effect does nothing - it's just for documentation
    return 0;
}

void CommentEffect::load_parameters(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    BinaryReader reader(data);

    // Single length-prefixed string
    std::string comment = reader.read_length_prefixed_string();
    parameters().set_string("comment", comment);
}

// Static member definition - UI layout from res.rc IDD_CFG_COMMENT
const PluginInfo CommentEffect::effect_info {
    .name = "Comment",
    .category = "Misc",
    .description = "Add a comment or note to the preset (does not affect rendering)",
    .author = "",
    .version = 1,
    .legacy_index = 21,  // R_Comment from rlib.cpp
    .factory = []() -> std::unique_ptr<avs::EffectBase> {
        return std::make_unique<CommentEffect>();
    },
    .ui_layout = {
        {
            {
                .id = "comment",
                .text = "",
                .type = ControlType::EDITTEXT,
                .x = 0, .y = 0, .w = 233, .h = 214,
                .default_val = ""
            }
        }
    }
};

// Register effect at startup
static bool register_comment = []() {
    PluginManager::instance().register_effect(CommentEffect::effect_info);
    return true;
}();

} // namespace avs
