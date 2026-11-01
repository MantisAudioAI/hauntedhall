// SPDX-License-Identifier: AGPL-3.0-or-later
// DRUM / STEP: the 5-row grid, bar pages, 8/16 bars, pattern A/B, RANDOM with
// row locks, clear / copy / paste, undo / redo, and the drum layer's knobs.
#include "HHUi.h"

#include <cstring>

namespace mt::hauntedhall::ui {

namespace {
const juce::Rectangle<int> kTopBanner{265, 4, 495, 38};   // "HAUNTED HALL - STEP MODE": back to MASTER
const char* const kRowNames[kDrumRows] = {"KICK", "SNARE", "HAT", "GHOST", "FX"};

juce::Rectangle<int> barStripArea()
{
    return {step::kColX[0], step::kBarStripY - 6, step::kColX[15] + step::kCellW - step::kColX[0], step::kBarStripH + 12};
}
juce::Rectangle<int> half(const Rect& r, bool right)
{
    return right ? juce::Rectangle<int>(r.x + r.w / 2, r.y, r.w - r.w / 2, r.h) : juce::Rectangle<int>(r.x, r.y, r.w / 2, r.h);
}
} // namespace

StepScreen::StepScreen(Context& c) : ScreenBase(c) { shown_ = readEngine(); }

Param StepScreen::knobParam(int i) const
{
    static const Param p[4] = {Param::DrumDecay, Param::DrumTone, Param::DrumSpook, Param::DrumSpace};
    return p[i & 3];
}

juce::Rectangle<int> StepScreen::cellRect(int c, int r) const
{
    return {step::kColX[c], step::kRowY[r], step::kCellW, step::kCellH};
}

juce::Rectangle<int> StepScreen::columnRect(int c) const
{
    const int top = step::kNumberY;
    return {step::kColX[c], top, step::kCellW, step::kRowY[kDrumRows - 1] + step::kCellH - top};
}

bool StepScreen::hitCell(juce::Point<int> p, int& c, int& r) const
{
    for (c = 0; c < kStepsPerBar; ++c)
        for (r = 0; r < kDrumRows; ++r)
            if (cellRect(c, r).expanded(2).contains(p)) return true;
    return false;
}

StepScreen::Shown StepScreen::readEngine() const
{
    Shown s;
    const PatternEditor& g = ctx.grid;
    s.pattern = ctx.viewPattern();
    s.bars = g.bars(s.pattern);
    s.viewBar = viewBar_ < s.bars ? viewBar_ : 0;
    for (int c = 0; c < kStepsPerBar; ++c)
    {
        for (int r = 0; r < kDrumRows; ++r) s.cells[c][r] = g.step(s.pattern, s.viewBar, c, r);
        const MelodyStep m = g.laneStep(s.pattern, s.viewBar, c);
        s.lane[c] = m.count ? m.length : 0;
    }
    for (int b = 0; b < kMaxBars; ++b) s.barHasNotes[b] = b < s.bars && g.barHasNotes(s.pattern, b);
    s.locks = ctx.grid.rowLocks();

    const int stepNow = ctx.engine.currentStep();
    if (ctx.playing && stepNow >= 0 && ctx.engine.playingPattern() == s.pattern)
    {
        s.playBar = stepNow / kStepsPerBar;
        s.playColumn = s.playBar == s.viewBar ? stepNow % kStepsPerBar : -1;
        s.decrypt = juce::jlimit(0, 12, (stepNow + 1) * 12 / (s.bars * kStepsPerBar));
    }
    return s;
}

void StepScreen::tick()
{
    if (viewBar_ >= ctx.grid.bars(ctx.viewPattern())) viewBar_ = 0;
    // Follow: while playing, the view pages along with the playhead, unless
    // you paged by hand in the last 4 seconds (then it stays where you are).
    const int stepNow = ctx.engine.currentStep();
    if (ctx.playing && stepNow >= 0 && ctx.engine.playingPattern() == ctx.viewPattern()
        && juce::Time::getMillisecondCounterHiRes() - lastManualPage_ > 4000.0)
        viewBar_ = stepNow / kStepsPerBar;
    const Shown now = readEngine();
    for (int c = 0; c < kStepsPerBar; ++c)
        for (int r = 0; r < kDrumRows; ++r)
            if (now.cells[c][r] != shown_.cells[c][r]) repaint(cellRect(c, r));
    if (now.playColumn != shown_.playColumn)
    {
        if (shown_.playColumn >= 0) repaint(columnRect(shown_.playColumn));
        if (now.playColumn >= 0) repaint(columnRect(now.playColumn));
    }
    bool stripChanged = now.bars != shown_.bars || now.viewBar != shown_.viewBar || now.playBar != shown_.playBar;
    for (int b = 0; b < kMaxBars && !stripChanged; ++b) stripChanged = now.barHasNotes[b] != shown_.barHasNotes[b];
    if (stripChanged) repaint(barStripArea());
    if (now.viewBar != shown_.viewBar || now.bars != shown_.bars) repaint(rect(step::kBarSelector));
    if (now.bars != shown_.bars) repaint(rect(step::kBars816));
    if (now.pattern != shown_.pattern) repaint(rect(step::kPatternAB));
    if (now.locks != shown_.locks)
        for (int r = 0; r < kDrumRows; ++r) repaint(rect(step::kRowLock[r]).expanded(2));
    if (std::memcmp(now.lane, shown_.lane, sizeof now.lane) != 0) repaint(rect(step::kMelodyStrip).expanded(0, 2));
    if (now.decrypt != shown_.decrypt) repaint(rect(step::kDecryptBar));
    shown_ = now;

    if (ticks % 6 == 0)   // bat at 5 fps
    {
        batFrame_ = (batFrame_ + 1) % 4;
        repaint(step::kBatX, step::kBatY, 40, 40);
    }
}

void StepScreen::paintScreen(juce::Graphics& g)
{
    const Assets& a = ctx.assets;
    const Shown& s = shown_;
    g.drawImageAt(a.bgStep, 0, 0);

    // cells
    for (int c = 0; c < kStepsPerBar; ++c)
        for (int r = 0; r < kDrumRows; ++r)
        {
            const uint8_t v = s.cells[c][r];
            const auto cell = cellRect(c, r);
            if (!v || !g.clipRegionIntersects(cell)) continue;
            unsigned fill = colour::kMint, edge = colour::kMintHi;
            if (v >= 117) fill = colour::kAccent;
            else if (v < 74) { fill = colour::kSoft; edge = colour::kSoftEdge; }
            g.setColour(col(fill));
            g.fillRect(cell.getX() + 3, cell.getY() + 3, cell.getWidth() - 8, cell.getHeight() - 7);
            g.setColour(col(edge));
            g.fillRect(cell.getX() + 3, cell.getY() + 3, cell.getWidth() - 8, 2);
        }

    // playhead
    if (s.playColumn >= 0 && g.clipRegionIntersects(columnRect(s.playColumn)))
    {
        const int x = step::kColX[s.playColumn];
        g.setColour(col(colour::kMint));
        g.drawRect(x + 1, step::kNumberY + 1, step::kCellW - 3, step::kNumberH - 3, 2);
        for (int r = 0; r < kDrumRows; ++r) g.drawRect(cellRect(s.playColumn, r).reduced(1), 1);
    }

    // bar strip: existing bars, the viewed bar (mint), the playing bar (bright top line)
    if (g.clipRegionIntersects(barStripArea()))
        for (int b = 0; b < s.bars; ++b)
        {
            const juce::Rectangle<int> slot(step::kColX[b] + 3, step::kBarStripY + 2, step::kCellW - 8, step::kBarStripH - 4);
            g.setColour(col(b == s.viewBar ? colour::kMint : (s.barHasNotes[b] ? colour::kLavenderMid : colour::kFaderLine),
                            b == s.viewBar ? 1.0f : 0.8f));
            g.fillRect(slot);
            if (b == s.playBar)
            {
                g.setColour(col(colour::kMintHi));
                g.fillRect(slot.getX(), slot.getY() - 4, slot.getWidth(), 2);
            }
        }

    const juce::Font f15 = a.font(15.0f);
    if (g.clipRegionIntersects(rect(step::kBarSelector)))
    {
        g.setColour(col(colour::kMint));
        g.setFont(f15);
        g.drawText(juce::String::formatted("<  BAR %02d/%02d  >", s.viewBar + 1, s.bars), rect(step::kBarSelector),
                   juce::Justification::centred);
    }
    auto underline = [&g](const juce::Rectangle<int>& r) {
        g.setColour(col(colour::kMint));
        g.fillRect(r.getX() + 4, r.getBottom() - 3, r.getWidth() - 8, 2);
    };
    if (g.clipRegionIntersects(rect(step::kBars816))) underline(half(step::kBars816, s.bars == 16));
    if (g.clipRegionIntersects(rect(step::kPatternAB))) underline(half(step::kPatternAB, s.pattern == 1));

    for (int r = 0; r < kDrumRows; ++r)
    {
        const auto lk = rect(step::kRowLock[r]);
        if (!g.clipRegionIntersects(lk.expanded(2))) continue;
        g.setColour(juce::Colour(0xff6e6e76));
        g.fillRect(lk.expanded(1));
        g.drawImageAt(((s.locks >> r) & 1u) ? a.lockClosed : a.lockOpen, lk.getX(), lk.getY());
    }

    const auto mel = rect(step::kMelodyStrip);
    if (g.clipRegionIntersects(mel.expanded(0, 2)))
    {
        g.setColour(col(colour::kMint));
        for (int c = 0; c < kStepsPerBar; ++c)
            if (s.lane[c])
            {
                const int x = step::kColX[c] + 3;
                const int w = std::min(int(s.lane[c]), kStepsPerBar - c) * 50 - 8;
                g.fillRect(x, mel.getY(), w, mel.getHeight());
            }
    }

    for (int i = 0; i < 4; ++i)
        if (g.clipRegionIntersects(knobBounds(step::kKnobs[i])))
            drawKnob(g, step::kKnobs[i], normalised(ctx.engine, knobParam(i)));

    const auto dec = rect(step::kDecryptBar);
    if (g.clipRegionIntersects(dec))
    {
        g.setColour(col(colour::kText));
        g.setFont(a.font(10.0f));
        g.drawText("DECRYPTING...", dec.getX(), dec.getY() - 2, dec.getWidth(), 12, juce::Justification::centredLeft);
        const float sw = float(dec.getWidth() - 4) / 12.0f;
        for (int i = 0; i < 12; ++i)
        {
            const juce::Rectangle<float> seg(float(dec.getX()) + 2.0f + i * sw + 1.0f, float(dec.getY()) + 14.0f, sw - 3.0f,
                                             float(dec.getHeight()) - 16.0f);
            g.setColour(col(i < s.decrypt ? colour::kMint : colour::kDark));
            g.fillRect(seg);
            g.setColour(col(colour::kFaderLine));
            g.drawRect(seg, 1.0f);
        }
    }
    const juce::Rectangle<int> bat(step::kBatX, step::kBatY, 40, 40);
    if (g.clipRegionIntersects(bat)) g.drawImageAt(a.bat[size_t(batFrame_)], bat.getX(), bat.getY());
}

void StepScreen::tapped(juce::Point<int> p)
{
    PatternEditor& g = ctx.grid;
    const int pat = ctx.viewPattern();
    const int bars = g.bars(pat);
    int c = 0, r = 0;

    if (hitCell(p, c, r))
    {
        g.toggleStep(pat, viewBar_, c, r);
        lastManualPage_ = juce::Time::getMillisecondCounterHiRes();
        tick();
        return;
    }

    if (barStripArea().contains(p))
    {
        for (int b = 0; b < bars; ++b)
            if (p.x >= step::kColX[b] && p.x < step::kColX[b] + step::kCellW)
            {
                viewBar_ = b;
                lastManualPage_ = juce::Time::getMillisecondCounterHiRes();
                tick();
                return;
            }
        return;
    }
    const auto sel = rect(step::kBarSelector);
    if (sel.contains(p))
    {
        lastManualPage_ = juce::Time::getMillisecondCounterHiRes();
        if (p.x < sel.getX() + sel.getWidth() * 2 / 5) viewBar_ = (viewBar_ + bars - 1) % bars;
        else if (p.x > sel.getX() + sel.getWidth() * 3 / 5) viewBar_ = (viewBar_ + 1) % bars;
        tick();
        return;
    }
    if (rect(step::kBars816).contains(p))
    {
        const int want = half(step::kBars816, true).contains(p) ? 16 : 8;
        g.setBars(pat, want);
        if (viewBar_ >= want) viewBar_ = 0;
        toast(want == 16 ? "16 BARS" : "8 BARS", toastArea());
        tick();
        return;
    }
    if (rect(step::kPatternAB).contains(p))
    {
        const int want = half(step::kPatternAB, true).contains(p) ? 1 : 0;
        ctx.engine.requestPattern(want);
        toast(ctx.playing ? juce::String("PATTERN ") + (want ? "B" : "A") + " NEXT" : juce::String("PATTERN ") + (want ? "B" : "A"),
              toastArea());
        tick();
        return;
    }
    for (r = 0; r < kDrumRows; ++r)
    {
        if (rect(step::kRowLock[r]).expanded(5).contains(p))
        {
            g.toggleRowLock(r);
            toast(juce::String(kRowNames[r]) + (g.rowLocked(r) ? " LOCKED" : " UNLOCKED"), toastArea());
            tick();
            return;
        }
        if (rect(step::kRowLabel[r]).contains(p)) { ctx.engine.trigger(Layer::Drum, kDrumNotes[r], 0.85f); return; }
    }
    if (rect(step::kRandom).contains(p))
    {
        g.randomize(pat);
        toast(juce::String("RANDOM: ") + grooveStyleName(g.generator().style), toastArea());
    }
    else if (rect(step::kClear).contains(p)) { g.clearBar(pat, viewBar_); toast(juce::String::formatted("BAR %d CLEARED", viewBar_ + 1), toastArea()); }
    else if (rect(step::kCopy).contains(p)) { g.copyBar(pat, viewBar_); toast(juce::String::formatted("BAR %d COPIED", viewBar_ + 1), toastArea()); }
    else if (rect(step::kPaste).contains(p))
    {
        toast(g.pasteBar(pat, viewBar_) ? juce::String::formatted("PASTED TO BAR %d", viewBar_ + 1) : juce::String("NOTHING COPIED"),
              toastArea());
    }
    else if (rect(step::kUndo).contains(p)) toast(g.undo() ? "UNDO" : "NOTHING TO UNDO", toastArea());
    else if (kTopBanner.contains(p)) { ctx.show(ScreenId::Master); return; }
    tick();
}

void StepScreen::longPressed(juce::Point<int> p)
{
    PatternEditor& g = ctx.grid;
    const int pat = ctx.viewPattern();
    int c = 0, r = 0;
    if (hitCell(p, c, r)) { g.cycleStep(pat, viewBar_, c, r); tick(); return; }
    if (rect(step::kRandom).contains(p))
    {
        // long-press RANDOM: next groove style, regenerated with the same seed
        styleIndex_ = (int(g.generator().style) + 1) % int(GrooveStyle::Count);
        g.generator().style = GrooveStyle(styleIndex_);
        g.regenerate(pat);
        toast(juce::String("STYLE: ") + grooveStyleName(g.generator().style), toastArea());
    }
    else if (rect(step::kClear).contains(p)) { g.clearPattern(pat); toast("PATTERN CLEARED", toastArea()); }
    else if (rect(step::kCopy).contains(p)) { g.copyPattern(pat); toast(juce::String("PATTERN ") + (pat ? "B" : "A") + " COPIED", toastArea()); }
    else if (rect(step::kPaste).contains(p))
        toast(g.pastePattern(pat) ? juce::String("PATTERN PASTED") : juce::String("NO PATTERN COPIED"), toastArea());
    else if (rect(step::kUndo).contains(p)) toast(g.redo() ? "REDO" : "NOTHING TO REDO", toastArea());
    else { tapped(p); return; }
    tick();
}

} // namespace mt::hauntedhall::ui
