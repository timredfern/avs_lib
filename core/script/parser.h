// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include "lexer.h"
#include "event_bus.h"
#include "script_engine.h"
#include <memory>
#include <map>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <chrono>
#include <ctime>

namespace avs {

// Forward declaration
class VariableTable;

// Abstract syntax tree nodes
// Supports both map-based evaluation (legacy) and pointer-based evaluation (optimized)
struct ASTNode {
    virtual ~ASTNode() = default;

    // Legacy map-based evaluation (for backward compatibility and testing)
    virtual double evaluate(std::map<std::string, double>& variables) const = 0;

    // Optimized pointer-based evaluation (for compiled scripts)
    // The vars parameter is unused but kept for API compatibility
    virtual double evaluate_slots(double* vars) const = 0;

    // Resolve variable names (builds list of variable names used)
    virtual void resolve_slots(VariableTable& table) = 0;

    // Bind variable pointers from engine storage
    virtual void bind_variables(ScriptEngine& engine) { (void)engine; }

    // Set engine pointer for user array access (propagates to children)
    virtual void set_engine(ScriptEngine* engine) { (void)engine; }
};

// Variable table for mapping names to slot indices
class VariableTable {
public:
    // Get or assign a slot for a variable name
    size_t get_or_create_slot(const std::string& name) {
        auto it = name_to_slot_.find(name);
        if (it != name_to_slot_.end()) {
            return it->second;
        }
        size_t slot = slot_names_.size();
        slot_names_.push_back(name);
        name_to_slot_[name] = slot;
        return slot;
    }

    // Get the number of slots
    size_t slot_count() const { return slot_names_.size(); }

    // Get slot names in order (for syncing with engine)
    const std::vector<std::string>& slot_names() const { return slot_names_; }

private:
    std::vector<std::string> slot_names_;
    std::unordered_map<std::string, size_t> name_to_slot_;
};

struct NumberNode : public ASTNode {
    double value;
    NumberNode(double v) : value(v) {}

    double evaluate(std::map<std::string, double>& variables) const override {
        return value;
    }

    double evaluate_slots(double* vars) const override {
        return value;
    }

    void resolve_slots(VariableTable& table) override {
        // Nothing to resolve for constants
    }
};

struct VariableNode : public ASTNode {
    std::string name;
    double* var_ptr_ = nullptr;  // Direct pointer to engine storage

    VariableNode(const std::string& n) : name(n) {}

    double evaluate(std::map<std::string, double>& variables) const override {
        auto it = variables.find(name);
        return (it != variables.end()) ? it->second : 0.0;
    }

    double evaluate_slots(double* vars) const override {
        (void)vars;  // Unused - we use var_ptr_ directly
        return var_ptr_ ? *var_ptr_ : 0.0;
    }

    void resolve_slots(VariableTable& table) override {
        table.get_or_create_slot(name);  // Register the name
    }

    void bind_variables(ScriptEngine& engine) override {
        var_ptr_ = &engine.var_ref(name);
    }
};

struct BinaryOpNode : public ASTNode {
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    TokenType op;

    BinaryOpNode(std::unique_ptr<ASTNode> l, TokenType o, std::unique_ptr<ASTNode> r)
        : left(std::move(l)), right(std::move(r)), op(o) {}

    void set_engine(ScriptEngine* engine) override {
        left->set_engine(engine);
        right->set_engine(engine);
    }

