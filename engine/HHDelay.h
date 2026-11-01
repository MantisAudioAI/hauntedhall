// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: the master digital delay, modelled on the character of
// mid-'80s compact digital delay pedals: dark, slightly grainy 12-bit
// repeats, repeats that thin out, and a pitch glide when TIME is turned.
#pragma once

#include "HHCommon.h"
#include <vector>

namespace mt::hauntedhall {

class DigitalDelay
{
public:
    static constexpr float kMaxSeconds = 2.0f;

    void prepare(float sr);
    void reset();
    void setTimeMs(float ms) { targetSamples_ = clampf(ms, 20.0f, kMaxSeconds * 1000.0f - 20.0f) * 0.001f * sr_; }
    void setFeedback(float fb) { feedback_ = clampf(fb, 0.0f, 0.92f); }
    void setBypass(bool b) { bypass_ = b; }
    // Mono send in; ADDS the wet stereo signal (unscaled) into wetL / wetR.
    // A bypassed pedal keeps its trails ringing out but takes no new input.
    void process(const float* in, float* wetL, float* wetR, int n);
    bool silent() const { return silent_; }

private:
    float read(const std::vector<float>& buf, float delaySamples) const;

    float sr_ = 48000.0f;
    std::vector<float> bufL_, bufR_;
    int mask_ = 0, w_ = 0;
    float curSamples_ = 18000.0f, targetSamples_ = 18000.0f;
    float feedback_ = 0.35f;
    bool bypass_ = false;
    float inGain_ = 1.0f;
    OnePole lpL_, lpR_, hpL_, hpR_;
    float energy_ = 0.0f;
    int quietCount_ = 0;
    bool silent_ = true;
};

} // namespace mt::hauntedhall
