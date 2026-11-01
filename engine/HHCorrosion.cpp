// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHCorrosion.h"

namespace mt::hauntedhall {

void Corrosion::prepare(float sampleRate, uint32_t seed)
{
    sr_ = sampleRate;
    seed_ = seed;
    int size = 1;
    while (size < int(sampleRate * 0.1f)) size <<= 1;   // >= 100 ms of history
    hist_.assign(size_t(size), 0.0f);
    mask_ = size - 1;
    frameLen_ = int(sampleRate * 0.06f);
    gateStep_ = 1.0f / (0.001f * sampleRate);
    reset();
}

void Corrosion::reset()
{
    std::fill(hist_.begin(), hist_.end(), 0.0f);
    rng_.seed(seed_);
    w_ = 0;
    amount_ = target_;
    wobbleCur_ = wobbleTarget_ = 0.0f;
    wobbleCount_ = 0;
    holdAcc_ = 0.0f; held_ = 0.0f;
    frameCount_ = 0;
    dropRemain_ = dropLen_ = 0;
    gate_ = 1.0f;
    stutterRemain_ = stutterLen_ = stutterPos_ = stutterStart_ = 0;
}

void Corrosion::process(float* buf, int n)
{
    // Ramp the amount across the block (no zipper noise when SPOOK moves).
    const float a0 = amount_;
    const float a1 = target_;
    amount_ = a1;
    const float aStep = (a1 - a0) / float(n > 0 ? n : 1);

    // Fully clean: keep history warm (so turning SPOOK up is seamless) and leave.
    if (a0 < 1e-4f && a1 < 1e-4f && stutterRemain_ == 0 && dropRemain_ == 0 && gate_ >= 1.0f)
    {
        for (int i = 0; i < n; ++i) { hist_[size_t(w_)] = buf[i]; w_ = (w_ + 1) & mask_; }
        held_ = n > 0 ? buf[n - 1] : held_;
        return;
    }

    const float wobbleMaxSamples = 0.0035f * sr_;
    const int fade = std::max(1, int(0.002f * sr_));

    // Bit depth follows the amount at block rate (it is a coarse, stepped control anyway).
    const float bAmt = clampf(std::max(a0, a1) / 0.7f, 0.0f, 1.0f);
    const bool bitsOn = bAmt > 0.0f;
    const float q = std::exp2(15.0f - 11.0f * bAmt);
    const float invQ = 1.0f / q;

    float a = a0;
    for (int i = 0; i < n; ++i, a += aStep)
    {
        const float x = buf[i];
        hist_[size_t(w_)] = x;

        // --- wobble: random slow drift of a short delay ---------------------
        const float wAmt = clampf(a * 1.6f, 0.0f, 1.0f);
        if (--wobbleCount_ <= 0)
        {
            wobbleCount_ = int(sr_ * (0.08f + 0.12f * rng_.uni()));
            wobbleTarget_ = rng_.uni();
        }
        wobbleCur_ += (wobbleTarget_ - wobbleCur_) * 0.00012f;
        const float d = wAmt * (0.15f + 0.85f * wobbleCur_) * wobbleMaxSamples;
        const int di = int(d);
        const float df = d - float(di);
        const float s0 = hist_[size_t((w_ - di) & mask_)];
        const float s1 = hist_[size_t((w_ - di - 1) & mask_)];
        float y = s0 + (s1 - s0) * df;

        // --- stutter: replay a recent chunk once -----------------------------
        if (stutterRemain_ > 0)
        {
            const float rep = hist_[size_t((stutterStart_ + stutterPos_) & mask_)];
            float mix = 1.0f;
            if (stutterPos_ < fade) mix = float(stutterPos_) / float(fade);
            else if (stutterRemain_ < fade) mix = float(stutterRemain_) / float(fade);
            y += (rep - y) * mix;
            ++stutterPos_;
            --stutterRemain_;
        }

        // --- rate reduction (sample and hold) -------------------------------
        const float r = clampf((a - 0.15f) / 0.85f, 0.0f, 1.0f);
        const float hold = 1.0f + 7.0f * r * r;
        holdAcc_ += 1.0f;
        if (holdAcc_ >= hold) { holdAcc_ -= hold; held_ = y; }
        y = held_;

        // --- bit reduction ---------------------------------------------------
        if (bitsOn)
            y = std::floor(y * q + 0.5f) * invQ;

        // --- dropout gate ----------------------------------------------------
        if (dropRemain_ > 0)
        {
            --dropRemain_;
            gate_ = std::max(0.0f, gate_ - gateStep_);
        }
        else if (gate_ < 1.0f)
            gate_ = std::min(1.0f, gate_ + gateStep_);
        y *= gate_;

        // --- schedule events once per 60 ms frame ----------------------------
        if (++frameCount_ >= frameLen_)
        {
            frameCount_ = 0;
            const float s = clampf((a - 0.66f) / 0.34f, 0.0f, 1.0f);
            if (s > 0.0f)
            {
                const float pDrop = 0.22f * s * s;
                const float pStut = 0.18f * s * s;
                const float roll = rng_.uni();
                if (roll < pDrop && dropRemain_ == 0)
                    dropRemain_ = int(sr_ * (0.008f + 0.027f * rng_.uni()));
                else if (roll < pDrop + pStut && stutterRemain_ == 0)
                {
                    stutterLen_ = int(sr_ * (0.025f + 0.035f * rng_.uni()));
                    stutterLen_ = std::min(stutterLen_, mask_ / 2);
                    stutterStart_ = (w_ - stutterLen_) & mask_;
                    stutterPos_ = 0;
                    stutterRemain_ = stutterLen_;
                }
            }
        }

        w_ = (w_ + 1) & mask_;
        buf[i] = y;
    }
}

} // namespace mt::hauntedhall
