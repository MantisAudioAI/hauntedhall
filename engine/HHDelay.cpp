// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHDelay.h"

namespace mt::hauntedhall {

void DigitalDelay::prepare(float sr)
{
    sr_ = sr;
    int size = 1;
    while (size < int(sr * kMaxSeconds) + 8) size <<= 1;
    bufL_.assign(size_t(size), 0.0f);
    bufR_.assign(size_t(size), 0.0f);
    mask_ = size - 1;
    lpL_.setLowpass(4200.0f, sr);  lpR_ = lpL_;
    hpL_.setLowpass(110.0f, sr);   hpR_ = hpL_;
    curSamples_ = targetSamples_;
    reset();
}

void DigitalDelay::reset()
{
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    w_ = 0;
    lpL_.z = lpR_.z = hpL_.z = hpR_.z = 0.0f;
    curSamples_ = targetSamples_;
    energy_ = 0.0f;
    quietCount_ = 0;
    silent_ = true;
}

float DigitalDelay::read(const std::vector<float>& buf, float d) const
{
    const int di = int(d);
    const float f = d - float(di);
    const float a = buf[size_t((w_ - di) & mask_)];
    const float b = buf[size_t((w_ - di - 1) & mask_)];
    return a + (b - a) * f;
}

void DigitalDelay::process(const float* in, float* wetL, float* wetR, int n)
{
    // Skip entirely when nothing goes in and the trails have died away.
    bool anyIn = false;
    if (!bypass_)
        for (int i = 0; i < n; ++i) if (in[i] != 0.0f) { anyIn = true; break; }
    if (silent_ && !anyIn) { inGain_ = bypass_ ? 0.0f : 1.0f; return; }
    silent_ = false;

    const float glide = 1.0f - expCoef(0.12f, sr_);             // TIME changes glide over ~0.1 s
    const float gainStep = 1.0f / (0.01f * sr_);                 // 10 ms in/out of bypass
    const float spread = 0.007f * sr_;                           // right side 7 ms later: width
    const float q = 2048.0f;                                     // 12-bit repeats
    float peak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        curSamples_ += (targetSamples_ - curSamples_) * glide;
        const float tapL = read(bufL_, curSamples_);
        const float tapR = read(bufR_, std::min(curSamples_ + spread, float(mask_ - 2)));

        if (bypass_) inGain_ = std::max(0.0f, inGain_ - gainStep);
        else inGain_ = std::min(1.0f, inGain_ + gainStep);
        const float x = in[i] * inGain_;

        float fbL = hpL_.hp(lpL_.lp(tapL)) * feedback_;
        float fbR = hpR_.hp(lpR_.lp(tapR)) * feedback_;
        fbL = std::floor(softSat(fbL) * q + 0.5f) / q;
        fbR = std::floor(softSat(fbR) * q + 0.5f) / q;

        bufL_[size_t(w_)] = x + fbL;
        bufR_[size_t(w_)] = x + fbR;
        w_ = (w_ + 1) & mask_;

        wetL[i] += tapL;
        wetR[i] += tapR;
        peak = std::max(peak, std::fabs(tapL) + std::fabs(x));
    }

    // Idle detection: 0.5 s below about -100 dBFS with no input.
    if (peak < 1e-5f && !anyIn)
    {
        quietCount_ += n;
        if (quietCount_ > int(sr_ * 0.5f) + int(curSamples_))
            silent_ = true;   // what is left is below -100 dBFS (and the 12-bit repeats round it to 0)
    }
    else
        quietCount_ = 0;
}

} // namespace mt::hauntedhall
