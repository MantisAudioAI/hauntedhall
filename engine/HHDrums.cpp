// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HHDrums.h"

namespace mt::hauntedhall {

int drumRowForNote(uint8_t note)
{
    for (int r = 0; r < kDrumRows; ++r)
        if (kDrumNotes[r] == note) return r;
    if (note == 35) return 0;   // acoustic bass drum
    if (note == 40) return 1;   // electric snare
    if (note == 44 || note == 46) return 2;
    return -1;
}

void DrumKit::prepare(float sr, uint32_t seed)
{
    sr_ = sr;
    corrosion_.prepare(sr, seed);
    tailLen_ = int(sr * 0.12f);
    kick_ = snare_ = hat_ = ghost_ = fx_ = Hit{};
    kickNoise_.reset(); snareNoise_.reset(); hatNoise_.reset();
    kickPhase_ = snarePhase_ = ghostPhase_ = fxPhase_ = 0.0f;
    snareLp_.z = hatHp_.z = hatHp2_.z = clickLp_.z = 0.0f;
    tail_ = 0;
}

void DrumKit::trigger(int row, float velocity)
{
    Hit* h = nullptr;
    float* phase = nullptr;
    switch (DrumRow(row))
    {
        case DrumRow::Kick:  h = &kick_;  phase = &kickPhase_; break;
        case DrumRow::Snare: h = &snare_; phase = &snarePhase_; break;
        case DrumRow::Hat:   h = &hat_;   break;
        case DrumRow::Ghost: h = &ghost_; phase = &ghostPhase_; break;
        case DrumRow::Fx:    h = &fx_;    phase = &fxPhase_; break;
        default: return;
    }
    const float vel = 0.3f + 0.7f * clampf(velocity, 0.0f, 1.0f);
    // A hit that is still ringing keeps its phase and ramps from its current
    // level; a fresh hit starts at a fixed phase (deterministic, punchy).
    h->from = h->on ? clampf(h->env * h->vel / vel, 0.0f, 1.0f) : 0.0f;
    if (!h->on && phase) *phase = 0.0f;
    h->on = true;
    h->t = 0.0f;
    h->vel = vel;
    h->env = 1.0f;
}

bool DrumKit::active() const
{
    return kick_.on || snare_.on || hat_.on || ghost_.on || fx_.on || tail_ > 0;
}

void DrumKit::process(float* out, int n)
{
    std::fill(out, out + n, 0.0f);
    const bool any = kick_.on || snare_.on || hat_.on || ghost_.on || fx_.on;
    if (any) tail_ = tailLen_;
    else if (tail_ > 0) tail_ = std::max(0, tail_ - n);
    else return;

    const KitPatch& p = patch_;
    const float dt = 1.0f / sr_;
    const float dScale = std::exp2((decay_ - 0.5f) * 3.0f);   // 0.35x .. 2.8x
    const float tScale = std::exp2((tone_ - 0.5f) * 2.0f);    // brightness, -1 .. +1 octave
    const float tPitch = std::exp2((tone_ - 0.5f) * 0.5f);    // small pitch lift with TONE

    // per-block coefficients
    const float kickCoef = expCoef(p.kickDecay * dScale * 0.3f, sr_);
    const float sweepCoef = expCoef(p.kickSweepTime, sr_);
    const float snBodyCoef = expCoef(p.snareBodyDecay * dScale * 0.35f, sr_);
    const float snNoiseCoef = expCoef(p.snareNoiseDecay * dScale * 0.35f, sr_);
    const float hatCoef = expCoef(p.hatDecay * dScale * 0.35f, sr_);
    const float ghostCoef = expCoef(p.ghostDecay * dScale * 0.35f, sr_);
    const float fxCoef = expCoef(p.fxDecay * dScale * 0.35f, sr_);
    snareLp_.setLowpass(p.snareTone * tScale, sr_);
    hatHp_.setLowpass(p.hatHighpass, sr_);
    hatHp2_.setLowpass(p.hatHighpass * 0.7f, sr_);
    clickLp_.setLowpass(2500.0f * tScale, sr_);
    const float driveGain = 1.0f + 3.0f * p.drive;
    const float driveNorm = 1.0f / (1.0f + 0.9f * p.drive);
    const float attackLen = 0.0015f;   // 1.5 ms soft attack on tonal hits (no clicks)

    for (int i = 0; i < n; ++i)
    {
        float y = 0.0f;

        if (kick_.on)
        {
            if (kick_.t == 0.0f) kickSweep_ = p.kickSweep * tPitch;
            kickSweep_ *= sweepCoef;
            if ((i & 7) == 0 || kick_.t == 0.0f) kickFreq_ = p.kickHz * tPitch * std::exp2(kickSweep_ * (1.0f / 12.0f));
            const float f = kickFreq_;
            const float sine = sineCycles(kickPhase_);
            const float tri = 1.0f - 4.0f * std::fabs(kickPhase_ - 0.5f);
            const float stepped = std::floor((tri + 1.0f) * 7.5f + 0.5f) * (1.0f / 7.5f) - 1.0f;
            float body = sine + (stepped - sine) * p.kickStepped;
            kickPhase_ += f * dt;
            if (kickPhase_ >= 1.0f) kickPhase_ -= 1.0f;
            float click = 0.0f;
            if (kick_.t < 0.005f)
                click = clickLp_.lp(kickNoise_.process(20000.0f, sr_, false)) * (1.0f - kick_.t / 0.005f);
            kick_.env *= kickCoef;
            const float atk = kick_.attack(0.0004f);
            y += (body * kick_.env * atk + click * p.kickClick * tScale) * kick_.vel * p.kickLevel;
            kick_.t += dt;
            if (kick_.env < 1e-4f) kick_.on = false;
        }

        if (snare_.on)
        {
            if ((i & 7) == 0 || snare_.t == 0.0f)
                snareFreq_ = p.snareHz * tPitch * std::exp2(-3.0f * std::min(1.0f, snare_.t / 0.03f) / 12.0f);
            const float f = snareFreq_;
            const float tri = 1.0f - 4.0f * std::fabs(snarePhase_ - 0.5f);
            const float stepped = std::floor((tri + 1.0f) * 7.5f + 0.5f) * (1.0f / 7.5f) - 1.0f;
            snarePhase_ += f * dt;
            if (snarePhase_ >= 1.0f) snarePhase_ -= 1.0f;
            if (snare_.t == 0.0f) snareNoiseEnv_ = 1.0f;
            snare_.env *= snBodyCoef;
            snareNoiseEnv_ *= snNoiseCoef;
            const float nz = snareLp_.lp(snareNoise_.process(p.snareNoiseClock * tScale, sr_, false));
            const float atk = snare_.attack(attackLen);
            y += (stepped * snare_.env * p.snareBody + nz * snareNoiseEnv_ * 1.4f) * atk * snare_.vel * p.snareLevel;
            snare_.t += dt;
            if (snare_.env < 1e-4f && snareNoiseEnv_ < 1e-4f) snare_.on = false;
        }

        if (hat_.on)
        {
            hat_.env *= hatCoef;
            float nz = hatNoise_.process(p.hatClock * tScale, sr_, p.hatMetallic);
            nz = hatHp2_.hp(hatHp_.hp(nz));
            const float atk = hat_.attack(0.0005f);
            y += nz * hat_.env * atk * hat_.vel * p.hatLevel;
            hat_.t += dt;
            if (hat_.env < 1e-4f) hat_.on = false;
        }

        if (ghost_.on)
        {
            const float prog = std::min(1.0f, ghost_.t / std::max(0.02f, p.ghostDecay * dScale));
            if ((i & 7) == 0 || ghost_.t == 0.0f)
            {
                const float wob = p.ghostWobble * sineCycles(ghost_.t * 6.5f);
                ghostFreq_ = p.ghostHz * tPitch * std::exp2((-p.ghostDrop * prog + wob) / 12.0f);
            }
            const float f = ghostFreq_;
            const float tri = 1.0f - 4.0f * std::fabs(ghostPhase_ - 0.5f);
            const float sine = sineCycles(ghostPhase_);
            const float s3 = sineCycles(ghostPhase_ * 3.0f + 0.1f);
            // Ghost: hollow sine + 3rd; Soft: sine; Hollow/Organ: triangle. Then 4-bit steps.
            float w = 0.7f * sine + 0.3f * s3;
            if (p.ghostTable == WaveTable::Soft) w = sine;
            else if (p.ghostTable != WaveTable::Ghost) w = tri;
            w = std::floor((clampf(w, -1.0f, 1.0f) + 1.0f) * 7.5f + 0.5f) * (1.0f / 7.5f) - 1.0f;
            ghostPhase_ += f * dt;
            if (ghostPhase_ >= 1.0f) ghostPhase_ -= 1.0f;
            ghost_.env *= ghostCoef;
            const float atk = ghost_.attack(0.004f);
            y += w * ghost_.env * atk * ghost_.vel * p.ghostLevel;
            ghost_.t += dt;
            if (ghost_.env < 1e-4f) ghost_.on = false;
        }

        if (fx_.on)
        {
            const float prog = std::min(1.0f, fx_.t / std::max(0.005f, p.fxTime));
            if ((i & 7) == 0 || fx_.t == 0.0f) fxFreq_ = p.fxStartHz * tPitch * std::pow(p.fxEndHz / p.fxStartHz, prog);
            const float f = fxFreq_;
            const float fdt = clampf(f * dt, 0.0f, 0.45f);
            float pulse = fxPhase_ < p.fxDuty ? 1.0f : -1.0f;
            pulse += polyBlep(fxPhase_, fdt);
            float t2 = fxPhase_ - p.fxDuty;
            if (t2 < 0.0f) t2 += 1.0f;
            pulse -= polyBlep(t2, fdt);
            pulse -= (2.0f * p.fxDuty - 1.0f);
            fxPhase_ += fdt;
            if (fxPhase_ >= 1.0f) fxPhase_ -= 1.0f;
            fx_.env *= fxCoef;
            const float atk = fx_.attack(attackLen);
            y += pulse * fx_.env * atk * fx_.vel * p.fxLevel;
            fx_.t += dt;
            if (fx_.env < 1e-4f) fx_.on = false;
        }

        if (p.drive > 0.0f) y = softSat(y * driveGain) * driveNorm;
        out[i] = y;
    }

    corrosion_.setAmount(clampf(p.corrosion + spook_, 0.0f, 1.0f));
    corrosion_.process(out, n);
    for (int i = 0; i < n; ++i) out[i] *= p.level;
}

} // namespace mt::hauntedhall
