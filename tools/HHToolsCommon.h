// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall dev tools: WAV writing and a tiny stand-in for KICK's sequencer.
#pragma once

#include "../engine/HHEngine.h"

#include <cstdio>
#include <string>
#include <vector>

namespace hhtools {

using namespace mt::hauntedhall;

inline bool writeWav16(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t frames = uint32_t(L.size());
    const uint32_t dataBytes = frames * 4;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(2); u32(uint32_t(sr)); u32(uint32_t(sr) * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    uint32_t dither = 22222u;
    for (uint32_t i = 0; i < frames; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            dither = dither * 1664525u + 1013904223u; const float d1 = float(dither >> 8) / 16777216.0f;
            dither = dither * 1664525u + 1013904223u; const float d2 = float(dither >> 8) / 16777216.0f;
            const float x = (c == 0 ? L[i] : R[i]) * 32767.0f + (d1 - d2);   // TPDF dither
            const float cl = x > 32767.0f ? 32767.0f : (x < -32768.0f ? -32768.0f : x);
            const int16_t s = int16_t(std::lrint(cl));
            std::fwrite(&s, 2, 1, f);
        }
    }
    std::fclose(f);
    return true;
}

// Plays note lists into the engine the way a host sequencer would
// (sample-accurate noteOn/noteOff), while the engine's own patterns run too.
struct HostNote { double start, length; Layer layer; uint8_t note; uint8_t vel; };

class HostSim
{
public:
    HostSim(Engine& e, int sr, double bpm) : e_(e), sr_(sr), bpm_(bpm) {}
    std::vector<HostNote> notes;
    std::vector<std::pair<double, std::pair<Param, float>>> automation;   // beat -> (param, value)
    bool playPatterns = true;

    void render(double beats, double tailSeconds, std::vector<float>& L, std::vector<float>& R, int block = 128)
    {
        const double spb = sr_ * 60.0 / bpm_;
        const size_t frames = size_t(beats * spb + tailSeconds * sr_);
        L.assign(frames, 0.0f);
        R.assign(frames, 0.0f);
        Transport t;
        t.bpm = bpm_;
        for (size_t pos = 0; pos < frames; pos += size_t(block))
        {
            const int n = int(std::min<size_t>(size_t(block), frames - pos));
            const double b0 = double(pos) / spb, b1 = double(pos + size_t(n)) / spb;
            for (auto& a : automation)
                if (a.first >= b0 && a.first < b1) e_.setParam(a.second.first, a.second.second);
            for (auto& hn : notes)
            {
                if (hn.start >= b0 && hn.start < b1)
                    e_.noteOn(hn.layer, hn.note, hn.vel / 127.0f, int((hn.start - b0) * spb));
                const double end = hn.start + hn.length;
                if (end >= b0 && end < b1)
                    e_.noteOff(hn.layer, hn.note, int((end - b0) * spb));
            }
            t.beats = b0;
            t.playing = playPatterns && b0 < beats;
            e_.process(&L[pos], &R[pos], n, t);
        }
    }

    // Adds a generated lane (made in a scratch project) as host notes on a layer.
    void addLane(LaneStyle style, uint8_t root, Scale sc, uint32_t seed, Layer layer, int bars)
    {
        static Project scratch;   // tool code, not real-time
        scratch.patterns[0].clear();
        scratch.patterns[0].bars.store(uint8_t(bars));
        generateLane(scratch, 0, style, root, sc, seed);
        std::vector<ExportNote> ex(4096);
        const int n = exportNotes(scratch, 0, ex.data(), int(ex.size()));
        for (int i = 0; i < n; ++i)
        {
            const ExportNote& x = ex[size_t(i)];
            if (x.track == 1) notes.push_back({x.startBeats, x.lengthBeats * 0.95, layer, x.note, x.velocity});
        }
    }

private:
    Engine& e_;
    int sr_;
    double bpm_;
};

inline void soloLayer(Engine& e, Layer keep)
{
    for (int l = 0; l < kLayers; ++l)
        e.setParam(channelParam(Layer(l), ChannelParam::On), Layer(l) == keep ? 1.0f : 0.0f);
}

} // namespace hhtools
