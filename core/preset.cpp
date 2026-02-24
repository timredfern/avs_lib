// avs_lib - Portable Advanced Visualization Studio library
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "preset.h"
#include "binary_reader.h"
#include "color.h"
#include "json.h"
#include "plugin_manager.h"
#include "effect_container.h"
#include "effects/unsupported.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>

namespace avs {

// ============================================================================
// Binary format constants and helpers
// ============================================================================

static const char AVS_HEADER[] = "Nullsoft AVS Preset 0.";
static const size_t AVS_HEADER_LEN = 25;  // 22 (prefix) + 1 (version) + 1 (0x1a) + 1 (root mode)
static const uint32_t EFFECT_LIST_INDEX = 0xFFFFFFFE;
static const uint32_t DLLRENDERBASE = 16384;

// APE plugins that became built-in effects
// Maps APE string IDs to legacy effect indices (from rlib.cpp NamedApeToBuiltinTrans)
static const struct {
    const char* id;
    int legacy_index;
} APE_TO_BUILTIN[] = {
    {"Winamp Brightness APE v1", 22},
    {"Winamp Interleave APE v1", 23},
    {"Winamp Grain APE v1", 24},
    {"Winamp ClearScreen APE v1", 25},
    {"Nullsoft MIRROR v1", 26},
    {"Winamp Starfield v1", 27},
    {"Winamp Text v1", 28},
    {"Winamp Bump v1", 29},
    {"Winamp Mosaic v1", 30},
    {"Winamp AVIAPE v1", 32},
    {"Nullsoft Picture Rendering v1", 34},
    {"Winamp Interf APE v1", 41},
};

// Returns legacy index if APE name maps to a builtin, -1 otherwise
static int ape_to_builtin_index(const std::string& ape_id) {
    for (const auto& entry : APE_TO_BUILTIN) {
        if (ape_id == entry.id) {
            return entry.legacy_index;
        }
    }
    return -1;
}

std::string Preset::last_error_;

// ============================================================================
// Helper functions for parameter serialization
// ============================================================================

static JsonValue param_to_json(const Parameter& param) {
    switch (param.type()) {
        case ParameterType::FLOAT:
            return param.as_float();
        case ParameterType::INT:
        case ParameterType::ENUM:
            return param.as_int();
        case ParameterType::BOOL:
            return param.as_bool();
        case ParameterType::COLOR: {
            // Internal format is ARGB (0xAARRGGBB), save directly as hex
            uint32_t color = param.as_color();
            char hex[10];
            snprintf(hex, sizeof(hex), "#%08X", color);
            return std::string(hex);
        }
        case ParameterType::STRING:
            return param.as_string();
        default:
            return nullptr;
    }
}

static void json_to_param(const JsonValue& json, Parameter& param) {
    switch (param.type()) {
        case ParameterType::FLOAT:
            if (json.is_number()) param.set_value(json.as_number());
            break;
        case ParameterType::INT:
        case ParameterType::ENUM:
            if (json.is_number()) param.set_value(json.as_int());
            break;
        case ParameterType::BOOL:
            if (json.is_bool()) param.set_value(json.as_bool());
            break;
        case ParameterType::COLOR:
            if (json.is_string()) {
                // Parse hex string like "#FFFF8000" - internal format is ARGB
                std::string s = json.as_string();
                if (!s.empty() && s[0] == '#') {
                    uint32_t color = static_cast<uint32_t>(std::stoul(s.substr(1), nullptr, 16));
                    param.set_value(color);
                }
            }
            break;
        case ParameterType::STRING:
            if (json.is_string()) param.set_value(json.as_string());
            break;
    }
}

// ============================================================================
// Effect serialization
// ============================================================================

static JsonValue effect_to_json(const EffectBase* effect) {
    JsonObject obj;

    // Effect type name
    obj["type"] = effect->get_plugin_info().name;

    // Enabled state
    obj["enabled"] = effect->is_enabled();

    // Parameters
    JsonObject params;

    // Check for num_colors to limit color array serialization
    int num_colors = effect->parameters().has_parameter("num_colors")
                     ? effect->parameters().get_int("num_colors") : 0;

    for (const auto& [name, param] : effect->parameters().all_parameters()) {
        // Skip 'enabled' - we handle it separately
        if (name == "enabled") continue;

        // Skip color_N parameters beyond num_colors
        if (num_colors > 0 && name.substr(0, 6) == "color_") {
            int color_idx = std::stoi(name.substr(6));
            if (color_idx >= num_colors) continue;
        }

        params[name] = param_to_json(*param);
    }
    if (!params.empty()) {
        obj["params"] = std::move(params);
    }

    // Children (if container)
    const auto* container = dynamic_cast<const EffectContainer*>(effect);
    if (container && container->child_count() > 0) {
        JsonArray children;
        for (size_t i = 0; i < container->child_count(); i++) {
            children.push_back(effect_to_json(container->get_child(i)));
        }
        obj["effects"] = std::move(children);
    }

    return obj;
}

static std::unique_ptr<EffectBase> json_to_effect(const JsonValue& json) {
    if (!json.is_object()) return nullptr;

    // Get effect type
    if (!json.has("type") || !json["type"].is_string()) return nullptr;
    std::string type_name = json["type"].as_string();

    // Create effect instance
    auto effect = PluginManager::instance().create_effect(type_name);
    if (!effect) return nullptr;

    // Set enabled state
    if (json.has("enabled") && json["enabled"].is_bool()) {
        effect->set_enabled(json["enabled"].as_bool());
    }

    // Set parameters and notify effect of changes
    if (json.has("params") && json["params"].is_object()) {
        const auto& params_json = json["params"].as_object();
        for (const auto& [name, value] : params_json) {
            auto param = effect->parameters().get_parameter(name);
            if (param) {
                json_to_param(value, *param);
                effect->on_parameter_changed(name);
            }
        }
    }

    // Load children (if container)
    auto* container = dynamic_cast<EffectContainer*>(effect.get());
    if (container && json.has("effects") && json["effects"].is_array()) {
        for (const auto& child_json : json["effects"].as_array()) {
            auto child = json_to_effect(child_json);
            if (child) {
                container->add_child(std::move(child));
            }
        }
    }

    return effect;
}

// ============================================================================
// Public API
// ============================================================================

std::string Preset::to_json(const EffectContainer& root) {
    JsonObject preset;
    preset["version"] = "1.0";
    preset["format"] = "avs-json";

    // Save root settings
    preset["clear_each_frame"] = root.parameters().get_bool("clear_each_frame");

    // Serialize all children of root
    JsonArray effects;
    for (size_t i = 0; i < root.child_count(); i++) {
        effects.push_back(effect_to_json(root.get_child(i)));
    }
    preset["effects"] = std::move(effects);

    return json_write(preset, true);
}

bool Preset::from_json(const std::string& json, EffectContainer& root) {
    try {
        JsonValue parsed = json_parse(json);

        if (!parsed.is_object()) {
            last_error_ = "Invalid JSON: expected object at root";
            return false;
        }

        // Clear existing effects
        while (root.child_count() > 0) {
            root.remove_child(0);
        }

        // Load root settings
        if (parsed.has("clear_each_frame") && parsed["clear_each_frame"].is_bool()) {
            root.parameters().set_bool("clear_each_frame", parsed["clear_each_frame"].as_bool());
        }

        // Load effects array
        if (parsed.has("effects") && parsed["effects"].is_array()) {
            for (const auto& effect_json : parsed["effects"].as_array()) {
                auto effect = json_to_effect(effect_json);
                if (effect) {
                    root.add_child(std::move(effect));
                }
            }
        }

        last_error_.clear();
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool Preset::save_effect(const std::string& path, const EffectBase* effect) {
    if (!effect) {
        last_error_ = "No effect to save";
        return false;
    }

    try {
        // Use same format as full presets - just with one effect in the array
        JsonObject wrapper;
        JsonArray effects;
        effects.push_back(effect_to_json(effect));
        wrapper["effects"] = effects;

        std::string json = json_write(wrapper, true);

        std::ofstream file(path);
        if (!file.is_open()) {
            last_error_ = "Failed to open file for writing: " + path;
            return false;
        }

        file << json;
        file.close();

        if (file.fail()) {
            last_error_ = "Failed to write to file: " + path;
            return false;
        }

        last_error_.clear();
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("Save error: ") + e.what();
        return false;
    }
}

std::unique_ptr<EffectBase> Preset::load_effect(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            last_error_ = "Failed to open file: " + path;
            return nullptr;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();

        JsonValue parsed = json_parse(json_str);

        if (!parsed.is_object()) {
            last_error_ = "Invalid JSON: expected object at root";
            return nullptr;
        }

        // Load first effect from effects array
        if (parsed.has("effects") && parsed["effects"].is_array()) {
            const auto& effects = parsed["effects"].as_array();
            if (effects.empty()) {
                last_error_ = "No effects in file";
                return nullptr;
            }
            last_error_.clear();
            return json_to_effect(effects[0]);
        } else {
            last_error_ = "Invalid effect file: missing effects array";
            return nullptr;
        }

    } catch (const std::exception& e) {
        last_error_ = std::string("Load error: ") + e.what();
        return nullptr;
    }
}

bool Preset::save_json(const std::string& path, const EffectContainer& root) {
    try {
        std::string json = to_json(root);

        std::ofstream file(path);
        if (!file.is_open()) {
            last_error_ = "Failed to open file for writing: " + path;
            return false;
        }

        file << json;
        file.close();

        if (file.fail()) {
            last_error_ = "Failed to write to file: " + path;
            return false;
        }

        last_error_.clear();
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("Save error: ") + e.what();
        return false;
    }
}

bool Preset::load_json(const std::string& path, EffectContainer& root) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            last_error_ = "Failed to open file: " + path;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();

        return from_json(json, root);

    } catch (const std::exception& e) {
        last_error_ = std::string("Load error: ") + e.what();
        return false;
    }
}

