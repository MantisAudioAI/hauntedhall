// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHUi.h"

#include <cmath>

namespace mt::hauntedhall::ui {

// ---------------------------------------------------------------------------
// Assets
namespace {

const char* const kCartFiles[size_t(Cartridge::Count)] = {
    "cart_haunted_castle.png", "cart_skeleton.png",     "cart_haunted_organ.png",
    "cart_alien_saucer.png",   "cart_night_racer.png",  "cart_boss_dragon.png",
    "cart_crystal_sword.png",  "cart_platform_block.png", "cart_flying_ghosts.png",
};

// Cuts frame i of n from a horizontal strip.
juce::Image frameOf(const juce::Image& strip, int i, int n)
{
    if (!strip.isValid()) return {};
    const int w = strip.getWidth() / n;
    return strip.getClippedImage({i * w, 0, w, strip.getHeight()}).createCopy();
}

// Scales pixel art by an integer factor with hard edges (no smoothing).
juce::Image pixelScale(const juce::Image& src, int factor)
{
    if (!src.isValid()) return {};
    juce::Image out(juce::Image::ARGB, src.getWidth() * factor, src.getHeight() * factor, false);
    const juce::Image::BitmapData s(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData d(out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < out.getHeight(); ++y)
        for (int x = 0; x < out.getWidth(); ++x)
            d.setPixelColour(x, y, s.getPixelColour(x / factor, y / factor));
    return out;
}

} // namespace

juce::StringArray Assets::load(const std::function<juce::Image(const juce::String&)>& get,
                               const std::function<juce::MemoryBlock(const juce::String&)>& getFont)
{
    juce::StringArray missing;
    auto need = [&](const char* file) {
        juce::Image im = get(file);
        if (!im.isValid()) missing.add(file);
        return im;
    };

    bgMaster = need("bg_master.png");
    bgStep = need("bg_step.png");
    bgBrowser = need("bg_browser.png");

    // Browser window: 96 x 96 art at 2x, trimmed to the window's width.
    const Rect art = browser::kCartArt;
    const int keepCols = art.w / 2;
    const int trim = (96 - keepCols) / 2;
    for (size_t i = 0; i < cartLarge.size(); ++i)
    {
        const juce::Image src = need(kCartFiles[i]);
        if (!src.isValid()) continue;
        cartLarge[i] = pixelScale(src.getClippedImage({trim, 0, keepCols, 96}).createCopy(), 2);
        cartThumb[i] = src.rescaled(48, 48, juce::Graphics::highResamplingQuality);   // exact half: averages 2x2
    }

    const juce::Image ghostStrip = need("ghost_8x64x64.png");
    const juce::Image batStrip = need("bat_4x64x64.png");
    const juce::Image glitchStrip = need("glitch_4x96x64.png");
    for (int i = 0; i < 8; ++i) ghost[size_t(i)] = frameOf(ghostStrip, i, 8).rescaled(48, 48, juce::Graphics::lowResamplingQuality);
    for (int i = 0; i < 4; ++i) bat[size_t(i)] = frameOf(batStrip, i, 4).rescaled(40, 40, juce::Graphics::lowResamplingQuality);
    for (int i = 0; i < 4; ++i) glitch[size_t(i)] = frameOf(glitchStrip, i, 4);

    lockOpen = need("lock_open_20.png").rescaled(14, 14, juce::Graphics::highResamplingQuality);
    lockClosed = need("lock_closed_20.png").rescaled(14, 14, juce::Graphics::highResamplingQuality);
    starOn = need("star_on_20.png");
    starOff = need("star_off_20.png");

    if (getFont)
    {
        const juce::MemoryBlock data = getFont("pixel_font.ttf");
        if (data.getSize() > 0)
            pixelFont = juce::Typeface::createSystemTypefaceFor(data.getData(), data.getSize());
    }
    return missing;
}

juce::Font Assets::font(float height) const
{
    if (pixelFont != nullptr) return juce::Font(pixelFont).withHeight(height);
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), height, juce::Font::bold);
}

// ---------------------------------------------------------------------------
// Drawing helpers
juce::Colour col(unsigned rgb, float alpha)
{
    return juce::Colour(juce::uint32(0xff000000u | rgb)).withAlpha(alpha);
}

juce::Rectangle<int> rect(const Rect& r) { return {r.x, r.y, r.w, r.h}; }

juce::Rectangle<int> knobBounds(const Knob& k)
{
    const int r = k.arc + 5;
    return {k.cx - r, k.cy - r, 2 * r, 2 * r};
}

