// SPDX-License-Identifier: AGPL-3.0-or-later
// Renders Haunted Hall demo WAVs for listening (round-1 presets, kits,
// generated grooves, SPOOK sweep, and the export-to-audio path).
//   hh_render [outDir]
#include "HHToolsCommon.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>

using namespace hhtools;

namespace {

constexpr int kSR = 48000;

std::string slug(const char* s)
{
    std::string o;
    for (const char* p = s; *p; ++p)
    {
        const char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o += c;
        else if (c >= 'A' && c <= 'Z') o += char(c - 'A' + 'a');
        else if (!o.empty() && o.back() != '_') o += '_';
    }
    while (!o.empty() && o.back() == '_') o.pop_back();
    return o;
}

void report(const std::string& name, const std::vector<float>& L, const std::vector<float>& R)
{
    double sum = 0.0; float peak = 0.0f;
    for (size_t i = 0; i < L.size(); ++i)
    {
        peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
        sum += 0.5 * (double(L[i]) * L[i] + double(R[i]) * R[i]);
    }
    const double rms = std::sqrt(sum / double(std::max<size_t>(1, L.size())));
    std::printf("  %-44s peak %6.1f dBFS   rms %6.1f dBFS   %5.1f s\n", name.c_str(),
                20.0 * std::log10(std::max(1e-9f, peak)), 20.0 * std::log10(std::max(1e-9, rms)),
                double(L.size()) / kSR);
}

void save(const std::string& dir, const std::string& name, const std::vector<float>& L, const std::vector<float>& R)
{
    writeWav16(dir + "/" + name + ".wav", L, R, kSR);
    report(name, L, R);
}

std::unique_ptr<Engine> freshEngine()
{
    auto e = std::make_unique<Engine>();
    e->prepare(kSR);
    return e;
}

// A short phrase per category so each preset is heard in a musical context.
void addPhrase(HostSim& sim, Category cat, Layer layer)
{
    auto n = [&](double start, double len, int note, int vel = 100) {
        sim.notes.push_back({start, len, layer, uint8_t(note), uint8_t(vel)});
    };
    if (cat == Category::Bass)
    {
        const int r[8] = {45, 45, 57, 45, 41, 41, 53, 43};   // A minor riff, F, G
        for (int bar = 0; bar < 2; ++bar)
            for (int i = 0; i < 8; ++i) n(bar * 4 + i * 0.5, i % 4 == 0 ? 0.45 : 0.3, r[i], i % 2 ? 90 : 110);
        n(8.0, 2.0, 33, 110);
    }
    else if (cat == Category::Lead)
    {
        const int m[10] = {69, 72, 76, 74, 72, 71, 72, 69, 67, 69};
        const double t[10] = {0, 0.5, 1, 2, 2.5, 3, 3.5, 4, 5, 6};
        const double l[10] = {0.45, 0.45, 0.95, 0.45, 0.45, 0.45, 0.45, 0.95, 0.95, 2.5};
        for (int i = 0; i < 10; ++i) n(t[i], l[i], m[i]);
        // a chord at the end: poly leads play it, the arp lead turns it into an arpeggio
        n(9.0, 1.5, 57); n(9.0, 1.5, 60); n(9.0, 1.5, 64);
    }
    else
    {
        const int ch[4][3] = {{57, 60, 64}, {53, 57, 60}, {55, 59, 62}, {52, 55, 59}};   // Am F G Em
        for (int b = 0; b < 4; ++b)
            for (int k = 0; k < 3; ++k) n(b * 2.5, 2.2, ch[b][k], 92);
    }
}

} // namespace

