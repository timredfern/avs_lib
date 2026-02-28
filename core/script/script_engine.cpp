// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "script_engine.h"
#include "lexer.h"
#include "parser.h"
#include "event_bus.h"
#include "core/effect_base.h"  // For AUDIO_SPECTRUM, AUDIO_WAVEFORM constants
#include <memory>
#include <map>
#include <deque>
#include <unordered_map>
#include <stdexcept>
#include <cstring>

namespace avs {

// CompiledScript implementation
CompiledScript::CompiledScript() = default;
CompiledScript::~CompiledScript() = default;
CompiledScript::CompiledScript(CompiledScript&& other) noexcept = default;
CompiledScript& CompiledScript::operator=(CompiledScript&& other) noexcept = default;

double& CompiledScript::var_ref(const std::string& name) {
    auto it = var_ptrs_.find(name);
    if (it == var_ptrs_.end()) {
        throw std::runtime_error("Variable not found in compiled script: " + name);
    }
    return *it->second;
}

class ScriptEngine::Impl {
public:
    // Stable variable storage - deque never invalidates pointers on growth
    std::deque<double> var_storage;
    std::unordered_map<std::string, size_t> var_indices;

    std::map<std::string, double> eval_vars;  // Reusable map for legacy evaluation
    std::string last_error;

    // User-defined arrays (vector-based for fast sequential access)
    std::unordered_map<std::string, std::vector<double>> user_arrays;

    // AVS-specific state
    struct PixelContext {
        int x, y, width, height;
    } pixel_context = {0, 0, 1, 1};

    struct ColorContext {
        double r, g, b;
    } color_context = {0.0, 0.0, 0.0};

    struct AudioContext {
        AudioData visdata;
        bool is_beat;
        bool has_data;

        AudioContext() : is_beat(false), has_data(false) {}
    } audio_context;

    // Get or create stable pointer to variable storage
    double* get_var_ptr(const std::string& name) {
        auto it = var_indices.find(name);
        if (it != var_indices.end()) {
            return &var_storage[it->second];
        }
        size_t idx = var_storage.size();
        var_storage.push_back(0.0);
        var_indices[name] = idx;
        return &var_storage[idx];
    }