// ============================================================================
// Legacy binary format loading
// ============================================================================

static const char AVS_EXT_SIGSTR[] = "AVS 2.8+ Effect List Config";

// Forward declaration for recursive Effect List loading
static std::unique_ptr<EffectBase> load_legacy_effect(BinaryReader& reader);

// Load children of an Effect List from a sub-reader containing the list's config data
static void load_effect_list_from_config(BinaryReader& config_reader, EffectContainer* container) {
    if (config_reader.remaining() < 1) return;

    // Read mode byte
    uint8_t mode = config_reader.read_u8();

    // Extended data if high bit set
    // The amount of extended data varies by AVS version:
    // - AVS 2.8+: 4 (mode_ext) + 32 (8 values) = 36 bytes, followed by ext config marker
    // - Older: 4 (mode_ext) + 24 (6 values) = 28 bytes, no marker
    // We scan for the "AVS 2.8+ Effect List Config" marker to find the correct position
    if (mode & 0x80) {
        const uint8_t* data = config_reader.ptr();
        size_t len = config_reader.remaining();

        // Default to older format (6 values = 28 bytes) if no marker found
        size_t ext_data_end = std::min(static_cast<size_t>(28), len);

        // Scan for DLLRENDERBASE followed by "AVS 2.8+" signature
        // This marker is always present after extended data in AVS 2.8+ presets
        // The marker can be at position 28 (older) or 36 (2.8+)
        for (size_t pos = 4; pos + 12 <= len; pos += 4) {
            uint32_t value = data[pos] | (data[pos+1] << 8) |
                            (data[pos+2] << 16) | (data[pos+3] << 24);

            if (value == DLLRENDERBASE) {
                // Check for "AVS 2.8+" signature at pos+4
                if (pos + 4 + 8 <= len &&
                    std::memcmp(data + pos + 4, "AVS 2.8+", 8) == 0) {
                    ext_data_end = pos;
                    break;
                }
            }
        }

        // Parse extended data: mode_ext, inblendval, outblendval, bufferin, bufferout, ininvert, outinvert
        // And for 2.8+: beat_render, beat_render_frames
        if (ext_data_end >= 4) {
            uint32_t mode_ext = config_reader.read_u32();
            // Combine mode byte (without 0x80) with mode_ext to form full mode value
            // Original: mode = (mode & ~0x80) | mode_ext;
            uint32_t full_mode = (mode & 0x7F) | mode_ext;

            // Extract blend modes from full_mode:
            // bits 8-12 = blendin, bits 16-20 = blendout
            int blendin = (full_mode >> 8) & 31;
            int blendout = (full_mode >> 16) & 31;
            container->parameters().set_int("blendin", blendin);
            container->parameters().set_int("blendout", blendout);

            // bit 1 = NOT enabled (when clear, effect is enabled)
            bool enabled = !((full_mode >> 1) & 1);
            container->parameters().set_bool("enabled", enabled);

            // bit 2 = clear_each_frame
            bool clearfb = (full_mode >> 2) & 1;
            container->parameters().set_bool("clear_each_frame", clearfb);

            if (ext_data_end >= 8) {
                int inblendval = config_reader.read_u32();
                container->parameters().set_int("inblendval", inblendval);
            }
            if (ext_data_end >= 12) {
                int outblendval = config_reader.read_u32();
                container->parameters().set_int("outblendval", outblendval);
            }
            if (ext_data_end >= 16) {
                int bufferin = config_reader.read_u32();
                container->parameters().set_int("bufferin", bufferin);
            }
            if (ext_data_end >= 20) {
                int bufferout = config_reader.read_u32();
                container->parameters().set_int("bufferout", bufferout);
            }
            if (ext_data_end >= 24) {
                int ininvert = config_reader.read_u32();
                container->parameters().set_int("ininvert", ininvert);
            }
            if (ext_data_end >= 28) {
                int outinvert = config_reader.read_u32();
                container->parameters().set_int("outinvert", outinvert);
            }
            // 2.8+ extended values
            if (ext_data_end >= 32) {
                int beat_render = config_reader.read_u32();
                container->parameters().set_int("beat_render", beat_render);
            }
            if (ext_data_end >= 36) {
                int beat_render_frames = config_reader.read_u32();
                container->parameters().set_int("beat_render_frames", beat_render_frames);
            }
        } else {
            config_reader.skip(ext_data_end);
        }
    }

    // Check for "AVS 2.8+ Effect List Config" extended config
    // This appears as a plugin effect entry with the signature string as its ID
    while (config_reader.remaining() >= 8) {
        uint32_t effect_index = config_reader.read_u32();

        if (effect_index >= DLLRENDERBASE && effect_index != EFFECT_LIST_INDEX) {
            // Plugin effect - check if it's the ext config marker
            std::string plugin_id = config_reader.read_string_fixed(32);
            uint32_t entry_len = config_reader.read_u32();

            if (plugin_id == AVS_EXT_SIGSTR) {
                // Load the extended config data (use_code, init/frame scripts)
                if (entry_len >= 4) {
                    BinaryReader ext_reader = config_reader.sub_reader(entry_len);
                    int use_code = ext_reader.read_u32();
                    container->parameters().set_bool("use_code", use_code != 0);

                    std::string init_script = ext_reader.read_length_prefixed_string();
                    container->parameters().set_string("init_script", init_script);

                    std::string frame_script = ext_reader.read_length_prefixed_string();
                    container->parameters().set_string("frame_script", frame_script);
                }
                config_reader.skip(entry_len);
                continue;
            }

            // Check if APE maps to a builtin
            int builtin_index = ape_to_builtin_index(plugin_id);
            std::unique_ptr<EffectBase> effect;

            if (builtin_index >= 0) {
                // APE that became a builtin effect
                effect = PluginManager::instance().create_by_legacy_index(builtin_index);
                if (!effect) {
                    const char* name = get_legacy_effect_name(builtin_index);
                    if (name) {
                        effect = std::make_unique<UnsupportedEffect>(name, builtin_index);
                    } else {
                        effect = std::make_unique<UnsupportedEffect>(plugin_id, builtin_index);
                    }
                }
                // Pass config data to effect
                if (effect && entry_len > 0 && config_reader.remaining() >= entry_len) {
                    std::vector<uint8_t> config_data(config_reader.ptr(), config_reader.ptr() + entry_len);
                    effect->load_parameters(config_data);
                }
            } else {
                // Try looking up by effect name (for APE plugins like Channel Shift)
                effect = PluginManager::instance().create_effect(plugin_id);
                if (effect) {
                    // Pass config data to effect
                    if (entry_len > 0 && config_reader.remaining() >= entry_len) {
                        std::vector<uint8_t> config_data(config_reader.ptr(), config_reader.ptr() + entry_len);
                        effect->load_parameters(config_data);
                    }
                } else {
                    // True external plugin - unsupported
                    effect = std::make_unique<UnsupportedEffect>(
                        "Plugin: " + plugin_id, static_cast<int>(effect_index));
                }
            }
            config_reader.skip(entry_len);
            container->add_child(std::move(effect));
        } else if (effect_index == EFFECT_LIST_INDEX) {
            // Nested Effect List
            uint32_t entry_len = config_reader.read_u32();
            if (entry_len > 0 && config_reader.remaining() >= entry_len) {
                auto effect = PluginManager::instance().create_effect("Effect List");
                if (effect) {
                    // Create a sub-reader for this nested Effect List's config
                    BinaryReader nested_reader = config_reader.sub_reader(entry_len);
                    auto* nested_container = dynamic_cast<EffectContainer*>(effect.get());
                    if (nested_container) {
                        load_effect_list_from_config(nested_reader, nested_container);
                    }
                    container->add_child(std::move(effect));
                }
            }
            config_reader.skip(entry_len);
        } else {
            // Built-in effect
            uint32_t entry_len = config_reader.read_u32();

            auto effect = PluginManager::instance().create_by_legacy_index(static_cast<int>(effect_index));
            if (!effect) {
                const char* name = get_legacy_effect_name(static_cast<int>(effect_index));
                if (name) {
                    effect = std::make_unique<UnsupportedEffect>(name, static_cast<int>(effect_index));
                } else {
                    effect = std::make_unique<UnsupportedEffect>(
                        "Unknown Effect #" + std::to_string(effect_index),
                        static_cast<int>(effect_index));
                }
            }

            // Pass config data to effect for parsing
            if (effect && entry_len > 0 && config_reader.remaining() >= entry_len) {
                std::vector<uint8_t> config_data(config_reader.ptr(), config_reader.ptr() + entry_len);
                effect->load_parameters(config_data);
            }
            config_reader.skip(entry_len);
            container->add_child(std::move(effect));
        }
    }
}

