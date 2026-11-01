// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: the engine. The only class a host talks to.
//
//   drum kit ──┐                       ┌─ dry ───────────────────────┐
//   synth A ───┤ 4-channel line mixer ─┤                             ├─ LEVEL ─ limiter ─ out
//   synth B ───┤ (level, on, mute,     └─ sends ─ master delay ─ MIX ┘
//   synth C ───┘  pan, delay send)        (drum send capped at 20 %)
//
// Patterns: two per project, 8 or 16 bars, synced to the host's clock (the
// host passes its tempo and beat position each block). The same patterns can
// also be exported as plain notes (exportNotes in HHPattern.h) so a host
// sequencer can own them instead.
#pragma once

#include "HHDelay.h"
#include "HHDrums.h"
#include "HHPattern.h"
#include "HHPresets.h"
#include "HHSynth.h"

namespace mt::hauntedhall {

enum class Layer : uint8_t { Drum, A, B, C, Count };
constexpr int kLayers = int(Layer::Count);

// The seven controls every channel has. DECAY / TONE / SPOOK / SPACE are the
// four knobs on each layer's page (KICK's four hardware knobs).
enum class ChannelParam : uint8_t { Level, On, Mute, Decay, Tone, Spook, Space, Count };
constexpr int kChannelParams = int(ChannelParam::Count);

enum class Param : uint16_t
{
    // 4 channels x 7 (see channelParam())
    DrumLevel, DrumOn, DrumMute, DrumDecay, DrumTone, DrumSpook, DrumSpace,
    ALevel, AOn, AMute, ADecay, ATone, ASpook, ASpace,
    BLevel, BOn, BMute, BDecay, BTone, BSpook, BSpace,
    CLevel, COn, CMute, CDecay, CTone, CSpook, CSpace,
    // master delay (the four knobs on the master page are TIME, FEEDBACK, MIX, LEVEL)
    DelayTime, DelaySync, DelayFeedback, DelayMix, DelayLevel, DelayBypass,
    // which synth layer the melody/chord lane plays (0 = A, 1 = B, 2 = C)
    MelodyLayer,
    Count
};
constexpr int kNumParams = int(Param::Count);

constexpr Param channelParam(Layer l, ChannelParam p) { return Param(int(l) * kChannelParams + int(p)); }

// The drum channel's SPACE knob (0..1) sends at most this much to the delay.
constexpr float kDrumDelaySendMax = 0.2f;
// Mixer headroom: four layers at unity sum without slamming the limiter
// (single layers peak around -8 to -12 dBFS, a full mix around -3 dBFS).
constexpr float kChannelHeadroom = 0.56f;   // -5 dB

struct ParamInfo
{
    const char* id;      // stable identifier for saving
    const char* label;   // what the screen shows
    float min, max, def;
    const char* unit;
    bool stepped;        // switches and choices
};

struct Transport
{
    bool playing = false;
    double bpm = 120.0;
    double beats = 0.0;   // host position at the start of the block, in quarter notes
};

class Engine
{
public:
    static constexpr int kChunk = 256;     // internal processing chunk
    static constexpr int kMaxHostEvents = 128;

    struct State
    {
        float params[kNumParams];
        int presets[kLayers];
        PatternData patterns[kPatterns];
        int requestedPattern;
    };

    Engine();

    // Allocates everything (about 1.2 MB at 48 kHz: 1 MB of delay line, 128 KB of corrosion history).
    // Call before audio starts, or when the sample rate changes.
    void prepare(double sampleRate);
    double sampleRate() const { return sr_; }

    // ---- parameters (any thread) ----
    void setParam(Param p, float value);
    float getParam(Param p) const { return params_[int(p)].load(std::memory_order_relaxed); }
    static const ParamInfo& paramInfo(Param p);

    // ---- presets (UI thread) ----
    // Drum layer: kit preset index. Synth layers: synth preset index.
    void loadPreset(Layer layer, int index);
    int presetIndex(Layer layer) const { return presetReq_[int(layer)].load(std::memory_order_relaxed); }