    // Get pointer if exists, nullptr otherwise
    double* try_get_var_ptr(const std::string& name) {
        auto it = var_indices.find(name);
        if (it != var_indices.end()) {
            return &var_storage[it->second];
        }
        return nullptr;
    }
};

ScriptEngine::ScriptEngine() : pImpl(std::make_unique<Impl>()) {
    // Initialize built-in constants
    var_ref("$PI") = M_PI;
    var_ref("$E") = M_E;
    var_ref("$PHI") = 1.6180339887498948482;  // Golden ratio
}

ScriptEngine::~ScriptEngine() = default;

double& ScriptEngine::var_ref(const std::string& name) {
    return *pImpl->get_var_ptr(name);
}

CompiledScript ScriptEngine::compile(const std::string& expression) {
    CompiledScript compiled;

    if (expression.empty()) {
        compiled.empty_ = true;
        return compiled;
    }

    try {
        pImpl->last_error.clear();
        Lexer lexer(expression);
        Parser parser(lexer);
        compiled.ast_ = parser.parse();

        // Resolve variable names (builds list of variables used)
        VariableTable table;
        compiled.ast_->resolve_slots(table);

        // Bind variable pointers from engine storage
        compiled.ast_->bind_variables(*this);

        // Cache var_ptrs in CompiledScript for external access via var_ref()
        for (const auto& name : table.slot_names()) {
            compiled.var_ptrs_[name] = &var_ref(name);
        }

        // Set engine pointer for user array access
        compiled.ast_->set_engine(this);

        compiled.empty_ = false;
    } catch (const std::exception& e) {
        pImpl->last_error = e.what();
        compiled.empty_ = true;

        // Debug output for script compilation failures
        std::string script_preview = expression;
        if (script_preview.length() > 60) {
            script_preview = script_preview.substr(0, 57) + "...";
        }
        for (char& c : script_preview) {
            if (c == '\n' || c == '\r') c = ' ';
        }
        fprintf(stderr, "[ScriptEngine] Compile error: %s\n  Script: \"%s\"\n",
                e.what(), script_preview.c_str());
    }

    return compiled;
}

double ScriptEngine::execute(const CompiledScript& script) {
    if (script.is_empty() || !script.is_valid()) {
        return 0.0;
    }

    try {
        pImpl->last_error.clear();

        // Sync dynamic MIDI variables (global state that can change)
        var_ref("midi_note_count") = static_cast<double>(EventBus::instance().get_midi_data().note_count);
        var_ref("midi_any_note") = EventBus::instance().get_midi_data().any_note_on ? 1.0 : 0.0;
        var_ref("midi_pitch_bend") = EventBus::instance().get_midi_data().pitch_bend;

        // AST nodes use var_ptr_ directly, no slot array needed
        return script.ast_->evaluate_slots(nullptr);
    } catch (const std::exception& e) {
        pImpl->last_error = e.what();
        return 0.0;
    }
}

double ScriptEngine::evaluate(const std::string& expression) {
    // Empty expression is a no-op
    if (expression.empty()) {
        return 0.0;
    }

    try {
        pImpl->last_error.clear();

        // Build variable map from stable storage
        pImpl->eval_vars.clear();
        for (const auto& [name, idx] : pImpl->var_indices) {
            pImpl->eval_vars[name] = pImpl->var_storage[idx];
        }

        // Add AVS-specific read-only variables
        // (w, h, beat are read-only; x, y, r, g, b come from var_storage set by context)
        pImpl->eval_vars["w"] = static_cast<double>(pImpl->pixel_context.width);
        pImpl->eval_vars["h"] = static_cast<double>(pImpl->pixel_context.height);
        pImpl->eval_vars["beat"] = pImpl->audio_context.is_beat ? 1.0 : 0.0;

        // Add MIDI variables
        pImpl->eval_vars["midi_note_count"] = static_cast<double>(EventBus::instance().get_midi_data().note_count);
        pImpl->eval_vars["midi_any_note"] = EventBus::instance().get_midi_data().any_note_on ? 1.0 : 0.0;
        pImpl->eval_vars["midi_pitch_bend"] = EventBus::instance().get_midi_data().pitch_bend;

        // Parse and evaluate
        Lexer lexer(expression);
        Parser parser(lexer);
        auto ast = parser.parse();

        // Set engine pointer for user array access
        ast->set_engine(this);

        double result = ast->evaluate(pImpl->eval_vars);

        // Sync modified variables back to stable storage
        for (const auto& [name, value] : pImpl->eval_vars) {
            // Skip read-only built-ins (but allow x, y, r, g, b as they can be modified by scripts)
            if (name == "w" || name == "h" || name == "beat" ||
                name == "midi_note_count" || name == "midi_any_note" || name == "midi_pitch_bend") {
                continue;
            }
            var_ref(name) = value;
        }

        return result;
    } catch (const std::exception& e) {
        pImpl->last_error = e.what();
        return 0.0;
    }
}

void ScriptEngine::set_variable(const std::string& name, double value) {
    var_ref(name) = value;
}

double ScriptEngine::get_variable(const std::string& name) {
    double* ptr = pImpl->try_get_var_ptr(name);
    if (ptr) {
        return *ptr;
    }

    // Check AVS-specific read-only variables (not in var_storage)
    if (name == "w") return static_cast<double>(pImpl->pixel_context.width);
    if (name == "h") return static_cast<double>(pImpl->pixel_context.height);
    if (name == "beat") return pImpl->audio_context.is_beat ? 1.0 : 0.0;

    // MIDI scalar variables
    if (name == "midi_note_count") return static_cast<double>(EventBus::instance().get_midi_data().note_count);
    if (name == "midi_any_note") return EventBus::instance().get_midi_data().any_note_on ? 1.0 : 0.0;
    if (name == "midi_pitch_bend") return EventBus::instance().get_midi_data().pitch_bend;

    // Audio variables
    if (pImpl->audio_context.has_data) {
        const AudioData& vis = pImpl->audio_context.visdata;

        // Waveform v1-v8 (left channel)
        if (name.length() == 2 && name[0] == 'v' && name[1] >= '1' && name[1] <= '8') {
            int idx = name[1] - '1';
            int sample_idx = idx * 72;
            if (sample_idx < MIN_AUDIO_SAMPLES) {
                return static_cast<double>(vis[AUDIO_WAVEFORM][AUDIO_LEFT][sample_idx]) / 127.0;
            }
        }
        // Waveform vr1-vr8 (right channel)
        if (name.length() == 3 && name[0] == 'v' && name[1] == 'r' && name[2] >= '1' && name[2] <= '8') {
            int idx = name[2] - '1';
            int sample_idx = idx * 72;
            if (sample_idx < MIN_AUDIO_SAMPLES) {
                return static_cast<double>(vis[AUDIO_WAVEFORM][AUDIO_RIGHT][sample_idx]) / 127.0;
            }
        }
        // Spectrum s1-s8
        if (name.length() == 2 && name[0] == 's' && name[1] >= '1' && name[1] <= '8') {
            int idx = name[1] - '1';
            int sample_idx = idx * 72;
            if (sample_idx < MIN_AUDIO_SAMPLES) {
                return static_cast<double>(vis[AUDIO_SPECTRUM][AUDIO_LEFT][sample_idx]) / 127.0;
            }
        }
    }

    return 0.0;  // Default to 0 if variable not found
}

void ScriptEngine::set_pixel_context(int pixel_x, int pixel_y, int width, int height) {
    pImpl->pixel_context.x = pixel_x;
    pImpl->pixel_context.y = pixel_y;
    pImpl->pixel_context.width = width;
    pImpl->pixel_context.height = height;

    // Update stable storage for compiled scripts
    var_ref("w") = static_cast<double>(width);
    var_ref("h") = static_cast<double>(height);

    // Normalized coordinates (x/w-1, y/h-1)
    double norm_x = (width > 1) ? static_cast<double>(pixel_x) / (width - 1) : 0.0;
    double norm_y = (height > 1) ? static_cast<double>(pixel_y) / (height - 1) : 0.0;
    var_ref("x") = norm_x;
    var_ref("y") = norm_y;
}

void ScriptEngine::set_color_context(double red, double green, double blue) {
    pImpl->color_context.r = red;
    pImpl->color_context.g = green;
    pImpl->color_context.b = blue;

    // Update stable storage for compiled scripts
    var_ref("r") = red;
    var_ref("g") = green;
    var_ref("b") = blue;
}

void ScriptEngine::set_audio_context(AudioData visdata, bool is_beat) {
    std::memcpy(pImpl->audio_context.visdata, visdata, sizeof(AudioData));
    pImpl->audio_context.is_beat = is_beat;
    pImpl->audio_context.has_data = true;

    // Update stable storage for compiled scripts
    var_ref("beat") = is_beat ? 1.0 : 0.0;

    // Pre-compute audio variables v1-v8 (left channel waveform)
    for (int i = 0; i < 8; i++) {
        char name[3] = {'v', static_cast<char>('1' + i), '\0'};
        int sample_idx = i * 72;
        double value = static_cast<double>(visdata[AUDIO_WAVEFORM][AUDIO_LEFT][sample_idx]) / 127.0;
        var_ref(name) = value;
    }

    // Pre-compute vr1-vr8 (right channel waveform)
    for (int i = 0; i < 8; i++) {
        char name[4] = {'v', 'r', static_cast<char>('1' + i), '\0'};
        int sample_idx = i * 72;
        double value = static_cast<double>(visdata[AUDIO_WAVEFORM][AUDIO_RIGHT][sample_idx]) / 127.0;
        var_ref(name) = value;
    }

    // Pre-compute s1-s8 (spectrum)
    for (int i = 0; i < 8; i++) {
        char name[3] = {'s', static_cast<char>('1' + i), '\0'};
        int sample_idx = i * 72;
        double value = static_cast<double>(visdata[AUDIO_SPECTRUM][AUDIO_LEFT][sample_idx]) / 127.0;
        var_ref(name) = value;
    }
}

bool ScriptEngine::has_error() const {
    return !pImpl->last_error.empty();
}

std::string ScriptEngine::get_error() const {
    return pImpl->last_error;
}

// Parameter binding implementation
void ScriptEngine::bind_int_param(const std::string& var, const std::string& param, int min_val, int max_val) {
    bindings_.push_back({var, param, static_cast<double>(min_val), static_cast<double>(max_val), true, 0.0});
}

void ScriptEngine::bind_float_param(const std::string& var, const std::string& param, double min_val, double max_val) {
    bindings_.push_back({var, param, min_val, max_val, false, 0.0});
}

void ScriptEngine::sync_from_params(ParameterGroup& params) {
    for (auto& binding : bindings_) {
        double value;
        if (binding.is_int) {
            value = static_cast<double>(params.get_int(binding.param_name));
        } else {
            value = params.get_float(binding.param_name);
        }
        set_variable(binding.var_name, value);
        binding.last_value = value;
    }
}

void ScriptEngine::sync_to_params(ParameterGroup& params) {
    for (auto& binding : bindings_) {
        double value = std::clamp(get_variable(binding.var_name), binding.min_val, binding.max_val);

        // Only update if value changed
        if (binding.is_int) {
            int int_value = static_cast<int>(value);
            if (int_value != static_cast<int>(binding.last_value)) {
                params.set_int(binding.param_name, int_value);
                binding.last_value = static_cast<double>(int_value);
            }
        } else {
            if (value != binding.last_value) {
                params.set_float(binding.param_name, static_cast<float>(value));
                binding.last_value = value;
            }
        }
    }
}

void ScriptEngine::clear_bindings() {
    bindings_.clear();
}

// User-defined array operations
void ScriptEngine::array_write(const std::string& name, int idx, double value) {
    if (idx >= 0) {
        auto& arr = pImpl->user_arrays[name];
        if (idx >= static_cast<int>(arr.size())) {
            arr.resize(idx + 1, 0.0);  // Auto-grow, fill with 0
        }
        arr[idx] = value;
    }
}

double ScriptEngine::array_read(const std::string& name, int idx) const {
    auto it = pImpl->user_arrays.find(name);
    if (it == pImpl->user_arrays.end()) return 0.0;
    const auto& arr = it->second;
    return (idx >= 0 && idx < static_cast<int>(arr.size())) ? arr[idx] : 0.0;
}

void ScriptEngine::clear_user_arrays() {
    pImpl->user_arrays.clear();
}

} // namespace avs
