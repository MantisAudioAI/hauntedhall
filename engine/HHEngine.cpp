// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHEngine.h"

#include <cstring>

namespace mt::hauntedhall {

namespace {

enum EventKind : uint8_t { kDrumHit, kSynthOn, kSynthOff };

const ParamInfo kParamInfo[kNumParams] = {
    // id                label        min    max     def     unit  stepped
    {"drum_level",   "LEVEL",    0.0f, 1.0f, 0.8f, "",   false},
    {"drum_on",      "ON",       0.0f, 1.0f, 1.0f, "",   true},
    {"drum_mute",    "MUTE",     0.0f, 1.0f, 0.0f, "",   true},
    {"drum_decay",   "DECAY",    0.0f, 1.0f, 0.5f, "",   false},
    {"drum_tone",    "TONE",     0.0f, 1.0f, 0.5f, "",   false},
    {"drum_spook",   "SPOOK",    0.0f, 1.0f, 0.0f, "",   false},
    {"drum_space",   "SPACE",    0.0f, 1.0f, 0.5f, "",   false},   // x 0.2 = actual send
    {"a_level",      "LEVEL",    0.0f, 1.0f, 0.8f, "",   false},
    {"a_on",         "ON",       0.0f, 1.0f, 1.0f, "",   true},
    {"a_mute",       "MUTE",     0.0f, 1.0f, 0.0f, "",   true},
    {"a_decay",      "DECAY",    0.0f, 1.0f, 0.5f, "",   false},
    {"a_tone",       "TONE",     0.0f, 1.0f, 0.5f, "",   false},
    {"a_spook",      "SPOOK",    0.0f, 1.0f, 0.0f, "",   false},
    {"a_space",      "SPACE",    0.0f, 1.0f, 0.4f, "",   false},
    {"b_level",      "LEVEL",    0.0f, 1.0f, 0.8f, "",   false},
    {"b_on",         "ON",       0.0f, 1.0f, 1.0f, "",   true},
    {"b_mute",       "MUTE",     0.0f, 1.0f, 0.0f, "",   true},
    {"b_decay",      "DECAY",    0.0f, 1.0f, 0.5f, "",   false},
    {"b_tone",       "TONE",     0.0f, 1.0f, 0.5f, "",   false},
    {"b_spook",      "SPOOK",    0.0f, 1.0f, 0.0f, "",   false},
    {"b_space",      "SPACE",    0.0f, 1.0f, 0.4f, "",   false},
    {"c_level",      "LEVEL",    0.0f, 1.0f, 0.8f, "",   false},
    {"c_on",         "ON",       0.0f, 1.0f, 1.0f, "",   true},
    {"c_mute",       "MUTE",     0.0f, 1.0f, 0.0f, "",   true},
    {"c_decay",      "DECAY",    0.0f, 1.0f, 0.5f, "",   false},
    {"c_tone",       "TONE",     0.0f, 1.0f, 0.5f, "",   false},
    {"c_spook",      "SPOOK",    0.0f, 1.0f, 0.0f, "",   false},
    {"c_space",      "SPACE",    0.0f, 1.0f, 0.4f, "",   false},
    {"delay_time",   "TIME",    20.0f, 1500.0f, 375.0f, "ms", false},
    {"delay_sync",   "SYNC",     0.0f, 6.0f, 0.0f, "",   true},     // free, 1/4, 1/8., 1/8, 1/8T, 1/16, 1/2
    {"delay_fb",     "FEEDBACK", 0.0f, 0.9f, 0.35f, "",  false},
    {"delay_mix",    "MIX",      0.0f, 1.0f, 0.3f, "",   false},
    {"delay_level",  "LEVEL",    0.0f, 1.0f, 0.8f, "",   false},    // pedal output; 0.8 = unity
    {"delay_bypass", "BYPASS",   0.0f, 1.0f, 0.0f, "",   true},
    {"melody_layer", "MELODY",   0.0f, 2.0f, 0.0f, "",   true},
};

constexpr float kSyncBeats[7] = {0.0f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f, 2.0f};
constexpr float kPan[kLayers] = {0.0f, -0.25f, 0.25f, 0.0f};

inline float faderGain(float f) { return f * f * 1.5625f; }   // 0.8 -> unity, 1.0 -> +3.9 dB

} // namespace

const ParamInfo& Engine::paramInfo(Param p) { return kParamInfo[int(p) % kNumParams]; }

Engine::Engine()
{
    for (int i = 0; i < kNumParams; ++i) params_[i].store(kParamInfo[i].def, std::memory_order_relaxed);
    const int defaults[kLayers] = {0, findSynthPreset("Dungeon Bass"), findSynthPreset("Haunted Choir"),
                                   findSynthPreset("Crystal Lead")};
    for (int l = 0; l < kLayers; ++l)
    {
        presetReq_[l].store(std::max(0, defaults[l]), std::memory_order_relaxed);
        peaks_[l].store(0.0f, std::memory_order_relaxed);
    }
    for (int l = 0; l < kLayers; ++l)
    {
        const float a = (kPan[l] + 1.0f) * 0.25f * 3.14159265f;
        panL_[l] = std::cos(a) * 1.41421356f;
        panR_[l] = std::sin(a) * 1.41421356f;
    }
}

void Engine::prepare(double sampleRate)
{
    sr_ = sampleRate;
    const float sr = float(sampleRate);
    (void)sineTable();
    (void)synthPreset(0);
    (void)kitPreset(0);
    kit_.prepare(sr, 0xD5u);
    for (int i = 0; i < 3; ++i) synths_[i].prepare(sr, 0xA1u + uint32_t(i) * 0x101u);
    delay_.prepare(sr);
    for (auto& a : presetApplied_) a = -1;
    hostEventCount_ = 0;
    wasPlaying_ = false;
    activePattern_ = project_.requested.load(std::memory_order_relaxed);
    playingPattern_.store(activePattern_, std::memory_order_relaxed);
    currentStep_.store(-1, std::memory_order_relaxed);
    limEnv_ = 0.0f;
    limRel_ = expCoef(0.08f, sr);
    dcR_ = 1.0f - kTwoPi * 8.0f / sr;   // 8 Hz
    dcXL_ = dcYL_ = dcXR_ = dcYR_ = 0.0f;
    peakDecay_ = std::pow(expCoef(0.3f, sr), float(kChunk));
    for (int l = 0; l < kLayers; ++l)
    {
        chGain_[l] = 0.0f;
        peaks_[l].store(0.0f, std::memory_order_relaxed);
    }
    applyParams(120.0);
    for (int l = 0; l < kLayers; ++l) chGain_[l] = chGainTarget_[l];
    outLevelCur_ = outLevel_;
}

void Engine::setParam(Param p, float v)
{
    const ParamInfo& info = paramInfo(p);
    v = clampf(v, info.min, info.max);
    if (info.stepped) v = std::floor(v + 0.5f);
    params_[int(p)].store(v, std::memory_order_relaxed);
}

void Engine::loadPreset(Layer layer, int index)
{
    const int n = layer == Layer::Drum ? numKitPresets() : numSynthPresets();
    presetReq_[int(layer)].store(std::max(0, std::min(index, n - 1)), std::memory_order_relaxed);
}

bool Engine::trigger(Layer layer, uint8_t note, float velocity)
{
    return queue_.push({uint8_t(layer), note, 1, velocity});
}

bool Engine::release(Layer layer, uint8_t note)
{
    return queue_.push({uint8_t(layer), note, 0, 0.0f});
}

void Engine::noteOn(Layer layer, uint8_t note, float velocity, int offset)
{
    if (hostEventCount_ >= kMaxHostEvents) return;
    Event e{};
    e.offset = std::max(0, offset);
    e.layer = uint8_t(layer);
    e.velocity = clampf(velocity, 0.0f, 1.0f);
    if (layer == Layer::Drum)
    {
        const int row = drumRowForNote(note);
        if (row < 0) return;
        e.kind = kDrumHit;
        e.notes[0] = uint8_t(row);
    }
    else
    {
        e.kind = kSynthOn;
        e.notes[0] = note;
        e.count = 1;
        e.gate = -1;
    }
    hostEvents_[hostEventCount_++] = e;
}

void Engine::noteOff(Layer layer, uint8_t note, int offset)
{
    if (layer == Layer::Drum || hostEventCount_ >= kMaxHostEvents) return;
    Event e{};
    e.offset = std::max(0, offset);
    e.kind = kSynthOff;
    e.layer = uint8_t(layer);
    e.notes[0] = note;
    hostEvents_[hostEventCount_++] = e;
}

void Engine::applyParams(double bpm)
{
    auto get = [this](Param p) { return params_[int(p)].load(std::memory_order_relaxed); };

    for (int l = 0; l < kLayers; ++l)
    {
        const Layer layer = Layer(l);
        const bool on = get(channelParam(layer, ChannelParam::On)) >= 0.5f;
        const bool mute = get(channelParam(layer, ChannelParam::Mute)) >= 0.5f;
        if (chOn_[l] && !on)
        {
            if (l > 0) synths_[l - 1].allNotesOff();
        }
        chOn_[l] = on;
        chAudible_[l] = on && !mute;
        chGainTarget_[l] = chAudible_[l] ? faderGain(get(channelParam(layer, ChannelParam::Level))) * kChannelHeadroom : 0.0f;
        float space = get(channelParam(layer, ChannelParam::Space));
        if (layer == Layer::Drum) space *= kDrumDelaySendMax;   // the drum send never exceeds 20 %
        chSend_[l] = space;

        const float decay = get(channelParam(layer, ChannelParam::Decay));
        const float tone = get(channelParam(layer, ChannelParam::Tone));
        const float spook = get(channelParam(layer, ChannelParam::Spook));
        const int want = presetReq_[l].load(std::memory_order_relaxed);
        if (layer == Layer::Drum)
        {
            if (want != presetApplied_[l]) { kit_.setPatch(kitPreset(want).patch); presetApplied_[l] = want; }
            kit_.setMacros(decay, tone, spook);
        }
        else
        {
            SynthLayer& s = synths_[l - 1];
            if (want != presetApplied_[l]) { s.setPatch(synthPreset(want).patch); presetApplied_[l] = want; }
            s.setMacros(decay, tone, spook);
        }
    }

    const int sync = int(get(Param::DelaySync) + 0.5f);
    float ms = get(Param::DelayTime);
    if (sync > 0 && sync < 7 && bpm > 0.0) ms = float(kSyncBeats[sync] * 60000.0 / bpm);
    delay_.setTimeMs(ms);
    delay_.setFeedback(get(Param::DelayFeedback));
    delay_.setBypass(get(Param::DelayBypass) >= 0.5f);
    delayMix_ = get(Param::DelayMix);
    outLevel_ = faderGain(get(Param::DelayLevel));
    melodyLayer_ = std::max(0, std::min(2, int(get(Param::MelodyLayer) + 0.5f)));
}

void Engine::handleEvent(const Event& e)
{
    if (e.layer >= kLayers || !chOn_[e.layer]) return;
    switch (e.kind)
    {
        case kDrumHit: kit_.trigger(e.notes[0], e.velocity); break;
        case kSynthOn: synths_[e.layer - 1].noteOn(e.notes, e.count, e.velocity, e.gate); break;
        case kSynthOff: synths_[e.layer - 1].noteOff(e.notes[0]); break;
        default: break;
    }
}

void Engine::addStepEvents(long long globalStep, int offset, double samplesPerStep, Event* list, int& count, int cap)
{
    long long rel = globalStep - patternOrigin_;
    int steps = project_.patterns[activePattern_].steps();
    if (rel >= 0 && rel % steps == 0)
    {
        // Pattern boundary: switch to a newly requested pattern here.
        const int req = project_.requested.load(std::memory_order_relaxed);
        if (req != activePattern_)
        {
            activePattern_ = req;
            patternOrigin_ = globalStep;
            rel = 0;
            steps = project_.patterns[activePattern_].steps();
            playingPattern_.store(activePattern_, std::memory_order_relaxed);
        }
    }
    if (rel < 0) return;
    const int ps = int(rel % steps);
    currentStep_.store(ps, std::memory_order_relaxed);
    const Pattern& pt = project_.patterns[activePattern_];

    for (int r = 0; r < kDrumRows && count < cap; ++r)
    {
        const uint8_t v = pt.drums[ps][r].load(std::memory_order_relaxed);
        if (!v) continue;
        Event e{};
        e.offset = offset; e.kind = kDrumHit; e.layer = uint8_t(Layer::Drum);
        e.notes[0] = uint8_t(r); e.velocity = float(v) / 127.0f;
        list[count++] = e;
    }
    const MelodyStep m = MelodyStep::unpack(pt.melody[ps].load(std::memory_order_relaxed));
    if (m.count > 0 && count < cap)
    {
        Event e{};
        e.offset = offset; e.kind = kSynthOn; e.layer = uint8_t(1 + melodyLayer_);
        e.count = m.count;
        for (int i = 0; i < 3; ++i) e.notes[i] = m.notes[i];
        e.velocity = float(m.velocity) / 127.0f;
        e.gate = std::max(1, int(m.length * samplesPerStep * 0.92));
        list[count++] = e;
    }
}

void Engine::renderSources(int start, int end)
{
    const int n = end - start;
    if (n <= 0) return;
    if (chOn_[0] || kit_.active()) kit_.process(buf_[0] + start, n);
    else std::fill(buf_[0] + start, buf_[0] + end, 0.0f);
    for (int s = 0; s < 3; ++s)
    {
        if (chOn_[s + 1] || synths_[s].active()) synths_[s].process(buf_[s + 1] + start, n);
        else std::fill(buf_[s + 1] + start, buf_[s + 1] + end, 0.0f);
    }
}

void Engine::process(float* left, float* right, int numSamples, const Transport& transport)
{
    ScopedFlushDenormals ftz;

    // Notes from other threads land at the start of this block.
    QueuedNote q;
    while (queue_.pop(q))
    {
        if (q.on) noteOn(Layer(q.layer), q.note, q.velocity, 0);
        else noteOff(Layer(q.layer), q.note, 0);
    }

    const double bpm = transport.bpm > 1.0 ? transport.bpm : 120.0;
    applyParams(bpm);

    const double samplesPerBeat = sr_ * 60.0 / bpm;
    const double samplesPerStep = samplesPerBeat * 0.25;
    const double beatsPerSample = 1.0 / samplesPerBeat;

    if (transport.playing && !wasPlaying_)
    {
        // Transport start: the pattern begins on the first step at or after here.
        patternOrigin_ = (long long)std::ceil(transport.beats * 4.0 - 1e-6);
        lastStep_ = patternOrigin_ - 1;
        activePattern_ = project_.requested.load(std::memory_order_relaxed);
        playingPattern_.store(activePattern_, std::memory_order_relaxed);
    }
    if (!transport.playing && wasPlaying_)
    {
        for (auto& s : synths_) s.releaseSequenced();
        currentStep_.store(-1, std::memory_order_relaxed);
    }
    wasPlaying_ = transport.playing;

    float chunkPeak[kLayers] = {};

    for (int c0 = 0; c0 < numSamples; c0 += kChunk)
    {
        const int m = std::min(kChunk, numSamples - c0);

        // gather this chunk's events
        Event ev[kMaxHostEvents + 64];
        int evCount = 0;
        for (int i = 0; i < hostEventCount_; ++i)
        {
            const int o = hostEvents_[i].offset;
            if ((o >= c0 && o < c0 + m) || (c0 + m >= numSamples && o >= numSamples))
            {
                ev[evCount] = hostEvents_[i];
                ev[evCount].offset = std::min(o - c0, m - 1);
                ++evCount;
            }
        }
        if (transport.playing)
        {
            const double b0 = transport.beats + double(c0) * beatsPerSample;
            const double b1 = b0 + double(m) * beatsPerSample;
            // Steps whose time falls in [b0, b1). lastStep_ guards against a
            // boundary being played twice when host and engine round differently;
            // a jump backwards (host loop) is followed.
            long long k = (long long)std::ceil(b0 * 4.0 - 1e-6);
            if (k <= lastStep_ && k >= lastStep_ - 1) k = lastStep_ + 1;
            for (; double(k) * 0.25 < b1 - 1e-9; ++k)
            {
                lastStep_ = k;
                const int off = std::max(0, std::min(m - 1, int((double(k) * 0.25 - b0) * samplesPerBeat + 0.5)));
                addStepEvents(k, off, samplesPerStep, ev, evCount, kMaxHostEvents + 64);
            }
        }

        // idle: nothing sounding, nothing to start, delay quiet
        bool busy = evCount > 0 || !delay_.silent();
        for (int s = 0; s < 3 && !busy; ++s) busy = synths_[s].active();
        if (!busy) busy = kit_.active();
        if (!busy) continue;

        // sort by offset (stable insertion sort; lists are tiny)
        for (int i = 1; i < evCount; ++i)
        {
            Event key = ev[i];
            int j = i - 1;
            while (j >= 0 && ev[j].offset > key.offset) { ev[j + 1] = ev[j]; --j; }
            ev[j + 1] = key;
        }

        // render the sources in segments that end exactly on each event
        int pos = 0;
        for (int i = 0; i < evCount; ++i)
        {
            renderSources(pos, ev[i].offset);
            pos = std::max(pos, ev[i].offset);
            handleEvent(ev[i]);
        }
        renderSources(pos, m);

        // mix: 4 line channels -> dry bus + delay send
        std::fill(send_, send_ + m, 0.0f);
        std::fill(wetL_, wetL_ + m, 0.0f);
        std::fill(wetR_, wetR_ + m, 0.0f);
        float dryL[kChunk], dryR[kChunk];
        std::fill(dryL, dryL + m, 0.0f);
        std::fill(dryR, dryR + m, 0.0f);
        for (int l = 0; l < kLayers; ++l)
        {
            const float g0 = chGain_[l], g1 = chGainTarget_[l];
            chGain_[l] = g1;
            if (g0 == 0.0f && g1 == 0.0f) continue;
            const float gStep = (g1 - g0) / float(m);
            const float* b = buf_[l];
            const float pl = panL_[l], pr = panR_[l], snd = chSend_[l];
            float g = g0, pk = 0.0f;
            for (int i = 0; i < m; ++i, g += gStep)
            {
                const float x = b[i] * g;
                dryL[i] += x * pl;
                dryR[i] += x * pr;
                send_[i] += x * snd;
                pk = std::max(pk, std::fabs(x));
            }
            chunkPeak[l] = std::max(chunkPeak[l], pk);
        }
        delay_.process(send_, wetL_, wetR_, m);

        // master: dry + wet * MIX, pedal LEVEL, then a peak limiter and soft clip (ceiling -0.3 dBFS)
        const float lv0 = outLevelCur_, lv1 = outLevel_;
        outLevelCur_ = lv1;
        const float lvStep = (lv1 - lv0) / float(m);
        float lv = lv0;
        const float thr = 0.89f;
        for (int i = 0; i < m; ++i, lv += lvStep)
        {
            float l = (dryL[i] + wetL_[i] * delayMix_) * lv;
            float r = (dryR[i] + wetR_[i] * delayMix_) * lv;
            dcYL_ = l - dcXL_ + dcR_ * dcYL_; dcXL_ = l; l = dcYL_;
            dcYR_ = r - dcXR_ + dcR_ * dcYR_; dcXR_ = r; r = dcYR_;
            const float pk = std::max(std::fabs(l), std::fabs(r));
            limEnv_ = pk > limEnv_ ? pk : limEnv_ * limRel_;
            if (limEnv_ > thr) { const float g = thr / limEnv_; l *= g; r *= g; }
            auto clip = [](float x) {
                const float ax = std::fabs(x);
                if (ax <= 0.85f) return x;
                const float y = 0.85f + 0.116f * softSat((ax - 0.85f) / 0.116f);
                return x < 0.0f ? -y : y;
            };
            left[c0 + i] += clip(l);
            right[c0 + i] += clip(r);
        }
    }
    hostEventCount_ = 0;

    // meters
    for (int l = 0; l < kLayers; ++l)
    {
        const float old = peaks_[l].load(std::memory_order_relaxed) * peakDecay_;
        peaks_[l].store(std::max(old, chunkPeak[l]), std::memory_order_relaxed);
    }
}

void Engine::captureState(State& out) const
{
    for (int i = 0; i < kNumParams; ++i) out.params[i] = params_[i].load(std::memory_order_relaxed);
    for (int l = 0; l < kLayers; ++l) out.presets[l] = presetReq_[l].load(std::memory_order_relaxed);
    for (int p = 0; p < kPatterns; ++p) project_.copyTo(p, out.patterns[p]);
    out.requestedPattern = project_.requested.load(std::memory_order_relaxed);
}

void Engine::loadState(const State& in)
{
    for (int i = 0; i < kNumParams; ++i) setParam(Param(i), in.params[i]);
    for (int l = 0; l < kLayers; ++l) loadPreset(Layer(l), in.presets[l]);
    for (int p = 0; p < kPatterns; ++p) project_.copyFrom(p, in.patterns[p]);
    requestPattern(in.requestedPattern);
}

// ---------------------------------------------------------------------------
size_t framesForPattern(int bars, double bpm, double sampleRate, double tailSeconds)
{
    const double beats = double(bars) * 4.0;
    return size_t(std::ceil((beats * 60.0 / bpm + tailSeconds) * sampleRate));
}

size_t renderPatternOffline(Engine& scratch, const Engine::State& state, int pattern, double bpm,
                            double sampleRate, float* left, float* right, size_t frames)
{
    Engine::State s = state;
    s.requestedPattern = pattern % kPatterns;
    scratch.loadState(s);
    scratch.prepare(sampleRate);   // fresh voices, delay and sequencer
    std::fill(left, left + frames, 0.0f);
    std::fill(right, right + frames, 0.0f);

    const int bars = s.patterns[s.requestedPattern].bars == 16 ? 16 : 8;
    const double patternBeats = bars * 4.0;
    Transport t;
    t.bpm = bpm;
    // The pattern plays once from beat 0; the transport stops after its last
    // step, so held notes release and the delay rings out into the tail.
    for (size_t pos = 0; pos < frames;)
    {
        const int n = int(std::min<size_t>(Engine::kChunk, frames - pos));
        t.beats = double(pos) * bpm / (60.0 * sampleRate);
        t.playing = t.beats < patternBeats - 1e-9;
        scratch.process(left + pos, right + pos, n, t);
        pos += size_t(n);
    }
    return frames;
}

} // namespace mt::hauntedhall
