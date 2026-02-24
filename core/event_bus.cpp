// avs_lib - Portable Advanced Visualization Studio library
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "event_bus.h"
#include <algorithm>
#include <cstring>

namespace avs {

EventBus& EventBus::instance() {
    static EventBus instance;
    return instance;
}

void EventBus::push_event(const Event& e) {
    pending_.push(e);
}

void EventBus::process_frame() {
    // Reset per-frame flags
    midi_data_.any_note_on = false;

    // Process all pending events
    Event event;
    while (pending_.pop(event)) {
        switch (event.type) {
            case Event::Type::MIDI_NOTE_ON:
                if (event.data1 >= 0 && event.data1 < 128) {
                    // Normalize velocity to 0.0-1.0
                    midi_data_.note[event.data1] = event.data2 / 127.0;
                    midi_data_.any_note_on = true;

                    // Add to active note index if not already there
                    bool found = false;
                    for (int i = 0; i < midi_data_.note_count; i++) {
                        if (midi_data_.note_index[i] == event.data1) {
                            found = true;
                            break;
                        }
                    }
                    if (!found && midi_data_.note_count < 128) {
                        midi_data_.note_index[midi_data_.note_count++] = event.data1;
                    }
                }
                break;

            case Event::Type::MIDI_NOTE_OFF:
                if (event.data1 >= 0 && event.data1 < 128) {
                    midi_data_.note[event.data1] = 0.0;

                    // Remove from active note index
                    for (int i = 0; i < midi_data_.note_count; i++) {
                        if (midi_data_.note_index[i] == event.data1) {
                            // Shift remaining notes down
                            for (int j = i; j < midi_data_.note_count - 1; j++) {
                                midi_data_.note_index[j] = midi_data_.note_index[j + 1];
                            }
                            midi_data_.note_count--;
                            break;
                        }
                    }
                }
                break;

            case Event::Type::MIDI_CC:
                if (event.data1 >= 0 && event.data1 < 128) {
                    // Normalize CC value to 0.0-1.0
                    midi_data_.cc[event.data1] = event.data2 / 127.0;
                }
                break;

            case Event::Type::MIDI_PITCH_BEND:
                // Pitch bend is 14-bit (0-16383), center is 8192
                // Normalize to -1.0 to 1.0
                midi_data_.pitch_bend = (event.data1 - 8192) / 8192.0;
                break;
        }
    }
}

void EventBus::emit_event(const Event& e) {
    outgoing_.push(e);
}

void EventBus::reset() {
    // Clear all MIDI state
    std::memset(midi_data_.cc, 0, sizeof(midi_data_.cc));
    std::memset(midi_data_.note, 0, sizeof(midi_data_.note));
    std::memset(midi_data_.note_index, 0, sizeof(midi_data_.note_index));
    midi_data_.note_count = 0;
    midi_data_.pitch_bend = 0.0;
    midi_data_.any_note_on = false;

    // Drain any pending events
    Event event;
    while (pending_.pop(event)) {}
    while (outgoing_.pop(event)) {}
}

} // namespace avs
