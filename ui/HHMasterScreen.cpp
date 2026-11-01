// SPDX-License-Identifier: AGPL-3.0-or-later
// MASTER: four channel strips, the 4-channel line mixer and the master delay.
#include "HHUi.h"

#include <cmath>

namespace mt::hauntedhall::ui {

namespace {
const char* const kSyncNames[7] = {"FREE", "1/4", "1/8.", "1/8", "1/8T", "1/16", "1/2"};
const juce::Rectangle<int> kGlitchArea{640, 2, 96, 64};
}

MasterScreen::MasterScreen(Context& c) : ScreenBase(c) { shown_ = readEngine(); }

Param MasterScreen::knobParam(int i) const
{
    static const Param p[4] = {Param::DelayTime, Param::DelayFeedback, Param::DelayMix, Param::DelayLevel};
    return p[i & 3];
}

juce::Rectangle<int> MasterScreen::faderColumn(int i) const
{
    return {master::kFaderX[i] - 18, master::kFaderTop - 10, 36, master::kFaderBottom - master::kFaderTop + 20};
}

juce::Rectangle<int> MasterScreen::rowNameArea(int i) const
{
    // the icon + name part of each channel strip opens that layer's cartridge browser
    return {32, master::kOn[i].y - 14, 210, master::kOn[i].h + 28};
}

MasterScreen::Shown MasterScreen::readEngine() const
{
    Shown s;
    const Engine& e = ctx.engine;
    for (int i = 0; i < 4; ++i)
    {
        const Layer l = Layer(i);
        s.on[i] = e.getParam(channelParam(l, ChannelParam::On)) >= 0.5f;
        s.mute[i] = e.getParam(channelParam(l, ChannelParam::Mute)) >= 0.5f;
        s.fader[i] = e.getParam(channelParam(l, ChannelParam::Level));
        const float pk = e.channelPeak(l);
        const float db = pk > 1e-6f ? 20.0f * std::log10(pk) : -100.0f;
        s.meter[i] = juce::jlimit(0, master::kMeterSegments, int(std::ceil((db + 48.0f) / 48.0f * master::kMeterSegments)));
    }
    s.bypass = e.getParam(Param::DelayBypass) >= 0.5f;
    const int sync = int(e.getParam(Param::DelaySync) + 0.5f);
    s.time = sync > 0 ? juce::String(kSyncNames[sync % 7])
                      : juce::String(juce::roundToInt(e.getParam(Param::DelayTime))) + " ms";
    return s;
}

void MasterScreen::tick()
{
    const Shown now = readEngine();
    for (int i = 0; i < 4; ++i)
    {
        if (now.on[i] != shown_.on[i]) repaint(rect(master::kOn[i]));
        if (now.mute[i] != shown_.mute[i]) repaint(rect(master::kMute[i]));
        if (now.meter[i] != shown_.meter[i]) repaint(rect(master::kMeter[i]));
        if (std::fabs(now.fader[i] - shown_.fader[i]) > 0.002f) repaint(faderColumn(i));
    }
    if (now.bypass != shown_.bypass) repaint(rect(master::kBypass));
    if (now.time != shown_.time) repaint(rect(master::kTimeDisplay));
    shown_ = now;

    if (ticks % 5 == 0)   // glitch pixels at 6 fps
    {
        glitchFrame_ = (glitchFrame_ + 1) % 4;
        repaint(kGlitchArea);
    }
}

void MasterScreen::paintScreen(juce::Graphics& g)
{
    const Assets& a = ctx.assets;
    g.drawImageAt(a.bgMaster, 0, 0);
    const Shown& s = shown_;
    const juce::Font small = a.font(14.0f);

    for (int i = 0; i < 4; ++i)
    {
        const auto on = rect(master::kOn[i]);
        if (g.clipRegionIntersects(on))
        {
            g.setFont(small);
            if (s.on[i])
            {
                g.setColour(col(colour::kMint));
                g.fillRect(on.reduced(3));
                g.setColour(col(colour::kDarkText));
            }
            else
                g.setColour(col(colour::kText, 0.45f));
            g.drawText("ON", on, juce::Justification::centred);
        }
        const auto mute = rect(master::kMute[i]);
        if (s.mute[i] && g.clipRegionIntersects(mute))
        {
            g.setColour(col(colour::kMint));
            g.drawRect(mute.reduced(1), 3);
        }
        const auto meter = rect(master::kMeter[i]);
        if (g.clipRegionIntersects(meter))
        {
            const float sw = float(meter.getWidth()) / master::kMeterSegments;
            for (int k = 0; k < s.meter[i]; ++k)
            {
                g.setColour(col(k < 9 ? colour::kMint : (k < 11 ? colour::kMeterYellow : colour::kMeterRed)));
                g.fillRect(juce::Rectangle<float>(float(meter.getX()) + k * sw + 2.0f, float(meter.getY()) + 2.0f,
                                                  sw - 4.0f, float(meter.getHeight()) - 4.0f));
            }
        }
        if (g.clipRegionIntersects(faderColumn(i)))
        {
            const float cy = float(master::kFaderBottom) - s.fader[i] * float(master::kFaderBottom - master::kFaderTop);
            const juce::Rectangle<float> cap(float(master::kFaderX[i]) - master::kFaderCapW / 2.0f,
                                             cy - master::kFaderCapH / 2.0f, float(master::kFaderCapW),
                                             float(master::kFaderCapH));
            g.setColour(col(colour::kFaderCap));
            g.fillRect(cap);
            g.setColour(col(colour::kLavender));
            g.drawRect(cap, 1.0f);
            g.setColour(col(colour::kFaderLine));
            g.drawLine(cap.getX() + 3.0f, cy, cap.getRight() - 3.0f, cy, 2.0f);
        }
    }

    if (s.bypass && g.clipRegionIntersects(rect(master::kBypass)))
    {
        g.setColour(col(colour::kMint));
        g.fillEllipse(float(master::kBypassLedX - master::kBypassLedR), float(master::kBypassLedY - master::kBypassLedR),
                      2.0f * master::kBypassLedR, 2.0f * master::kBypassLedR);
    }
    if (g.clipRegionIntersects(rect(master::kTimeDisplay)))
    {
        g.setColour(col(colour::kMint));
        g.setFont(a.font(19.0f));
        g.drawText(s.time, rect(master::kTimeDisplay), juce::Justification::centred);
    }
    for (int i = 0; i < 4; ++i)
        if (g.clipRegionIntersects(knobBounds(master::kKnobs[i])))
            drawKnob(g, master::kKnobs[i], normalised(ctx.engine, knobParam(i)));

    const auto tab = rect(master::kTabMaster);
    if (g.clipRegionIntersects(tab))
    {
        g.setColour(col(colour::kMint, 0.27f));
        g.fillRect(tab.reduced(4));
        g.setColour(col(colour::kMint));
        g.drawRect(tab.reduced(2), 3);
    }
    if (g.clipRegionIntersects(kGlitchArea))
        g.drawImageAt(a.glitch[size_t(glitchFrame_)], kGlitchArea.getX(), kGlitchArea.getY());
}

void MasterScreen::tapped(juce::Point<int> p)
{
    Engine& e = ctx.engine;
    auto toggle = [&e](Param prm) { e.setParam(prm, e.getParam(prm) >= 0.5f ? 0.0f : 1.0f); };
    for (int i = 0; i < 4; ++i)
    {
        if (rect(master::kOn[i]).expanded(4).contains(p)) { toggle(channelParam(Layer(i), ChannelParam::On)); tick(); return; }
        if (rect(master::kMute[i]).expanded(4).contains(p)) { toggle(channelParam(Layer(i), ChannelParam::Mute)); tick(); return; }
        if (rowNameArea(i).contains(p)) { ctx.openBrowser(Layer(i)); return; }
    }
    if (rect(master::kBypass).contains(p)) { toggle(Param::DelayBypass); tick(); return; }
    if (rect(master::kTimeDisplay).contains(p))
    {
        // tap the display: free time, then the tempo-synced divisions
        e.setParam(Param::DelaySync, float((int(e.getParam(Param::DelaySync) + 0.5f) + 1) % 7));
        tick();
        return;
    }
    if (rect(master::kTabDrum).contains(p)) ctx.show(ScreenId::Step);
}

int MasterScreen::dragTarget(juce::Point<int> p)
{
    for (int i = 0; i < 4; ++i)
        if (faderColumn(i).contains(p)) return i;
    return -1;
}

void MasterScreen::dragTo(int i, juce::Point<int> p)
{
    const float v = float(master::kFaderBottom - p.y) / float(master::kFaderBottom - master::kFaderTop);
    ctx.engine.setParam(channelParam(Layer(i), ChannelParam::Level), juce::jlimit(0.0f, 1.0f, v));
    tick();
}

} // namespace mt::hauntedhall::ui
