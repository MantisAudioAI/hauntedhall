// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall test suite and benchmark.
//   hh_tests          run the tests, then the benchmark
#include "HHToolsCommon.h"
#include "../engine/HHGrid.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>

using namespace hhtools;

// ---- heap allocation counter (for the real-time test) ----------------------
static std::atomic<long> gAllocs{0};
static std::atomic<bool> gCounting{false};
void* operator new(std::size_t n)
{
    if (gCounting.load(std::memory_order_relaxed)) gAllocs.fetch_add(1);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

constexpr int kSR = 48000;
int gPass = 0, gFail = 0;

void check(bool ok, const char* name, const std::string& detail = "")
{
    std::printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name, detail.empty() ? "" : " - ", detail.c_str());
    (ok ? gPass : gFail)++;
}

std::string fmt(const char* f, double a, double b = 0.0)
{
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

// A full 4-layer project used by several tests.
void setupFull(Engine& e, GrooveStyle style = GrooveStyle::CryptHop)
{
    e.loadPreset(Layer::Drum, 0);
    e.loadPreset(Layer::A, findSynthPreset("Dungeon Bass"));
    e.loadPreset(Layer::B, findSynthPreset("Haunted Choir"));
    e.loadPreset(Layer::C, findSynthPreset("Crystal Lead"));
    e.project().patterns[0].bars.store(8);
    generateDrums(e.project(), 0, style, 1u);
    generateLane(e.project(), 0, LaneStyle::Bassline, 57, Scale::Minor, 1u);
}

void renderFull(Engine& e, double beats, std::vector<float>& L, std::vector<float>& R, double bpm = 100.0)
{
    HostSim sim(e, kSR, bpm);
    sim.addLane(LaneStyle::PadChords, 57, Scale::Minor, 2u, Layer::B, 8);
    sim.addLane(LaneStyle::Lead, 57, Scale::Minor, 3u, Layer::C, 8);
    sim.render(beats, 2.0, L, R);
}

uint64_t hashAudio(const std::vector<float>& L, const std::vector<float>& R)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < L.size(); ++i)
    {
        uint32_t a, b;
        std::memcpy(&a, &L[i], 4);
        std::memcpy(&b, &R[i], 4);
        h = (h ^ a) * 1099511628211ull;
        h = (h ^ b) * 1099511628211ull;
    }
    return h;
}

} // namespace

