# Haunted Hall Engine: Code Guide in Plain English

A walkthrough of how a program talks to the Haunted Hall engine: setting it
up, changing sounds and patterns, editing the step grid, and making sound in
real time. It's written for newer programmers, so it explains the "why" as
well as the "what".

> **This is an overview, not a complete program.** Names like `seed`, `bar`,
> `column`, `isPlaying` and the audio buffers are placeholders that a real
> program would declare. For a complete, runnable example, see
> [`tools/hh_render.cpp`](../tools/hh_render.cpp), which renders Haunted Hall
> to WAV files.

## The code

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

## First, the big idea: two threads

A music program does two jobs at the same time, on two separate **threads**
(think of them as two workers running side by side):

- **The UI thread** handles the screen, touches and knobs. If it takes a few
  extra milliseconds, nobody notices.
- **The audio thread** fills the speakers with sound, one small **block** of
  samples at a time (for example 128 samples, about 2.7 ms at 48 kHz). It has
  a hard deadline: if a block is late, you hear a click or a dropout.

So the audio thread must never wait for anything, load a file, or ask the
system for memory. Haunted Hall is built around that rule: everything slow
happens on the UI thread or in `prepare()`, and the two threads share data
through tools that never make either one wait.

Each section below says which thread its code belongs on.

## 1. Setup

```cpp
#include "HHEngine.h"
#include "HHGrid.h"
```
Brings in the engine and the step grid editor, so the compiler knows what
`Engine`, `PatternEditor` and the rest are.

```cpp
namespace hh = mt::hauntedhall;
```
Haunted Hall's code lives in the namespace `mt::hauntedhall`. This line
creates a short nickname, `hh`, so you can write `hh::Engine` instead of
`mt::hauntedhall::Engine`.

```cpp
hh::Engine engine;
engine.prepare(48000.0);
```
Creates the engine and prepares it to run at a **sample rate** of 48,000
samples per second (48 kHz, a common rate for audio devices). `prepare()`
sets aside all the memory the engine will ever need, about 1.2 MB, most of it
for the delay. That's why it must run **before audio starts** (or when the
sample rate changes), never on the audio thread.

## 2. Sounds and patterns (UI thread)

```cpp
engine.loadPreset(hh::Layer::B, hh::findSynthPreset("Haunted Choir"));
```
Loads the preset named "Haunted Choir" onto Synth B. `findSynthPreset`
looks the name up and returns its number in the preset list (or -1 if
there's no preset with that name). Haunted Hall has four layers: `Drum`,
`A`, `B` and `C`.

```cpp
engine.setParam(hh::Param::BSpook, 0.4f);
```
Turns Synth B's SPOOK control to 0.4 (40 % of its range). SPOOK wears the
sound down like an old, corroded game cartridge, adding to whatever wear the
preset already has. Parameters can be changed from any thread at any time.

```cpp
hh::generateDrums(engine.project(), 0, hh::GrooveStyle::CryptHop, seed);
```
Writes a drum groove in the "Crypt Hop" style into **pattern 0** (pattern A;
there are two patterns, A and B). The **seed** is a number you choose:
the same seed always gives the same groove, and a different seed gives a
different one. That makes results repeatable, which is handy for saving,
testing and undo.

```cpp
hh::generateLane(engine.project(), 0, hh::LaneStyle::Bassline, 57, hh::Scale::Dorian, seed);
```
Writes a bassline into pattern 0's **melody lane**, the one line of notes
that plays on a single synth layer (Synth A unless you choose another). The
57 is a **MIDI note** number (A3) that sets the key; a bassline plays two
octaves below it. `Scale::Dorian` picks which notes of the key it uses.

```cpp
engine.trigger(hh::Layer::Drum, 36, 0.9f);
```
Plays a drum hit right away, like tapping a pad: MIDI note 36 is the kick in
Haunted Hall's drum map. The 0.9 is the **velocity**, how hard the pad was
hit (0 to 1), which mostly affects loudness. `trigger` is safe from any
thread: it drops the note into a queue, and the audio thread plays it at the
start of its next block.

## 3. The step grid (UI thread)

```cpp
hh::PatternEditor grid(engine.project());
```
Creates the grid editor, everything the STEP screen does to a pattern. Each
edit is heard immediately, even while the pattern is playing, and is added
to an undo history (the last 32 edits).

```cpp
grid.toggleStep(0, bar, column, int(hh::DrumRow::Snare));
```
Turns the snare on or off at one step of pattern 0. `bar` counts from 0
(bar 1 is 0), and `column` is the step within the bar, 0 to 15. It returns
`true` if the step is now on.

```cpp
grid.toggleRowLock(int(hh::DrumRow::Kick));
```
Locks the kick row, so the RANDOM button leaves it exactly as it is.

```cpp
grid.randomize(0);
```
The RANDOM button: picks new seeds and rerolls pattern 0's unlocked drum
rows (here, everything except the kick). It also rerolls the melody lane,
unless the lane is locked too.

```cpp
grid.undo();
```
Takes back the last edit, here the reroll, so the pattern is exactly as it
was before. `grid.redo()` puts it back.

## 4. Making sound (audio thread, every block)

```cpp
hh::Transport t{isPlaying, bpm, beatPosition};
```
Tells the engine where the music is right now: whether it's playing, the
tempo in **BPM** (beats per minute), and the **beat position** at the start
of this block, counted in quarter notes. The engine uses this to play each
step of the pattern on exactly the right sample, in time with the rest of
the device.

```cpp
engine.noteOn(hh::Layer::C, 64, 0.8f, sampleOffset);
```
Starts a note on Synth C: MIDI note 64 (E4) with velocity 0.8. The
**sample offset** says how far into the coming block the note starts (0 is
the first sample), so notes land exactly in time rather than on block edges.
Call it on the audio thread, just before `process` for the same block. The
note keeps sounding until a matching `engine.noteOff(...)`.

```cpp
engine.process(left, right, numSamples, t);
```
Produces the next block of sound: plays any steps and notes that fall inside
it, runs every layer through the mixer and delay, and writes the result into
the `left` and `right` buffers.

**Important:** `process` **adds** its sound to whatever is already in the
buffers, so other sounds can share them. If you only want Haunted Hall,
**clear the buffers to zero first**, or you'll hear leftover noise from the
last block.

A typical audio callback looks like this:

```cpp
void audioCallback(float* left, float* right, int numSamples)
{
    std::fill(left, left + numSamples, 0.0f);     // start from silence
    std::fill(right, right + numSamples, 0.0f);

    hh::Transport t{isPlaying, bpm, beatPosition};
    engine.process(left, right, numSamples, t);

    if (isPlaying)                                // move the clock forward
        beatPosition += numSamples * bpm / 60.0 / sampleRate;
}
```

## Words worth knowing

| Term | Meaning |
|---|---|
| Sample rate | How many audio samples per second, e.g. 48,000 |
| Block | A small batch of samples the audio thread fills at once |
| MIDI note | A number for a pitch; 60 is middle C, each step is a semitone |
| Velocity | How hard a note is played, 0 to 1 |
| BPM | Tempo, in beats per minute |
| Seed | A number that makes "random" results repeatable |
| Thread | One line of work running alongside others |

## Where to look next

- [`README.md`](../README.md): what Haunted Hall is, and how to build it.
- [`ARCHITECTURE.md`](../ARCHITECTURE.md): the design in more depth,
  including the full threading rules.
- [`tools/hh_render.cpp`](../tools/hh_render.cpp): a complete program that
  uses all of the above and writes WAV files you can listen to.
- [`engine/HHEngine.h`](../engine/HHEngine.h): every engine call, with
  comments.
