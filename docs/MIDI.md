# MIDI Support in AVS

## Overview

MIDI support via a generalized EventBus architecture. Scripts access MIDI data through built-in arrays for maximum performance.

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Event system | Singleton EventBus | No render() signature changes, zero overhead for effects that don't use MIDI |
| Array syntax | Virtual built-in arrays | MIDI arrays are read-only built-ins, not general array support |
| Data flow | EventBus singleton | UI pushes events, scripts query state - like AudioData but via singleton |
| Performance | Compile-time resolution | Array names resolved at parse time, runtime is array indexing |
| Thread safety | SPSC ring buffer | Lock-free, single-producer (UI) single-consumer (render), no mutex needed |
| MIDI channels | Single channel filter | Input-level selection: Omni (all) or specific channel 1-16 |

## Channel Selection

AVS treats MIDI as a **single-channel instrument**. The user selects one channel (1-16) or "Omni" (all channels) at the input level, and all MIDI data flows through as a single merged stream.

**Rationale:**
- Visualizers typically respond to one controller/keyboard at a time
- Per-effect channel routing adds complexity rarely needed for visual feedback
- Original AVS had no MIDI support, so there's no compatibility constraint
- Simpler mental model: "my controller controls the visuals"

**Omni mode (default):** All 16 channels merged. Note/CC from any channel affects the same `midi_note[]` and `midi_cc[]` arrays.

**Single channel mode:** Filter to one channel. Useful when multiple devices are connected but only one should drive visuals.

## Script API

```
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

### Example

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
│  - MidiInput: live MIDI via ofxMidi                 │
│  - MidiFile: SMF playback synced to audio           │
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
└─────────────────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────┐
│  Scripts access via ArrayAccessNode                 │
│  midi_cc[n] → EventBus::instance().get_midi_data()  │
└─────────────────────────────────────────────────────┘
```

## Implementation

### avs_lib Core (`libs/avs_lib/core/`)

**event_bus.h/cpp** - EventBus singleton with:
- `MidiData` struct: CC values, note velocities, active note tracking
- `Event` struct: type, channel, data1/data2, timestamp
- `SPSCQueue<T,N>`: lock-free ring buffer for thread-safe event passing
- `process_frame()`: called by Renderer before effects run

**script/parser.h** - `ArrayAccessNode` for `midi_cc[n]` syntax:
- Resolves array name at parse time
- Runtime: index evaluation + direct MidiData access

**script/script_engine.cpp** - MIDI scalar variables:
- `midi_note_count`, `midi_any_note`, `midi_pitch_bend`
- Synced from EventBus at execute time

**renderer.cpp** - Calls `EventBus::process_frame()` before effect chain

### ofxAVS UI Layer (`src/`)

**MidiInput.h/cpp** - Live MIDI input:
- Device enumeration and selection
- Channel filter (Omni / 1-16)
- Debug log with scrolling message history
- Routes messages to EventBus

**MidiFile.h/cpp** - MIDI file playback:
- Standard MIDI File (SMF) parser
- Synced to audio file position
- Events pushed to EventBus with timing

**ofxAVS.cpp** - UI integration:
- Input panel with device/channel dropdowns
- Debug button opens message log window
- Settings persisted to `audio_settings.json`

## Files

| File | Purpose |
|------|---------|
| `libs/avs_lib/core/event_bus.h` | EventBus singleton, MidiData, Event, SPSCQueue |
| `libs/avs_lib/core/event_bus.cpp` | EventBus implementation |
| `libs/avs_lib/core/script/parser.h` | ArrayAccessNode for midi_cc[n] syntax |
| `libs/avs_lib/core/script/script_engine.cpp` | MIDI scalar variables |
| `src/MidiInput.h/cpp` | Live MIDI input handler |
| `src/MidiFile.h/cpp` | MIDI file parser and playback |
| `src/ofxAVS.cpp` | UI, debug window, persistence |

## Performance

- **Array access**: Singleton lookup + array index. MidiData is <1KB, stays in L1 cache.
- **No string parsing at runtime**: Array names resolved at compile time.
- **Event queue**: Lock-free SPSC ring buffer. No mutex contention.
- **Zero overhead**: Effects not using MIDI have zero cost.

## Future Extensions

- **OSC support** - `osc_register("/path")` → ID, then `osc_val(id)` for value
- **Event emission** - Effects send MIDI/OSC out via `EventBus::emit_event()`
