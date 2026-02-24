// avs_lib - Portable Advanced Visualization Studio library
// Tests for script engine extensions: comparison operators, comments, user arrays
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/script/script_engine.h"

using namespace avs;

// =============================================================================
// Test: Comparison Operators
// =============================================================================

TEST_CASE("Comparison operators", "[script][comparison]") {
    ScriptEngine engine;

    SECTION("Less than operator") {
        auto script = engine.compile("result = 3 < 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 5 < 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));

        script = engine.compile("result = 3 < 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Greater than operator") {
        auto script = engine.compile("result = 5 > 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 3 > 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Less than or equal operator") {
        auto script = engine.compile("result = 3 <= 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 3 <= 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 5 <= 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Greater than or equal operator") {
        auto script = engine.compile("result = 5 >= 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 3 >= 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 3 >= 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Equal operator") {
        auto script = engine.compile("result = 3 == 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 3 == 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Not equal operator") {
        auto script = engine.compile("result = 3 != 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = 3 != 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Comparison with variables") {
        engine.set_variable("a", 10.0);
        engine.set_variable("b", 5.0);

        auto script = engine.compile("result = a > b");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = a < b");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Comparison in if() function") {
        engine.set_variable("x", 10.0);

        auto script = engine.compile("result = if(x > 5, 100, 0)");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(100.0));

        engine.set_variable("x", 3.0);
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Comparison with bitwise AND") {
        // skip = (c < 0.5) & (sq > 0)
        engine.set_variable("c", 0.0);
        engine.set_variable("sq", 1.0);

        auto script = engine.compile("skip = (c < 0.5) & (sq > 0)");
        engine.execute(script);
        REQUIRE(engine.get_variable("skip") == Catch::Approx(1.0));

        engine.set_variable("c", 1.0);  // c >= 0.5
        engine.execute(script);
        REQUIRE(engine.get_variable("skip") == Catch::Approx(0.0));
    }

    SECTION("Operator precedence: comparison lower than arithmetic") {
        // 3 + 2 < 10 should be (3 + 2) < 10, not 3 + (2 < 10)
        auto script = engine.compile("result = 3 + 2 < 10");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));  // 5 < 10 = true

        // 3 * 2 > 5 should be (3 * 2) > 5
        script = engine.compile("result = 3 * 2 > 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));  // 6 > 5 = true
    }

    SECTION("Operator precedence: comparison higher than bitwise") {
        // 3 < 5 & 2 < 4 should be (3 < 5) & (2 < 4)
        auto script = engine.compile("result = 3 < 5 & 2 < 4");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));  // 1 & 1 = 1
    }
}

// =============================================================================
// Test: Modulo Operator
// =============================================================================

TEST_CASE("Modulo operator", "[script][modulo]") {
    ScriptEngine engine;

    SECTION("Basic integer modulo") {
        auto script = engine.compile("result = 10 % 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));  // 10 % 3 = 1

        script = engine.compile("result = 15 % 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));  // 15 % 5 = 0

        script = engine.compile("result = 7 % 4");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(3.0));  // 7 % 4 = 3
    }

    SECTION("Modulo with floats truncates to int") {
        // 5.56 % 5 should be int(5.56) % 5 = 5 % 5 = 0
        auto script = engine.compile("result = 5.56 % 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));

        // 4.99 % 5 should be int(4.99) % 5 = 4 % 5 = 4
        script = engine.compile("result = 4.99 % 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(4.0));

        // 10.7 % 3 should be int(10.7) % 3 = 10 % 3 = 1
        script = engine.compile("result = 10.7 % 3");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));
    }

    SECTION("Modulo with expression") {
        engine.set_variable("i", 0.556);  // Simulates i*n where i=0.556, n=10 -> 5.56
        engine.set_variable("n", 10.0);
        auto script = engine.compile("result = (i * n) % 5");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));  // int(5.56) % 5 = 0
    }

    SECTION("Modulo for corner detection pattern") {
        // Simulating SuperScope pattern: corner = (i * n) % 5
        // With n=10, i goes 0, 0.111, 0.222, ... 1.0
        engine.set_variable("n", 10.0);

        // i=0 (first point of first shape)
        engine.set_variable("i", 0.0);
        engine.execute(engine.compile("corner = (i * n) % 5"));
        REQUIRE(engine.get_variable("corner") == Catch::Approx(0.0));

        // i=0.111 (second point, i*n=1.11)
        engine.set_variable("i", 0.111);
        engine.execute(engine.compile("corner = (i * n) % 5"));
        REQUIRE(engine.get_variable("corner") == Catch::Approx(1.0));

        // i=0.556 (first point of second shape, i*n=5.56)
        engine.set_variable("i", 0.556);
        engine.execute(engine.compile("corner = (i * n) % 5"));
        REQUIRE(engine.get_variable("corner") == Catch::Approx(0.0));

        // i=0.667 (second point of second shape, i*n=6.67)
        engine.set_variable("i", 0.667);
        engine.execute(engine.compile("corner = (i * n) % 5"));
        REQUIRE(engine.get_variable("corner") == Catch::Approx(1.0));
    }
}

// =============================================================================
// Test: Line Comments
// =============================================================================

TEST_CASE("Line comments", "[script][comments]") {
    ScriptEngine engine;

    SECTION("Comment at end of line") {
        auto script = engine.compile("x = 5 // this is a comment");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(5.0));
    }

    SECTION("Full line comment") {
        auto script = engine.compile("// this is a comment\nx = 10");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(10.0));
    }

    SECTION("Multiple comments") {
        auto script = engine.compile(
            "x = 1; // first\n"
            "// middle comment\n"
            "y = 2 // second"
        );
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(1.0));
        REQUIRE(engine.get_variable("y") == Catch::Approx(2.0));
    }

    SECTION("Comment does not affect division") {
        // x = 10/2 should work (division, not comment)
        auto script = engine.compile("x = 10/2");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(5.0));
    }

    SECTION("Division followed by comment") {
        auto script = engine.compile("x = 10/2 // divide by 2");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(5.0));
    }

    SECTION("Adjacent slashes start comment") {
        // x = 10//2 means x = 10 then comment
        auto script = engine.compile("x = 10//2");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(10.0));
    }
}

