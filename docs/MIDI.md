# MIDI/EventBus Support for AVS

## Overview

Add MIDI support to avs_lib with a generalized EventBus architecture that can later support OSC and other event protocols. Scripts get direct array access to MIDI data for maximum performance.

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Event system | Singleton EventBus | No render() signature changes, zero overhead for effects that don't use MIDI |
| Array syntax | Virtual built-in arrays | MIDI arrays are read-only built-ins, not general array support |
| Data flow | EventBus singleton | UI pushes events, scripts query state - like AudioData but via singleton |
| Performance | Compile-time resolution | Array names resolved at parse time, runtime is array indexing |
| Thread safety | SPSC ring buffer | Lock-free, single-producer (UI) single-consumer (render), no mutex needed |
| MIDI channels | Global (all merged) | Future: per-effect channel selection, explicit channel access |

## Script API

```cpp
// MIDI CC values (0.0-1.0 normalized)
midi_cc[0..127]

// Note velocities (0.0 = off, 0.0-1.0 = velocity)
midi_note[0..127]

// Active note iteration
midi_note_count      // Number of currently held notes
midi_note_index[i]   // Note number at position i in active list

// Additional scalars
midi_pitch_bend      // -1.0 to 1.0
midi_any_note        // 1.0 if any note triggered this frame
```

### Example Usage

```
// SuperScope responding to MIDI modwheel
init: n=100
frame: r = midi_cc[1] * $PI * 2
point:
  vel = midi_note[60 + (i % 12)];
  x = cos(r + i*$PI*2) * vel;
  y = sin(r + i*$PI*2) * vel
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  ofxAVS (UI layer)                                  │
│  - ofxMidi / RtMidi for MIDI I/O                    │
│  - MIDI file playback                               │
│  - Converts messages to avs::Event                  │
└──────────────────────┬──────────────────────────────┘
                       │ push_event()
                       ▼
┌─────────────────────────────────────────────────────┐
│  avs::EventBus (singleton in avs_lib)               │
│                                                     │
│  push_event(event)     // UI → avs_lib              │
│  process_frame()       // Aggregate events          │
│  get_midi_data()       // Scripts query this        │
│  emit_event(event)     // avs_lib → UI (future)     │
└─────────────────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────┐
│  Scripts access via ArrayAccessNode                 │
│  midi_cc[n] → EventBus::instance().get_midi_data()  │
└─────────────────────────────────────────────────────┘
```

## Implementation Phases

### Phase 1: Core Data Structures

**New file: `libs/avs_lib/core/event_bus.h`**

```cpp
namespace avs {

struct MidiData {
    double cc[128] = {0.0};           // CC values 0.0-1.0
    double note[128] = {0.0};         // Note velocities
    int note_index[128] = {0};        // Active note numbers
    int note_count = 0;               // Number of active notes
    double pitch_bend = 0.0;          // -1.0 to 1.0
    bool any_note_on = false;         // Note triggered this frame
};

struct Event {
    enum class Type { MIDI_NOTE_ON, MIDI_NOTE_OFF, MIDI_CC, MIDI_PITCH_BEND };
    Type type;
    int channel;
    int data1;      // Note/CC number
    int data2;      // Velocity/value
    double timestamp;
};

// Simple SPSC (single-producer single-consumer) ring buffer
template<typename T, size_t N>
class SPSCQueue {
    std::array<T, N> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
public:
    bool push(const T& item);  // Returns false if full
    bool pop(T& item);         // Returns false if empty
};

class EventBus {
public:
    static EventBus& instance();

    void push_event(const Event& e);      // Called by UI thread (lock-free)
    void process_frame();                  // Called by Renderer before effects
    const MidiData& get_midi_data() const; // Called by scripts
    void emit_event(const Event& e);       // For bidirectional (future)

private:
    SPSCQueue<Event, 256> pending_;       // Lock-free ring buffer
    MidiData midi_data_;
};

} // namespace avs
```

### Phase 2: Array Syntax in ScriptEngine

**Modify: `libs/avs_lib/core/script/lexer.h`**

Add tokens:
```cpp
LBRACKET,    // [
RBRACKET,    // ]
```

**Modify: `libs/avs_lib/core/script/parser.h`**

Add new AST node:
```cpp
struct ArrayAccessNode : public ASTNode {
    std::string array_name;       // "midi_cc", "midi_note", "midi_note_index"
    std::unique_ptr<ASTNode> index;

    double evaluate_slots(double* vars) const override {
        int idx = static_cast<int>(index->evaluate_slots(vars));
        const auto& midi = EventBus::instance().get_midi_data();

        if (array_name == "midi_cc" && idx >= 0 && idx < 128)
            return midi.cc[idx];
        if (array_name == "midi_note" && idx >= 0 && idx < 128)
            return midi.note[idx];
        if (array_name == "midi_note_index" && idx >= 0 && idx < midi.note_count)
            return static_cast<double>(midi.note_index[idx]);
        return 0.0;
    }
};
```

**Modify: `libs/avs_lib/core/script/parser.cpp`**