static std::unique_ptr<EffectBase> load_legacy_effect(BinaryReader& reader) {
    if (reader.remaining() < 8) return nullptr;

    uint32_t effect_index = reader.read_u32();
    std::string plugin_id;

    // Check for plugin effect (has string ID)
    if (effect_index >= DLLRENDERBASE && effect_index != EFFECT_LIST_INDEX) {
        plugin_id = reader.read_string_fixed(32);
    }

    uint32_t config_length = reader.read_u32();

    // Create effect instance
    std::unique_ptr<EffectBase> effect;

    if (effect_index == EFFECT_LIST_INDEX) {
        // Effect List - children are embedded within config_length
        effect = PluginManager::instance().create_effect("Effect List");

        if (effect && config_length > 0 && reader.remaining() >= config_length) {
            // Create a sub-reader limited to this Effect List's config data
            BinaryReader config_reader = reader.sub_reader(config_length);

            auto* container = dynamic_cast<EffectContainer*>(effect.get());
            if (container) {
                load_effect_list_from_config(config_reader, container);
            }
        }
        reader.skip(config_length);
    } else if (effect_index < DLLRENDERBASE) {
        // Built-in effect by index
        effect = PluginManager::instance().create_by_legacy_index(static_cast<int>(effect_index));

        // If effect not implemented, create placeholder with the effect name
        if (!effect) {
            const char* name = get_legacy_effect_name(static_cast<int>(effect_index));
            if (name) {
                effect = std::make_unique<UnsupportedEffect>(name, static_cast<int>(effect_index));
            } else {
                // Unknown effect index
                effect = std::make_unique<UnsupportedEffect>(
                    "Unknown Effect #" + std::to_string(effect_index),
                    static_cast<int>(effect_index));
            }
        }

        // Pass config data to effect for parsing
        if (effect && config_length > 0 && reader.remaining() >= config_length) {
            std::vector<uint8_t> config_data(reader.ptr(), reader.ptr() + config_length);
            effect->load_parameters(config_data);
        }
        reader.skip(config_length);
    } else {
        // Plugin effect by string ID - check if it maps to a builtin
        int builtin_index = ape_to_builtin_index(plugin_id);
        if (builtin_index >= 0) {
            // APE that became a builtin effect
            effect = PluginManager::instance().create_by_legacy_index(builtin_index);
            if (!effect) {
                const char* name = get_legacy_effect_name(builtin_index);
                if (name) {
                    effect = std::make_unique<UnsupportedEffect>(name, builtin_index);
                } else {
                    effect = std::make_unique<UnsupportedEffect>(plugin_id, builtin_index);
                }
            }
            // Pass config data to effect for parsing
            if (effect && config_length > 0 && reader.remaining() >= config_length) {
                std::vector<uint8_t> config_data(reader.ptr(), reader.ptr() + config_length);
                effect->load_parameters(config_data);
            }
        } else {
            // Try looking up by effect name (for APE plugins like Channel Shift)
            effect = PluginManager::instance().create_effect(plugin_id);
            if (effect) {
                // Pass config data to effect for parsing
                if (config_length > 0 && reader.remaining() >= config_length) {
                    std::vector<uint8_t> config_data(reader.ptr(), reader.ptr() + config_length);
                    effect->load_parameters(config_data);
                }
            } else {
                // True external plugin - unsupported
                effect = std::make_unique<UnsupportedEffect>(
                    "Plugin: " + plugin_id,
                    static_cast<int>(effect_index));
            }
        }
        reader.skip(config_length);
    }

    return effect;
}