// =============================================================================
// Test: User-Defined Arrays
// =============================================================================

TEST_CASE("User-defined arrays", "[script][arrays]") {
    ScriptEngine engine;

    SECTION("Basic array write and read") {
        auto write_script = engine.compile("arr[0] = 42");
        engine.execute(write_script);

        auto read_script = engine.compile("result = arr[0]");
        engine.execute(read_script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(42.0));
    }

    SECTION("Array auto-grows on write") {
        auto script = engine.compile("arr[10] = 100");
        engine.execute(script);

        auto read_script = engine.compile("result = arr[10]");
        engine.execute(read_script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(100.0));
    }

    SECTION("Uninitialized array elements return 0") {
        auto script = engine.compile("result = nonexistent[5]");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Multiple arrays") {
        engine.execute(engine.compile("x_arr[0] = 1; y_arr[0] = 2"));
        engine.execute(engine.compile("result = x_arr[0] + y_arr[0]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(3.0));
    }

    SECTION("Array with variable index") {
        engine.set_variable("i", 3.0);
        engine.execute(engine.compile("data[i] = 99"));
        engine.execute(engine.compile("result = data[3]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(99.0));
    }

    SECTION("Array with expression index") {
        engine.execute(engine.compile("data[2 + 3] = 55"));
        engine.execute(engine.compile("result = data[5]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(55.0));
    }

    SECTION("Array write returns value (in separate statement)") {
        // Array assignment returns the value (useful for chaining)
        auto script = engine.compile("arr[0] = 77; x = arr[0]");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(77.0));
    }

    SECTION("Negative index ignored for write, returns 0 for read") {
        engine.execute(engine.compile("arr[-1] = 100"));  // Should do nothing
        auto script = engine.compile("result = arr[-1]");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Sequential array operations in loop pattern") {
        // Simulate frame script writing, then point script reading
        engine.execute(engine.compile(
            "shape_x[0] = 0.5; shape_x[1] = 0.7; shape_x[2] = 0.9"
        ));

        engine.set_variable("i", 1.0);
        engine.execute(engine.compile("result = shape_x[i]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.7));
    }

    SECTION("Clear user arrays") {
        engine.execute(engine.compile("test_arr[0] = 123"));
        engine.execute(engine.compile("result = test_arr[0]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(123.0));

        engine.clear_user_arrays();

        engine.execute(engine.compile("result = test_arr[0]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("Array persists across multiple script executions") {
        auto write_script = engine.compile("arr[0] = arr[0] + 1");

        engine.execute(write_script);
        engine.execute(write_script);
        engine.execute(write_script);

        engine.execute(engine.compile("result = arr[0]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(3.0));
    }

    SECTION("Built-in MIDI arrays take precedence") {
        // midi_cc is a built-in array, user can't override it
        engine.execute(engine.compile("midi_cc[1] = 999"));  // Should NOT write to built-in
        // Note: we can't easily test this returns the built-in value without MIDI events,
        // but the key is that writing doesn't crash and reading returns 0 (no MIDI data)
        engine.execute(engine.compile("result = midi_cc[1]"));
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));  // Built-in returns 0
    }
}

// =============================================================================
// Test: Combined Features
// =============================================================================

TEST_CASE("Combined script features", "[script][integration]") {
    ScriptEngine engine;

    SECTION("Comparison with arrays and comments") {
        engine.execute(engine.compile(
            "// Store thresholds\n"
            "thresh[0] = 0.5; // first threshold\n"
            "thresh[1] = 0.8; // second threshold\n"
            "value = 0.6"
        ));

        engine.execute(engine.compile(
            "// Check if value exceeds first threshold\n"
            "result = if(value > thresh[0], 1, 0)"
        ));
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));
    }

    SECTION("SuperScope-like pattern with new syntax") {
        // Simulate pre-calculating geometry in frame, using in point
        engine.execute(engine.compile(
            "// Frame: pre-calculate positions\n"
            "num_shapes = 3;\n"
            "shape_x[0] = -0.5; shape_y[0] = 0.0;\n"
            "shape_x[1] = 0.0;  shape_y[1] = 0.5;\n"
            "shape_x[2] = 0.5;  shape_y[2] = 0.0"
        ));

        // Point script using new comparison operators
        for (int i = 0; i < 3; i++) {
            engine.set_variable("idx", static_cast<double>(i));
            engine.execute(engine.compile(
                "valid = idx < num_shapes;\n"
                "x = if(valid, shape_x[idx], 0);\n"
                "y = if(valid, shape_y[idx], 0)"
            ));

            double x = engine.get_variable("x");
            double y = engine.get_variable("y");

            if (i == 0) {
                REQUIRE(x == Catch::Approx(-0.5));
                REQUIRE(y == Catch::Approx(0.0));
            } else if (i == 1) {
                REQUIRE(x == Catch::Approx(0.0));
                REQUIRE(y == Catch::Approx(0.5));
            } else {
                REQUIRE(x == Catch::Approx(0.5));
                REQUIRE(y == Catch::Approx(0.0));
            }
        }
    }
}

// =============================================================================
// Test: While and Loop constructs
// =============================================================================

TEST_CASE("While loop", "[script][while]") {
    ScriptEngine engine;

    SECTION("Basic while loop") {
        auto script = engine.compile(
            "i = 0; sum = 0; while(i < 5, sum = sum + i; i = i + 1); result = sum"
        );
        engine.execute(script);
        // sum = 0 + 1 + 2 + 3 + 4 = 10
        REQUIRE(engine.get_variable("result") == Catch::Approx(10.0));
    }

    SECTION("While loop with array writes") {
        auto script = engine.compile(
            "i = 0; while(i < 3, arr[i] = i * 10; i = i + 1)"
        );
        engine.execute(script);

        engine.execute(engine.compile("r0 = arr[0]; r1 = arr[1]; r2 = arr[2]"));
        REQUIRE(engine.get_variable("r0") == Catch::Approx(0.0));
        REQUIRE(engine.get_variable("r1") == Catch::Approx(10.0));
        REQUIRE(engine.get_variable("r2") == Catch::Approx(20.0));
    }

    SECTION("While loop that never executes") {
        engine.set_variable("x", 10.0);
        auto script = engine.compile("while(x < 5, x = x + 1)");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(10.0));
    }

    SECTION("While loop with comparison operators") {
        auto script = engine.compile(
            "count = 0; val = 100; while(val > 10, val = val / 2; count = count + 1)"
        );
        engine.execute(script);
        // 100 -> 50 -> 25 -> 12 -> 6 (stops when val <= 10)
        REQUIRE(engine.get_variable("count") == Catch::Approx(4.0));
    }
}

TEST_CASE("Loop construct", "[script][loop]") {
    ScriptEngine engine;

    SECTION("Basic loop") {
        auto script = engine.compile(
            "sum = 0; loop(5, sum = sum + 1); result = sum"
        );
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(5.0));
    }

    SECTION("Loop with array writes") {
        // Note: loop doesn't provide an index variable, so we track our own
        auto script = engine.compile(
            "i = 0; loop(4, arr[i] = i * i; i = i + 1)"
        );
        engine.execute(script);

        engine.execute(engine.compile("r0 = arr[0]; r1 = arr[1]; r2 = arr[2]; r3 = arr[3]"));
        REQUIRE(engine.get_variable("r0") == Catch::Approx(0.0));
        REQUIRE(engine.get_variable("r1") == Catch::Approx(1.0));
        REQUIRE(engine.get_variable("r2") == Catch::Approx(4.0));
        REQUIRE(engine.get_variable("r3") == Catch::Approx(9.0));
    }

    SECTION("Loop with zero count") {
        engine.set_variable("x", 5.0);
        auto script = engine.compile("loop(0, x = x + 1)");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(5.0));
    }

    SECTION("Loop with negative count") {
        engine.set_variable("x", 5.0);
        auto script = engine.compile("loop(-3, x = x + 1)");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(5.0));  // Should not execute
    }

    SECTION("Loop with variable count") {
        engine.set_variable("n", 3.0);
        auto script = engine.compile("sum = 0; loop(n, sum = sum + 10)");
        engine.execute(script);
        REQUIRE(engine.get_variable("sum") == Catch::Approx(30.0));
    }
}
