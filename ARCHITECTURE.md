# Haunted Hall engine: architecture

## Design principles

- **Sound only, host-agnostic.** Plain C++17, no JUCE or KICK headers. A host
  sends notes, tempo and parameter values; the engine returns stereo audio.
- **Real-time safe.** No locks, no allocation and no denormal slow paths on the
  audio thread. All memory is allocated in `prepare()`.
- **Deterministic.** Same notes and settings, same audio, bit for bit. The
  random parts (corrosion events, generators) use seeded generators.
- **Cheap when quiet.** Layers that are off aren't processed; silent layers skip
  their effects; the whole engine sleeps when nothing is sounding.

## Files (`engine/`, namespace `mt::hauntedhall`)

| File | Contents |
|---|---|
| `HHEngine.h/.cpp` | The engine: layers, mixer, delay, limiter, sequencer clock, parameters, state, offline export |
| `HHSynth.h/.cpp` | `ChipOsc`, `SynthVoice`, 4-voice `SynthLayer`, the `SynthPatch` preset format |
| `HHDrums.h/.cpp` | `DrumKit`: kick, snare, hat, ghost, FX, and the `KitPatch` format |
| `HHCorrosion.h/.cpp` | SPOOK: wobble, rate drop, bit drop, stutters, dropouts |
| `HHDelay.h/.cpp` | The master digital delay |
| `HHPattern.h/.cpp` | Patterns A/B, the generators (with row locks), export to notes |
| `HHGrid.h/.cpp` | `PatternEditor`: everything the step screen does (taps, bars, clear/copy/paste, RANDOM with locks, lane, undo/redo) |
| `HHPresets.h/.cpp` | Preset tables with category and cartridge art |
| `HHCommon.h` | Shared DSP and real-time helpers |

## Signal flow

```
Drum kit ─ drive ─ corrosion ─┐
Synth A  ─ voices ─ corrosion ┤  per channel: fader x headroom (-5 dB), pan,
Synth B  ─ voices ─ corrosion ┤  post-fader delay send (drums capped at 20 %)
Synth C  ─ voices ─ corrosion ┘
        dry bus ──────────────────────────┐
        send bus ─ digital delay ─ x MIX ─┴─ x LEVEL ─ DC block ─ limiter ─ soft clip (-0.3 dBFS)
```

Each synth voice: oscillator A + oscillator B (mix, octave, detune) → drive →
state-variable low-pass (cutoff follows pitch and the envelope) → ADSR. Pitch
movement: glide, a start-of-note sweep, delayed vibrato, pulse-width LFO, and a
chip-style "fake chord" arpeggio when a chord hits an arp patch.

## The four knobs

On every layer page, KICK's four hardware knobs are the same four controls:

| Knob | Drum layer | Synth layers |
|---|---|---|
| DECAY | all drum decays, 0.35x to 2.8x | decay and release, 1/4x to 4x |
| TONE | brightness and a small pitch lift | filter cutoff, -2 to +2 octaves |
| SPOOK | corrosion, added to the kit's own wear | corrosion, added to the preset's own wear |
| SPACE | delay send, 0 to 20 % | delay send, 0 to 100 % |

On the master page they are TIME, FEEDBACK, MIX and LEVEL.

SPOOK stages fade in at different points: wobble and bit loss first, then
sample-rate drop, and only in the top third the random stutters and dropouts
of a cartridge that's failing.

## Patterns and the host clock

The host passes `Transport {playing, bpm, beats}` each block. The engine finds
each 16th-note boundary inside the block and plays the step on its exact
sample. On transport start, the pattern starts at the first step at or after
the play position. A requested pattern (A/B) takes over at the end of the
current pattern. When the transport stops, sequenced notes release; notes the
host holds keep sounding until their noteOff.

Two ways to sequence, both supported:

1. **Haunted Hall's own grid** (above), synced to KICK's clock.
2. **KICK's sequencer owns the notes**: `exportNotes()` turns a pattern into a
   note list (drums on GM notes 36/38/42/45/39, the lane on its own track); the
   host plays them back with `noteOn()`/`noteOff()`.

Grid cells are atomics, so the screen and the generators can edit while the
pattern plays.

## Threading

| Call | Thread |
|---|---|
| `prepare()` | before audio (allocates) |
| `setParam`, `loadPreset`, `requestPattern`, generators, `PatternEditor`, `captureState`, `loadState` | UI |
| `trigger`, `release` | any, including several at once (lock-free queue, 64 slots) |
| `noteOn`, `noteOff`, `process` | audio only |
| `currentStep`, `playingPattern`, `channelPeak` | any (for the screens) |
| `renderPatternOffline` | worker thread, with its own second `Engine` |

## Export to audio (for cratedust 12)

`captureState()` copies the live settings and patterns; `renderPatternOffline()`
plays them once through a separate engine at any tempo, plus a delay tail. The
test suite checks that the export is sample-for-sample identical to live
playback. Writing the file into KICK's sample storage, and whether cratedust 12
can pick it up there, is part of the KICK hookup.

## Numbers (v0.1)

| | |
|---|---|
| Memory | about 1.2 MB at 48 kHz (1 MB delay line, 128 KB corrosion history, 19 KB engine) |
| CPU, full project (drums + 3 layers + delay) | 42-45 us per 128-sample block on a desktop x86 core: about 1.6 % of one core |
| CPU, worst case (pads on all layers, SPOOK 1.0 everywhere) | 62-69 us per block: about 2.4 % |
| Idle | 0.06 us per block |
| Allocations in the audio callback | 0 (tested over 3,000 callbacks, and while the grid is edited during playback) |

The ARM figure on KICK is not measured yet; expect several times the desktop
cost per block. Plenty of room, but it needs measuring on the device.

## Adding presets

Presets are plain structs built in `HHPresets.cpp`: start from a default
`SynthPatch` or `KitPatch`, set the fields that matter, give it a name, a
category (Drum, Bass, Lead, Pad) and a cartridge picture.