In `parse_factor()`, after IDENTIFIER token:
```cpp
if (lexer.peek_token().type == TokenType::LBRACKET) {
    // Array access: midi_cc[expr]
    lexer.next_token();  // consume '['
    auto idx = parse_expression();
    expect(TokenType::RBRACKET);
    auto node = std::make_unique<ArrayAccessNode>();
    node->array_name = token.value;
    node->index = std::move(idx);
    return node;
}
```

### Phase 3: Scalar MIDI Variables

**Modify: `libs/avs_lib/core/script/script_engine.cpp`**

In `populate_slots()`, add MIDI scalars:
```cpp
} else if (name == "midi_note_count") {
    slots[i] = static_cast<double>(EventBus::instance().get_midi_data().note_count);
} else if (name == "midi_any_note") {
    slots[i] = EventBus::instance().get_midi_data().any_note_on ? 1.0 : 0.0;
} else if (name == "midi_pitch_bend") {
    slots[i] = EventBus::instance().get_midi_data().pitch_bend;
}
```

### Phase 4: Renderer Integration

**Modify: `libs/avs_lib/core/renderer.h`**

In `render()`, before effect chain:
```cpp
void render(AudioData visdata, bool is_beat, PixelType* output_buffer) {
    EventBus::instance().process_frame();  // Process pending MIDI events
    // ... existing render code ...
}
```

### Phase 5: ofxAVS Integration

**Modify: `src/ofxAVS.h`**

Add MIDI input:
```cpp
#include "ofxMidi.h"

class ofxAVS : public ofBaseSoundInput, public ofxMidiListener {
    ofxMidiIn midi_in_;

    void newMidiMessage(ofxMidiMessage& msg) override;
    void setupMidi();
    void drawMidiUI();  // Device selector
};
```

**Modify: `src/ofxAVS.cpp`**

```cpp
void ofxAVS::newMidiMessage(ofxMidiMessage& msg) {
    avs::Event event;
    event.timestamp = ofGetElapsedTimef();
    event.channel = msg.channel;

    switch (msg.status) {
        case MIDI_NOTE_ON:
            event.type = avs::Event::Type::MIDI_NOTE_ON;
            event.data1 = msg.pitch;
            event.data2 = msg.velocity;
            break;
        case MIDI_NOTE_OFF:
            event.type = avs::Event::Type::MIDI_NOTE_OFF;
            event.data1 = msg.pitch;
            event.data2 = 0;
            break;
        case MIDI_CONTROL_CHANGE:
            event.type = avs::Event::Type::MIDI_CC;
            event.data1 = msg.control;
            event.data2 = msg.value;
            break;
        case MIDI_PITCH_BEND:
            event.type = avs::Event::Type::MIDI_PITCH_BEND;
            event.data1 = msg.value;  // 14-bit value
            break;
        default:
            return;
    }

    avs::EventBus::instance().push_event(event);
}
```

### Phase 6: MIDI File Playback (Future)

- Add ofxMidiFile or similar for SMF playback
- Sync with audio file playback
- Push events with timestamps, process in sync with audio

## Files Summary

| File | Action | Purpose |
|------|--------|---------|
| `libs/avs_lib/core/event_bus.h` | CREATE | EventBus singleton, MidiData, Event structs |
| `libs/avs_lib/core/event_bus.cpp` | CREATE | EventBus implementation |
| `libs/avs_lib/core/script/lexer.h` | MODIFY | Add LBRACKET, RBRACKET tokens |
| `libs/avs_lib/core/script/lexer.cpp` | MODIFY | Lex '[' and ']' |
| `libs/avs_lib/core/script/parser.h` | MODIFY | Add ArrayAccessNode |
| `libs/avs_lib/core/script/parser.cpp` | MODIFY | Parse array subscript syntax |
| `libs/avs_lib/core/script/script_engine.cpp` | MODIFY | Add MIDI scalar variables |
| `libs/avs_lib/core/renderer.h` | MODIFY | Call EventBus::process_frame() |
| `src/ofxAVS.h` | MODIFY | Add ofxMidiListener |
| `src/ofxAVS.cpp` | MODIFY | MIDI callback, UI for device selection |

## Performance Notes

1. **Array access** - `ArrayAccessNode::evaluate_slots()` does singleton lookup + array index. MidiData is <1KB, stays in L1 cache.

2. **No string parsing at runtime** - Array names resolved at compile time. `midi_cc[1]` compiles to direct EventBus access.

3. **Event queue** - Lock-free SPSC ring buffer. UI thread pushes, render thread consumes. No mutex contention, cache-friendly.

4. **Zero overhead** - Effects not using MIDI have zero cost. EventBus::process_frame() is O(n) where n = events this frame.

## Future Extensions

- **MIDI channel selection** - Per-effect function `midi_channel(n)` to filter to specific channel
- **Explicit channel access** - `midi_cc_ch(channel, cc)` or `midi_cc_ch[channel][cc]` syntax
- **OSC support** - Add `osc_register("/path")` → returns ID, then `osc_val(id)` for value
- **Event emission** - Effects can send MIDI/OSC out via `EventBus::emit_event()`
- **C++ effects** - Can access full Event data including strings/blobs not exposed to scripts
