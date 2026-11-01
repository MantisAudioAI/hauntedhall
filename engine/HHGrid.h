// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: the step grid editor. Everything the DRUM / STEP screen does
// to a pattern goes through here: tapping steps, bar pages, clear / copy /
// paste, 8 or 16 bars, the RANDOM button with locked rows, the melody lane,
// and undo / redo.
//
// UI thread only. Edits write the pattern's atomic cells, so they are heard
// immediately while the pattern plays, with no locks and no audio glitches.
#pragma once

#include "HHPattern.h"

namespace mt::hauntedhall {

class PatternEditor
{
public:
    static constexpr int kUndoDepth = 32;

    // Velocities used by the grid.
    static constexpr uint8_t kVelSoft = 64, kVelNormal = 100, kVelAccent = 127;

    // What the RANDOM button uses. Change these, then regenerate() or randomize().
    struct Generator
    {
        GrooveStyle style = GrooveStyle::CryptHop;
        float density = 0.5f;        // 0 = sparse, 0.5 = as written, 1 = busy
        uint8_t lockedRows = 0;      // bit r set = RANDOM leaves row r alone
        uint32_t drumSeed = 1;
        LaneStyle lane = LaneStyle::Bassline;
        Scale scale = Scale::Minor;
        uint8_t root = 57;           // A3
        bool laneLocked = false;     // true = RANDOM leaves the melody lane alone
        uint32_t laneSeed = 1;
    };

    explicit PatternEditor(Project& project) : p_(project) {}

    // ---- reading (for drawing the grid) ----
    int bars(int pattern) const;
    static int stepIndex(int bar, int column) { return bar * kStepsPerBar + column; }
    uint8_t step(int pattern, int bar, int column, int row) const;
    MelodyStep laneStep(int pattern, int bar, int column) const;
    bool barHasNotes(int pattern, int bar) const;   // for dots on the bar-page buttons

    // ---- drum grid ----
    bool toggleStep(int pattern, int bar, int column, int row);    // returns true if now on
    void cycleStep(int pattern, int bar, int column, int row);     // off > normal > accent > soft > off
    void setStep(int pattern, int bar, int column, int row, uint8_t velocity);   // 0 = off
    void clearRow(int pattern, int row);
    void clearBar(int pattern, int bar);
    void clearPattern(int pattern);

    // ---- copy / paste (a bar is its drums and its melody lane) ----
    void copyBar(int pattern, int bar);
    bool pasteBar(int pattern, int bar);
    bool hasBarClip() const { return hasBarClip_; }
    void copyPattern(int pattern);
    bool pastePattern(int pattern);
    bool hasPatternClip() const { return hasPatternClip_; }

    // ---- length ----
    // Going 8 -> 16 copies bars 1-8 into 9-16 when 9-16 are empty, so the
    // pattern keeps playing the same; going 16 -> 8 hides bars 9-16 but keeps
    // them, so switching back brings them back.
    void setBars(int pattern, int bars);

    // ---- melody lane (one synth layer) ----
    void setLaneStep(int pattern, int bar, int column, const MelodyStep& m);
    void clearLane(int pattern);
    void transposeLane(int pattern, int semitones);

    // ---- generator ----
    Generator& generator() { return gen_; }
    void randomize(int pattern);    // RANDOM: new seeds, regenerate unlocked rows and the lane
    void regenerate(int pattern);   // same seeds (after changing style, density, key...)
    void toggleRowLock(int row) { gen_.lockedRows ^= uint8_t(1u << row); }
    bool rowLocked(int row) const { return (gen_.lockedRows >> row) & 1u; }
    uint8_t rowLocks() const { return gen_.lockedRows; }

    // ---- undo ----
    bool undo();
    bool redo();
    bool canUndo() const { return undoCount_ > 0; }
    bool canRedo() const { return redoCount_ > 0; }

private:
    struct Snapshot { int pattern = 0; PatternData data; };
    struct BarClip { uint8_t drums[kStepsPerBar][kDrumRows] = {}; uint32_t melody[kStepsPerBar] = {}; };

    void pushUndo(int pattern);
    void writeStep(int pattern, int bar, int column, int row, uint8_t v);
    static int clampPattern(int p) { return p < 0 ? 0 : (p >= kPatterns ? kPatterns - 1 : p); }
    static uint32_t nextSeed(uint32_t s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s ? s : 1u; }

    Project& p_;
    Generator gen_;

    Snapshot undo_[kUndoDepth];
    int undoHead_ = 0, undoCount_ = 0;
    Snapshot redo_[kUndoDepth];
    int redoCount_ = 0;

    BarClip barClip_;
    bool hasBarClip_ = false;
    PatternData patternClip_;
    bool hasPatternClip_ = false;
};

} // namespace mt::hauntedhall
