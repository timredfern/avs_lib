// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "../effect_base.h"

namespace avs {

/**
 * Parameter binding for syncing script variables with UI parameters
 */
struct ParamBinding {
    std::string var_name;    // Script variable name
    std::string param_name;  // Parameter name in ParameterGroup
    double min_val;          // Clamp minimum
    double max_val;          // Clamp maximum
    bool is_int;             // True for int params, false for float
    double last_value;       // Track last value for change detection
};

// Forward declaration
struct ASTNode;

/**
 * CompiledScript - A pre-parsed script ready for fast repeated execution
 *
 * Instead of parsing the script text every time, parse once and execute many times.
 * This is critical for per-pixel or per-grid-point scripts that run thousands of
 * times per frame.
 *
 * Variable references for hot loops:
 *
 * After compiling a script, get references to the variables it uses.
 * These reference the engine's storage directly - no copying or syncing.
 *
 * Example:
 *   auto script = engine.compile("x = x * 2; y = sin(x)");
 *   double& x = script.var_ref("x");
 *   double& y = script.var_ref("y");
 *
 *   for (...) {
 *       x = input_value;
 *       engine.execute(script);
 *       output(x, y);  // Modified by script
 *   }
 *
 * Note: References are stable for the engine's lifetime.
 * All scripts on the same engine share the variable namespace.
 */
class CompiledScript {
public:
    CompiledScript();
    ~CompiledScript();
    CompiledScript(CompiledScript&& other) noexcept;
    CompiledScript& operator=(CompiledScript&& other) noexcept;

    // Non-copyable (owns AST)
    CompiledScript(const CompiledScript&) = delete;
    CompiledScript& operator=(const CompiledScript&) = delete;

    bool is_valid() const { return ast_ != nullptr; }
    bool is_empty() const { return empty_; }

    /**
     * Get a reference to a script variable for direct access in hot loops.
     *
     * @param name Variable name used in the script
     * @return Reference to the variable's storage
     * @throws std::runtime_error if variable not found in this script
     */
    double& var_ref(const std::string& name);

private:
    friend class ScriptEngine;
    std::unique_ptr<ASTNode> ast_;
    std::unordered_map<std::string, double*> var_ptrs_;  // Cached pointers to engine storage
    bool empty_ = true;
};

/**
 * NS-EEL Script Engine
 *
 * This class provides a simple interface to the NS-EEL expression evaluation library.
 * It can compile and execute mathematical expressions with support for variables,
 * built-in functions, and AVS-specific integration.
 *
 * For performance-critical code (per-pixel scripts), use compile() + execute():
 *   auto script = engine.compile("d = d * 0.9");
 *   for (each point) {
 *       engine.execute(script);  // No re-parsing, fast slot-based vars!
 *   }
 */
class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // Basic expression evaluation (parses every time - use for one-off scripts)
    double evaluate(const std::string& expression);

    // Compiled script evaluation (parse once, execute many)
    CompiledScript compile(const std::string& expression);
    double execute(const CompiledScript& script);

    /**
     * Get a reference to a variable's storage.
     *
     * Creates the variable if it doesn't exist. The reference remains valid
     * for the lifetime of the engine (uses std::deque for stable addresses).
     *
     * @param name Variable name
     * @return Reference to the variable's storage
     */
    double& var_ref(const std::string& name);

    // Variable management
    void set_variable(const std::string& name, double value);
    double get_variable(const std::string& name);

    // AVS-specific context
    void set_pixel_context(int pixel_x, int pixel_y, int width, int height);
    void set_color_context(double red, double green, double blue);
    void set_audio_context(AudioData visdata, bool is_beat);

    // Error handling
    bool has_error() const;
    std::string get_error() const;

    // User-defined arrays
    void array_write(const std::string& name, int idx, double value);
    double array_read(const std::string& name, int idx) const;
    void clear_user_arrays();

    // Parameter binding - sync script variables with UI parameters
    void bind_int_param(const std::string& var, const std::string& param, int min_val, int max_val);
    void bind_float_param(const std::string& var, const std::string& param, double min_val, double max_val);
    void sync_from_params(ParameterGroup& params);  // params → variables (call before scripts)
    void sync_to_params(ParameterGroup& params);    // variables → params (call after scripts, only if changed)
    void clear_bindings();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    // Parameter bindings
    std::vector<ParamBinding> bindings_;
};

} // namespace avs