    void bind_variables(ScriptEngine& engine) override {
        left->bind_variables(engine);
        right->bind_variables(engine);
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        double l_val = left->evaluate(variables);
        double r_val = right->evaluate(variables);

        switch (op) {
            case TokenType::PLUS: return l_val + r_val;
            case TokenType::MINUS: return l_val - r_val;
            case TokenType::MULTIPLY: return l_val * r_val;
            case TokenType::DIVIDE: return (r_val != 0.0) ? l_val / r_val : 0.0;
            case TokenType::MODULO: {
                // AVS uses integer modulo (truncates to int first)
                int l = static_cast<int>(l_val);
                int r = static_cast<int>(r_val);
                return (r != 0) ? static_cast<double>(l % r) : 0.0;
            }
            case TokenType::BITWISE_AND: return static_cast<double>(static_cast<int>(l_val) & static_cast<int>(r_val));
            case TokenType::BITWISE_OR: return static_cast<double>(static_cast<int>(l_val) | static_cast<int>(r_val));
            // Comparison operators return 1.0 (true) or 0.0 (false)
            case TokenType::LESS: return (l_val < r_val) ? 1.0 : 0.0;
            case TokenType::GREATER: return (l_val > r_val) ? 1.0 : 0.0;
            case TokenType::LESS_EQUAL: return (l_val <= r_val) ? 1.0 : 0.0;
            case TokenType::GREATER_EQUAL: return (l_val >= r_val) ? 1.0 : 0.0;
            case TokenType::EQUAL_EQUAL: return (l_val == r_val) ? 1.0 : 0.0;
            case TokenType::NOT_EQUAL: return (l_val != r_val) ? 1.0 : 0.0;
            default: return 0.0;
        }
    }

    double evaluate_slots(double* vars) const override {
        double l_val = left->evaluate_slots(vars);
        double r_val = right->evaluate_slots(vars);

        switch (op) {
            case TokenType::PLUS: return l_val + r_val;
            case TokenType::MINUS: return l_val - r_val;
            case TokenType::MULTIPLY: return l_val * r_val;
            case TokenType::DIVIDE: return (r_val != 0.0) ? l_val / r_val : 0.0;
            case TokenType::MODULO: {
                // AVS uses integer modulo (truncates to int first)
                int l = static_cast<int>(l_val);
                int r = static_cast<int>(r_val);
                return (r != 0) ? static_cast<double>(l % r) : 0.0;
            }
            case TokenType::BITWISE_AND: return static_cast<double>(static_cast<int>(l_val) & static_cast<int>(r_val));
            case TokenType::BITWISE_OR: return static_cast<double>(static_cast<int>(l_val) | static_cast<int>(r_val));
            // Comparison operators return 1.0 (true) or 0.0 (false)
            case TokenType::LESS: return (l_val < r_val) ? 1.0 : 0.0;
            case TokenType::GREATER: return (l_val > r_val) ? 1.0 : 0.0;
            case TokenType::LESS_EQUAL: return (l_val <= r_val) ? 1.0 : 0.0;
            case TokenType::GREATER_EQUAL: return (l_val >= r_val) ? 1.0 : 0.0;
            case TokenType::EQUAL_EQUAL: return (l_val == r_val) ? 1.0 : 0.0;
            case TokenType::NOT_EQUAL: return (l_val != r_val) ? 1.0 : 0.0;
            default: return 0.0;
        }
    }

    void resolve_slots(VariableTable& table) override {
        left->resolve_slots(table);
        right->resolve_slots(table);
    }
};

struct UnaryOpNode : public ASTNode {
    std::unique_ptr<ASTNode> operand;
    TokenType op;

    UnaryOpNode(TokenType o, std::unique_ptr<ASTNode> operand)
        : operand(std::move(operand)), op(o) {}

    void set_engine(ScriptEngine* engine) override {
        operand->set_engine(engine);
    }

    void bind_variables(ScriptEngine& engine) override {
        operand->bind_variables(engine);
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        double val = operand->evaluate(variables);
        switch (op) {
            case TokenType::MINUS: return -val;
            case TokenType::PLUS: return val;
            default: return val;
        }
    }

    double evaluate_slots(double* vars) const override {
        double val = operand->evaluate_slots(vars);
        switch (op) {
            case TokenType::MINUS: return -val;
            case TokenType::PLUS: return val;
            default: return val;
        }
    }

    void resolve_slots(VariableTable& table) override {
        operand->resolve_slots(table);
    }
};

struct AssignmentNode : public ASTNode {
    std::string variable_name;
    std::unique_ptr<ASTNode> value;
    double* var_ptr_ = nullptr;  // Direct pointer to engine storage

    AssignmentNode(const std::string& name, std::unique_ptr<ASTNode> val)
        : variable_name(name), value(std::move(val)) {}

    void set_engine(ScriptEngine* engine) override {
        value->set_engine(engine);
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        double result = value->evaluate(variables);
        variables[variable_name] = result;
        return result;
    }

