# Haunted Hall

A 4-layer chip-synth groovebox engine for KICK: a 5-row drum cartridge and three
synth layers (A, B, C) through a 4-channel line mixer into a master digital
delay. 8-bit and 16-bit console-style sounds, with a SPOOK control that wears
the sound down like an old, corroded cartridge. Every sound is synthesized;
there are no samples.

Status: **v0.3**. Engine (standalone C++17, no JUCE), the three screens (JUCE),
art assets, and a standalone desktop test app. Not yet hooked into the KICK app;
see `KICK_INTEGRATION.md`. Testers: start with `TESTING.md`.

## Build (Windows, Visual Studio 2022)

1. File > Open > Folder, pick this folder. Visual Studio configures CMake.
2. Build All. Run `hh_tests` (expect 25 passed), then `hh_render` and listen
   to the WAVs in `renders/`.

Linux / macOS: `cmake -B build && cmake --build build && ./build/hh_tests`.

## What's in it

| | |
|---|---|
| Layers | Drum (kick, snare, hat, ghost, FX) + Synth A, B, C (4 voices each) |
| Oscillators | variable pulse, 4-bit stepped triangle, LFSR noise (hiss and metallic), 4-bit wave tables, band-limited saw, 2-operator FM |
| Per layer | the same four knobs everywhere: DECAY, TONE, SPOOK, SPACE |
| Mixer | LEVEL, ON (off = not processed at all), MUTE, fixed pan, delay send |
| Delay | TIME (20-1500 ms or tempo-synced), FEEDBACK, MIX, LEVEL, BYPASS with trails; dark 12-bit repeats |
| Drum delay | SPACE on the drum layer sends at most 20 % |
| Patterns | 2 per project (A/B), 8 or 16 bars x 16 steps, synced to the host clock; switch at the pattern end |
| Generators | one-click drums (5 styles, density, row locks), one melody/chord lane (bassline, lead, chord stabs, pad chords) in 4 scales |
| Step grid | tap / accent / soft steps, bar pages, 8 or 16 bars, clear / copy / paste, pattern A to B, transpose, 32-step undo and redo |
| Export | patterns as notes for KICK's sequencer, or as audio (offline render) for cratedust 12 |

Round-1 presets (approved by ear; tweaks to come): kits Crypt Kit, 8-Bit Kit, Grave
Trap; bass Dungeon Bass, Sub Runner, Ghost Bass; lead Crystal Lead, Dragon Horn,
Sword Tone, Hero Arp; pad Haunted Choir, Cathedral, Space Pad, Corrupted Save,
Night Stabs, Glowing Ambient.

## Using the engine

```cpp
#include "HHEngine.h"
#include "HHGrid.h"
namespace hh = mt::hauntedhall;

hh::Engine engine;
engine.prepare(48000.0);                       // allocates; before audio starts

// UI thread
engine.loadPreset(hh::Layer::B, hh::findSynthPreset("Haunted Choir"));
engine.setParam(hh::Param::BSpook, 0.4f);
hh::generateDrums(engine.project(), 0, hh::GrooveStyle::CryptHop, seed);
hh::generateLane(engine.project(), 0, hh::LaneStyle::Bassline, 57, hh::Scale::Dorian, seed);
engine.trigger(hh::Layer::Drum, 36, 0.9f);     // pad hit, any thread

hh::PatternEditor grid(engine.project());       // the step screen, UI thread
grid.toggleStep(0, bar, column, int(hh::DrumRow::Snare));
grid.toggleRowLock(int(hh::DrumRow::Kick));
grid.randomize(0);                              // RANDOM: kick stays, the rest rerolls
grid.undo();

// Audio thread, every block
hh::Transport t{isPlaying, bpm, beatPosition};
engine.noteOn(hh::Layer::C, 64, 0.8f, sampleOffset);   // sample-accurate
engine.process(left, right, numSamples, t);            // ADDS into the buffers
```

Folders: `engine/` (sound, patterns, step grid), `ui/` (screens), `standalone/`
(desktop test app), `assets/` (art), `art_tools/` (layout measurements and
preview renderer), `tools/` (tests, WAV renders).

See `ARCHITECTURE.md` for the design, threading rules and numbers, `SCREENS.md`
for how the screens are built to stay smooth on KICK, `ART_REQUESTS.md` for the
art to request, and `CREDITS.md`.

## License

AGPL-3.0-or-later, like the KICK software.