bool Preset::load_legacy(const std::string& path, EffectContainer& root) {
    try {
        // Read entire file
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            last_error_ = "Failed to open file: " + path;
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            last_error_ = "Failed to read file: " + path;
            return false;
        }

        return from_legacy(data, root);

    } catch (const std::exception& e) {
        last_error_ = std::string("Load error: ") + e.what();
        return false;
    }
}

bool Preset::from_legacy(const std::vector<uint8_t>& data, EffectContainer& root) {
    if (data.size() < AVS_HEADER_LEN) {
        last_error_ = "File too small for AVS header";
        return false;
    }

    BinaryReader reader(data);

    // Validate header
    std::string header = reader.read_string_fixed(22);
    if (header != AVS_HEADER) {
        last_error_ = "Invalid AVS header";
        return false;
    }

    // Version byte (position 22)
    uint8_t version = reader.read_u8();
    if (version != '1' && version != '2') {
        last_error_ = "Unsupported AVS version";
        return false;
    }

    // Skip EOF marker (0x1a at position 23)
    reader.skip(1);

    // Root mode byte (position 24)
    // Bit 0 = clear every frame
    uint8_t mode = reader.read_u8();
    root.parameters().set_bool("clear_each_frame", (mode & 1) != 0);

    // Clear existing effects
    while (root.child_count() > 0) {
        root.remove_child(0);
    }

    // Load effects
    int loaded = 0;
    int skipped = 0;

    while (!reader.eof() && reader.remaining() >= 8) {
        auto effect = load_legacy_effect(reader);
        if (effect) {
            root.add_child(std::move(effect));
            loaded++;
        } else {
            skipped++;
        }
    }

    if (loaded == 0 && skipped > 0) {
        last_error_ = "No supported effects found in preset";
        return false;
    }

    last_error_.clear();
    return true;
}