    double evaluate_slots(double* vars) const override {
        (void)vars;  // Unused - we use var_ptr_ directly
        double result = value->evaluate_slots(vars);
        if (var_ptr_) *var_ptr_ = result;
        return result;
    }

    void resolve_slots(VariableTable& table) override {
        value->resolve_slots(table);
        table.get_or_create_slot(variable_name);  // Register the name
    }

    void bind_variables(ScriptEngine& engine) override {
        var_ptr_ = &engine.var_ref(variable_name);
        value->bind_variables(engine);
    }
};

struct FunctionCallNode : public ASTNode {
    std::string function_name;
    std::vector<std::unique_ptr<ASTNode>> arguments;

    FunctionCallNode(const std::string& name) : function_name(name) {}

    void set_engine(ScriptEngine* engine) override {
        for (auto& arg : arguments) {
            arg->set_engine(engine);
        }
    }

    void bind_variables(ScriptEngine& engine) override {
        for (auto& arg : arguments) {
            arg->bind_variables(engine);
        }
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        std::vector<double> arg_values;
        for (const auto& arg : arguments) {
            arg_values.push_back(arg->evaluate(variables));
        }
        return call_function(arg_values);
    }

    double evaluate_slots(double* vars) const override {
        std::vector<double> arg_values;
        for (const auto& arg : arguments) {
            arg_values.push_back(arg->evaluate_slots(vars));
        }
        return call_function(arg_values);
    }

