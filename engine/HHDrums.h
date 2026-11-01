// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: the 5-row chip drum kit (kick, snare, hat, ghost, FX).
// Every hit is synthesized from pulse, stepped-triangle, wave-table and LFSR
// noise sources; there are no samples.
#pragma once

#include "HHCommon.h"
#include "HHCorrosion.h"
#include "HHSynth.h"

namespace mt::hauntedhall {

enum class DrumRow : uint8_t { Kick, Snare, Hat, Ghost, Fx, Count };
constexpr int kDrumRows = int(DrumRow::Count);

// MIDI notes for each row (General MIDI drum map where one fits):
// kick 36, snare 38, closed hat 42, low tom 45 (ghost), hand clap 39 (FX).
constexpr uint8_t kDrumNotes[kDrumRows] = {36, 38, 42, 45, 39};
int drumRowForNote(uint8_t note);   // -1 when the note isn't mapped

struct KitPatch
{
    // kick: stepped triangle / sine body with a pitch sweep and a noise click
    float kickHz = 52.0f, kickSweep = 30.0f, kickSweepTime = 0.022f, kickDecay = 0.32f;
    float kickClick = 0.5f, kickStepped = 0.5f, kickLevel = 1.0f;
    // snare: stepped triangle body + long noise
    float snareHz = 190.0f, snareBodyDecay = 0.06f, snareNoiseDecay = 0.14f;
    float snareNoiseClock = 18000.0f, snareTone = 7000.0f, snareBody = 0.5f, snareLevel = 0.7f;
    // hat: LFSR noise through a high-pass
    float hatClock = 30000.0f, hatDecay = 0.045f, hatHighpass = 6000.0f, hatLevel = 0.32f;
    bool hatMetallic = false;
    // ghost: a falling, wobbling wave-table tom
    float ghostHz = 120.0f, ghostDrop = 7.0f, ghostDecay = 0.28f, ghostWobble = 0.4f, ghostLevel = 0.5f;
    WaveTable ghostTable = WaveTable::Ghost;
    // fx: a swept pulse "zap"
    float fxStartHz = 300.0f, fxEndHz = 1800.0f, fxTime = 0.09f, fxDecay = 0.16f, fxDuty = 0.25f, fxLevel = 0.3f;
    // kit colour
    float drive = 0.2f;
    float corrosion = 0.0f;
    float level = 0.9f;
};

class DrumKit
{
public:
    void prepare(float sr, uint32_t seed);
    void setPatch(const KitPatch& p) { patch_ = p; }
    void setMacros(float decay, float tone, float spook) { decay_ = decay; tone_ = tone; spook_ = spook; }
    void trigger(int row, float velocity);
    bool active() const;
    // WRITES mono into out.
    void process(float* out, int n);

private:
    struct Hit
    {
        bool on = false; float t = 0.0f; float vel = 1.0f; float env = 0.0f;
        float from = 0.0f;   // level the attack ramps up from (retriggers don't click)
        float attack(float len) const { return t >= len ? 1.0f : from + (1.0f - from) * (t / len); }
    };

    float sr_ = 48000.0f;
    KitPatch patch_;
    float decay_ = 0.5f, tone_ = 0.5f, spook_ = 0.0f;
    Corrosion corrosion_;

    Hit kick_, snare_, hat_, ghost_, fx_;
    float kickPhase_ = 0.0f, kickSweep_ = 0.0f;
    LfsrNoise kickNoise_, snareNoise_, hatNoise_;
    float snarePhase_ = 0.0f, snareNoiseEnv_ = 0.0f;
    OnePole snareLp_, hatHp_, hatHp2_, clickLp_;
    float ghostPhase_ = 0.0f;
    float fxPhase_ = 0.0f;
    float kickFreq_ = 50.0f, snareFreq_ = 190.0f, ghostFreq_ = 120.0f, fxFreq_ = 300.0f;   // control-rate pitch
    int tail_ = 0, tailLen_ = 4800;
};

} // namespace mt::hauntedhall
