// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall screens (JUCE). Three screens built from the art in assets/:
// MASTER (mixer + delay), STEP (drum grid) and the CARTRIDGE BROWSER.
//
// Drawing rules (see SCREENS.md): every screen is opaque and draws its baked
// background plus only the live parts; a 30 Hz tick compares what is shown
// with the engine and repaints just the rectangles that changed.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <set>

#include "../engine/HHEngine.h"
#include "../engine/HHGrid.h"
#include "HHLayout.h"

namespace mt::hauntedhall::ui {

// ---------------------------------------------------------------------------
// Assets: loaded and pre-scaled once, so painting is plain image blits.
struct Assets
{
    juce::Image bgMaster, bgStep, bgBrowser;
    std::array<juce::Image, size_t(Cartridge::Count)> cartLarge;   // 2x, trimmed to the browser window
    std::array<juce::Image, size_t(Cartridge::Count)> cartThumb;   // 48 x 48 for the carousel
    std::array<juce::Image, 8> ghost;
    std::array<juce::Image, 4> bat, glitch;
    juce::Image lockOpen, lockClosed, starOn, starOff;
    juce::Typeface::Ptr pixelFont;   // optional: assets/fonts/*.ttf (an OFL pixel font)

    // `load` returns the image for a file name such as "bg_master.png".
    // Returns the list of files that could not be loaded (empty = all good).
    juce::StringArray load(const std::function<juce::Image(const juce::String&)>& load,
                           const std::function<juce::MemoryBlock(const juce::String&)>& loadFont = {});
    juce::Font font(float height) const;
};

// ---------------------------------------------------------------------------
enum class ScreenId { Master, Step, Browser };

// Everything the screens share. Message thread only.
struct Context
{
    Context(Engine& e, PatternEditor& g, Assets& a) : engine(e), grid(g), assets(a) {}

    Engine& engine;
    PatternEditor& grid;
    Assets& assets;

    std::function<void(ScreenId)> show;
    std::function<void(Layer)> openBrowser;

    // Set by the host (KICK, or the standalone app) for display.
    bool playing = false;
    double bpm = 100.0;
    double beats = 0.0;

    // Favourites (kept per preset table) and the layer the keyboard plays.
    std::set<int> favouriteKits, favouriteSynths;
    Layer keyboardLayer = Layer::A;

    int viewPattern() const { return engine.project().requested.load(std::memory_order_relaxed); }
};

// ---------------------------------------------------------------------------
// Shared drawing helpers
juce::Colour col(unsigned rgb, float alpha = 1.0f);
juce::Rectangle<int> rect(const Rect& r);
juce::Rectangle<int> knobBounds(const Knob& k);
void drawKnob(juce::Graphics& g, const Knob& k, float value01);
float normalised(const Engine& e, Param p);
void setNormalised(Engine& e, Param p, float v01);

// ---------------------------------------------------------------------------
// Base for the three screens: knob dragging, the four hardware knobs, taps,
// long-presses, toasts and the 30 Hz tick.
class ScreenBase : public juce::Component, private juce::Timer
{
public:
    explicit ScreenBase(Context& c);
    ~ScreenBase() override;

    // KICK's four hardware encoders: delta in knob turns (0.01 = 1 % of the range).
    void hardwareKnob(int index, float delta);

    double lastPaintMs() const { return lastPaintMs_; }
    double maxPaintMs() const { return maxPaintMs_; }
    void resetPaintStats() { maxPaintMs_ = 0.0; }

    void paint(juce::Graphics& g) override final;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void visibilityChanged() override;

protected:
    // Four knobs per screen, bound to engine parameters.
    virtual Knob knobGeometry(int index) const = 0;
    virtual Param knobParam(int index) const = 0;

    virtual void paintScreen(juce::Graphics& g) = 0;
    virtual void tick() {}
    virtual void tapped(juce::Point<int>) {}
    virtual void longPressed(juce::Point<int> p) { tapped(p); }
    // Return >= 0 to start a custom drag (e.g. a fader) at p.
    virtual int dragTarget(juce::Point<int>) { return -1; }
    virtual void dragTo(int, juce::Point<int>) {}

    void toast(const juce::String& text, const juce::Rectangle<int>& where);
    void paintToast(juce::Graphics& g);
    void knobChanged(int index);

    Context& ctx;
    int ticks = 0;   // counts 30 Hz ticks, for sprite animation

private:
    void timerCallback() override;

    enum class Gesture { None, Knob, Drag, Press };
    Gesture gesture_ = Gesture::None;
    int gestureIndex_ = -1;
    float dragStartValue_ = 0.0f;
    juce::Point<int> downPos_;
    double downTime_ = 0.0;
    bool longFired_ = false;

    std::array<float, 4> shownKnob_{{-1, -1, -1, -1}};
    juce::String toastText_;
    juce::Rectangle<int> toastArea_;
    int toastTicks_ = 0;

