// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHSynth.h"

namespace mt::hauntedhall {

namespace {

// 32-step, 4-bit wave tables (values -1..1 on 16 levels), built once.
struct WaveTables
{
    float v[int(WaveTable::Count)][32];
    WaveTables()
    {
        for (int i = 0; i < 32; ++i)
        {
            const double ph = double(i) / 32.0 * 6.283185307179586;
            double s[int(WaveTable::Count)];
            s[0] = std::sin(ph);                                                   // Soft
            s[1] = 0.6 * std::sin(ph) + 0.3 * std::sin(2 * ph) + 0.25 * std::sin(3 * ph)
                 + 0.12 * std::sin(4 * ph);                                        // Organ
            s[2] = 0.7 * std::sin(ph) + 0.35 * std::sin(3 * ph + 0.6)
                 + 0.2 * std::sin(5 * ph + 1.1);                                   // Ghost (hollow, uneven)
            s[3] = (i < 12 ? 0.9 : (i < 16 ? -0.2 : (i < 28 ? -0.9 : 0.2)));      // Hollow (notched square)
            for (int t = 0; t < int(WaveTable::Count); ++t)
            {
                double x = s[t];
                // normalise roughly to -1..1, then quantise to 16 levels (4-bit)
                x = x > 1.0 ? 1.0 : (x < -1.0 ? -1.0 : x);
                const double q = std::floor((x + 1.0) * 7.5 + 0.5) / 7.5 - 1.0;
                v[t][i] = float(q);
            }
        }
    }
};
const WaveTables& waveTables()
{
    static const WaveTables tables;
    return tables;
}

inline float macroScale(float m01, float octaves) { return std::exp2((m01 - 0.5f) * 2.0f * octaves); }

} // namespace

// ---------------------------------------------------------------------------
float ChipOsc::process(Wave w, float freq, float sr, float duty, WaveTable table, float fmRatio, float fmIndex)
{
    const float dt = clampf(freq / sr, 0.0f, 0.45f);
    float y = 0.0f;
    switch (w)
    {
        case Wave::Off: return 0.0f;
        case Wave::Pulse:
        {
            const float d = clampf(duty, 0.05f, 0.95f);
            y = phase_ < d ? 1.0f : -1.0f;
            y += polyBlep(phase_, dt);
            float t2 = phase_ - d;
            if (t2 < 0.0f) t2 += 1.0f;
            y -= polyBlep(t2, dt);
            y -= (2.0f * d - 1.0f);           // remove DC of uneven pulses
            break;
        }
        case Wave::Triangle:
        {
            const float tri = 1.0f - 4.0f * std::fabs(phase_ - 0.5f);
            y = std::floor((tri + 1.0f) * 7.5f + 0.5f) * (1.0f / 7.5f) - 1.0f;
            break;
        }
        case Wave::Noise:
            y = noise_.process(2000.0f + freq * 24.0f, sr, false);
            break;
        case Wave::NoiseTone:
            y = noise_.process(freq * 93.0f, sr, true);
            break;
        case Wave::Table:
        {
            int i = int(phase_ * 32.0f);
            if (i > 31) i = 31;
            y = waveTables().v[int(table)][i];
            break;
        }
        case Wave::Saw:
            y = 2.0f * phase_ - 1.0f - polyBlep(phase_, dt);
            break;
        case Wave::Fm:
        {
            const float mod = sineCycles(modPhase_);
            y = sineCycles(phase_ + fmIndex * 0.159154943f * mod);
            modPhase_ += dt * fmRatio;
            if (modPhase_ >= 1.0f) modPhase_ -= std::floor(modPhase_);
            break;
        }
    }
    phase_ += dt;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    return y;
}

// ---------------------------------------------------------------------------
void SynthVoice::prepare(float sr)
{
    sr_ = sr;
    kill();
}

void SynthVoice::kill()
{
    stage_ = Stage::Idle;
    env_ = 0.0f;
    filter_.reset();
    oscA_.reset();
    oscB_.reset(0.25f);
}

void SynthVoice::start(const SynthPatch& p, const uint8_t* notes, int count, float velocity, int gateSamples,
                       bool legato, uint32_t age)
{
    noteCount_ = std::max(1, std::min(count, kMaxChord));
    for (int i = 0; i < kMaxChord; ++i) notes_[i] = i < noteCount_ ? notes[i] : notes[0];
    velocity_ = clampf(velocity, 0.0f, 1.0f);
    gate_ = gateSamples;
    age_ = age;
    targetPitch_ = float(notes_[0] + p.transpose);
    arpIndex_ = 0;
    arpCounter_ = 0.0f;

    if (legato && gated())
        return;   // mono legato: keep the envelope running, glide to the new pitch

    if (stage_ == Stage::Idle || p.glide <= 0.0f)
        pitch_ = targetPitch_;
    if (stage_ == Stage::Idle)
    {
        // Free-running phases would make every hit identical; restarting the
        // oscillators at fixed phases keeps renders deterministic and punchy.
        oscA_.reset(0.0f);
        oscB_.reset(0.25f);
        filter_.reset();
    }
    sweep_ = p.sweepSemis;
    sweepCoef_ = expCoef(std::max(0.001f, p.sweepTime), sr_);
    fmIndexEnv_ = 1.0f;
    fmCoef_ = expCoef(std::max(0.005f, p.fmIndexDecay) * 0.72f, sr_);   // ~25 % after fmIndexDecay
    t_ = 0.0f;
    vibPhase_ = 0.0f;
    stage_ = Stage::Attack;
    controlCounter_ = 0;
}

void SynthVoice::release()
{
    if (stage_ != Stage::Idle) stage_ = Stage::Release;
}

void SynthVoice::updateControl(const SynthPatch& p, const LayerMacros& m)
{
    const float timeScale = macroScale(m.decay, 2.0f);
    attackInc_ = 1.0f / (std::max(0.0005f, p.attack) * sr_);
    decayCoef_ = expCoef(std::max(0.002f, p.decay * timeScale) * 0.25f, sr_);
    releaseCoef_ = expCoef(std::max(0.002f, p.release * timeScale) * 0.25f, sr_);

    // pitch: arp step, glide, sweep, vibrato
    float note = targetPitch_;
    if (p.arp && noteCount_ > 1)
        note = float(notes_[arpIndex_] + p.transpose);
    if (p.mono && p.glide > 0.0f)
    {
        const float g = 1.0f - expCoef(p.glide * 0.4f, sr_ / 16.0f);
        pitch_ += (note - pitch_) * g;
    }
    else
        pitch_ = note;

    float vib = 0.0f;
    if (p.vibDepth > 0.0f)
    {
        const float fadeIn = clampf((t_ - p.vibDelay) / 0.3f, 0.0f, 1.0f);
        vib = p.vibDepth * fadeIn * sineCycles(vibPhase_);
    }
    const float pitchA = pitch_ + sweep_ + vib;
    freqA_ = midiToHz(pitchA);
    freqB_ = midiToHz(pitchA + p.octaveB + p.detuneB * 0.01f);

    float pwm = 0.0f;
    if (p.pwmDepth > 0.0f) pwm = p.pwmDepth * sineCycles(pwmPhase_);
    dutyA_ = clampf(p.dutyA + pwm, 0.05f, 0.95f);
    dutyB_ = clampf(p.dutyB - pwm, 0.05f, 0.95f);

    // filter: base * tone macro * key tracking * envelope
    const float key = std::exp2((pitchA - 60.0f) / 12.0f * p.keyTrack);
    const float envOct = p.envOctaves * env_;
    const float cutoff = p.cutoff * macroScale(m.tone, 2.0f) * key * std::exp2(envOct);
    filter_.set(cutoff, p.resonance, sr_);
}

void SynthVoice::process(const SynthPatch& p, const LayerMacros& m, float* out, int n)
{
    if (stage_ == Stage::Idle) return;

    const float ctlDt = 16.0f / sr_;
    const float vibInc = p.vibRate * ctlDt;
    const float pwmInc = p.pwmRate * ctlDt;
    const float arpSamples = sr_ / std::max(1.0f, p.arpRate);
    const float amp = (0.35f + 0.65f * velocity_);
    const float driveGain = 1.0f + 3.0f * p.drive;
    const float driveNorm = 1.0f / (1.0f + 0.9f * p.drive);
    const float mixA = 1.0f - p.mixB, mixB = p.mixB;

    for (int i = 0; i < n; ++i)
    {
        if (controlCounter_-- <= 0)
        {
            controlCounter_ = 15;
            updateControl(p, m);
            vibPhase_ += vibInc;
            pwmPhase_ += pwmInc;
            if (vibPhase_ >= 1.0f) vibPhase_ -= 1.0f;
            if (pwmPhase_ >= 1.0f) pwmPhase_ -= 1.0f;
            t_ += ctlDt;
        }

        // envelope
        switch (stage_)
        {
            case Stage::Attack:
                env_ += attackInc_;
                if (env_ >= 1.0f) { env_ = 1.0f; stage_ = Stage::Decay; }
                break;
            case Stage::Decay:
                env_ = p.sustain + (env_ - p.sustain) * decayCoef_;
                if (std::fabs(env_ - p.sustain) < 1e-4f)
                {
                    env_ = p.sustain;
                    stage_ = Stage::Sustain;
                    if (p.sustain <= 0.0f) { stage_ = Stage::Idle; env_ = 0.0f; }
                }
                break;
            case Stage::Sustain: break;
            case Stage::Release:
                env_ *= releaseCoef_;
                if (env_ < 1e-5f) { env_ = 0.0f; stage_ = Stage::Idle; }
                break;
            case Stage::Idle: break;
        }
        if (stage_ == Stage::Idle) break;

        if (gate_ > 0 && --gate_ == 0) stage_ = Stage::Release;

        // arp, sweep, FM index envelopes (audio rate, cheap)
        if (p.arp && noteCount_ > 1)
        {
            arpCounter_ += 1.0f;
            if (arpCounter_ >= arpSamples)
            {
                arpCounter_ -= arpSamples;
                arpIndex_ = (arpIndex_ + 1) % noteCount_;
            }
        }
        sweep_ *= sweepCoef_;
        fmIndexEnv_ = 0.18f + (fmIndexEnv_ - 0.18f) * fmCoef_;

        const float fmI = p.fmIndex * fmIndexEnv_;
        float y = mixA * oscA_.process(p.waveA, freqA_, sr_, dutyA_, p.tableA, p.fmRatio, fmI);
        if (p.waveB != Wave::Off)
            y += mixB * oscB_.process(p.waveB, freqB_, sr_, dutyB_, p.tableB, p.fmRatio, fmI);
        if (p.drive > 0.0f) y = softSat(y * driveGain) * driveNorm;
        y = filter_.process(y);
        out[i] += y * env_ * amp;
    }
}

// ---------------------------------------------------------------------------
void SynthLayer::prepare(float sr, uint32_t seed)
{
    for (auto& v : voices_) v.prepare(sr);
    corrosion_.prepare(sr, seed);
    tailLen_ = int(sr * 0.12f);
    tail_ = 0;
    (void)waveTables();
}

void SynthLayer::setMacros(float decay, float tone, float spook)
{
    macros_.decay = decay;
    macros_.tone = tone;
    spook_ = spook;
}

void SynthLayer::noteOn(const uint8_t* notes, int count, float velocity, int gateSamples)
{
    if (count <= 0) return;
    ++ageCounter_;
    if (patch_.mono)
    {
        SynthVoice& v = voices_[0];
        const bool legato = v.gated() && patch_.glide > 0.0f;
        v.start(patch_, notes, std::min(count, SynthVoice::kMaxChord), velocity, gateSamples, legato, ageCounter_);
        return;
    }
    auto pickVoice = [&]() -> SynthVoice& {
        // free voice first, then the oldest released, then the oldest
        SynthVoice* best = nullptr;
        for (auto& v : voices_) if (!v.active()) return v;
        for (auto& v : voices_)
            if (!v.gated() && (!best || v.age() < best->age())) best = &v;
        if (best) return *best;
        best = &voices_[0];
        for (auto& v : voices_) if (v.age() < best->age()) best = &v;
        return *best;
    };
    if (patch_.arp)
    {
        pickVoice().start(patch_, notes, std::min(count, SynthVoice::kMaxChord), velocity, gateSamples, false, ageCounter_);
        return;
    }
    for (int i = 0; i < count && i < kVoices; ++i)
        pickVoice().start(patch_, &notes[i], 1, velocity, gateSamples, false, ageCounter_);
}

void SynthLayer::noteOff(uint8_t note)
{
    for (auto& v : voices_)
        if (v.heldByHost() && v.firstNote() == note) v.release();
}

void SynthLayer::releaseSequenced()
{
    for (auto& v : voices_)
        if (v.gated() && !v.heldByHost()) v.release();
}

void SynthLayer::allNotesOff()
{
    for (auto& v : voices_) v.release();
}

bool SynthLayer::active() const
{
    for (const auto& v : voices_) if (v.active()) return true;
    return tail_ > 0;
}

void SynthLayer::process(float* out, int n)
{
    std::fill(out, out + n, 0.0f);
    bool any = false;
    for (auto& v : voices_)
        if (v.active()) { v.process(patch_, macros_, out, n); any = true; }

    if (any) tail_ = tailLen_;
    else if (tail_ > 0) tail_ = std::max(0, tail_ - n);
    else return;   // silent and settled: skip corrosion

    corrosion_.setAmount(clampf(patch_.corrosion + spook_, 0.0f, 1.0f));
    corrosion_.process(out, n);
    const float lvl = patch_.level;
    for (int i = 0; i < n; ++i) out[i] *= lvl;
}

} // namespace mt::hauntedhall
