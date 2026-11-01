// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: presets. Each preset belongs to a browser category and a
// cartridge (the picture shown on the cartridge browser screen).
#pragma once

#include "HHDrums.h"
#include "HHSynth.h"

namespace mt::hauntedhall {

enum class Category : uint8_t { Drum, Bass, Lead, Pad, Count };

// Cartridge art themes (the image, not the name, is what the browser shows).
enum class Cartridge : uint8_t
{
    HauntedCastle, Skeleton, HauntedOrgan, AlienSaucer, NightRacer,
    BossDragon, CrystalSword, PlatformBlock, FlyingGhosts, Count
};

struct SynthPreset
{
    const char* name;
    Category category;
    Cartridge cartridge;
    SynthPatch patch;
};

struct KitPreset
{
    const char* name;
    Cartridge cartridge;
    KitPatch patch;
};

const char* categoryName(Category c);
const char* cartridgeName(Cartridge c);

int numSynthPresets();
const SynthPreset& synthPreset(int index);   // index is clamped
int numKitPresets();
const KitPreset& kitPreset(int index);
int findSynthPreset(const char* name);      // -1 if not found

} // namespace mt::hauntedhall