int main(int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : "renders";
    std::filesystem::create_directories(out);
    std::printf("Haunted Hall renders -> %s/\n", out.c_str());

    // 1) Every round-1 synth preset, solo, with the default delay
    std::printf("Synth presets:\n");
    for (int i = 0; i < numSynthPresets(); ++i)
    {
        const SynthPreset& p = synthPreset(i);
        auto e = freshEngine();
        soloLayer(*e, Layer::A);
        e->loadPreset(Layer::A, i);
        HostSim sim(*e, kSR, 100.0);
        sim.playPatterns = false;
        addPhrase(sim, p.category, Layer::A);
        std::vector<float> L, R;
        sim.render(12.0, 2.0, L, R);
        save(out, std::string("preset_") + slug(categoryName(p.category)) + "_" + slug(p.name), L, R);
    }

    // 2) Every kit: its own groove, 4 bars, then each row on its own
    std::printf("Drum kits:\n");
    const GrooveStyle kitStyles[3] = {GrooveStyle::CryptHop, GrooveStyle::SpectralSynthwave, GrooveStyle::GraveTrap};
    for (int k = 0; k < numKitPresets(); ++k)
    {
        auto e = freshEngine();
        soloLayer(*e, Layer::Drum);
        e->loadPreset(Layer::Drum, k);
        e->project().patterns[0].bars.store(8);
        generateDrums(e->project(), 0, kitStyles[k], 7u);
        const double bpm = grooveTempo(kitStyles[k]);
        HostSim sim(*e, kSR, bpm);
        // after 4 bars, stop the pattern and play each row twice (beats 17..)
        std::vector<float> L, R;
        sim.render(16.0, 0.0, L, R);
        HostSim hits(*e, kSR, bpm);
        hits.playPatterns = false;
        for (int r = 0; r < kDrumRows; ++r)
        {
            hits.notes.push_back({r * 1.0 + 0.5, 0.1, Layer::Drum, kDrumNotes[r], 115});
            hits.notes.push_back({r * 1.0 + 1.0, 0.1, Layer::Drum, kDrumNotes[r], 70});
        }
        std::vector<float> L2, R2;
        hits.render(6.0, 1.0, L2, R2);
        L.insert(L.end(), L2.begin(), L2.end());
        R.insert(R.end(), R2.begin(), R2.end());
        save(out, std::string("kit_") + slug(kitPreset(k).name), L, R);
    }

    // 3) Generated grooves: drums + melody lane on A (engine patterns), pad and
    //    lead played as host notes on B and C (standing in for KICK's sequencer)
    std::printf("Generated demos (8 bars):\n");
    struct Demo { GrooveStyle style; Scale scale; uint8_t root; int kit; const char* a; const char* b; const char* c;
                  LaneStyle laneA; LaneStyle laneB; LaneStyle laneC; };
    const Demo demos[] = {
        {GrooveStyle::CryptHop, Scale::Dorian, 57, 0, "Ghost Bass", "Haunted Choir", "Sword Tone",
         LaneStyle::Bassline, LaneStyle::PadChords, LaneStyle::Lead},
        {GrooveStyle::GraveTrap, Scale::HarmonicMinor, 53, 2, "Sub Runner", "Cathedral", "Crystal Lead",
         LaneStyle::Bassline, LaneStyle::PadChords, LaneStyle::Lead},
        {GrooveStyle::DungeonMarch, Scale::Phrygian, 52, 1, "Dungeon Bass", "Night Stabs", "Dragon Horn",
         LaneStyle::Bassline, LaneStyle::ChordStabs, LaneStyle::Lead},
        {GrooveStyle::SpectralSynthwave, Scale::Minor, 57, 1, "Dungeon Bass", "Space Pad", "Hero Arp",
         LaneStyle::Bassline, LaneStyle::PadChords, LaneStyle::ChordStabs},
        {GrooveStyle::BossRush, Scale::HarmonicMinor, 52, 1, "Sub Runner", "Night Stabs", "Crystal Lead",
         LaneStyle::Bassline, LaneStyle::ChordStabs, LaneStyle::Lead},
    };
    for (const Demo& d : demos)
    {
        auto e = freshEngine();
        e->loadPreset(Layer::Drum, d.kit);
        e->loadPreset(Layer::A, findSynthPreset(d.a));
        e->loadPreset(Layer::B, findSynthPreset(d.b));
        e->loadPreset(Layer::C, findSynthPreset(d.c));
        e->setParam(Param::DelaySync, 2.0f);   // dotted eighth
        e->setParam(Param::DelayMix, 0.32f);
        e->project().patterns[0].bars.store(8);
        generateDrums(e->project(), 0, d.style, 11u);
        generateLane(e->project(), 0, d.laneA, d.root, d.scale, 5u);
        const double bpm = grooveTempo(d.style);
        HostSim sim(*e, kSR, bpm);
        sim.addLane(d.laneB, d.root, d.scale, 5u, Layer::B, 8);
        sim.addLane(d.laneC, d.root, d.scale, 9u, Layer::C, 8);
        std::vector<float> L, R;
        sim.render(32.0, 3.0, L, R);
        save(out, std::string("demo_") + slug(grooveStyleName(d.style)), L, R);
    }

    // 4) SPOOK sweep on the choir: clean -> fully corroded over 12 s
    std::printf("SPOOK sweep:\n");
    {
        auto e = freshEngine();
        soloLayer(*e, Layer::B);
        e->loadPreset(Layer::B, findSynthPreset("Haunted Choir"));
        HostSim sim(*e, kSR, 80.0);
        sim.playPatterns = false;
        for (int b = 0; b < 4; ++b)
        {
            const int ch[4][3] = {{57, 60, 64}, {53, 57, 60}, {50, 53, 57}, {52, 56, 59}};
            for (int k = 0; k < 3; ++k) sim.notes.push_back({b * 4.0, 3.8, Layer::B, uint8_t(ch[b][k]), 96});
        }
        for (int i = 0; i <= 64; ++i)
            sim.automation.push_back({i * 0.25, {Param::BSpook, std::min(1.0f, float(i) / 64.0f)}});
        std::vector<float> L, R;
        sim.render(16.0, 2.0, L, R);
        save(out, "spook_sweep_haunted_choir", L, R);
    }

    // 5) Drum delay send cap: SPACE at max on drums vs a synth
    std::printf("Delay send cap:\n");
    {
        auto e = freshEngine();
        e->setParam(Param::DrumSpace, 1.0f);   // = 20 % send, the cap
        e->setParam(Param::ASpace, 1.0f);      // = 100 % send
        e->setParam(Param::DelayMix, 0.6f);
        e->setParam(Param::DelayFeedback, 0.5f);
        e->loadPreset(Layer::A, findSynthPreset("Crystal Lead"));
        e->setParam(Param::BOn, 0.0f);
        e->setParam(Param::COn, 0.0f);
        HostSim sim(*e, kSR, 90.0);
        sim.playPatterns = false;
        sim.notes.push_back({0.0, 0.2, Layer::Drum, 38, 120});
        sim.notes.push_back({4.0, 0.4, Layer::A, 76, 110});
        std::vector<float> L, R;
        sim.render(8.0, 1.0, L, R);
        save(out, "delay_send_drums_capped_vs_lead", L, R);
    }

    // 6) Export to audio (what cratedust 12 would receive): offline render of a pattern
    std::printf("Export to audio:\n");
    {
        auto live = freshEngine();
        live->project().patterns[0].bars.store(8);
        generateDrums(live->project(), 0, GrooveStyle::CryptHop, 3u);
        generateLane(live->project(), 0, LaneStyle::Bassline, 57, Scale::Dorian, 3u);
        live->loadPreset(Layer::A, findSynthPreset("Dungeon Bass"));
        Engine::State st;
        live->captureState(st);
        Engine scratch;
        const double bpm = 88.0;
        const size_t frames = framesForPattern(8, bpm, kSR, 2.0);
        std::vector<float> L(frames), R(frames);
        renderPatternOffline(scratch, st, 0, bpm, kSR, L.data(), R.data(), frames);
        save(out, "export_pattern_a_8bars", L, R);
    }
    return 0;
}