    void resolve_slots(VariableTable& table) override {
        for (auto& arg : arguments) {
            arg->resolve_slots(table);
        }
    }

private:
    double call_function(const std::vector<double>& arg_values) const {
        if (function_name == "sin") {
            return arg_values.size() > 0 ? std::sin(arg_values[0]) : 0.0;
        } else if (function_name == "cos") {
            return arg_values.size() > 0 ? std::cos(arg_values[0]) : 0.0;
        } else if (function_name == "tan") {
            return arg_values.size() > 0 ? std::tan(arg_values[0]) : 0.0;
        } else if (function_name == "sqrt") {
            return arg_values.size() > 0 ? std::sqrt(arg_values[0]) : 0.0;
        } else if (function_name == "abs") {
            return arg_values.size() > 0 ? std::fabs(arg_values[0]) : 0.0;
        } else if (function_name == "log") {
            return arg_values.size() > 0 ? std::log(arg_values[0]) : 0.0;
        } else if (function_name == "log10") {
            return arg_values.size() > 0 ? std::log10(arg_values[0]) : 0.0;
        } else if (function_name == "pow") {
            return arg_values.size() > 1 ? std::pow(arg_values[0], arg_values[1]) : 0.0;
        } else if (function_name == "atan2") {
            return arg_values.size() > 1 ? std::atan2(arg_values[0], arg_values[1]) : 0.0;
        } else if (function_name == "floor") {
            return arg_values.size() > 0 ? std::floor(arg_values[0]) : 0.0;
        } else if (function_name == "ceil") {
            return arg_values.size() > 0 ? std::ceil(arg_values[0]) : 0.0;
        } else if (function_name == "min") {
            return arg_values.size() > 1 ? std::min(arg_values[0], arg_values[1]) : 0.0;
        } else if (function_name == "max") {
            return arg_values.size() > 1 ? std::max(arg_values[0], arg_values[1]) : 0.0;
        } else if (function_name == "rand") {
            return arg_values.size() > 0 ? static_cast<double>(rand()) / RAND_MAX * arg_values[0] : static_cast<double>(rand()) / RAND_MAX;
        } else if (function_name == "asin") {
            return arg_values.size() > 0 ? std::asin(arg_values[0]) : 0.0;
        } else if (function_name == "acos") {
            return arg_values.size() > 0 ? std::acos(arg_values[0]) : 0.0;
        } else if (function_name == "atan") {
            return arg_values.size() > 0 ? std::atan(arg_values[0]) : 0.0;
        } else if (function_name == "exp") {
            return arg_values.size() > 0 ? std::exp(arg_values[0]) : 0.0;
        } else if (function_name == "if") {
            // AVS EEL: if(cond, then, else) - returns then if cond != 0, else otherwise
            if (arg_values.size() >= 3) {
                return (arg_values[0] != 0.0) ? arg_values[1] : arg_values[2];
            } else if (arg_values.size() == 2) {
                return (arg_values[0] != 0.0) ? arg_values[1] : 0.0;
            }
            return 0.0;
        } else if (function_name == "above") {
            // Returns 1 if a > b, else 0
            return arg_values.size() > 1 ? (arg_values[0] > arg_values[1] ? 1.0 : 0.0) : 0.0;
        } else if (function_name == "below") {
            // Returns 1 if a < b, else 0
            return arg_values.size() > 1 ? (arg_values[0] < arg_values[1] ? 1.0 : 0.0) : 0.0;
        } else if (function_name == "equal") {
            // Returns 1 if a == b, else 0
            return arg_values.size() > 1 ? (arg_values[0] == arg_values[1] ? 1.0 : 0.0) : 0.0;
        } else if (function_name == "sign") {
            // Returns -1, 0, or 1
            if (arg_values.empty()) return 0.0;
            if (arg_values[0] > 0) return 1.0;
            if (arg_values[0] < 0) return -1.0;
            return 0.0;
        } else if (function_name == "sqr") {
            // Square of x
            return arg_values.size() > 0 ? arg_values[0] * arg_values[0] : 0.0;
        } else if (function_name == "invsqrt") {
            // 1/sqrt(x) - fast inverse square root
            return arg_values.size() > 0 && arg_values[0] > 0 ? 1.0 / std::sqrt(arg_values[0]) : 0.0;
        } else if (function_name == "sigmoid") {
            // Constrains value between 0 and 1: x / (1 + abs(x)) scaled to 0-1
            if (arg_values.empty()) return 0.0;
            double x = arg_values[0];
            double constrainer = arg_values.size() > 1 ? arg_values[1] : 1.0;
            return (x / constrainer) / (1.0 + std::fabs(x / constrainer)) * 0.5 + 0.5;
        } else if (function_name == "band") {
            // Boolean AND: returns non-zero if both args are non-zero
            return arg_values.size() > 1 ? ((arg_values[0] != 0.0 && arg_values[1] != 0.0) ? 1.0 : 0.0) : 0.0;
        } else if (function_name == "bor") {
            // Boolean OR: returns non-zero if either arg is non-zero
            return arg_values.size() > 1 ? ((arg_values[0] != 0.0 || arg_values[1] != 0.0) ? 1.0 : 0.0) : 0.0;
        } else if (function_name == "bnot") {
            // Boolean NOT: returns 1 if arg is zero, 0 otherwise
            return arg_values.size() > 0 ? (arg_values[0] == 0.0 ? 1.0 : 0.0) : 1.0;
        } else if (function_name == "gettime") {
            // Returns time: 0=seconds since midnight, 1=hours, 2=minutes, 3=seconds
            if (arg_values.empty()) return 0.0;
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            struct tm* tm_info = localtime(&time);
            int selector = static_cast<int>(arg_values[0]);
            switch (selector) {
                case 0: return tm_info->tm_hour * 3600.0 + tm_info->tm_min * 60.0 + tm_info->tm_sec;
                case 1: return tm_info->tm_hour;
                case 2: return tm_info->tm_min;
                case 3: return tm_info->tm_sec;
                default: return 0.0;
            }
        } else if (function_name == "getosc") {
            // Waveform data - requires audio context, returns 0 for now
            // TODO: Implement with audio context access
            return 0.0;
        } else if (function_name == "getspec") {
            // Spectrum data - requires audio context, returns 0 for now
            // TODO: Implement with audio context access
            return 0.0;
        } else if (function_name == "getkbmouse") {
            // Keyboard/mouse state - not implemented in this port
            return 0.0;
        } else if (function_name == "megabuf" || function_name == "gmegabuf") {
            // Memory buffer access - requires buffer implementation
            // TODO: Implement megabuf/gmegabuf
            return 0.0;
        } else if (function_name == "assign") {
            // assign(dest, value) - in EEL this assigns value to dest, returns value
            // Since we evaluate args first, we can't actually assign here
            // This is typically used for side effects; return last value
            return arg_values.size() > 1 ? arg_values[1] : 0.0;
        } else if (function_name == "exec2") {
            // exec2(e1, e2) - evaluates both, returns e2
            // Args are already evaluated, just return last
            return arg_values.size() > 1 ? arg_values[1] : (arg_values.size() > 0 ? arg_values[0] : 0.0);
        } else if (function_name == "exec3") {
            // exec3(e1, e2, e3) - evaluates all three, returns e3
            return arg_values.size() > 2 ? arg_values[2] :
                   arg_values.size() > 1 ? arg_values[1] :
                   arg_values.size() > 0 ? arg_values[0] : 0.0;
        } else if (function_name == "sin" || function_name == "cos" || function_name == "tan" ||
                   function_name == "asin" || function_name == "acos" || function_name == "atan") {
            // Already handled above, but include here for completeness check
            return 0.0;
        }

        // Unknown function is a script error
        throw std::runtime_error("Unknown function: " + function_name);
    }
};

// Array access node for MIDI arrays and user-defined arrays
struct ArrayAccessNode : public ASTNode {
    std::string array_name;
    std::unique_ptr<ASTNode> index;
    ScriptEngine* engine_ = nullptr;  // For user array access

