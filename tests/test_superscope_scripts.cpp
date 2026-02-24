// avs_lib - Portable Advanced Visualization Studio library
// Tests for SuperScope script features
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/script/script_engine.h"
#include "core/event_bus.h"

using namespace avs;

// Test 1: Variable scope between frame and point scripts
TEST_CASE("Variable scope between script phases", "[superscope]") {
    ScriptEngine engine;

    SECTION("Variables set in frame script are available in point script") {
        // Simulate frame script setting a variable
        auto frame_script = engine.compile("nc=5; t=t+0.02");
        engine.set_variable("t", 0.0);
        engine.execute(frame_script);

        // Verify variable is accessible
        double nc = engine.get_variable("nc");
        REQUIRE(nc == Catch::Approx(5.0));

        // Simulate point script using that variable
        auto point_script = engine.compile("x=nc*0.1");
        engine.execute(point_script);

        double x = engine.get_variable("x");
        REQUIRE(x == Catch::Approx(0.5));
    }

    SECTION("Variables persist across multiple point script executions") {
        auto frame_script = engine.compile("total=0");
        engine.execute(frame_script);

        auto point_script = engine.compile("total=total+1");

        // Execute point script 5 times (simulating 5 points)
        for (int i = 0; i < 5; i++) {
            engine.execute(point_script);
        }

        double total = engine.get_variable("total");
        REQUIRE(total == Catch::Approx(5.0));
    }
}

// Test 2: skip variable behavior
// NOTE: AVS uses below()/above()/equal() functions, NOT operators like < > ==
TEST_CASE("skip variable behavior", "[superscope]") {
    ScriptEngine engine;

    SECTION("skip can be set to 0 or 1") {
        engine.set_variable("skip", 0.0);

        auto script = engine.compile("skip=1");
        engine.execute(script);

        double skip = engine.get_variable("skip");
        REQUIRE(skip == Catch::Approx(1.0));
    }

    SECTION("below() function returns 0 or 1") {
        auto script = engine.compile("result = below(0, 0.5)");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(1.0));

        script = engine.compile("result = below(1, 0.5)");
        engine.execute(script);
        REQUIRE(engine.get_variable("result") == Catch::Approx(0.0));
    }

    SECTION("skip=below(c,0.5) pattern works correctly") {
        for (int c_val = 0; c_val < 5; c_val++) {
            engine.set_variable("c", static_cast<double>(c_val));
            auto script = engine.compile("skip = below(c, 0.5)");
            engine.execute(script);

            double skip = engine.get_variable("skip");
            if (c_val == 0) {
                REQUIRE(skip == Catch::Approx(1.0));  // 0 < 0.5 is true
            } else {
                REQUIRE(skip == Catch::Approx(0.0));  // 1,2,3,4 < 0.5 is false
            }
        }
    }

    SECTION("skip with logical AND using below() and above()") {
        // skip = below(c,0.5) & above(sq,0) should skip first point of squares 1+
        engine.set_variable("c", 0.0);
        engine.set_variable("sq", 0.0);
        auto script = engine.compile("skip = below(c, 0.5) & above(sq, 0)");
        engine.execute(script);
        REQUIRE(engine.get_variable("skip") == Catch::Approx(0.0));  // sq=0, don't skip

        engine.set_variable("sq", 1.0);
        engine.execute(script);
        REQUIRE(engine.get_variable("skip") == Catch::Approx(1.0));  // sq>0, skip
    }
}

// Test 3: MIDI variables in script context (simulating SuperScope usage)
TEST_CASE("MIDI variables in SuperScope-like context", "[superscope][midi]") {
    EventBus::instance().reset();
    ScriptEngine engine;

    SECTION("midi_note_count accessible in script") {
        // Send some MIDI notes
        for (int note : {60, 64, 67}) {
            Event event;
            event.type = Event::Type::MIDI_NOTE_ON;
            event.channel = 1;
            event.data1 = note;
            event.data2 = 100;
            event.timestamp = 0.0;
            EventBus::instance().push_event(event);
        }
        EventBus::instance().process_frame();

        auto script = engine.compile("nc = midi_note_count");
        engine.execute(script);

        double nc = engine.get_variable("nc");
        REQUIRE(nc == Catch::Approx(3.0));
    }

    SECTION("midi_note_count in expression with max()") {
        EventBus::instance().reset();
        EventBus::instance().process_frame();

        // No notes playing, midi_note_count should be 0
        auto script = engine.compile("nc = max(midi_note_count, 1)");
        engine.execute(script);

        double nc = engine.get_variable("nc");
        REQUIRE(nc == Catch::Approx(1.0));  // max(0, 1) = 1
    }

    SECTION("midi_cc array in expressions") {
        Event event;
        event.type = Event::Type::MIDI_CC;
        event.channel = 1;
        event.data1 = 1;      // Modwheel
        event.data2 = 127;    // Max
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        auto script = engine.compile("sz = 0.1 + midi_cc[1] * 0.2");
        engine.execute(script);

        double sz = engine.get_variable("sz");
        REQUIRE(sz == Catch::Approx(0.3).margin(0.01));  // 0.1 + 1.0 * 0.2 = 0.3
    }

    SECTION("midi_note_index array access with variable index") {
        EventBus::instance().reset();

        for (int note : {60, 64, 67}) {
            Event event;
            event.type = Event::Type::MIDI_NOTE_ON;
            event.channel = 1;
            event.data1 = note;
            event.data2 = 100;
            event.timestamp = 0.0;
            EventBus::instance().push_event(event);
        }
        EventBus::instance().process_frame();

        // Use sq as index (like in the SuperScope script)
        engine.set_variable("sq", 1.0);
        auto script = engine.compile("note = midi_note_index[sq]");
        engine.execute(script);

        double note = engine.get_variable("note");
        REQUIRE(note == Catch::Approx(64.0));  // Second note
    }
}

