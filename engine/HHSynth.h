// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: the console-style synth voice and the 4-voice synth layer.
#pragma once

#include "HHCommon.h"
#include "HHCorrosion.h"

namespace mt::hauntedhall {

// Oscillator shapes. The first group is the 8-bit side (stepped and raw),
// the last two are the 16-bit side.
enum class Wave : uint8_t
{
    Off,
    Pulse,       // variable-width pulse (12.5 / 25 / 50 % or anything between)
    Triangle,    // 32-step, 4-bit stepped triangle
    Noise,       // long LFSR noise (hiss)
    NoiseTone,   // short LFSR noise: a pitched metallic buzz
    Table,       // 32-sample, 4-bit wave table (see WaveTable)
    Saw,         // band-limited saw
    Fm           // 2-operator FM (sine carrier, sine modulator)
};

enum class WaveTable : uint8_t { Soft, Organ, Ghost, Hollow, Count };

// Everything a synth preset sets. Plain data with defaults; presets are built
// by changing only the fields that matter (see HHPresets.cpp).
struct SynthPatch
{
    // oscillators
    Wave waveA = Wave::Pulse;   float dutyA = 0.5f;  WaveTable tableA = WaveTable::Soft;
    Wave waveB = Wave::Off;     float dutyB = 0.25f; WaveTable tableB = WaveTable::Soft;
    float mixB = 0.5f;          // 0 = A only, 1 = B only
    float octaveB = 0.0f;       // semitones offset of B (e.g. 12, -12, 7)
    float detuneB = 0.0f;       // cents
    float fmRatio = 2.0f, fmIndex = 0.0f, fmIndexDecay = 0.3f;  // FM index falls to 25 % over this time
    // amplitude envelope (seconds, sustain 0..1)
    float attack = 0.002f, decay = 0.25f, sustain = 0.7f, release = 0.2f;
    // filter
    float cutoff = 6000.0f, resonance = 0.1f;
    float envOctaves = 0.0f;    // filter envelope depth (uses the amp envelope shape)
    float keyTrack = 0.5f;      // 0..1, cutoff follows pitch
    // pitch movement
    float vibDepth = 0.0f, vibRate = 5.5f, vibDelay = 0.25f;     // semitones, Hz, s
    float sweepSemis = 0.0f, sweepTime = 0.03f;                  // start offset that falls to 0
    float pwmDepth = 0.0f, pwmRate = 0.5f;                       // pulse width LFO
    // play mode
    bool arp = false;  float arpRate = 30.0f;  // chords cycle their notes at this rate (chip "fake chords")
    bool mono = false; float glide = 0.0f;     // mono legato with glide time (s)
    // colour
    float drive = 0.0f;         // 0..1 soft saturation
    float corrosion = 0.0f;     // baked-in cartridge wear, added to SPOOK
    float level = 0.5f;
    int transpose = 0;          // semitones
};

struct LayerMacros
{
    float decay = 0.5f;   // 0..1, scales decay/release 1/4x .. 4x
    float tone = 0.5f;    // 0..1, scales cutoff -2 .. +2 octaves
};

class ChipOsc
{
public:
    void reset(float phase = 0.0f) { phase_ = phase; modPhase_ = 0.0f; noise_.reset(); }
    float process(Wave w, float freq, float sr, float duty, WaveTable table, float fmRatio, float fmIndex);

private:
    float phase_ = 0.0f, modPhase_ = 0.0f;
    LfsrNoise noise_;
};

class SynthVoice
{
public:
    static constexpr int kMaxChord = 3;

    void prepare(float sr);
    // gateSamples < 0: held until release()
    void start(const SynthPatch& p, const uint8_t* notes, int count, float velocity, int gateSamples, bool legato, uint32_t age);
    void release();
    void kill();
    bool active() const { return stage_ != Stage::Idle; }
    bool gated() const { return stage_ != Stage::Idle && stage_ != Stage::Release; }
    bool heldByHost() const { return gate_ < 0 && gated(); }
    uint8_t firstNote() const { return notes_[0]; }
    uint32_t age() const { return age_; }
    float level() const { return env_; }
    // ADDS into out
    void process(const SynthPatch& p, const LayerMacros& m, float* out, int n);

private:
    enum class Stage : uint8_t { Idle, Attack, Decay, Sustain, Release };
    void updateControl(const SynthPatch& p, const LayerMacros& m);

    float sr_ = 48000.0f;
    ChipOsc oscA_, oscB_;
    SvfLowpass filter_;
    Stage stage_ = Stage::Idle;
    float env_ = 0.0f, attackInc_ = 1.0f, decayCoef_ = 0.0f, releaseCoef_ = 0.0f;
    float velocity_ = 1.0f;
    int gate_ = -1;
    uint8_t notes_[kMaxChord] = {60, 0, 0};
    int noteCount_ = 1, arpIndex_ = 0;
    float arpCounter_ = 0.0f;
    float pitch_ = 60.0f, targetPitch_ = 60.0f;
    float sweep_ = 0.0f, sweepCoef_ = 0.0f;
    float fmIndexEnv_ = 1.0f, fmCoef_ = 0.0f;
    float vibPhase_ = 0.0f, pwmPhase_ = 0.0f, t_ = 0.0f;
    float freqA_ = 261.6f, freqB_ = 261.6f, dutyA_ = 0.5f, dutyB_ = 0.25f;
    int controlCounter_ = 0;
    uint32_t age_ = 0;
};

class SynthLayer
{
public:
    static constexpr int kVoices = 4;

    void prepare(float sr, uint32_t seed);
    void setPatch(const SynthPatch& p) { patch_ = p; }
    const SynthPatch& patch() const { return patch_; }
    void setMacros(float decay, float tone, float spook);

    // Notes. gateSamples < 0: held until noteOff().
    void noteOn(const uint8_t* notes, int count, float velocity, int gateSamples);
    void noteOff(uint8_t note);
    void releaseSequenced();   // let go of sequencer-gated notes (transport stop)
    void allNotesOff();

    bool active() const;
    // WRITES mono into out (not adds).
    void process(float* out, int n);

private:
    SynthVoice voices_[kVoices];
    SynthPatch patch_;
    LayerMacros macros_;
    Corrosion corrosion_;
    float spook_ = 0.0f;
    uint32_t ageCounter_ = 0;
    int tail_ = 0, tailLen_ = 4800;
};

} // namespace mt::hauntedhall
