// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall standalone test app: the real engine and the real screens in a
// 1024 x 600 desktop window, so the sound and the screens can be tested before
// (and alongside) KICK and its simulator.
//
// Keys:  SPACE play/stop   UP/DOWN tempo   F2 performance overlay
//        Z X C V B  drum rows (kick, snare, hat, ghost, FX)
//        A W S E D F T G Y H U J K  notes C to C on the keyboard layer
//        1 2 3  keyboard plays Synth A / B / C     - =  octave down / up
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "BinaryData.h"
#include "../ui/HHUi.h"

namespace hh = mt::hauntedhall;

namespace {

juce::Image imageFromBinary(const juce::String& file)
{
    int size = 0;
    const juce::String name = file.replaceCharacter('.', '_').replaceCharacter('-', '_');
    if (const char* data = BinaryData::getNamedResource(name.toRawUTF8(), size))
        return juce::ImageFileFormat::loadFrom(data, size_t(size));
    return {};
}

juce::MemoryBlock fontFromBinary(const juce::String& file)
{
    int size = 0;
    const juce::String name = file.replaceCharacter('.', '_').replaceCharacter('-', '_');
    if (const char* data = BinaryData::getNamedResource(name.toRawUTF8(), size)) return {data, size_t(size)};
    return {};
}

constexpr const char* kPianoKeys = "awsedftgyhujk";   // C .. C
constexpr const char* kDrumKeys = "zxcvb";

} // namespace

class MainComponent : public juce::Component, public juce::AudioIODeviceCallback, private juce::Timer
{
public:
    MainComponent() : grid_(engine_.project())
    {
        const juce::StringArray missing = assets_.load(imageFromBinary, fontFromBinary);
        if (!missing.isEmpty())
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Haunted Hall",
                                                   "Missing assets:\n" + missing.joinIntoString("\n"));
        // a groove and a bassline to start from
        grid_.generator().style = hh::GrooveStyle::CryptHop;
        grid_.generator().lane = hh::LaneStyle::Bassline;
        grid_.regenerate(0);

        editor_ = std::make_unique<hh::ui::HauntedHallEditor>(engine_, grid_, assets_);
        addAndMakeVisible(*editor_);
        setSize(1024, 600);
        setWantsKeyboardFocus(true);