    double lastPaintMs_ = 0.0, maxPaintMs_ = 0.0;
};

// ---------------------------------------------------------------------------
class MasterScreen : public ScreenBase
{
public:
    explicit MasterScreen(Context& c);

private:
    Knob knobGeometry(int i) const override { return master::kKnobs[i]; }
    Param knobParam(int i) const override;
    void paintScreen(juce::Graphics& g) override;
    void tick() override;
    void tapped(juce::Point<int> p) override;
    int dragTarget(juce::Point<int> p) override;
    void dragTo(int fader, juce::Point<int> p) override;

    struct Shown
    {
        bool on[4] = {}, mute[4] = {};
        int meter[4] = {};
        float fader[4] = {};
        bool bypass = false;
        juce::String time;
    };
    Shown readEngine() const;
    juce::Rectangle<int> faderColumn(int i) const;
    juce::Rectangle<int> rowNameArea(int i) const;

    Shown shown_;
    int glitchFrame_ = 0;
};

// ---------------------------------------------------------------------------
class StepScreen : public ScreenBase
{
public:
    explicit StepScreen(Context& c);

private:
    Knob knobGeometry(int i) const override { return step::kKnobs[i]; }
    Param knobParam(int i) const override;
    void paintScreen(juce::Graphics& g) override;
    void tick() override;
    void tapped(juce::Point<int> p) override;
    void longPressed(juce::Point<int> p) override;

    struct Shown
    {
        uint8_t cells[kStepsPerBar][kDrumRows] = {};
        uint8_t lane[kStepsPerBar] = {};   // note length in steps (0 = none) per column
        int playColumn = -1;               // playhead column in the viewed bar
        int playBar = -1;
        int bars = 8, pattern = 0, viewBar = 0;
        uint8_t locks = 0;
        int decrypt = 0;                   // lit segments: progress through the pattern
        bool barHasNotes[kMaxBars] = {};
    };
    Shown readEngine() const;
    juce::Rectangle<int> cellRect(int col, int row) const;
    juce::Rectangle<int> columnRect(int col) const;
    bool hitCell(juce::Point<int> p, int& col, int& row) const;
    juce::Rectangle<int> toastArea() const { return rect(step::kBarSelector); }

    Shown shown_;
    int viewBar_ = 0;
    double lastManualPage_ = -1.0e9;   // ms; the view follows the playhead unless you paged in the last 4 s
    int batFrame_ = 0;
    int styleIndex_ = 0;
};

// ---------------------------------------------------------------------------
class BrowserScreen : public ScreenBase
{
public:
    explicit BrowserScreen(Context& c);
    void open(Layer layer);   // call before showing

private:
    Knob knobGeometry(int i) const override { return browser::kKnobs[i]; }
    Param knobParam(int i) const override;
    void paintScreen(juce::Graphics& g) override;
    void tick() override;
    void tapped(juce::Point<int> p) override;

    bool isDrum() const { return layer_ == Layer::Drum; }
    int presetCount() const;
    Cartridge cartridgeOf(int preset) const;
    Category categoryOf(int preset) const;
    const char* nameOf(int preset) const;
    bool inView(int preset) const;     // matches the current category and bank
    bool bankHasPresets(int cart) const;
    void listPresets();                 // fills list_ for the current category and bank
    void stepBank(int dir);
    void audition(int preset);
    std::set<int>& favourites();

    Layer layer_ = Layer::A;
    Category category_ = Category::Pad;
    int bank_ = 0;            // cartridge theme
    int carouselPage_ = 0;    // carousel shows cartridges [page*6, page*6+6)
    int selected_ = -1;       // preset index in its table
    int original_ = -1;       // preset when the browser opened (BACK restores it)
    std::array<int, 10> list_{};
    int listCount_ = 0;
    int ghostFrame_ = 0;
};

// ---------------------------------------------------------------------------
// The whole Haunted Hall UI: 1024 x 600, one screen visible at a time.
class HauntedHallEditor : public juce::Component, private juce::Timer
{
public:
    HauntedHallEditor(Engine& engine, PatternEditor& grid, Assets& assets);
    ~HauntedHallEditor() override;

    // From the host:
    void setTransport(bool playing, double bpm, double beats);
    void hardwareKnob(int index, float delta);   // KICK's four encoders
    void setAudioLoad(double load01) { audioLoad_ = load01; }
    void showPerformance(bool on);
    Context& context() { return ctx_; }

    void resized() override;
    void paintOverChildren(juce::Graphics& g) override;

private:
    void show(ScreenId id);
    ScreenBase* current();
    void timerCallback() override;

    Context ctx_;
    MasterScreen master_;
    StepScreen step_;
    BrowserScreen browser_;
    ScreenId currentId_ = ScreenId::Master;

    bool perf_ = false;
    double audioLoad_ = 0.0;
    juce::String perfText_;
};

} // namespace mt::hauntedhall::ui
