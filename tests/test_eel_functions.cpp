// avs_lib - Portable Advanced Visualization Studio library
// Tests for AVS EEL functions and operators

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/script/script_engine.h"
#include <cmath>

using namespace avs;

// ============================================================================
// BITWISE OPERATORS
// ============================================================================

TEST_CASE("Script Engine - Bitwise Operators", "[script][bitwise]") {
    ScriptEngine engine;

    SECTION("Bitwise AND") {
        REQUIRE(engine.evaluate("15 & 7") == 7.0);   // 1111 & 0111 = 0111
        REQUIRE(engine.evaluate("12 & 10") == 8.0);  // 1100 & 1010 = 1000
        REQUIRE(engine.evaluate("255 & 128") == 128.0);
    }

    SECTION("Bitwise OR") {
        REQUIRE(engine.evaluate("8 | 4") == 12.0);   // 1000 | 0100 = 1100
        REQUIRE(engine.evaluate("5 | 3") == 7.0);    // 0101 | 0011 = 0111
        REQUIRE(engine.evaluate("128 | 64") == 192.0);
    }

    SECTION("Modulo operator") {
        REQUIRE(engine.evaluate("10 % 3") == 1.0);
        REQUIRE(engine.evaluate("15 % 4") == 3.0);
        REQUIRE(engine.evaluate("100 % 7") == 2.0);
    }

    SECTION("Combined bitwise operations") {
        REQUIRE(engine.evaluate("(15 & 7) | 8") == 15.0);  // 7 | 8 = 15
        REQUIRE(engine.evaluate("x = 255; x & 15") == 15.0);
    }
}

// ============================================================================
// CONDITIONAL FUNCTIONS
// ============================================================================

TEST_CASE("Script Engine - Conditional Functions", "[script][conditional]") {
    ScriptEngine engine;

    SECTION("if function - basic") {
        REQUIRE(engine.evaluate("if(1, 10, 20)") == 10.0);
        REQUIRE(engine.evaluate("if(0, 10, 20)") == 20.0);
        REQUIRE(engine.evaluate("if(-1, 10, 20)") == 10.0);  // Non-zero is true
    }

    SECTION("if function - with expressions") {
        engine.set_variable("x", 5.0);
        REQUIRE(engine.evaluate("if(above(x, 3), 100, 200)") == 100.0);
        REQUIRE(engine.evaluate("if(below(x, 3), 100, 200)") == 200.0);
    }

    SECTION("above function") {
        REQUIRE(engine.evaluate("above(5, 3)") == 1.0);
        REQUIRE(engine.evaluate("above(3, 5)") == 0.0);
        REQUIRE(engine.evaluate("above(3, 3)") == 0.0);
    }

    SECTION("below function") {
        REQUIRE(engine.evaluate("below(3, 5)") == 1.0);
        REQUIRE(engine.evaluate("below(5, 3)") == 0.0);
        REQUIRE(engine.evaluate("below(3, 3)") == 0.0);
    }

    SECTION("equal function") {
        REQUIRE(engine.evaluate("equal(5, 5)") == 1.0);
        REQUIRE(engine.evaluate("equal(5, 3)") == 0.0);
    }
}

// ============================================================================
// BOOLEAN FUNCTIONS
// ============================================================================

TEST_CASE("Script Engine - Boolean Functions", "[script][boolean]") {
    ScriptEngine engine;

    SECTION("band function") {
        REQUIRE(engine.evaluate("band(1, 1)") == 1.0);
        REQUIRE(engine.evaluate("band(1, 0)") == 0.0);
        REQUIRE(engine.evaluate("band(0, 1)") == 0.0);
        REQUIRE(engine.evaluate("band(0, 0)") == 0.0);
        REQUIRE(engine.evaluate("band(5, 3)") == 1.0);  // Both non-zero
    }

    SECTION("bor function") {
        REQUIRE(engine.evaluate("bor(1, 1)") == 1.0);
        REQUIRE(engine.evaluate("bor(1, 0)") == 1.0);
        REQUIRE(engine.evaluate("bor(0, 1)") == 1.0);
        REQUIRE(engine.evaluate("bor(0, 0)") == 0.0);
    }

    SECTION("bnot function") {
        REQUIRE(engine.evaluate("bnot(0)") == 1.0);
        REQUIRE(engine.evaluate("bnot(1)") == 0.0);
        REQUIRE(engine.evaluate("bnot(5)") == 0.0);  // Non-zero is false for bnot
    }
}

// ============================================================================
// MATH FUNCTIONS
// ============================================================================