    // ---- patterns (UI thread) ----
    Project& project() { return project_; }
    const Project& project() const { return project_; }
    void requestPattern(int index) { project_.requested.store(index % kPatterns, std::memory_order_relaxed); }

    // ---- notes ----
    // Any thread: plays at the start of the next block (pads, previews).
    bool trigger(Layer layer, uint8_t note, float velocity);
    bool release(Layer layer, uint8_t note);
    // Audio thread only, sample-accurate inside the next process() call.
    void noteOn(Layer layer, uint8_t note, float velocity, int sampleOffset);
    void noteOff(Layer layer, uint8_t note, int sampleOffset);

    // ---- audio thread ----
    // ADDS stereo output into left/right.
    void process(float* left, float* right, int numSamples, const Transport& transport);

    // ---- feedback for the screens (any thread) ----
    int currentStep() const { return currentStep_.load(std::memory_order_relaxed); }      // -1 when stopped
    int playingPattern() const { return playingPattern_.load(std::memory_order_relaxed); }
    float channelPeak(Layer layer) const { return peaks_[int(layer)].load(std::memory_order_relaxed); }

    // ---- state (UI thread) ----
    void captureState(State& out) const;
    void loadState(const State& in);

private:
    struct Event
    {
        int offset;
        uint8_t kind;     // see .cpp
        uint8_t layer;
        uint8_t count;
        uint8_t notes[3];
        float velocity;
        int gate;
    };
    struct QueuedNote { uint8_t layer, note, on; float velocity; };

    void applyParams(double bpm);
    void handleEvent(const Event& e);
    void addStepEvents(long long globalStep, int offset, double samplesPerStep, Event* list, int& count, int cap);
    void renderSources(int start, int end);
    float smoothGain(int ch, float target, int n, float* ramp);

    double sr_ = 48000.0;
    std::atomic<float> params_[kNumParams];
    std::atomic<int> presetReq_[kLayers];
    int presetApplied_[kLayers] = {-1, -1, -1, -1};
    Project project_;

    DrumKit kit_;
    SynthLayer synths_[3];
    DigitalDelay delay_;

    MpscQueue<QueuedNote, 64> queue_;
    Event hostEvents_[kMaxHostEvents];
    int hostEventCount_ = 0;

    // per-block settings
    bool chOn_[kLayers] = {}, chAudible_[kLayers] = {};
    float chGainTarget_[kLayers] = {}, chGain_[kLayers] = {}, chSend_[kLayers] = {};
    float panL_[kLayers] = {}, panR_[kLayers] = {};
    float delayMix_ = 0.3f, outLevel_ = 1.0f, outLevelCur_ = 1.0f;
    int melodyLayer_ = 0;

    // sequencer
    bool wasPlaying_ = false;
    long long lastStep_ = 0;
    long long patternOrigin_ = 0;
    int activePattern_ = 0;
    std::atomic<int> currentStep_{-1};
    std::atomic<int> playingPattern_{0};

    // buffers
    float buf_[kLayers][kChunk] = {};
    float send_[kChunk] = {}, wetL_[kChunk] = {}, wetR_[kChunk] = {};

    // output
    float limEnv_ = 0.0f, limRel_ = 0.999f;
    float dcR_ = 0.999f, dcXL_ = 0.0f, dcYL_ = 0.0f, dcXR_ = 0.0f, dcYR_ = 0.0f;   // master DC blocker
    std::atomic<float> peaks_[kLayers];
    float peakDecay_ = 0.9f;
};

// ---- offline export (worker thread, never the audio thread) -----------------
// Frames needed to render one pass of a pattern plus a tail for the delay.
size_t framesForPattern(int bars, double bpm, double sampleRate, double tailSeconds = 2.0);
// Renders pattern `pattern` of `state` once through, from beat 0, into
// left/right (overwritten). `scratch` is a second Engine, separate from the
// live one so playback is untouched; this prepares it (allocates), so call it
// from a worker thread, never the audio thread.
size_t renderPatternOffline(Engine& scratch, const Engine::State& state, int pattern, double bpm,
                            double sampleRate, float* left, float* right, size_t frames);

} // namespace mt::hauntedhall
