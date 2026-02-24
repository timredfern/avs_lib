// avs_lib - Portable Advanced Visualization Studio library
// Tests for MIDI EventBus and script array access
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/event_bus.h"
#include "core/script/script_engine.h"

using namespace avs;

TEST_CASE("EventBus basic operations", "[midi]") {
    // Reset EventBus state before each test
    EventBus::instance().reset();

    SECTION("Initial state is zeroed") {
        const auto& midi = EventBus::instance().get_midi_data();
        REQUIRE(midi.note_count == 0);
        REQUIRE(midi.pitch_bend == Catch::Approx(0.0));
        REQUIRE(midi.any_note_on == false);

        for (int i = 0; i < 128; i++) {
            REQUIRE(midi.cc[i] == Catch::Approx(0.0));
            REQUIRE(midi.note[i] == Catch::Approx(0.0));
        }
    }

    SECTION("MIDI CC events are processed") {
        Event event;
        event.type = Event::Type::MIDI_CC;
        event.channel = 1;
        event.data1 = 1;      // CC number (modwheel)
        event.data2 = 127;    // Max value
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        const auto& midi = EventBus::instance().get_midi_data();
        REQUIRE(midi.cc[1] == Catch::Approx(1.0));  // Normalized to 0.0-1.0
    }

    SECTION("MIDI note on events are processed") {
        Event event;
        event.type = Event::Type::MIDI_NOTE_ON;
        event.channel = 1;
        event.data1 = 60;     // Middle C
        event.data2 = 100;    // Velocity
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        const auto& midi = EventBus::instance().get_midi_data();
        REQUIRE(midi.note[60] == Catch::Approx(100.0 / 127.0));
        REQUIRE(midi.any_note_on == true);
        REQUIRE(midi.note_count == 1);
        REQUIRE(midi.note_index[0] == 60);
    }

    SECTION("MIDI note off clears velocity and updates index") {
        // First send note on
        Event on_event;
        on_event.type = Event::Type::MIDI_NOTE_ON;
        on_event.channel = 1;
        on_event.data1 = 60;
        on_event.data2 = 100;
        on_event.timestamp = 0.0;

        EventBus::instance().push_event(on_event);
        EventBus::instance().process_frame();

        // Then send note off
        Event off_event;
        off_event.type = Event::Type::MIDI_NOTE_OFF;
        off_event.channel = 1;
        off_event.data1 = 60;
        off_event.data2 = 0;
        off_event.timestamp = 0.0;

        EventBus::instance().push_event(off_event);
        EventBus::instance().process_frame();

        const auto& midi = EventBus::instance().get_midi_data();
        REQUIRE(midi.note[60] == Catch::Approx(0.0));
        REQUIRE(midi.note_count == 0);
    }

    SECTION("Pitch bend is normalized to -1.0 to 1.0") {
        // Center value (8192)
        Event center_event;
        center_event.type = Event::Type::MIDI_PITCH_BEND;
        center_event.channel = 1;
        center_event.data1 = 8192;  // Center
        center_event.timestamp = 0.0;

        EventBus::instance().push_event(center_event);
        EventBus::instance().process_frame();

        const auto& midi = EventBus::instance().get_midi_data();
        REQUIRE(midi.pitch_bend == Catch::Approx(0.0));

        // Max value (16383)
        EventBus::instance().reset();
        Event max_event;
        max_event.type = Event::Type::MIDI_PITCH_BEND;
        max_event.channel = 1;
        max_event.data1 = 16383;  // Max
        max_event.timestamp = 0.0;

        EventBus::instance().push_event(max_event);
        EventBus::instance().process_frame();

        REQUIRE(EventBus::instance().get_midi_data().pitch_bend == Catch::Approx(1.0).margin(0.001));

        // Min value (0)
        EventBus::instance().reset();
        Event min_event;
        min_event.type = Event::Type::MIDI_PITCH_BEND;
        min_event.channel = 1;
        min_event.data1 = 0;  // Min
        min_event.timestamp = 0.0;

        EventBus::instance().push_event(min_event);
        EventBus::instance().process_frame();

        REQUIRE(EventBus::instance().get_midi_data().pitch_bend == Catch::Approx(-1.0));
    }
}