void drawKnob(juce::Graphics& g, const Knob& k, float v)
{
    // 11-segment tick ring (drawn over the art's ring), then the cap and pointer.
    constexpr int kSegs = 11;
    constexpr float a0 = -0.75f * juce::MathConstants<float>::pi;   // 7:30 o'clock
    constexpr float a1 = 0.75f * juce::MathConstants<float>::pi;    // 4:30 o'clock
    const float gap = 0.06f;
    for (int i = 0; i < kSegs; ++i)
    {
        const float s0 = a0 + (a1 - a0) * float(i) / kSegs + gap;
        const float s1 = a0 + (a1 - a0) * float(i + 1) / kSegs - gap;
        juce::Path seg;
        seg.addCentredArc(float(k.cx), float(k.cy), float(k.arc), float(k.arc), 0.0f, s0, s1, true);
        const bool lit = (float(i) + 0.5f) / kSegs <= v;
        g.setColour(col(lit ? colour::kMint : colour::kDimTick));
        g.strokePath(seg, juce::PathStrokeType(7.0f));
    }
    const float r = float(k.well - 3);
    const juce::Rectangle<float> cap(float(k.cx) - r, float(k.cy) - r, 2 * r, 2 * r);
    g.setColour(col(colour::kKnobCap));
    g.fillEllipse(cap);
    g.setColour(col(colour::kLavender));
    g.drawEllipse(cap, 2.0f);
    const float a = a0 + (a1 - a0) * v;   // 0 rad = 12 o'clock
    const float sx = std::sin(a), sy = -std::cos(a);
    g.drawLine(float(k.cx) + sx * r * 0.25f, float(k.cy) + sy * r * 0.25f,
               float(k.cx) + sx * (r - 3.0f), float(k.cy) + sy * (r - 3.0f), 3.0f);
}

float normalised(const Engine& e, Param p)
{
    const ParamInfo& info = Engine::paramInfo(p);
    return (e.getParam(p) - info.min) / (info.max - info.min);
}

void setNormalised(Engine& e, Param p, float v)
{
    const ParamInfo& info = Engine::paramInfo(p);
    e.setParam(p, info.min + clampf(v, 0.0f, 1.0f) * (info.max - info.min));
}

// ---------------------------------------------------------------------------
// ScreenBase
ScreenBase::ScreenBase(Context& c) : ctx(c)
{
    setOpaque(true);
    setSize(1024, 600);
}

ScreenBase::~ScreenBase() { stopTimer(); }

void ScreenBase::visibilityChanged()
{
    // Hidden screens cost nothing: no tick, no repaints.
    if (isVisible()) { startTimerHz(30); repaint(); }
    else stopTimer();
}

void ScreenBase::paint(juce::Graphics& g)
{
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    paintScreen(g);
    paintToast(g);
    lastPaintMs_ = juce::Time::getMillisecondCounterHiRes() - t0;
    maxPaintMs_ = std::max(maxPaintMs_, lastPaintMs_);
}

void ScreenBase::knobChanged(int i)
{
    shownKnob_[size_t(i)] = normalised(ctx.engine, knobParam(i));
    repaint(knobBounds(knobGeometry(i)));
}

void ScreenBase::hardwareKnob(int index, float delta)
{
    if (index < 0 || index > 3) return;
    const Param p = knobParam(index);
    setNormalised(ctx.engine, p, normalised(ctx.engine, p) + delta);
    knobChanged(index);
}

void ScreenBase::mouseDown(const juce::MouseEvent& e)
{
    downPos_ = e.getPosition();
    downTime_ = juce::Time::getMillisecondCounterHiRes();
    longFired_ = false;
    for (int i = 0; i < 4; ++i)
    {
        const Knob k = knobGeometry(i);
        if (downPos_.getDistanceFrom({k.cx, k.cy}) <= k.arc + 4)
        {
            gesture_ = Gesture::Knob;
            gestureIndex_ = i;
            dragStartValue_ = normalised(ctx.engine, knobParam(i));
            return;
        }
    }
    const int d = dragTarget(downPos_);
    if (d >= 0)
    {
        gesture_ = Gesture::Drag;
        gestureIndex_ = d;
        dragTo(d, downPos_);
        return;
    }
    gesture_ = Gesture::Press;
}

void ScreenBase::mouseDrag(const juce::MouseEvent& e)
{
    if (gesture_ == Gesture::Knob)
    {
        // Vertical drag: 200 px = the full range.
        const float v = dragStartValue_ + float(downPos_.y - e.getPosition().y) / 200.0f;
        setNormalised(ctx.engine, knobParam(gestureIndex_), v);
        knobChanged(gestureIndex_);
    }
    else if (gesture_ == Gesture::Drag)
        dragTo(gestureIndex_, e.getPosition());
    else if (gesture_ == Gesture::Press && e.getPosition().getDistanceFrom(downPos_) > 12)
        gesture_ = Gesture::None;   // a slide is not a tap
}