// Test 4: Array syntax edge cases
TEST_CASE("Array syntax edge cases", "[superscope]") {
    EventBus::instance().reset();
    ScriptEngine engine;

    SECTION("Array access with floor() result as index") {
        Event event;
        event.type = Event::Type::MIDI_CC;
        event.channel = 1;
        event.data1 = 7;
        event.data2 = 64;
        event.timestamp = 0.0;
        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        auto script = engine.compile("x = midi_cc[floor(7.9)]");
        engine.execute(script);

        double x = engine.get_variable("x");
        REQUIRE(x == Catch::Approx(64.0 / 127.0));
    }

    SECTION("Array access with computed expression as index") {
        for (int cc = 0; cc < 10; cc++) {
            Event event;
            event.type = Event::Type::MIDI_CC;
            event.channel = 1;
            event.data1 = cc;
            event.data2 = cc * 10;
            event.timestamp = 0.0;
            EventBus::instance().push_event(event);
        }
        EventBus::instance().process_frame();

        engine.set_variable("i", 0.5);
        auto script = engine.compile("x = midi_cc[floor(i * 10)]");
        engine.execute(script);

        double x = engine.get_variable("x");
        // i=0.5, floor(0.5*10)=5, cc[5]=50/127
        REQUIRE(x == Catch::Approx(50.0 / 127.0));
    }

    SECTION("Negative array index returns 0") {
        auto script = engine.compile("x = midi_cc[-1]");
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(0.0));
    }
}

// Test 5: Complex expression patterns used in SuperScope
// NOTE: AVS uses below()/above() functions, NOT operators like < > >=
TEST_CASE("Complex SuperScope expression patterns", "[superscope]") {
    ScriptEngine engine;

    SECTION("idx/sq/c calculation pattern") {
        // Simulate: idx=floor(i*24.99); sq=floor(idx/5); c=idx-sq*5
        engine.set_variable("n", 25.0);

        for (int point = 0; point < 25; point++) {
            double i = static_cast<double>(point) / 24.0;  // 0 to 1
            engine.set_variable("i", i);

            auto script = engine.compile("idx=floor(i*24.99); sq=floor(idx/5); c=idx-sq*5");
            engine.execute(script);

            double idx = engine.get_variable("idx");
            double sq = engine.get_variable("sq");
            double c = engine.get_variable("c");

            REQUIRE(idx == Catch::Approx(static_cast<double>(point)));
            REQUIRE(sq == Catch::Approx(static_cast<double>(point / 5)));
            REQUIRE(c == Catch::Approx(static_cast<double>(point % 5)));
        }
    }

    SECTION("Conditional skip pattern using if() with above()") {
        // if(above(sq, nc-1), 1, 0) is equivalent to if(sq >= nc, 1, 0)
        // Or use: if(below(sq, nc), 0, 1) which inverts the logic
        auto script = engine.compile("skip = if(below(sq, nc), 0, 1)");

        engine.set_variable("nc", 3.0);

        engine.set_variable("sq", 2.0);
        engine.execute(script);
        REQUIRE(engine.get_variable("skip") == Catch::Approx(0.0));  // 2 < 3, don't skip

        engine.set_variable("sq", 3.0);
        engine.execute(script);
        REQUIRE(engine.get_variable("skip") == Catch::Approx(1.0));  // 3 >= 3, skip

        engine.set_variable("sq", 5.0);
        engine.execute(script);
        REQUIRE(engine.get_variable("skip") == Catch::Approx(1.0));  // 5 >= 3, skip
    }

    SECTION("if() with below() as condition") {
        auto script = engine.compile("x = if(below(sq, nc), 1, 0)");

        engine.set_variable("nc", 5.0);
        engine.set_variable("sq", 3.0);
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(1.0));

        engine.set_variable("sq", 7.0);
        engine.execute(script);
        REQUIRE(engine.get_variable("x") == Catch::Approx(0.0));
    }
}
