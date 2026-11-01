// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHPattern.h"

namespace mt::hauntedhall {

// ---- MelodyStep packing: 3 x 7-bit notes, 7-bit velocity, 4-bit length-1 ----
uint32_t MelodyStep::pack() const
{
    if (count == 0) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 3; ++i)
        v |= uint32_t(i < count ? (notes[i] & 0x7F) : 0) << (7 * i);
    v |= uint32_t(std::max<uint8_t>(1, velocity) & 0x7F) << 21;
    v |= uint32_t((std::max<uint8_t>(1, std::min<uint8_t>(16, length)) - 1) & 0x0F) << 28;
    return v;
}

MelodyStep MelodyStep::unpack(uint32_t v)
{
    MelodyStep s;
    if (v == 0) return s;
    for (int i = 0; i < 3; ++i)
    {
        const uint8_t nn = uint8_t((v >> (7 * i)) & 0x7F);
        if (nn) s.notes[s.count++] = nn;
    }
    s.velocity = uint8_t((v >> 21) & 0x7F);
    s.length = uint8_t(((v >> 28) & 0x0F) + 1);
    return s;
}

void Pattern::clear()
{
    for (int s = 0; s < kMaxSteps; ++s)
    {
        for (int r = 0; r < kDrumRows; ++r) drums[s][r].store(0, std::memory_order_relaxed);
        melody[s].store(0, std::memory_order_relaxed);
    }
}

void Project::copyTo(int p, PatternData& out) const
{
    const Pattern& pt = patterns[p];
    out.bars = pt.bars.load();
    for (int s = 0; s < kMaxSteps; ++s)
    {
        for (int r = 0; r < kDrumRows; ++r) out.drums[s][r] = pt.drums[s][r].load(std::memory_order_relaxed);
        out.melody[s] = pt.melody[s].load(std::memory_order_relaxed);
    }
}

void Project::copyFrom(int p, const PatternData& in)
{
    Pattern& pt = patterns[p];
    pt.bars.store(in.bars == 16 ? 16 : 8);
    for (int s = 0; s < kMaxSteps; ++s)
    {
        for (int r = 0; r < kDrumRows; ++r) pt.drums[s][r].store(in.drums[s][r], std::memory_order_relaxed);
        pt.melody[s].store(in.melody[s], std::memory_order_relaxed);
    }
}

// ---- names ------------------------------------------------------------------
const char* grooveStyleName(GrooveStyle s)
{
    static const char* names[] = {"Crypt Hop", "Grave Trap", "Dungeon March", "Spectral Synthwave", "Boss Rush"};
    return names[int(s) % int(GrooveStyle::Count)];
}
const char* scaleName(Scale s)
{
    static const char* names[] = {"Minor", "Harmonic Minor", "Dorian", "Phrygian"};
    return names[int(s) % int(Scale::Count)];
}
const char* laneStyleName(LaneStyle s)
{
    static const char* names[] = {"Bassline", "Lead", "Chord Stabs", "Pad Chords"};
    return names[int(s) % int(LaneStyle::Count)];
}
float grooveTempo(GrooveStyle s)
{
    static const float bpm[] = {88.0f, 140.0f, 100.0f, 110.0f, 160.0f};
    return bpm[int(s) % int(GrooveStyle::Count)];
}

