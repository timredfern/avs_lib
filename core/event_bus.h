// avs_lib - Portable Advanced Visualization Studio library
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace avs {

// MIDI state data accessible by scripts
struct MidiData {
    double cc[128] = {0.0};           // CC values 0.0-1.0 normalized
    double note[128] = {0.0};         // Note velocities (0.0 = off, 0.0-1.0 = on)
    int note_index[128] = {0};        // Active note numbers for iteration
    int note_count = 0;               // Number of currently active notes
    double pitch_bend = 0.0;          // -1.0 to 1.0
    bool any_note_on = false;         // True if any note triggered this frame
};

// Event types for the EventBus
struct Event {
    enum class Type {
        MIDI_NOTE_ON,
        MIDI_NOTE_OFF,
        MIDI_CC,
        MIDI_PITCH_BEND
    };

    Type type;
    int channel;        // MIDI channel (currently ignored - all channels merged)
    int data1;          // Note number or CC number
    int data2;          // Velocity or CC value
    double timestamp;   // For future MIDI file sync
};

// Lock-free single-producer single-consumer ring buffer
// UI thread pushes events, render thread consumes
template<typename T, size_t N>
class SPSCQueue {
public:
    bool push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) % N;
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }
        item = buffer_[tail];
        tail_.store((tail + 1) % N, std::memory_order_release);
        return true;
    }

private:
    std::array<T, N> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

// Singleton event bus for MIDI/OSC events
// UI layer pushes events, scripts query state via get_midi_data()
class EventBus {
public:
    static EventBus& instance();

    // Called by UI thread (lock-free)
    void push_event(const Event& e);

    // Called by Renderer at start of each frame
    // Processes all pending events and updates midi_data_
    void process_frame();

    // Called by scripts to read current MIDI state
    const MidiData& get_midi_data() const { return midi_data_; }

    // For bidirectional communication (future use)
    // Effects can emit events to be sent out via MIDI/OSC
    void emit_event(const Event& e);

    // Clear all MIDI state (e.g., when switching presets)
    void reset();

private:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    SPSCQueue<Event, 256> pending_;    // Incoming events from UI
    SPSCQueue<Event, 256> outgoing_;   // Outgoing events from effects (future)
    MidiData midi_data_;
};

} // namespace avs