        deviceManager_.initialiseWithDefaultDevices(0, 2);
        deviceManager_.addAudioCallback(this);
        startTimerHz(30);
    }

    ~MainComponent() override
    {
        stopTimer();
        deviceManager_.removeAudioCallback(this);
    }

    void resized() override { editor_->setBounds(getLocalBounds()); }

    // ---- audio --------------------------------------------------------------
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        sampleRate_ = device->getCurrentSampleRate();
        engine_.prepare(sampleRate_);
        scratch_.assign(size_t(std::max(4096, device->getCurrentBufferSizeSamples())), 0.0f);
    }

    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* out, int numOut, int n,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        for (int c = 0; c < numOut; ++c)
            if (out[c] != nullptr) juce::FloatVectorOperations::clear(out[c], n);
        if (numOut <= 0 || n > int(scratch_.size())) return;

        if (startRequested_.exchange(false)) beats_ = 0.0;
        hh::Transport t;
        t.playing = playing_.load();
        t.bpm = bpm_.load();
        t.beats = beats_;
        float* left = out[0];
        float* right = numOut > 1 && out[1] != nullptr ? out[1] : scratch_.data();
        engine_.process(left, right, n, t);
        if (t.playing) beats_ += double(n) * t.bpm / 60.0 / sampleRate_;
        beatsShared_.store(beats_);
    }

    // ---- keys ---------------------------------------------------------------
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::spaceKey)
        {
            const bool now = !playing_.load();
            if (now) startRequested_ = true;
            playing_ = now;
            return true;
        }
        if (k == juce::KeyPress::upKey) { bpm_ = std::min(240.0, bpm_.load() + 1.0); return true; }
        if (k == juce::KeyPress::downKey) { bpm_ = std::max(40.0, bpm_.load() - 1.0); return true; }
        if (k == juce::KeyPress::F2Key) { perf_ = !perf_; editor_->showPerformance(perf_); return true; }
        const juce::juce_wchar ch = juce::CharacterFunctions::toLowerCase(k.getTextCharacter());
        if (ch == '1' || ch == '2' || ch == '3')
        {
            editor_->context().keyboardLayer = hh::Layer(int(ch - '0'));
            return true;
        }
        if (ch == '-') { octave_ = std::max(-3, octave_ - 1); return true; }
        if (ch == '=') { octave_ = std::min(3, octave_ + 1); return true; }
        return juce::String(kPianoKeys).containsChar(ch) || juce::String(kDrumKeys).containsChar(ch);
    }

    bool keyStateChanged(bool) override
    {
        // Poll the mapped keys (the same approach as JUCE's on-screen keyboard),
        // so presses and releases are both caught.
        const hh::Layer layer = editor_->context().keyboardLayer;
        for (int i = 0; kPianoKeys[i] != 0; ++i)
        {
            const bool down = juce::KeyPress(kPianoKeys[i], 0, 0).isCurrentlyDown();
            const uint8_t note = uint8_t(60 + 12 * octave_ + i);
            if (down && !pianoDown_[size_t(i)]) { engine_.trigger(layer, note, 0.85f); pianoNote_[size_t(i)] = note; pianoLayer_[size_t(i)] = layer; }
            if (!down && pianoDown_[size_t(i)]) engine_.release(pianoLayer_[size_t(i)], pianoNote_[size_t(i)]);
            pianoDown_[size_t(i)] = down;
        }
        for (int r = 0; kDrumKeys[r] != 0; ++r)
        {
            const bool down = juce::KeyPress(kDrumKeys[r], 0, 0).isCurrentlyDown();
            if (down && !drumDown_[size_t(r)]) engine_.trigger(hh::Layer::Drum, hh::kDrumNotes[r], 0.9f);
            drumDown_[size_t(r)] = down;
        }
        return true;
    }

private:
    void timerCallback() override
    {
        editor_->setTransport(playing_.load(), bpm_.load(), beatsShared_.load());
        editor_->setAudioLoad(deviceManager_.getCpuUsage());
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        {
            const juce::String title = juce::String("Haunted Hall test  |  ") + (playing_.load() ? "PLAYING" : "STOPPED")
                                       + "  |  " + juce::String(juce::roundToInt(bpm_.load())) + " BPM  |  keys play Synth "
                                       + juce::String::charToString(juce::juce_wchar('A' + int(editor_->context().keyboardLayer) - 1))
                                       + "  |  SPACE play, F2 performance";
            if (w->getName() != title) w->setName(title);
        }
    }

    hh::Engine engine_;
    hh::PatternEditor grid_;
    hh::ui::Assets assets_;
    std::unique_ptr<hh::ui::HauntedHallEditor> editor_;
    juce::AudioDeviceManager deviceManager_;

    std::vector<float> scratch_;
    double sampleRate_ = 48000.0;
    double beats_ = 0.0;   // audio thread only
    std::atomic<double> beatsShared_{0.0}, bpm_{100.0};
    std::atomic<bool> playing_{false}, startRequested_{false};

    bool perf_ = false;
    int octave_ = 0;
    std::array<bool, 13> pianoDown_{};
    std::array<uint8_t, 13> pianoNote_{};
    std::array<hh::Layer, 13> pianoLayer_{};
    std::array<bool, 5> drumDown_{};
};

// ---------------------------------------------------------------------------
class HauntedHallApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Haunted Hall"; }
    const juce::String getApplicationVersion() override { return "0.3"; }

    void initialise(const juce::String&) override
    {
        window_ = std::make_unique<Window>();
    }
    void shutdown() override { window_.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class Window : public juce::DocumentWindow
    {
    public:
        Window() : DocumentWindow("Haunted Hall", juce::Colours::black, DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(false, false);
            centreWithSize(1024, 600);
            setVisible(true);
            getContentComponent()->grabKeyboardFocus();
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window_;
};

START_JUCE_APPLICATION(HauntedHallApp)