    ArrayAccessNode(const std::string& name, std::unique_ptr<ASTNode> idx)
        : array_name(name), index(std::move(idx)) {}

    void set_engine(ScriptEngine* engine) override {
        engine_ = engine;
        index->set_engine(engine);
    }

    void bind_variables(ScriptEngine& engine) override {
        index->bind_variables(engine);
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        int idx = static_cast<int>(index->evaluate(variables));
        return lookup_array(idx);
    }

    double evaluate_slots(double* vars) const override {
        int idx = static_cast<int>(index->evaluate_slots(vars));
        return lookup_array(idx);
    }

    void resolve_slots(VariableTable& table) override {
        index->resolve_slots(table);
    }

private:
    double lookup_array(int idx) const {
        // Check built-in MIDI arrays first
        const auto& midi = EventBus::instance().get_midi_data();

        if (array_name == "midi_cc") {
            if (idx >= 0 && idx < 128) return midi.cc[idx];
            return 0.0;
        } else if (array_name == "midi_note") {
            if (idx >= 0 && idx < 128) return midi.note[idx];
            return 0.0;
        } else if (array_name == "midi_note_index") {
            if (idx >= 0 && idx < midi.note_count) return static_cast<double>(midi.note_index[idx]);
            return 0.0;
        }

        // Fall through to user arrays
        if (engine_) {
            return engine_->array_read(array_name, idx);
        }
        return 0.0;
    }
};

// Array assignment node for user-defined arrays: arr[i] = value
struct ArrayAssignmentNode : public ASTNode {
    std::string array_name;
    std::unique_ptr<ASTNode> index;
    std::unique_ptr<ASTNode> value;
    ScriptEngine* engine_ = nullptr;

    ArrayAssignmentNode(const std::string& name, std::unique_ptr<ASTNode> idx, std::unique_ptr<ASTNode> val)
        : array_name(name), index(std::move(idx)), value(std::move(val)) {}

    void set_engine(ScriptEngine* engine) override {
        engine_ = engine;
        index->set_engine(engine);
        value->set_engine(engine);
    }

    void bind_variables(ScriptEngine& engine) override {
        index->bind_variables(engine);
        value->bind_variables(engine);
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        int idx = static_cast<int>(index->evaluate(variables));
        double val = value->evaluate(variables);
        if (engine_) {
            engine_->array_write(array_name, idx, val);
        }
        return val;
    }

    double evaluate_slots(double* vars) const override {
        int idx = static_cast<int>(index->evaluate_slots(vars));
        double val = value->evaluate_slots(vars);
        if (engine_) {
            engine_->array_write(array_name, idx, val);
        }
        return val;
    }

    void resolve_slots(VariableTable& table) override {
        index->resolve_slots(table);
        value->resolve_slots(table);
    }
};

// While loop node: while(condition, body) - evaluates body while condition is non-zero
struct WhileNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;

    WhileNode(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> b)
        : condition(std::move(cond)), body(std::move(b)) {}

