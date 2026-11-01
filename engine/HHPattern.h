// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: patterns, the one-click generators, and export to note lists.
//
// A project holds two patterns (A and B). Each pattern is 8 or 16 bars of
// 16 steps, with a 5-row drum grid and one melody/chord lane that plays on a
// single synth layer. Every cell is an atomic, so the touchscreen (or the
// generator) can edit while the audio thread plays, with no locks.
#pragma once

#include "HHCommon.h"
#include "HHDrums.h"

namespace mt::hauntedhall {

constexpr int kStepsPerBar = 16;
constexpr int kMaxBars = 16;
constexpr int kMaxSteps = kStepsPerBar * kMaxBars;
constexpr int kPatterns = 2;

// One melody-lane step: up to 3 notes (a chord), velocity and length.
struct MelodyStep
{
    uint8_t notes[3] = {0, 0, 0};   // 0 = unused
    uint8_t count = 0;              // 0 = empty step
    uint8_t velocity = 100;         // 1..127
    uint8_t length = 1;             // steps, 1..16

    uint32_t pack() const;
    static MelodyStep unpack(uint32_t v);
};

struct Pattern
{
    std::atomic<uint8_t> bars{8};                       // 8 or 16
    std::atomic<uint8_t> drums[kMaxSteps][kDrumRows];   // velocity 1..127, 0 = off
    std::atomic<uint32_t> melody[kMaxSteps];            // packed MelodyStep, 0 = empty

    Pattern() { clear(); }
    void clear();
    int steps() const { return int(bars.load(std::memory_order_relaxed)) * kStepsPerBar; }
};

// Plain copy of a pattern (for saving, undo, copy/paste and offline export).
struct PatternData
{
    uint8_t bars = 8;
    uint8_t drums[kMaxSteps][kDrumRows] = {};
    uint32_t melody[kMaxSteps] = {};
};

struct Project
{
    Pattern patterns[kPatterns];
    std::atomic<int> requested{0};   // pattern to play; switches at the next pattern start

    void copyTo(int pattern, PatternData& out) const;
    void copyFrom(int pattern, const PatternData& in);
};

// ---- one-click generators (UI thread) -------------------------------------
enum class GrooveStyle : uint8_t { CryptHop, GraveTrap, DungeonMarch, SpectralSynthwave, BossRush, Count };
enum class Scale : uint8_t { Minor, HarmonicMinor, Dorian, Phrygian, Count };
enum class LaneStyle : uint8_t { Bassline, Lead, ChordStabs, PadChords, Count };

const char* grooveStyleName(GrooveStyle s);
const char* scaleName(Scale s);
const char* laneStyleName(LaneStyle s);
float grooveTempo(GrooveStyle s);   // suggested BPM

constexpr uint8_t kAllRows = 0x1F;
// density 0..1 (0.5 = the style as written). Same seed, same pattern.
// rowMask: bit r set = row r is (re)generated; other rows are left as they are.
void generateDrums(Project& project, int pattern, GrooveStyle style, uint32_t seed, float density = 0.5f,
                   uint8_t rowMask = kAllRows);
// rootNote: the key's tonic as a MIDI note (e.g. 57 = A3); the lane picks its octave.
void generateLane(Project& project, int pattern, LaneStyle style, uint8_t rootNote, Scale scale, uint32_t seed);

// ---- export as notes for a host sequencer (UI thread) ---------------------
struct ExportNote
{
    double startBeats;    // quarter notes from the pattern start
    double lengthBeats;
    uint8_t track;        // 0 = drums (note = the row's MIDI note), 1 = melody lane
    uint8_t note;
    uint8_t velocity;
};
// Returns the number of notes written (at most maxNotes).
int exportNotes(const Project& project, int pattern, ExportNote* out, int maxNotes);

} // namespace mt::hauntedhall