// ---- drum generator -----------------------------------------------------------
namespace {

// Probability (0..100) of a hit on each step of a normal bar, per row.
// Rows: kick, snare, hat, ghost, fx.
using BarTemplate = uint8_t[kDrumRows][kStepsPerBar];

const BarTemplate kTemplates[int(GrooveStyle::Count)] = {
    {   // Crypt Hop: boom-bap with swingless ghost notes
        {100,0,0,0, 0,0,25,45, 0,0,85,0, 0,0,20,0},
        {0,0,0,0, 100,0,0,0, 0,0,0,0, 100,0,0,10},
        {90,15,90,15, 90,15,90,15, 90,15,90,15, 90,15,90,15},
        {0,0,0,25, 0,0,0,0, 0,20,0,0, 0,30,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,8},
    },
    {   // Grave Trap: half-time, rolling hats
        {100,0,0,0, 0,0,0,40, 0,0,60,30, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 100,0,0,0, 0,0,0,0},
        {90,80,85,80, 90,80,85,80, 90,80,85,80, 90,80,85,80},
        {0,0,0,0, 0,0,25,0, 0,0,0,0, 0,0,30,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,20},
    },
    {   // Dungeon March: stomping, snare drags
        {100,0,0,0, 0,0,0,0, 100,0,0,30, 0,0,0,0},
        {0,0,0,0, 100,0,0,0, 0,0,0,0, 100,0,40,40},
        {0,0,80,0, 0,0,80,0, 0,0,80,0, 0,0,80,0},
        {0,0,0,0, 0,0,0,30, 0,0,0,0, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
    },
    {   // Spectral Synthwave: four on the floor, open off-beats
        {100,0,0,0, 100,0,0,0, 100,0,0,0, 100,0,0,0},
        {0,0,0,0, 100,0,0,0, 0,0,0,0, 100,0,0,0},
        {20,20,95,20, 20,20,95,20, 20,20,95,20, 20,20,95,20},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,25},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,15,0},
    },
    {   // Boss Rush: fast, busy kicks
        {100,0,0,50, 0,0,60,0, 100,0,0,50, 0,0,40,0},
        {0,0,0,0, 100,0,0,25, 0,0,0,0, 100,0,0,25},
        {90,0,90,0, 90,0,90,0, 90,0,90,0, 90,0,90,0},
        {0,10,0,0, 0,10,0,0, 0,10,0,0, 0,10,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,10},
    },
};

uint8_t humanVel(Rng& rng, int base) { return uint8_t(clampf(float(base) + rng.bi() * 12.0f, 1.0f, 127.0f)); }

} // namespace

void generateDrums(Project& project, int pattern, GrooveStyle style, uint32_t seed, float density, uint8_t rowMask)
{
    Pattern& pt = project.patterns[pattern % kPatterns];
    const BarTemplate& tpl = kTemplates[int(style) % int(GrooveStyle::Count)];
    Rng rng;
    rng.seed(seed * 2654435761u + uint32_t(style) * 97u + 1u);
    const float dens = clampf(density, 0.0f, 1.0f) * 2.0f;

    // One "home" bar and one variation, so the groove repeats like a written part.
    uint8_t home[kStepsPerBar][kDrumRows] = {};
    uint8_t vari[kStepsPerBar][kDrumRows] = {};
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& bar = pass == 0 ? home : vari;
        for (int r = 0; r < kDrumRows; ++r)
            for (int s = 0; s < kStepsPerBar; ++s)
            {
                const int p = tpl[r][s];
                if (p == 0) continue;
                const float prob = p >= 100 ? 1.0f : clampf(float(p) / 100.0f * dens, 0.0f, 0.95f);
                if (rng.uni() < prob)
                {
                    const bool onBeat = (s % 4) == 0;
                    int base = p >= 100 ? 108 : 78;
                    if (r == int(DrumRow::Hat)) base = onBeat ? 100 : 72;
                    if (r == int(DrumRow::Ghost)) base = 64;
                    bar[s][r] = humanVel(rng, base);
                }
            }
    }

    for (int bar = 0; bar < kMaxBars; ++bar)
    {
        const bool variation = (bar % 2) == 1;
        const bool fill = (bar % 4) == 3;
        for (int s = 0; s < kStepsPerBar; ++s)
        {
            const int step = bar * kStepsPerBar + s;
            for (int r = 0; r < kDrumRows; ++r)
            {
                uint8_t v = variation ? vari[s][r] : home[s][r];
                // Keep the backbone identical on every bar.
                if (tpl[r][s] >= 100) v = home[s][r];
                if (fill && s >= 12)
                {
                    if (r == int(DrumRow::Snare) && rng.uni() < 0.45f * dens) v = humanVel(rng, 70 + (s - 12) * 10);
                    if (r == int(DrumRow::Ghost) && rng.uni() < 0.3f * dens) v = humanVel(rng, 70);
                }
                if (fill && s == 15 && r == int(DrumRow::Fx) && rng.uni() < 0.5f) v = humanVel(rng, 90);
                if (bar % 8 == 0 && s == 0 && r == int(DrumRow::Fx) && bar > 0 && rng.uni() < 0.25f) v = humanVel(rng, 90);
                // Every row is computed either way, so a locked row never changes what
                // the same seed gives the others.
                if (rowMask & (1u << r)) pt.drums[step][r].store(v, std::memory_order_relaxed);
            }
        }
    }
}