TEST_CASE("Script Engine - Extended Math Functions", "[script][math]") {
    ScriptEngine engine;

    SECTION("sqr function") {
        REQUIRE(engine.evaluate("sqr(3)") == 9.0);
        REQUIRE(engine.evaluate("sqr(-4)") == 16.0);
        REQUIRE(engine.evaluate("sqr(0)") == 0.0);
    }

    SECTION("sign function") {
        REQUIRE(engine.evaluate("sign(5)") == 1.0);
        REQUIRE(engine.evaluate("sign(-5)") == -1.0);
        REQUIRE(engine.evaluate("sign(0)") == 0.0);
    }

    SECTION("invsqrt function") {
        REQUIRE(engine.evaluate("invsqrt(4)") == Catch::Approx(0.5));
        REQUIRE(engine.evaluate("invsqrt(1)") == Catch::Approx(1.0));
    }

    SECTION("sigmoid function") {
        // sigmoid should return values between 0 and 1
        double result = engine.evaluate("sigmoid(0, 1)");
        REQUIRE(result == Catch::Approx(0.5));  // sigmoid(0) = 0.5
    }
}

// ============================================================================
// CONSTANTS
// ============================================================================

TEST_CASE("Script Engine - Constants", "[script][constants]") {
    ScriptEngine engine;

    SECTION("$PI constant") {
        REQUIRE(engine.evaluate("$PI") == Catch::Approx(M_PI));
        REQUIRE(engine.evaluate("sin($PI)") == Catch::Approx(0.0).margin(0.0001));
    }

    SECTION("$E constant") {
        REQUIRE(engine.evaluate("$E") == Catch::Approx(M_E));
        REQUIRE(engine.evaluate("log($E)") == Catch::Approx(1.0));
    }

    SECTION("$PHI constant (golden ratio)") {
        REQUIRE(engine.evaluate("$PHI") == Catch::Approx(1.618033988749895));
        // PHI has the property: PHI^2 = PHI + 1
        REQUIRE(engine.evaluate("sqr($PHI) - $PHI - 1") == Catch::Approx(0.0).margin(0.0001));
    }
}

// ============================================================================
// TIME FUNCTIONS
// ============================================================================

TEST_CASE("Script Engine - Time Functions", "[script][time]") {
    ScriptEngine engine;

    SECTION("gettime returns reasonable values") {
        double seconds_since_midnight = engine.evaluate("gettime(0)");
        REQUIRE(seconds_since_midnight >= 0.0);
        REQUIRE(seconds_since_midnight < 86400.0);  // Less than 24 hours

        double hours = engine.evaluate("gettime(1)");
        REQUIRE(hours >= 0.0);
        REQUIRE(hours < 24.0);

        double minutes = engine.evaluate("gettime(2)");
        REQUIRE(minutes >= 0.0);
        REQUIRE(minutes < 60.0);

        double seconds = engine.evaluate("gettime(3)");
        REQUIRE(seconds >= 0.0);
        REQUIRE(seconds < 60.0);
    }
}

// ============================================================================
// CONTROL FLOW FUNCTIONS (STUBS)
// ============================================================================

TEST_CASE("Script Engine - Control Flow Functions", "[script][controlflow]") {
    ScriptEngine engine;

    SECTION("exec2 returns last value") {
        // exec2 evaluates both args and returns the second
        REQUIRE(engine.evaluate("exec2(5, 10)") == 10.0);
        REQUIRE(engine.evaluate("exec2(1+2, 3+4)") == 7.0);
    }

    SECTION("exec3 returns last value") {
        REQUIRE(engine.evaluate("exec3(1, 2, 3)") == 3.0);
        REQUIRE(engine.evaluate("exec3(1+1, 2+2, 3+3)") == 6.0);
    }

    SECTION("assign returns value") {
        // Note: assign can't actually assign in our implementation
        // but it should return the value
        REQUIRE(engine.evaluate("assign(x, 5)") == 5.0);
    }
}

// ============================================================================
// COMPILED SCRIPT EQUIVALENCE FOR NEW FEATURES
// ============================================================================

TEST_CASE("Script Engine - Compiled Script New Features", "[script][compiled][new]") {
    SECTION("Bitwise operators in compiled scripts") {
        ScriptEngine engine1, engine2;

        double map_result = engine1.evaluate("(15 & 7) | 4");

        auto compiled = engine2.compile("(15 & 7) | 4");
        double slot_result = engine2.execute(compiled);

        REQUIRE(slot_result == Catch::Approx(map_result));
    }

    SECTION("Conditional functions in compiled scripts") {
        ScriptEngine engine1, engine2;

        engine1.set_variable("x", 5.0);
        engine2.set_variable("x", 5.0);

        double map_result = engine1.evaluate("if(above(x, 3), 100, 200)");

        auto compiled = engine2.compile("if(above(x, 3), 100, 200)");
        double slot_result = engine2.execute(compiled);

        REQUIRE(slot_result == Catch::Approx(map_result));
    }

    SECTION("Constants in compiled scripts") {
        ScriptEngine engine1, engine2;

        double map_result = engine1.evaluate("$PI + $PHI");

        auto compiled = engine2.compile("$PI + $PHI");
        double slot_result = engine2.execute(compiled);

        REQUIRE(slot_result == Catch::Approx(map_result));
    }
}