int main()
{
    std::printf("Haunted Hall tests\n");

    // 1. Real-time safety: no heap allocation inside process(), across a busy
    //    project with patterns, host notes, preset changes and pattern switches.
    {
        Engine e;
        e.prepare(kSR);
        setupFull(e);
        generateDrums(e.project(), 1, GrooveStyle::BossRush, 2u);
        std::vector<float> L(128), R(128);
        Transport t;
        t.bpm = 128.0;
        t.playing = true;
        gAllocs = 0;
        gCounting = true;
        for (int b = 0; b < 3000; ++b)
        {
            if (b % 50 == 0) e.noteOn(Layer::C, uint8_t(60 + b % 12), 0.8f, 17);
            if (b % 50 == 25) e.noteOff(Layer::C, uint8_t(60 + (b - 25) % 12), 3);
            if (b == 1000) e.loadPreset(Layer::B, findSynthPreset("Space Pad"));
            if (b == 1500) e.requestPattern(1);
            if (b == 2000) e.setParam(Param::BSpook, 1.0f);
            e.process(L.data(), R.data(), 128, t);
            t.beats += 128.0 * t.bpm / 60.0 / kSR;
        }
        gCounting = false;
        check(gAllocs.load() == 0, "no heap allocation in 3,000 audio callbacks", fmt("%.0f allocations", double(gAllocs.load())));
    }

    // 2. Determinism: identical renders, every time.
    {
        Engine a, b;
        a.prepare(kSR); b.prepare(kSR);
        setupFull(a); setupFull(b);
        std::vector<float> L1, R1, L2, R2;
        renderFull(a, 32.0, L1, R1);
        renderFull(b, 32.0, L2, R2);
        check(hashAudio(L1, R1) == hashAudio(L2, R2), "same notes and settings give bit-identical audio");

        bool finite = true; float peak = 0.0f;
        for (size_t i = 0; i < L1.size(); ++i)
        {
            finite = finite && std::isfinite(L1[i]) && std::isfinite(R1[i]);
            peak = std::max(peak, std::max(std::fabs(L1[i]), std::fabs(R1[i])));
        }
        check(finite, "no NaN or Inf in a full 4-layer render");
        check(peak <= 0.967f, "output never exceeds the -0.3 dBFS ceiling", fmt("peak %.2f dBFS", 20.0 * std::log10(peak)));
    }

    // 3. Max SPOOK on every layer with the delay at full feedback stays finite and bounded.
    {
        Engine e;
        e.prepare(kSR);
        setupFull(e, GrooveStyle::BossRush);
        for (int l = 0; l < kLayers; ++l)
        {
            e.setParam(channelParam(Layer(l), ChannelParam::Spook), 1.0f);
            e.setParam(channelParam(Layer(l), ChannelParam::Level), 1.0f);
            e.setParam(channelParam(Layer(l), ChannelParam::Space), 1.0f);
        }
        e.setParam(Param::DelayFeedback, 0.9f);
        e.setParam(Param::DelayMix, 1.0f);
        e.setParam(Param::DelayLevel, 1.0f);
        std::vector<float> L, R;
        renderFull(e, 32.0, L, R, 160.0);
        bool ok = true; float peak = 0.0f;
        for (size_t i = 0; i < L.size(); ++i)
        {
            ok = ok && std::isfinite(L[i]) && std::isfinite(R[i]);
            peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
        }
        check(ok && peak <= 0.967f, "worst case (all SPOOK, all levels up, feedback 0.9) stays finite and under the ceiling",
              fmt("peak %.2f dBFS", 20.0 * std::log10(peak)));
    }

    // 4. Drum delay send is capped at 20 %: with MIX at 100 % and no feedback,
    //    a snare's echo is at most 0.2 x the snare itself; a synth's can reach 1.0.
    {
        auto echoRatio = [](Layer layer, uint8_t note) {
            Engine e;
            soloLayer(e, layer);
            e.setParam(channelParam(layer, ChannelParam::Space), 1.0f);
            e.setParam(Param::DelayFeedback, 0.0f);
            e.setParam(Param::DelayMix, 1.0f);
            e.setParam(Param::DelayTime, 500.0f);
            e.loadPreset(Layer::A, findSynthPreset("Night Stabs"));
            e.prepare(kSR);
            HostSim sim(e, kSR, 120.0);
            sim.playPatterns = false;
            sim.notes.push_back({0.0, 0.12, layer, note, 120});
            std::vector<float> L, R;
            sim.render(2.0, 0.0, L, R);
            float dry = 0.0f, wet = 0.0f;
            const size_t split = size_t(0.45 * kSR);
            for (size_t i = 0; i < L.size(); ++i)
                (i < split ? dry : wet) = std::max(i < split ? dry : wet, std::fabs(L[i]));
            return wet / std::max(1e-9f, dry);
        };
        const float drum = echoRatio(Layer::Drum, 38);
        const float synth = echoRatio(Layer::A, 60);
        check(drum <= 0.21f, "drum delay send capped at 20 %", fmt("drum echo %.2f x dry", drum));
        check(synth > 0.5f, "synth layers can send fully", fmt("synth echo %.2f x dry", synth));
    }

    // 5. Patterns: an 8-bar pattern wraps after step 127; a 16-bar one after 255.
    for (int bars : {8, 16})
    {
        Engine e;
        e.prepare(kSR);
        e.project().patterns[0].bars.store(uint8_t(bars));
        generateDrums(e.project(), 0, GrooveStyle::CryptHop, 1u);
        std::vector<float> L(128), R(128);
        Transport t;
        t.bpm = 150.0;
        t.playing = true;
        int maxStep = -1, wraps = 0, last = -1;
        const double totalBeats = bars * 4.0 * 2.0 + 0.5;
        while (t.beats < totalBeats)
        {
            e.process(L.data(), R.data(), 128, t);
            const int s = e.currentStep();
            maxStep = std::max(maxStep, s);
            if (s < last) ++wraps;
            last = s;
            t.beats += 128.0 * t.bpm / 60.0 / kSR;
        }
        check(maxStep == bars * 16 - 1 && wraps == 2,
              bars == 8 ? "8-bar pattern: 128 steps, loops" : "16-bar pattern: 256 steps, loops",
              fmt("last step %.0f, %.0f wraps", maxStep, wraps));
    }

    // 6. Pattern B requested mid-pattern starts exactly at the next pattern start.
    {
        Engine e;
        e.prepare(kSR);
        e.project().patterns[0].bars.store(8);
        e.project().patterns[1].bars.store(16);
        std::vector<float> L(64), R(64);
        Transport t;
        t.bpm = 200.0;
        t.playing = true;
        bool early = false, switched = false;
        double switchBeat = -1.0;
        while (t.beats < 70.0)
        {
            if (t.beats >= 10.0 && t.beats < 10.1) e.requestPattern(1);
            e.process(L.data(), R.data(), 64, t);
            if (e.playingPattern() == 1 && !switched) { switched = true; switchBeat = t.beats; }
            if (e.playingPattern() == 1 && t.beats < 31.9) early = true;
            t.beats += 64.0 * t.bpm / 60.0 / kSR;
        }
        check(switched && !early && e.currentStep() >= 0, "pattern switch waits for the end of the current pattern",
              fmt("switched at beat %.2f", switchBeat));
    }

    // 7. Melody step packing round-trips.
    {
        MelodyStep m;
        m.notes[0] = 57; m.notes[1] = 60; m.notes[2] = 64; m.count = 3; m.velocity = 101; m.length = 16;
        const MelodyStep u = MelodyStep::unpack(m.pack());
        check(u.count == 3 && u.notes[0] == 57 && u.notes[2] == 64 && u.velocity == 101 && u.length == 16,
              "melody step packing round-trips (chord, velocity, length 16)");
    }

    // 8. Generators are repeatable and seeds matter.
    {
        Project p1, p2, p3;
        generateDrums(p1, 0, GrooveStyle::GraveTrap, 42u);
        generateDrums(p2, 0, GrooveStyle::GraveTrap, 42u);
        generateDrums(p3, 0, GrooveStyle::GraveTrap, 43u);
        PatternData a, b, c;
        p1.copyTo(0, a); p2.copyTo(0, b); p3.copyTo(0, c);
        check(std::memcmp(&a, &b, sizeof a) == 0 && std::memcmp(&a, &c, sizeof a) != 0,
              "same seed, same groove; new seed, new groove");
    }

    // 9. Export as notes for a host sequencer matches the grid.
    {
        Project p;
        p.patterns[0].bars.store(8);
        generateDrums(p, 0, GrooveStyle::DungeonMarch, 5u);
        generateLane(p, 0, LaneStyle::ChordStabs, 57, Scale::Dorian, 5u);
        std::vector<ExportNote> ex(4096);
        const int n = exportNotes(p, 0, ex.data(), int(ex.size()));
        int grid = 0;
        for (int s = 0; s < 128; ++s)
        {
            for (int r = 0; r < kDrumRows; ++r) grid += p.patterns[0].drums[s][r].load() ? 1 : 0;
            grid += MelodyStep::unpack(p.patterns[0].melody[s].load()).count;
        }
        check(n == grid && n > 0, "export to notes: one note per grid hit and chord note", fmt("%.0f notes", n));
    }

    // 10. Offline export (for cratedust 12) sounds exactly like live playback.
    {
        Engine live;
        live.prepare(kSR);
        setupFull(live);
        live.setParam(Param::BOn, 0.0f);
        live.setParam(Param::COn, 0.0f);
        Engine::State st;
        live.captureState(st);

        Engine replay;
        replay.loadState(st);
        replay.prepare(kSR);
        HostSim sim(replay, kSR, 100.0);
        std::vector<float> L1, R1;
        sim.render(32.0, 2.0, L1, R1, Engine::kChunk);

        Engine scratch;
        const size_t frames = framesForPattern(8, 100.0, kSR, 2.0);
        std::vector<float> L2(frames), R2(frames);
        renderPatternOffline(scratch, st, 0, 100.0, kSR, L2.data(), R2.data(), frames);
        double maxDiff = 0.0;
        const size_t n = std::min(L1.size(), L2.size());
        for (size_t i = 0; i < n; ++i) maxDiff = std::max(maxDiff, double(std::fabs(L1[i] - L2[i])));
        check(maxDiff < 1e-6, "offline pattern export matches live playback", fmt("max difference %.2e", maxDiff));
    }

    // 11. Idle: once everything has rung out, process() costs almost nothing and outputs silence.
    {
        Engine e;
        e.prepare(kSR);
        setupFull(e);
        HostSim sim(e, kSR, 100.0);   // engine patterns only: play 2 bars, stop, ring out
        std::vector<float> L, R;
        sim.render(8.0, 4.0, L, R);
        std::vector<float> l(128, 0.0f), r(128, 0.0f);
        Transport t;
        for (int i = 0; i < 2000; ++i) e.process(l.data(), r.data(), 128, t);   // let the delay settle
        bool silent = true;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 20000; ++i)
        {
            e.process(l.data(), r.data(), 128, t);
        }
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 20000.0;
        for (float x : l) silent = silent && x == 0.0f;
        check(silent && us < 5.0, "idle engine outputs exact silence and sleeps", fmt("%.2f us per idle block", us));
    }

    // 12. Notes from several threads at once (pads, previews) while audio runs.
    {
        Engine e;
        e.prepare(kSR);
        std::atomic<bool> run{true};
        std::atomic<long> sent{0};
        auto producer = [&](Layer layer) {
            while (run.load()) { if (e.trigger(layer, 60, 0.7f)) sent.fetch_add(1); e.release(layer, 60); std::this_thread::yield(); }
        };
        std::thread t1(producer, Layer::A), t2(producer, Layer::Drum);
        std::vector<float> L(128), R(128);
        Transport t;
        bool ok = true;
        for (int i = 0; i < 4000; ++i)
        {
            e.process(L.data(), R.data(), 128, t);
            for (float x : L) ok = ok && std::isfinite(x);
            std::fill(L.begin(), L.end(), 0.0f);
            std::fill(R.begin(), R.end(), 0.0f);
        }
        run = false;
        t1.join(); t2.join();
        check(ok && sent.load() > 0, "notes from two other threads at once (lock-free queue)", fmt("%.0f notes", double(sent.load())));
    }

    // ---- step grid editor ---------------------------------------------------
    std::printf("Step grid\n");

    // 13. Tap a step on and off; cycle through normal, accent, soft, off.
    {
        Project p;
        PatternEditor ed(p);
        const bool on = ed.toggleStep(0, 2, 5, int(DrumRow::Snare));
        const uint8_t v1 = ed.step(0, 2, 5, int(DrumRow::Snare));
        const bool off = !ed.toggleStep(0, 2, 5, int(DrumRow::Snare));
        uint8_t seq[4];
        for (int i = 0; i < 4; ++i) { ed.cycleStep(0, 0, 0, 0); seq[i] = ed.step(0, 0, 0, 0); }
        check(on && off && v1 == PatternEditor::kVelNormal && seq[0] == 100 && seq[1] == 127 && seq[2] == 64 && seq[3] == 0,
              "tap toggles a step; long-press cycles normal > accent > soft > off");
    }

    // 14. Undo and redo walk back and forward through edits, across both patterns.
    {
        Project p;
        PatternEditor ed(p);
        ed.toggleStep(0, 0, 0, 0);
        ed.toggleStep(1, 0, 4, 1);
        ed.toggleStep(0, 0, 8, 2);
        const bool u1 = ed.undo() && ed.step(0, 0, 8, 2) == 0 && ed.step(1, 0, 4, 1) != 0;
        const bool u2 = ed.undo() && ed.step(1, 0, 4, 1) == 0 && ed.step(0, 0, 0, 0) != 0;
        const bool r1 = ed.redo() && ed.step(1, 0, 4, 1) != 0;
        const bool r2 = ed.redo() && ed.step(0, 0, 8, 2) != 0 && !ed.canRedo();
        ed.undo();
        ed.toggleStep(0, 3, 3, 3);   // a new edit ends the redo chain
        check(u1 && u2 && r1 && r2 && !ed.canRedo(), "undo / redo across both patterns; a new edit clears redo");
    }

    // 15. Undo is capped at 32 steps and never crashes past it.
    {
        Project p;
        PatternEditor ed(p);
        for (int i = 0; i < 50; ++i) ed.toggleStep(0, 0, i % 16, i % 5);
        int n = 0;
        while (ed.undo()) ++n;
        check(n == PatternEditor::kUndoDepth, "undo history holds the last 32 edits", fmt("%.0f undos", n));
    }

    // 16. Copy a bar (drums + melody lane) and paste it elsewhere; copy pattern A to B.
    {
        Project p;
        PatternEditor ed(p);
        generateDrums(p, 0, GrooveStyle::BossRush, 9u);
        generateLane(p, 0, LaneStyle::ChordStabs, 57, Scale::Minor, 9u);
        ed.copyBar(0, 1);
        ed.pasteBar(0, 6);
        bool same = true;
        for (int c = 0; c < 16; ++c)
        {
            for (int r = 0; r < kDrumRows; ++r) same = same && ed.step(0, 1, c, r) == ed.step(0, 6, c, r);
            same = same && ed.laneStep(0, 1, c).pack() == ed.laneStep(0, 6, c).pack();
        }
        ed.copyPattern(0);
        ed.pastePattern(1);
        PatternData a, b;
        p.copyTo(0, a); p.copyTo(1, b);
        check(same && std::memcmp(&a, &b, sizeof a) == 0, "copy / paste a bar, and pattern A to B");
    }

    // 17. 8 -> 16 bars doubles the pattern; 16 -> 8 -> 16 keeps the second half.
    {
        Project p;
        PatternEditor ed(p);
        p.patterns[0].clear();
        ed.toggleStep(0, 3, 7, 0);
        ed.setBars(0, 16);
        const bool doubled = ed.bars(0) == 16 && ed.step(0, 11, 7, 0) != 0;
        ed.toggleStep(0, 12, 0, 1);   // edit the second half
        ed.setBars(0, 8);
        ed.setBars(0, 16);
        const bool kept = ed.step(0, 12, 0, 1) != 0 && ed.step(0, 11, 7, 0) != 0;
        check(doubled && kept, "8 > 16 bars copies bars 1-8; switching back keeps hidden bars");
    }

    // 18. RANDOM with locked rows: locked rows and a locked lane stay exactly as they were.
    {
        Project p;
        PatternEditor ed(p);
        ed.generator().style = GrooveStyle::GraveTrap;
        ed.regenerate(0);
        PatternData before;
        p.copyTo(0, before);
        ed.toggleRowLock(int(DrumRow::Kick));
        ed.toggleRowLock(int(DrumRow::Snare));
        ed.generator().laneLocked = true;
        ed.randomize(0);
        PatternData after;
        p.copyTo(0, after);
        bool lockedSame = true, hatsChanged = false;
        for (int s = 0; s < kMaxSteps; ++s)
        {
            lockedSame = lockedSame && before.drums[s][0] == after.drums[s][0] && before.drums[s][1] == after.drums[s][1]
                         && before.melody[s] == after.melody[s];
            hatsChanged = hatsChanged || before.drums[s][2] != after.drums[s][2] || before.drums[s][3] != after.drums[s][3];
        }
        const bool undone = ed.undo();
        PatternData restored;
        p.copyTo(0, restored);
        check(lockedSame && hatsChanged && undone && std::memcmp(&before, &restored, sizeof before) == 0,
              "RANDOM rerolls unlocked rows only, keeps the locked lane, and can be undone");
    }

    // 19. Density: sparse < as written < busy (average over seeds).
    {
        auto hits = [](float density) {
            long total = 0;
            for (uint32_t seed = 1; seed <= 20; ++seed)
            {
                Project p;
                generateDrums(p, 0, GrooveStyle::CryptHop, seed, density);
                for (int s = 0; s < kMaxSteps; ++s)
                    for (int r = 0; r < kDrumRows; ++r) total += p.patterns[0].drums[s][r].load() ? 1 : 0;
            }
            return total;
        };
        const long lo = hits(0.1f), mid = hits(0.5f), hi = hits(0.95f);
        check(lo < mid && mid < hi, "density turns the generator from sparse to busy",
              fmt("%.0f / ", double(lo)) + fmt("%.0f / ", double(mid)) + fmt("%.0f hits", double(hi)));
    }

    // 20. Transpose the lane follows the key.
    {
        Project p;
        PatternEditor ed(p);
        generateLane(p, 0, LaneStyle::Bassline, 57, Scale::Minor, 3u);
        const MelodyStep m0 = ed.laneStep(0, 0, 0);
        ed.transposeLane(0, 2);
        const MelodyStep m1 = ed.laneStep(0, 0, 0);
        check(m0.count > 0 && m1.notes[0] == m0.notes[0] + 2 && ed.generator().root == 59, "transpose the melody lane +2 (key follows)");
    }

    // 21. Editing while the pattern plays: the audio thread runs alongside the
    //     editor hammering the grid (taps, RANDOM, paste, length changes).
    {
        Engine e;
        e.prepare(kSR);
        setupFull(e);
        PatternEditor ed(e.project());
        std::atomic<bool> run{true};
        std::atomic<long> edits{0};
        std::thread ui([&] {
            int i = 0;
            while (run.load())
            {
                switch (i++ % 5)
                {
                    case 0: ed.toggleStep(0, i % 8, i % 16, i % 5); break;
                    case 1: ed.randomize(0); break;
                    case 2: ed.copyBar(0, 0); ed.pasteBar(0, i % 8); break;
                    case 3: ed.setBars(0, (i / 5) % 2 ? 16 : 8); break;
                    default: ed.undo(); break;
                }
                edits.fetch_add(1);
                std::this_thread::yield();
            }
        });
        std::vector<float> L(128), R(128);
        Transport t;
        t.bpm = 170.0;
        t.playing = true;
        bool ok = true;
        gAllocs = 0;
        for (int b = 0; b < 6000; ++b)
        {
            gCounting = true;
            e.process(L.data(), R.data(), 128, t);
            gCounting = false;
            for (float x : L) ok = ok && std::isfinite(x) && std::fabs(x) <= 0.967f;
            std::fill(L.begin(), L.end(), 0.0f);
            std::fill(R.begin(), R.end(), 0.0f);
            t.beats += 128.0 * t.bpm / 60.0 / kSR;
        }
        run = false;
        ui.join();
        check(ok && gAllocs.load() == 0, "editing the grid while it plays: clean audio, no allocation",
              fmt("%.0f edits during playback", double(edits.load())));
    }

    std::printf("\n%d passed, %d failed\n", gPass, gFail);

    // ---- benchmark -----------------------------------------------------------
    std::printf("\nBenchmark (128-sample blocks at 48 kHz; one block = 2,667 us of audio)\n");
    auto bench = [](const char* name, auto setup) {
        Engine e;
        e.prepare(kSR);
        setup(e);
        HostSim sim(e, kSR, 140.0);
        sim.addLane(LaneStyle::PadChords, 57, Scale::Minor, 2u, Layer::B, 8);
        sim.addLane(LaneStyle::Lead, 57, Scale::Minor, 3u, Layer::C, 8);
        std::vector<float> L, R;
        const auto t0 = std::chrono::steady_clock::now();
        sim.render(32.0, 0.0, L, R);
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double audio = double(L.size()) / kSR;
        const double usPerBlock = sec / (double(L.size()) / 128.0) * 1e6;
        std::printf("  %-52s %6.1f us/block  %5.2f %% of one core\n", name, usPerBlock, 100.0 * sec / audio);
    };
    bench("full project: drums + 3 layers + delay", [](Engine& e) { setupFull(e); });
    bench("worst case: all layers SPOOK 1.0, pads on A/B/C", [](Engine& e) {
        setupFull(e, GrooveStyle::BossRush);
        e.loadPreset(Layer::A, findSynthPreset("Space Pad"));
        e.loadPreset(Layer::B, findSynthPreset("Space Pad"));
        e.loadPreset(Layer::C, findSynthPreset("Haunted Choir"));
        generateLane(e.project(), 0, LaneStyle::PadChords, 57, Scale::Minor, 1u);
        for (int l = 0; l < kLayers; ++l) e.setParam(channelParam(Layer(l), ChannelParam::Spook), 1.0f);
    });
    return gFail == 0 ? 0 : 1;
}
