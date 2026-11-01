# Testing Haunted Hall

Thank you for testing! Haunted Hall is a 4-layer chip-synth groovebox for KICK:
a drum cartridge and three synth layers, a 4-channel line mixer and a master
delay, with three screens (MASTER, STEP, CARTRIDGE BROWSER).

There are two ways to test it before it's hooked into KICK:

1. **The engine tests** (no JUCE needed): sound engine, step grid, timing, real-time safety.
2. **The standalone app** (needs JUCE 7+): the real engine and the real screens in a
   1024 x 600 desktop window, playable from the computer keyboard.

## 1. Engine tests

```
cmake -B build
cmake --build build --config Release
./build/hh_tests        (Windows: build\Release\hh_tests.exe)
./build/hh_render       writes WAV demos to renders/
```

Expect `25 passed, 0 failed` and a benchmark line. Please send the benchmark
line with your CPU model; numbers from ARM machines (Raspberry Pi, Apple
silicon) are especially useful, because KICK is ARM.

## 2. Standalone app

```
git clone https://github.com/juce-framework/JUCE.git     (any JUCE 7 or 8)
cmake -B build -DHH_JUCE_DIR=/path/to/JUCE
cmake --build build --config Release --target HauntedHallStandalone
```

**Heads-up:** the screen code has been compile-checked against a stand-in for
the JUCE API, not against JUCE itself (the author's build machine has no JUCE
yet). If the first build stops on a JUCE signature, please send the exact
error lines; fixes are expected to be small.

### Keys

| Key | Does |
|---|---|
| SPACE | play / stop |
| UP / DOWN | tempo +1 / -1 BPM |
| Z X C V B | drum rows: kick, snare, hat, ghost, FX |
| A W S E D F T G Y H U J K | notes C to C on the keyboard layer |
| 1 2 3 | keyboard plays Synth A / B / C (opening a synth's browser also selects it) |
| - = | octave down / up |
| F2 | performance overlay: screen paint time and audio load |

### Screens

- **MASTER**: tap ON / MUTE; drag the faders; tap BYPASS; tap the delay time
  display to cycle free time and tempo-synced divisions; drag the four knobs
  (TIME, FEEDBACK, MIX, LEVEL); tap a channel's icon or name to open its
  cartridge browser; tap DRUM to go to the step screen.
- **STEP**: tap a cell for a step, long-press for normal > accent > soft > off;
  bar slots and "< BAR 01/08 >" page through bars (while playing, the view follows
  the playhead unless you paged or edited in the last 4 seconds); 8 BARS / 16 BARS; A / B
  (switches at the end of the pattern while playing); padlocks lock a row from
  RANDOM; tap a row name to hear it; RANDOM (long-press: next groove style),
  CLEAR / COPY / PASTE (long-press: the whole pattern), UNDO (long-press: REDO);
  tap the "HAUNTED HALL - STEP MODE" banner to go back to MASTER.
- **BROWSER**: tabs pick the category; each cartridge picture is a bank (arrows
  and carousel); tapping a preset loads it immediately so you can play it;
  LOAD PRESET keeps it, BACK puts the old one back; the die picks a random
  preset; the star marks favourites.

## What to check

**Sound**
- [ ] Each layer plays; ON off silences a layer, MUTE mutes it
- [ ] SPOOK on each layer goes from clean to worn to broken (stutters only near the top)
- [ ] Drum SPACE never gives more than a light echo (the 20 % cap)
- [ ] Delay TIME glides when turned; tempo-synced times follow BPM changes
- [ ] No clicks when tapping steps, switching presets, or turning ON / MUTE

**Timing**
- [ ] The playhead moves with the beat; nothing drifts over a few minutes
- [ ] Pattern A / B switches exactly at the end of the pattern
- [ ] 8 > 16 bars doubles the pattern; going back to 8 and to 16 keeps your edits

**Screens**
- [ ] Every control does what its label says, in the right place
- [ ] Knobs, faders and meters follow smoothly
- [ ] F2 overlay: paint time stays low (a few ms at most) while playing

**Stress**
- [ ] Hammer RANDOM, UNDO and taps while playing at a fast tempo: audio stays clean
- [ ] All four layers at SPOOK 1.0, delay feedback high: no crackles, no runaway volume

## Reporting

Please open an issue (or reply in the thread) with:

```
What I did:
What I expected:
What happened:
Build: engine tests / standalone, OS, CPU, JUCE version
Benchmark line (if relevant):
F2 overlay numbers (if relevant):
```

Audio clips or a short screen recording help a lot.