    void set_engine(ScriptEngine* engine) override {
        condition->set_engine(engine);
        body->set_engine(engine);
    }

    void bind_variables(ScriptEngine& engine) override {
        condition->bind_variables(engine);
        body->bind_variables(engine);
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        double result = 0.0;
        int iterations = 0;
        const int max_iterations = 1000000;  // Prevent infinite loops
        while (condition->evaluate(variables) != 0.0 && iterations < max_iterations) {
            result = body->evaluate(variables);
            iterations++;
        }
        return result;
    }

    double evaluate_slots(double* vars) const override {
        double result = 0.0;
        int iterations = 0;
        const int max_iterations = 1000000;
        while (condition->evaluate_slots(vars) != 0.0 && iterations < max_iterations) {
            result = body->evaluate_slots(vars);
            iterations++;
        }
        return result;
    }

    void resolve_slots(VariableTable& table) override {
        condition->resolve_slots(table);
        body->resolve_slots(table);
    }
};

// Loop node: loop(count, body) - evaluates body count times
struct LoopNode : public ASTNode {
    std::unique_ptr<ASTNode> count;
    std::unique_ptr<ASTNode> body;

    LoopNode(std::unique_ptr<ASTNode> cnt, std::unique_ptr<ASTNode> b)
        : count(std::move(cnt)), body(std::move(b)) {}

    void set_engine(ScriptEngine* engine) override {
        count->set_engine(engine);
        body->set_engine(engine);
    }

    void bind_variables(ScriptEngine& engine) override {
        count->bind_variables(engine);
        body->bind_variables(engine);
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        int n = static_cast<int>(count->evaluate(variables));
        if (n < 0) n = 0;
        if (n > 1000000) n = 1000000;  // Prevent excessive iterations
        double result = 0.0;
        for (int i = 0; i < n; i++) {
            result = body->evaluate(variables);
        }
        return result;
    }

    double evaluate_slots(double* vars) const override {
        int n = static_cast<int>(count->evaluate_slots(vars));
        if (n < 0) n = 0;
        if (n > 1000000) n = 1000000;
        double result = 0.0;
        for (int i = 0; i < n; i++) {
            result = body->evaluate_slots(vars);
        }
        return result;
    }

    void resolve_slots(VariableTable& table) override {
        count->resolve_slots(table);
        body->resolve_slots(table);
    }
};

struct StatementSequenceNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;

    StatementSequenceNode() {}

    void set_engine(ScriptEngine* engine) override {
        for (auto& stmt : statements) {
            stmt->set_engine(engine);
        }
    }

    void bind_variables(ScriptEngine& engine) override {
        for (auto& stmt : statements) {
            stmt->bind_variables(engine);
        }
    }

    double evaluate(std::map<std::string, double>& variables) const override {
        double last_result = 0.0;
        for (const auto& stmt : statements) {
            last_result = stmt->evaluate(variables);
        }
        return last_result; // Return result of last statement
    }

    double evaluate_slots(double* vars) const override {
        double last_result = 0.0;
        for (const auto& stmt : statements) {
            last_result = stmt->evaluate_slots(vars);
        }
        return last_result;
    }

    void resolve_slots(VariableTable& table) override {
        for (auto& stmt : statements) {
            stmt->resolve_slots(table);
        }
    }
};

class Parser {
public:
    explicit Parser(Lexer& lexer);
    std::unique_ptr<ASTNode> parse();

private:
    Lexer& lexer;

    std::unique_ptr<ASTNode> parse_statement_sequence();
    std::unique_ptr<ASTNode> parse_statement();
    std::unique_ptr<ASTNode> parse_assignment_or_expression();
    std::unique_ptr<ASTNode> parse_expression();
    std::unique_ptr<ASTNode> parse_expression_with_first_term(std::unique_ptr<ASTNode> first_term);
    std::unique_ptr<ASTNode> parse_comparison();
    std::unique_ptr<ASTNode> parse_additive();
    std::unique_ptr<ASTNode> parse_term();
    std::unique_ptr<ASTNode> parse_factor();
    std::unique_ptr<ASTNode> parse_loop_body();
    bool is_comparison_op(TokenType type) const;
};

} // namespace avs