TEST_CASE("MIDI array access in scripts", "[midi]") {
    EventBus::instance().reset();
    ScriptEngine engine;

    SECTION("midi_cc array access") {
        // Set up CC value
        Event event;
        event.type = Event::Type::MIDI_CC;
        event.channel = 1;
        event.data1 = 7;      // Volume CC
        event.data2 = 64;     // Half value
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        double result = engine.evaluate("midi_cc[7]");
        REQUIRE(result == Catch::Approx(64.0 / 127.0));
    }

    SECTION("midi_note array access") {
        Event event;
        event.type = Event::Type::MIDI_NOTE_ON;
        event.channel = 1;
        event.data1 = 48;     // C3
        event.data2 = 80;     // Velocity
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        double result = engine.evaluate("midi_note[48]");
        REQUIRE(result == Catch::Approx(80.0 / 127.0));
    }

    SECTION("midi_note_index array access") {
        // Add multiple notes
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

        // Check note_count
        double count = engine.evaluate("midi_note_count");
        REQUIRE(count == Catch::Approx(3.0));

        // Check note_index values
        double note0 = engine.evaluate("midi_note_index[0]");
        REQUIRE(note0 == Catch::Approx(60.0));

        double note1 = engine.evaluate("midi_note_index[1]");
        REQUIRE(note1 == Catch::Approx(64.0));

        double note2 = engine.evaluate("midi_note_index[2]");
        REQUIRE(note2 == Catch::Approx(67.0));
    }

    SECTION("midi_pitch_bend scalar variable") {
        Event event;
        event.type = Event::Type::MIDI_PITCH_BEND;
        event.channel = 1;
        event.data1 = 12288;  // 3/4 of the way up
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        double result = engine.evaluate("midi_pitch_bend");
        REQUIRE(result == Catch::Approx((12288 - 8192) / 8192.0));
    }

    SECTION("midi_any_note scalar variable") {
        // No notes initially
        EventBus::instance().process_frame();
        double no_note = engine.evaluate("midi_any_note");
        REQUIRE(no_note == Catch::Approx(0.0));

        // Add a note
        Event event;
        event.type = Event::Type::MIDI_NOTE_ON;
        event.channel = 1;
        event.data1 = 60;
        event.data2 = 100;
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        double has_note = engine.evaluate("midi_any_note");
        REQUIRE(has_note == Catch::Approx(1.0));
    }

    SECTION("Array index can be an expression") {
        // Set up multiple CC values
        for (int cc = 0; cc < 5; cc++) {
            Event event;
            event.type = Event::Type::MIDI_CC;
            event.channel = 1;
            event.data1 = cc;
            event.data2 = cc * 25;  // 0, 25, 50, 75, 100
            event.timestamp = 0.0;
            EventBus::instance().push_event(event);
        }
        EventBus::instance().process_frame();

        // Use expression as index
        engine.set_variable("i", 3.0);
        double result = engine.evaluate("midi_cc[i]");
        REQUIRE(result == Catch::Approx(75.0 / 127.0));

        // Arithmetic in index
        result = engine.evaluate("midi_cc[1 + 2]");
        REQUIRE(result == Catch::Approx(75.0 / 127.0));
    }

    SECTION("Out of bounds access returns 0") {
        double result = engine.evaluate("midi_cc[200]");
        REQUIRE(result == Catch::Approx(0.0));

        result = engine.evaluate("midi_note[-1]");
        REQUIRE(result == Catch::Approx(0.0));

        result = engine.evaluate("midi_note_index[100]");
        REQUIRE(result == Catch::Approx(0.0));
    }
}

TEST_CASE("MIDI compiled script execution", "[midi]") {
    EventBus::instance().reset();
    ScriptEngine engine;

    SECTION("Compiled script can access MIDI arrays") {
        // Set up CC value
        Event event;
        event.type = Event::Type::MIDI_CC;
        event.channel = 1;
        event.data1 = 1;      // Modwheel
        event.data2 = 100;
        event.timestamp = 0.0;

        EventBus::instance().push_event(event);
        EventBus::instance().process_frame();

        // Compile and execute
        auto script = engine.compile("x = midi_cc[1] * 2");
        engine.execute(script);

        double x = engine.get_variable("x");
        REQUIRE(x == Catch::Approx((100.0 / 127.0) * 2));
    }
}
