// SPDX-License-Identifier: AGPL-3.0-or-later
// Haunted Hall: shared DSP building blocks and real-time helpers.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86_FP)
#include <xmmintrin.h>
#define HH_X86_FTZ 1
#endif

namespace mt::hauntedhall {

constexpr float kTwoPi = 6.28318530718f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float midiToHz(float note) { return 440.0f * std::exp2((note - 69.0f) * (1.0f / 12.0f)); }
inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }

// Rational tanh approximation; accurate enough for saturation, cheap, and
// stable under fast-math.
inline float softSat(float x)
{
    x = clampf(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// PolyBLEP residual (Valimaki & Huovilainen, 2007) for band-limited edges.
inline float polyBlep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Small, fast, deterministic random generator (xorshift32).
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return float(next() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    float bi() { return uni() * 2.0f - 1.0f; }                           // [-1,1)
    int below(int n) { return int(next() % uint32_t(n)); }
};

// Sets flush-to-zero for the scope of one audio block and restores the
// previous mode afterwards (denormals can slow a CPU down 10-100x).
class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals()
    {
#if defined(HH_X86_FTZ)
        saved_ = _mm_getcsr();
        _mm_setcsr(saved_ | 0x8040u);           // FTZ | DAZ
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        uint64_t v;
        asm volatile("mrs %0, fpcr" : "=r"(v));
        saved_ = v;
        asm volatile("msr fpcr, %0" : : "r"(v | (uint64_t(1) << 24)));
#endif
    }
    ~ScopedFlushDenormals()
    {
#if defined(HH_X86_FTZ)
        _mm_setcsr(static_cast<unsigned>(saved_));
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        uint64_t v = saved_;
        asm volatile("msr fpcr, %0" : : "r"(v));
#endif
    }
    ScopedFlushDenormals(const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator=(const ScopedFlushDenormals&) = delete;

private:
    uint64_t saved_ = 0;
};

// 2048-point sine table with linear interpolation. Built once; Engine::prepare()
// touches it so the audio thread never pays for the first-use initialisation.
struct SineTable
{
    static constexpr int N = 2048;
    float t[N + 1];
    SineTable()
    {
        for (int i = 0; i <= N; ++i)
            t[i] = float(std::sin(double(i) * 6.283185307179586 / N));
    }
};
inline const SineTable& sineTable()
{
    static const SineTable table;
    return table;
}
// phase in cycles; any real value
inline float sineCycles(float phase)
{
    phase -= std::floor(phase);
    const float x = phase * float(SineTable::N);
    int i = int(x);
    if (i >= SineTable::N) i = SineTable::N - 1;
    const float f = x - float(i);
    const float* t = sineTable().t;
    return t[i] + (t[i + 1] - t[i]) * f;
}

// Topology-preserving state-variable low-pass (Zavalishin). Coefficients are
// updated at control rate by the caller.
struct SvfLowpass
{
    float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    void set(float cutoffHz, float resonance01, float sampleRate)
    {
        cutoffHz = clampf(cutoffHz, 20.0f, sampleRate * 0.45f);
        g = std::tan(3.14159265f * cutoffHz / sampleRate);
        k = 2.0f - 1.9f * clampf(resonance01, 0.0f, 1.0f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    float process(float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

struct OnePole
{
    float a = 0.5f, z = 0;
    void setLowpass(float hz, float sr) { a = 1.0f - std::exp(-kTwoPi * clampf(hz, 1.0f, sr * 0.49f) / sr); }
    float lp(float x) { z += a * (x - z); return z; }
    float hp(float x) { z += a * (x - z); return x - z; }
};

// Exponential time constant helper: coefficient that reaches ~1/e in `seconds`.
inline float expCoef(float seconds, float sr)
{
    return seconds <= 0.0f ? 0.0f : std::exp(-1.0f / (seconds * sr));
}

// 15-bit linear-feedback shift register noise, the classic chip noise source.
// Long mode (tap 1) is hiss; short mode (tap 6) is a metallic 93-step buzz.
struct LfsrNoise
{
    uint16_t reg = 0x7FFF;
    float acc = 0.0f;
    float out = 1.0f;
    void reset() { reg = 0x7FFF; acc = 0.0f; out = 1.0f; }
    float process(float clockHz, float sr, bool shortMode)
    {
        acc += clampf(clockHz, 1.0f, sr * 4.0f) / sr;
        int guard = 0;
        while (acc >= 1.0f && guard++ < 8)
        {
            acc -= 1.0f;
            const uint16_t bit = uint16_t((reg ^ (reg >> (shortMode ? 6 : 1))) & 1u);
            reg = uint16_t((reg >> 1) | (bit << 14));
            out = (reg & 1u) ? -1.0f : 1.0f;
        }
        if (acc >= 1.0f) acc -= std::floor(acc);
        return out;
    }
};

// Bounded lock-free multi-producer / single-consumer queue (after Dmitry
// Vyukov's bounded MPMC design). Producers never block; push() returns false
// when full.
template <typename T, size_t N>
class MpscQueue
{
    static_assert((N & (N - 1)) == 0, "N must be a power of two");

public:
    MpscQueue()
    {
        for (size_t i = 0; i < N; ++i) cells_[i].seq.store(i, std::memory_order_relaxed);
    }
    bool push(const T& v)
    {
        size_t pos = head_.load(std::memory_order_relaxed);
        for (;;)
        {
            Cell& c = cells_[pos & (N - 1)];
            const size_t seq = c.seq.load(std::memory_order_acquire);
            const intptr_t dif = intptr_t(seq) - intptr_t(pos);
            if (dif == 0)
            {
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    c.value = v;
                    c.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (dif < 0)
                return false;
            else
                pos = head_.load(std::memory_order_relaxed);
        }
    }
    bool pop(T& out)
    {
        Cell& c = cells_[tail_ & (N - 1)];
        const size_t seq = c.seq.load(std::memory_order_acquire);
        if (intptr_t(seq) - intptr_t(tail_ + 1) < 0) return false;
        out = c.value;
        c.seq.store(tail_ + N, std::memory_order_release);
        ++tail_;
        return true;
    }

private:
    struct Cell
    {
        std::atomic<size_t> seq;
        T value;
    };
    Cell cells_[N];
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) size_t tail_ = 0;
};

} // namespace mt::hauntedhall