// ---- melody / chord lane generator ------------------------------------------
namespace {

const int kScales[int(Scale::Count)][7] = {
    {0, 2, 3, 5, 7, 8, 10},   // minor
    {0, 2, 3, 5, 7, 8, 11},   // harmonic minor
    {0, 2, 3, 5, 7, 9, 10},   // dorian
    {0, 1, 3, 5, 7, 8, 10},   // phrygian
};
// Four-bar progressions as scale degrees (0 = i).
const int kProgressions[int(Scale::Count)][3][4] = {
    {{0, 5, 2, 6}, {0, 3, 5, 4}, {0, 6, 5, 6}},
    {{0, 5, 3, 4}, {0, 3, 4, 0}, {0, 5, 1, 4}},
    {{0, 3, 0, 3}, {0, 6, 3, 0}, {0, 2, 3, 3}},
    {{0, 1, 0, 6}, {0, 1, 5, 1}, {0, 6, 1, 0}},
};

int scaleNote(int root, Scale sc, int degree)
{
    const int* iv = kScales[int(sc)];
    int oct = 0;
    while (degree < 0) { degree += 7; --oct; }
    oct += degree / 7;
    return root + iv[degree % 7] + 12 * oct;
}

void put(Pattern& pt, int step, const uint8_t* notes, int count, int vel, int len)
{
    MelodyStep m;
    for (int i = 0; i < count && i < 3; ++i) m.notes[i] = uint8_t(clampf(float(notes[i]), 1.0f, 127.0f));
    m.count = uint8_t(std::min(count, 3));
    m.velocity = uint8_t(clampf(float(vel), 1.0f, 127.0f));
    m.length = uint8_t(std::max(1, std::min(16, len)));
    pt.melody[step].store(m.pack(), std::memory_order_relaxed);
}

} // namespace

