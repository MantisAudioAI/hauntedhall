// SPDX-License-Identifier: AGPL-3.0-or-later
// Round-1 reference presets: three or four per category, to be approved by ear
// before the full banks are built. Names are original to Haunted Hall.
#include "HHPresets.h"

#include <array>
#include <cstring>

namespace mt::hauntedhall {

namespace {

SynthPreset make(const char* name, Category c, Cartridge cart, SynthPatch p) { return {name, c, cart, p}; }

std::array<SynthPreset, 13> buildSynthPresets()
{
    std::array<SynthPreset, 13> out{};
    int i = 0;

    // ---------------- Bass ----------------
    {   // the classic stepped-triangle console bass with a thin pulse on top
        SynthPatch p;
        p.waveA = Wave::Triangle;
        p.waveB = Wave::Pulse; p.dutyB = 0.125f; p.mixB = 0.3f;
        p.attack = 0.002f; p.decay = 0.35f; p.sustain = 0.65f; p.release = 0.08f;
        p.cutoff = 900.0f; p.resonance = 0.2f; p.envOctaves = 2.0f; p.keyTrack = 0.3f;
        p.mono = true; p.glide = 0.03f;
        p.drive = 0.25f; p.level = 0.62f;
        out[i++] = make("Dungeon Bass", Category::Bass, Cartridge::HauntedCastle, p);
    }
    {   // 16-bit FM bass: bright pluck falling into a round body
        SynthPatch p;
        p.waveA = Wave::Fm; p.fmRatio = 1.0f; p.fmIndex = 2.6f; p.fmIndexDecay = 0.12f;
        p.waveB = Wave::Triangle; p.mixB = 0.3f;
        p.attack = 0.002f; p.decay = 0.4f; p.sustain = 0.55f; p.release = 0.07f;
        p.cutoff = 2600.0f; p.resonance = 0.1f; p.envOctaves = 1.0f; p.keyTrack = 0.4f;
        p.mono = true; p.glide = 0.05f;
        p.drive = 0.15f; p.level = 0.58f;
        out[i++] = make("Sub Runner", Category::Bass, Cartridge::NightRacer, p);
    }
    {   // worn-out cartridge bass: hollow table wave, wobble and bit rot baked in
        SynthPatch p;
        p.waveA = Wave::Table; p.tableA = WaveTable::Ghost;
        p.waveB = Wave::Pulse; p.dutyB = 0.25f; p.mixB = 0.35f; p.detuneB = 8.0f;
        p.attack = 0.003f; p.decay = 0.5f; p.sustain = 0.5f; p.release = 0.12f;
        p.cutoff = 1400.0f; p.resonance = 0.25f; p.envOctaves = 1.2f; p.keyTrack = 0.3f;
        p.vibDepth = 0.08f; p.vibRate = 3.2f; p.vibDelay = 0.1f;
        p.mono = true; p.glide = 0.02f;
        p.corrosion = 0.45f; p.level = 0.6f;
        out[i++] = make("Ghost Bass", Category::Bass, Cartridge::FlyingGhosts, p);
    }

    // ---------------- Lead ----------------
    {   // bright hero lead: two pulses an octave apart, delayed vibrato
        SynthPatch p;
        p.waveA = Wave::Pulse; p.dutyA = 0.25f;
        p.waveB = Wave::Pulse; p.dutyB = 0.125f; p.octaveB = 12.0f; p.mixB = 0.28f;
        p.attack = 0.005f; p.decay = 0.4f; p.sustain = 0.72f; p.release = 0.25f;
        p.cutoff = 5200.0f; p.resonance = 0.12f; p.keyTrack = 0.5f;
        p.vibDepth = 0.25f; p.vibRate = 5.8f; p.vibDelay = 0.2f;
        p.pwmDepth = 0.08f; p.pwmRate = 0.6f;
        p.mono = true; p.glide = 0.04f;
        p.level = 0.36f;
        out[i++] = make("Crystal Lead", Category::Lead, Cartridge::CrystalSword, p);
    }
    {   // detuned saw horn that swells, for big boss themes (poly, so it can play chords)
        SynthPatch p;
        p.waveA = Wave::Saw;
        p.waveB = Wave::Saw; p.detuneB = 9.0f; p.mixB = 0.5f;
        p.attack = 0.04f; p.decay = 0.6f; p.sustain = 0.75f; p.release = 0.3f;
        p.cutoff = 1300.0f; p.resonance = 0.3f; p.envOctaves = 1.6f; p.keyTrack = 0.6f;
        p.vibDepth = 0.18f; p.vibRate = 5.0f; p.vibDelay = 0.35f;
        p.drive = 0.4f; p.level = 0.3f;
        out[i++] = make("Dragon Horn", Category::Lead, Cartridge::BossDragon, p);
    }
    {   // metallic FM shimmer lead
        SynthPatch p;
        p.waveA = Wave::Fm; p.fmRatio = 3.0f; p.fmIndex = 3.0f; p.fmIndexDecay = 0.25f;
        p.waveB = Wave::Pulse; p.dutyB = 0.125f; p.octaveB = 12.0f; p.mixB = 0.18f;
        p.attack = 0.003f; p.decay = 0.5f; p.sustain = 0.6f; p.release = 0.4f;
        p.cutoff = 6500.0f; p.resonance = 0.05f; p.keyTrack = 0.4f;
        p.vibDepth = 0.15f; p.vibRate = 6.0f; p.vibDelay = 0.25f;
        p.mono = true; p.glide = 0.02f;
        p.level = 0.34f;
        out[i++] = make("Sword Tone", Category::Lead, Cartridge::CrystalSword, p);
    }
    {   // chip "fake chord": any chord played on it becomes a fast arpeggio
        SynthPatch p;
        p.waveA = Wave::Pulse; p.dutyA = 0.25f;
        p.attack = 0.002f; p.decay = 0.3f; p.sustain = 0.6f; p.release = 0.15f;
        p.cutoff = 6000.0f; p.keyTrack = 0.4f;
        p.arp = true; p.arpRate = 30.0f;
        p.level = 0.34f;
        out[i++] = make("Hero Arp", Category::Lead, Cartridge::PlatformBlock, p);
    }

    // ---------------- Pad ----------------
    {   // ghostly choir: uneven table waves, slow swell, a little cartridge wear
        SynthPatch p;
        p.waveA = Wave::Table; p.tableA = WaveTable::Ghost;
        p.waveB = Wave::Table; p.tableB = WaveTable::Soft; p.detuneB = 12.0f; p.mixB = 0.5f;
        p.attack = 0.35f; p.decay = 1.0f; p.sustain = 0.85f; p.release = 1.2f;
        p.cutoff = 2600.0f; p.resonance = 0.1f; p.keyTrack = 0.3f;
        p.vibDepth = 0.12f; p.vibRate = 4.8f; p.vibDelay = 0.1f;
        p.corrosion = 0.2f; p.level = 0.3f;
        out[i++] = make("Haunted Choir", Category::Pad, Cartridge::HauntedCastle, p);
    }
    {   // pipe-organ pad from the organ table, octave doubled
        SynthPatch p;
        p.waveA = Wave::Table; p.tableA = WaveTable::Organ;
        p.waveB = Wave::Table; p.tableB = WaveTable::Organ; p.octaveB = 12.0f; p.detuneB = 4.0f; p.mixB = 0.4f;
        p.attack = 0.02f; p.decay = 0.5f; p.sustain = 1.0f; p.release = 0.5f;
        p.cutoff = 4500.0f; p.keyTrack = 0.3f;
        p.vibDepth = 0.08f; p.vibRate = 6.0f; p.vibDelay = 0.0f;
        p.level = 0.26f;
        out[i++] = make("Cathedral", Category::Pad, Cartridge::HauntedOrgan, p);
    }
    {   // wide, slow pulse-width pad with a saw underneath
        SynthPatch p;
        p.waveA = Wave::Pulse; p.dutyA = 0.5f; p.pwmDepth = 0.3f; p.pwmRate = 0.35f;
        p.waveB = Wave::Saw; p.detuneB = 12.0f; p.mixB = 0.45f;
        p.attack = 0.6f; p.decay = 1.0f; p.sustain = 0.9f; p.release = 1.5f;
        p.cutoff = 1800.0f; p.resonance = 0.15f; p.keyTrack = 0.3f;
        p.level = 0.28f;
        out[i++] = make("Space Pad", Category::Pad, Cartridge::AlienSaucer, p);
    }
    {   // the choir on a badly worn cartridge
        SynthPatch p;
        p.waveA = Wave::Table; p.tableA = WaveTable::Ghost;
        p.waveB = Wave::Triangle; p.octaveB = -12.0f; p.mixB = 0.3f;
        p.attack = 0.25f; p.decay = 1.0f; p.sustain = 0.8f; p.release = 1.0f;
        p.cutoff = 2200.0f; p.keyTrack = 0.3f;
        p.vibDepth = 0.2f; p.vibRate = 0.9f;
        p.corrosion = 0.78f; p.level = 0.3f;
        out[i++] = make("Corrupted Save", Category::Pad, Cartridge::Skeleton, p);
    }
    {   // short pulse stab pad for chord parts
        SynthPatch p;
        p.waveA = Wave::Pulse; p.dutyA = 0.5f;
        p.waveB = Wave::Pulse; p.dutyB = 0.25f; p.detuneB = 7.0f; p.mixB = 0.5f;
        p.attack = 0.004f; p.decay = 0.35f; p.sustain = 0.35f; p.release = 0.25f;
        p.cutoff = 2400.0f; p.envOctaves = 1.2f; p.keyTrack = 0.4f;
        p.level = 0.26f;
        out[i++] = make("Night Stabs", Category::Pad, Cartridge::NightRacer, p);
    }
    {   // FM glass-bell pad
        SynthPatch p;
        p.waveA = Wave::Fm; p.fmRatio = 3.5f; p.fmIndex = 1.8f; p.fmIndexDecay = 0.8f;
        p.waveB = Wave::Table; p.tableB = WaveTable::Soft; p.mixB = 0.45f;
        p.attack = 0.01f; p.decay = 1.4f; p.sustain = 0.4f; p.release = 1.4f;
        p.cutoff = 5000.0f; p.keyTrack = 0.3f;
        p.level = 0.26f;
        out[i++] = make("Glowing Ambient", Category::Pad, Cartridge::AlienSaucer, p);
    }
    return out;
}

std::array<KitPreset, 3> buildKitPresets()
{
    std::array<KitPreset, 3> out{};
    {   // boom-bap chip kit
        KitPatch k;
        k.kickHz = 50.0f; k.kickSweep = 28.0f; k.kickStepped = 0.35f; k.kickDecay = 0.34f;
        k.snareHz = 185.0f; k.snareNoiseClock = 16000.0f; k.snareTone = 6500.0f;
        k.hatClock = 26000.0f; k.hatDecay = 0.045f;
        k.ghostHz = 115.0f;
        k.drive = 0.25f; k.corrosion = 0.1f;
        out[0] = {"Crypt Kit", Cartridge::Skeleton, k};
    }
    {   // pure 8-bit: fully stepped kick, metallic hats
        KitPatch k;
        k.kickHz = 55.0f; k.kickSweep = 36.0f; k.kickSweepTime = 0.018f; k.kickStepped = 1.0f; k.kickDecay = 0.26f;
        k.snareNoiseClock = 12000.0f; k.snareTone = 9000.0f; k.snareBody = 0.35f;
        k.hatMetallic = true; k.hatClock = 22000.0f; k.hatDecay = 0.03f; k.hatLevel = 0.22f;
        k.fxDuty = 0.125f;
        k.drive = 0.1f;
        out[1] = {"8-Bit Kit", Cartridge::PlatformBlock, k};
    }
    {   // long boom kick, tight hats, lower ghost
        KitPatch k;
        k.kickHz = 46.0f; k.kickSweep = 24.0f; k.kickDecay = 0.9f; k.kickStepped = 0.15f; k.kickClick = 0.35f;
        k.snareHz = 200.0f; k.snareNoiseDecay = 0.18f;
        k.hatClock = 34000.0f; k.hatDecay = 0.03f;
        k.ghostHz = 95.0f; k.ghostDrop = 9.0f;
        k.drive = 0.35f; k.corrosion = 0.05f;
        out[2] = {"Grave Trap", Cartridge::HauntedCastle, k};
    }
    return out;
}

const std::array<SynthPreset, 13>& synthTable() { static const auto t = buildSynthPresets(); return t; }
const std::array<KitPreset, 3>& kitTable() { static const auto t = buildKitPresets(); return t; }

} // namespace

const char* categoryName(Category c)
{
    static const char* n[] = {"Drum", "Bass", "Lead", "Pad"};
    return n[int(c) % int(Category::Count)];
}

const char* cartridgeName(Cartridge c)
{
    static const char* n[] = {"Haunted Castle", "Skeleton", "Haunted Organ", "Alien Saucer", "Night Racer",
                              "Boss Dragon", "Crystal Sword", "Platform Block", "Flying Ghosts"};
    return n[int(c) % int(Cartridge::Count)];
}

int numSynthPresets() { return int(synthTable().size()); }
const SynthPreset& synthPreset(int i) { return synthTable()[size_t(std::max(0, std::min(i, numSynthPresets() - 1)))]; }
int numKitPresets() { return int(kitTable().size()); }
const KitPreset& kitPreset(int i) { return kitTable()[size_t(std::max(0, std::min(i, numKitPresets() - 1)))]; }

int findSynthPreset(const char* name)
{
    for (int i = 0; i < numSynthPresets(); ++i)
        if (std::strcmp(synthTable()[size_t(i)].name, name) == 0) return i;
    return -1;
}

} // namespace mt::hauntedhall