void ScreenBase::mouseUp(const juce::MouseEvent&)
{
    if (gesture_ == Gesture::Press && !longFired_) tapped(downPos_);
    gesture_ = Gesture::None;
}

void ScreenBase::timerCallback()
{
    ++ticks;
    // long-press: 450 ms without sliding
    if (gesture_ == Gesture::Press && !longFired_
        && juce::Time::getMillisecondCounterHiRes() - downTime_ > 450.0)
    {
        longFired_ = true;
        longPressed(downPos_);
    }
    // knobs moved from elsewhere (another screen, the host)
    for (int i = 0; i < 4; ++i)
        if (std::fabs(normalised(ctx.engine, knobParam(i)) - shownKnob_[size_t(i)]) > 0.002f) knobChanged(i);
    if (toastTicks_ > 0 && --toastTicks_ == 0) repaint(toastArea_);
    tick();
}

void ScreenBase::toast(const juce::String& text, const juce::Rectangle<int>& where)
{
    if (!toastArea_.isEmpty()) repaint(toastArea_);
    toastText_ = text;
    toastArea_ = where;
    toastTicks_ = 45;   // 1.5 s
    repaint(toastArea_);
}

void ScreenBase::paintToast(juce::Graphics& g)
{
    if (toastTicks_ <= 0 || toastArea_.isEmpty() || !g.clipRegionIntersects(toastArea_)) return;
    g.setColour(col(colour::kDark));
    g.fillRect(toastArea_.reduced(2));
    g.setColour(col(colour::kMint));
    g.drawRect(toastArea_.reduced(2), 1);
    g.setFont(ctx.assets.font(13.0f));
    g.drawFittedText(toastText_, toastArea_.reduced(6, 2), juce::Justification::centred, 2);
}

// ---------------------------------------------------------------------------
// Editor
HauntedHallEditor::HauntedHallEditor(Engine& engine, PatternEditor& grid, Assets& assets)
    : ctx_(engine, grid, assets), master_(ctx_), step_(ctx_), browser_(ctx_)
{
    ctx_.show = [this](ScreenId id) { show(id); };
    ctx_.openBrowser = [this](Layer l) {
        browser_.open(l);
        if (l != Layer::Drum) ctx_.keyboardLayer = l;
        show(ScreenId::Browser);
    };
    addChildComponent(master_);
    addChildComponent(step_);
    addChildComponent(browser_);
    setSize(1024, 600);
    show(ScreenId::Master);
}

HauntedHallEditor::~HauntedHallEditor() { stopTimer(); }

void HauntedHallEditor::resized()
{
    for (auto* s : {static_cast<juce::Component*>(&master_), static_cast<juce::Component*>(&step_),
                    static_cast<juce::Component*>(&browser_)})
        s->setBounds(getLocalBounds());
}

ScreenBase* HauntedHallEditor::current()
{
    switch (currentId_)
    {
        case ScreenId::Master: return &master_;
        case ScreenId::Step: return &step_;
        case ScreenId::Browser: return &browser_;
    }
    return &master_;
}

void HauntedHallEditor::show(ScreenId id)
{
    currentId_ = id;
    master_.setVisible(id == ScreenId::Master);
    step_.setVisible(id == ScreenId::Step);
    browser_.setVisible(id == ScreenId::Browser);
}

void HauntedHallEditor::setTransport(bool playing, double bpm, double beats)
{
    ctx_.playing = playing;
    ctx_.bpm = bpm;
    ctx_.beats = beats;
}

void HauntedHallEditor::hardwareKnob(int index, float delta) { current()->hardwareKnob(index, delta); }

void HauntedHallEditor::showPerformance(bool on)
{
    perf_ = on;
    if (on) startTimerHz(4);
    else stopTimer();
    repaint(0, 580, 1024, 20);
}

void HauntedHallEditor::timerCallback()
{
    ScreenBase* s = current();
    perfText_ = juce::String("paint ") + juce::String(s->lastPaintMs(), 2) + " ms (max "
                + juce::String(s->maxPaintMs(), 2) + ")   audio " + juce::String(audioLoad_ * 100.0, 1) + " %";
    repaint(0, 580, 1024, 20);
}

void HauntedHallEditor::paintOverChildren(juce::Graphics& g)
{
    if (!perf_) return;
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillRect(0, 580, 1024, 20);
    g.setColour(col(colour::kMint));
    g.setFont(ctx_.assets.font(13.0f));
    g.drawText(perfText_, 8, 580, 1008, 20, juce::Justification::centredLeft);
}

} // namespace mt::hauntedhall::ui