void generateLane(Project& project, int pattern, LaneStyle style, uint8_t rootNote, Scale scale, uint32_t seed)
{
    Pattern& pt = project.patterns[pattern % kPatterns];
    for (int s = 0; s < kMaxSteps; ++s) pt.melody[s].store(0, std::memory_order_relaxed);

    Rng rng;
    rng.seed(seed * 2246822519u + uint32_t(style) * 131u + 7u);
    const int* prog = kProgressions[int(scale)][rng.below(3)];
    const int root = int(rootNote);

    // Lead motif: generated once per two-bar phrase pair and reused, so it sings.
    struct MotifNote { int step; int degreeOffset; int len; };
    MotifNote motif[2][8];
    int motifCount[2] = {0, 0};
    for (int b = 0; b < 2; ++b)
    {
        int s = 0, deg = rng.below(3) * 2;   // start on a chord tone
        while (s < kStepsPerBar && motifCount[b] < 8)
        {
            const int len = (s == 0 || rng.uni() < 0.4f) ? 2 + rng.below(2) : 1 + rng.below(2);
            motif[b][motifCount[b]++] = {s, deg, len};
            s += len + (rng.uni() < 0.35f ? 1 + rng.below(2) : 0);
            deg += rng.below(5) - 2;
            deg = std::max(-2, std::min(7, deg));
        }
    }
    const int bassRhythms[3][6][2] = {   // {step, len}, len 0 = end
        {{0, 3}, {3, 1}, {6, 2}, {10, 2}, {14, 1}, {0, 0}},
        {{0, 2}, {4, 2}, {7, 1}, {8, 2}, {11, 1}, {12, 3}},
        {{0, 6}, {8, 3}, {11, 1}, {14, 2}, {0, 0}, {0, 0}},
    };
    const int bassChoice = rng.below(3);
    const int stabChoice = rng.below(2);

    for (int bar = 0; bar < kMaxBars; ++bar)
    {
        const int degree = prog[bar % 4];
        const int nextDegree = prog[(bar + 1) % 4];
        const int base = bar * kStepsPerBar;

        switch (style)
        {
            case LaneStyle::Bassline:
            {
                const int r = root - 24;
                for (int k = 0; k < 6; ++k)
                {
                    const int st = bassRhythms[bassChoice][k][0], len = bassRhythms[bassChoice][k][1];
                    if (len == 0) break;
                    int n = scaleNote(r, scale, degree);
                    const float roll = rng.uni();
                    if (k > 0 && roll < 0.2f) n += 12;                                  // octave jump
                    else if (k > 0 && roll < 0.32f) n = scaleNote(r, scale, degree + 4); // fifth
                    if (st >= 14) n = scaleNote(r, scale, nextDegree + (rng.uni() < 0.5f ? 1 : -1)); // approach
                    const uint8_t nn = uint8_t(n);
                    put(pt, base + st, &nn, 1, k == 0 ? 110 : 92, len);
                }
                break;
            }
            case LaneStyle::Lead:
            {
                const int r = root + 12;
                const int which = (bar % 4 == 3) ? 1 : 0;   // answer phrase on the 4th bar
                for (int k = 0; k < motifCount[which]; ++k)
                {
                    const MotifNote& m = motif[which][k];
                    int len = m.len;
                    if (k == motifCount[which] - 1) len = std::max(len, kStepsPerBar - m.step);
                    len = std::min(len, kStepsPerBar - m.step);
                    const uint8_t nn = uint8_t(scaleNote(r, scale, degree + m.degreeOffset));
                    put(pt, base + m.step, &nn, 1, m.step % 4 == 0 ? 104 : 88, len);
                }
                break;
            }
            case LaneStyle::ChordStabs:
            {
                const uint8_t chord[3] = {uint8_t(scaleNote(root, scale, degree)),
                                          uint8_t(scaleNote(root, scale, degree + 2)),
                                          uint8_t(scaleNote(root, scale, degree + 4))};
                if (stabChoice == 0)
                {
                    put(pt, base + 0, chord, 3, 104, 3);
                    put(pt, base + 6, chord, 3, 86, 2);
                    put(pt, base + 10, chord, 3, 92, 2);
                }
                else
                {
                    put(pt, base + 0, chord, 3, 104, 4);
                    put(pt, base + 8, chord, 3, 88, 2);
                    put(pt, base + 11, chord, 3, 94, 3);
                }
                break;
            }
            case LaneStyle::PadChords:
            {
                const uint8_t chord[3] = {uint8_t(scaleNote(root, scale, degree)),
                                          uint8_t(scaleNote(root, scale, degree + 2)),
                                          uint8_t(scaleNote(root, scale, degree + 4))};
                put(pt, base, chord, 3, 92, 16);
                break;
            }
            default: break;
        }
    }
}

// ---- export ---------------------------------------------------------------------
int exportNotes(const Project& project, int pattern, ExportNote* out, int maxNotes)
{
    const Pattern& pt = project.patterns[pattern % kPatterns];
    const int steps = pt.steps();
    int n = 0;
    for (int s = 0; s < steps && n < maxNotes; ++s)
    {
        const double beat = s * 0.25;
        for (int r = 0; r < kDrumRows && n < maxNotes; ++r)
        {
            const uint8_t v = pt.drums[s][r].load(std::memory_order_relaxed);
            if (v) out[n++] = {beat, 0.25, 0, kDrumNotes[r], v};
        }
        const MelodyStep m = MelodyStep::unpack(pt.melody[s].load(std::memory_order_relaxed));
        for (int i = 0; i < m.count && n < maxNotes; ++i)
            out[n++] = {beat, m.length * 0.25, 1, m.notes[i], m.velocity};
    }
    return n;
}

} // namespace mt::hauntedhall
