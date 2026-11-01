// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: "corrosion", the sound of a worn cartridge.
//
// One amount (0..1) drives five stages, in this order:
//   wobble   - a slowly drifting short delay, so pitch sags and warps
//   rate     - sample-and-hold rate reduction (grit, aliasing)
//   bits     - bit-depth reduction, 16 bits down to 5
//   stutter  - now and then a 25-60 ms chunk repeats, like a skipping read
//   dropout  - now and then the signal cuts out for 8-35 ms (with 1 ms ramps)
// Stages fade in at different points so low amounts stay musical; stutters and
// dropouts only appear in the upper third. Random events come from a seeded
// generator, so the same input and settings always give the same output.
#pragma once

#include "HHCommon.h"
#include <vector>

namespace mt::hauntedhall {

class Corrosion
{
public:
    void prepare(float sampleRate, uint32_t seed);
    void reset();
    void setAmount(float amount01) { target_ = clampf(amount01, 0.0f, 1.0f); }
    float amount() const { return amount_; }
    // In place, mono.
    void process(float* buf, int n);

private:
    float sr_ = 48000.0f;
    float target_ = 0.0f, amount_ = 0.0f;
    Rng rng_;
    uint32_t seed_ = 1;

    // wobble
    std::vector<float> hist_;
    int mask_ = 0;
    int w_ = 0;
    float wobbleCur_ = 0.0f, wobbleTarget_ = 0.0f;
    int wobbleCount_ = 0;
    // rate
    float holdAcc_ = 0.0f, held_ = 0.0f;
    // events
    int frameCount_ = 0, frameLen_ = 2880;
    int dropRemain_ = 0, dropLen_ = 0;
    float gate_ = 1.0f, gateStep_ = 0.001f;
    int stutterRemain_ = 0, stutterLen_ = 0, stutterPos_ = 0, stutterStart_ = 0;
};

} // namespace mt::hauntedhall
