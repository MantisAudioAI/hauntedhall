// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHGrid.h"

#include <cstring>

namespace mt::hauntedhall {

namespace {
inline bool validCell(int bar, int column, int row)
{
    return bar >= 0 && bar < kMaxBars && column >= 0 && column < kStepsPerBar && row >= 0 && row < kDrumRows;
}
} // namespace

// ---- reading ----------------------------------------------------------------
int PatternEditor::bars(int pattern) const
{
    return p_.patterns[clampPattern(pattern)].bars.load(std::memory_order_relaxed);
}

uint8_t PatternEditor::step(int pattern, int bar, int column, int row) const
{
    if (!validCell(bar, column, row)) return 0;
    return p_.patterns[clampPattern(pattern)].drums[stepIndex(bar, column)][row].load(std::memory_order_relaxed);
}

MelodyStep PatternEditor::laneStep(int pattern, int bar, int column) const
{
    if (!validCell(bar, column, 0)) return {};
    return MelodyStep::unpack(p_.patterns[clampPattern(pattern)].melody[stepIndex(bar, column)].load(std::memory_order_relaxed));
}

bool PatternEditor::barHasNotes(int pattern, int bar) const
{
    const Pattern& pt = p_.patterns[clampPattern(pattern)];
    for (int c = 0; c < kStepsPerBar; ++c)
    {
        const int s = stepIndex(bar, c);
        if (pt.melody[s].load(std::memory_order_relaxed)) return true;
        for (int r = 0; r < kDrumRows; ++r)
            if (pt.drums[s][r].load(std::memory_order_relaxed)) return true;
    }
    return false;
}

// ---- undo -------------------------------------------------------------------
void PatternEditor::pushUndo(int pattern)
{
    Snapshot& s = undo_[undoHead_];
    s.pattern = clampPattern(pattern);
    p_.copyTo(s.pattern, s.data);
    undoHead_ = (undoHead_ + 1) % kUndoDepth;
    undoCount_ = std::min(undoCount_ + 1, kUndoDepth);
    redoCount_ = 0;   // a new edit ends the redo chain
}

bool PatternEditor::undo()
{
    if (undoCount_ == 0) return false;
    undoHead_ = (undoHead_ + kUndoDepth - 1) % kUndoDepth;
    --undoCount_;
    const Snapshot& s = undo_[undoHead_];
    if (redoCount_ < kUndoDepth)
    {
        redo_[redoCount_].pattern = s.pattern;
        p_.copyTo(s.pattern, redo_[redoCount_].data);
        ++redoCount_;
    }
    p_.copyFrom(s.pattern, s.data);
    return true;
}

bool PatternEditor::redo()
{
    if (redoCount_ == 0) return false;
    const Snapshot r = redo_[--redoCount_];
    // Save the current state for undo without clearing the rest of the redo chain.
    Snapshot& u = undo_[undoHead_];
    u.pattern = r.pattern;
    p_.copyTo(r.pattern, u.data);
    undoHead_ = (undoHead_ + 1) % kUndoDepth;
    undoCount_ = std::min(undoCount_ + 1, kUndoDepth);
    p_.copyFrom(r.pattern, r.data);
    return true;
}

// ---- drum grid --------------------------------------------------------------
void PatternEditor::writeStep(int pattern, int bar, int column, int row, uint8_t v)
{
    p_.patterns[clampPattern(pattern)].drums[stepIndex(bar, column)][row].store(v, std::memory_order_relaxed);
}

bool PatternEditor::toggleStep(int pattern, int bar, int column, int row)
{
    if (!validCell(bar, column, row)) return false;
    pushUndo(pattern);
    const bool on = step(pattern, bar, column, row) == 0;
    writeStep(pattern, bar, column, row, on ? kVelNormal : 0);
    return on;
}

void PatternEditor::cycleStep(int pattern, int bar, int column, int row)
{
    if (!validCell(bar, column, row)) return;
    pushUndo(pattern);
    const uint8_t v = step(pattern, bar, column, row);
    uint8_t next = kVelNormal;
    if (v == 0) next = kVelNormal;
    else if (v < kVelSoft + 10) next = 0;            // soft -> off
    else if (v >= kVelAccent - 10) next = kVelSoft;  // accent -> soft
    else next = kVelAccent;                          // normal -> accent
    writeStep(pattern, bar, column, row, next);
}

void PatternEditor::setStep(int pattern, int bar, int column, int row, uint8_t velocity)
{
    if (!validCell(bar, column, row)) return;
    pushUndo(pattern);
    writeStep(pattern, bar, column, row, std::min<uint8_t>(velocity, 127));
}

void PatternEditor::clearRow(int pattern, int row)
{
    if (row < 0 || row >= kDrumRows) return;
    pushUndo(pattern);
    for (int s = 0; s < kMaxSteps; ++s) p_.patterns[clampPattern(pattern)].drums[s][row].store(0, std::memory_order_relaxed);
}

void PatternEditor::clearBar(int pattern, int bar)
{
    if (bar < 0 || bar >= kMaxBars) return;
    pushUndo(pattern);
    Pattern& pt = p_.patterns[clampPattern(pattern)];
    for (int c = 0; c < kStepsPerBar; ++c)
    {
        const int s = stepIndex(bar, c);
        for (int r = 0; r < kDrumRows; ++r) pt.drums[s][r].store(0, std::memory_order_relaxed);
        pt.melody[s].store(0, std::memory_order_relaxed);
    }
}

void PatternEditor::clearPattern(int pattern)
{
    pushUndo(pattern);
    p_.patterns[clampPattern(pattern)].clear();
}

// ---- copy / paste -----------------------------------------------------------
void PatternEditor::copyBar(int pattern, int bar)
{
    if (bar < 0 || bar >= kMaxBars) return;
    const Pattern& pt = p_.patterns[clampPattern(pattern)];
    for (int c = 0; c < kStepsPerBar; ++c)
    {
        const int s = stepIndex(bar, c);
        for (int r = 0; r < kDrumRows; ++r) barClip_.drums[c][r] = pt.drums[s][r].load(std::memory_order_relaxed);
        barClip_.melody[c] = pt.melody[s].load(std::memory_order_relaxed);
    }
    hasBarClip_ = true;
}

bool PatternEditor::pasteBar(int pattern, int bar)
{
    if (!hasBarClip_ || bar < 0 || bar >= kMaxBars) return false;
    pushUndo(pattern);
    Pattern& pt = p_.patterns[clampPattern(pattern)];
    for (int c = 0; c < kStepsPerBar; ++c)
    {
        const int s = stepIndex(bar, c);
        for (int r = 0; r < kDrumRows; ++r) pt.drums[s][r].store(barClip_.drums[c][r], std::memory_order_relaxed);
        pt.melody[s].store(barClip_.melody[c], std::memory_order_relaxed);
    }
    return true;
}

void PatternEditor::copyPattern(int pattern)
{
    p_.copyTo(clampPattern(pattern), patternClip_);
    hasPatternClip_ = true;
}

bool PatternEditor::pastePattern(int pattern)
{
    if (!hasPatternClip_) return false;
    pushUndo(pattern);
    p_.copyFrom(clampPattern(pattern), patternClip_);
    return true;
}

// ---- length -----------------------------------------------------------------
void PatternEditor::setBars(int pattern, int newBars)
{
    newBars = newBars >= 16 ? 16 : 8;
    Pattern& pt = p_.patterns[clampPattern(pattern)];
    const int old = pt.bars.load(std::memory_order_relaxed);
    if (old == newBars) return;
    pushUndo(pattern);
    if (newBars == 16)
    {
        bool secondHalfEmpty = true;
        for (int b = 8; b < 16 && secondHalfEmpty; ++b) secondHalfEmpty = !barHasNotes(pattern, b);
        if (secondHalfEmpty)
            for (int s = 0; s < 8 * kStepsPerBar; ++s)
            {
                const int d = s + 8 * kStepsPerBar;
                for (int r = 0; r < kDrumRows; ++r)
                    pt.drums[d][r].store(pt.drums[s][r].load(std::memory_order_relaxed), std::memory_order_relaxed);
                pt.melody[d].store(pt.melody[s].load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
    }
    pt.bars.store(uint8_t(newBars), std::memory_order_relaxed);
}

// ---- melody lane ------------------------------------------------------------
void PatternEditor::setLaneStep(int pattern, int bar, int column, const MelodyStep& m)
{
    if (!validCell(bar, column, 0)) return;
    pushUndo(pattern);
    p_.patterns[clampPattern(pattern)].melody[stepIndex(bar, column)].store(m.pack(), std::memory_order_relaxed);
}

void PatternEditor::clearLane(int pattern)
{
    pushUndo(pattern);
    Pattern& pt = p_.patterns[clampPattern(pattern)];
    for (int s = 0; s < kMaxSteps; ++s) pt.melody[s].store(0, std::memory_order_relaxed);
}

void PatternEditor::transposeLane(int pattern, int semitones)
{
    pushUndo(pattern);
    Pattern& pt = p_.patterns[clampPattern(pattern)];
    for (int s = 0; s < kMaxSteps; ++s)
    {
        MelodyStep m = MelodyStep::unpack(pt.melody[s].load(std::memory_order_relaxed));
        if (m.count == 0) continue;
        for (int i = 0; i < m.count; ++i)
            m.notes[i] = uint8_t(std::max(1, std::min(127, int(m.notes[i]) + semitones)));
        pt.melody[s].store(m.pack(), std::memory_order_relaxed);
    }
    gen_.root = uint8_t(std::max(1, std::min(127, int(gen_.root) + semitones)));
}

// ---- generator ----------------------------------------------------------------
void PatternEditor::regenerate(int pattern)
{
    pushUndo(pattern);
    const uint8_t mask = uint8_t(kAllRows & ~gen_.lockedRows);
    if (mask) generateDrums(p_, clampPattern(pattern), gen_.style, gen_.drumSeed, gen_.density, mask);
    if (!gen_.laneLocked) generateLane(p_, clampPattern(pattern), gen_.lane, gen_.root, gen_.scale, gen_.laneSeed);
}

void PatternEditor::randomize(int pattern)
{
    gen_.drumSeed = nextSeed(gen_.drumSeed + 0x9E37u);
    if (!gen_.laneLocked) gen_.laneSeed = nextSeed(gen_.laneSeed + 0x7F4Au);
    regenerate(pattern);
}

} // namespace mt::hauntedhall