PresetFormat Preset::detect_format(const std::string& path) {
    // Check extension
    size_t dot = path.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = path.substr(dot);
        if (ext == ".json") return PresetFormat::JSON;
        if (ext == ".avs") return PresetFormat::LEGACY;
    }

    // Try to detect from content
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
        char header[25];
        file.read(header, 25);
        if (file.gcount() >= 25) {
            // Check for AVS legacy header
            if (std::string(header, 23) == "Nullsoft AVS Preset 0.") {
                return PresetFormat::LEGACY;
            }
        }
    }

    // Default to JSON
    return PresetFormat::JSON;
}

bool Preset::save(const std::string& path, const EffectContainer& root, PresetFormat format) {
    if (format == PresetFormat::AUTO) {
        format = detect_format(path);
    }

    switch (format) {
        case PresetFormat::JSON:
            return save_json(path, root);
        case PresetFormat::LEGACY:
            last_error_ = "Legacy AVS format saving not yet implemented";
            return false;
        default:
            return save_json(path, root);
    }
}

bool Preset::load(const std::string& path, EffectContainer& root, PresetFormat format) {
    if (format == PresetFormat::AUTO) {
        format = detect_format(path);
    }

    switch (format) {
        case PresetFormat::JSON:
            return load_json(path, root);
        case PresetFormat::LEGACY:
            return load_legacy(path, root);
        default:
            return load_json(path, root);
    }
}

} // namespace avs
