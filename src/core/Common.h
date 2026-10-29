// Reality Desync - portable DSP core. No third-party dependencies.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef RD_FFT_SIZE
#define RD_FFT_SIZE 2048  // spectral engine window; also the plugin latency in samples
#endif

namespace rd {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

constexpr int kFftSize = RD_FFT_SIZE;
constexpr int kOverlap = 8;           // 87.5% overlap keeps slice triggers within ~5 ms of the grid
constexpr int kHop = kFftSize / kOverlap;
constexpr int kNumBins = kFftSize / 2 + 1;
constexpr int kMaxBands = 16;
constexpr int kCtrlBlock = 32;  // control-rate tick, in samples
constexpr int kNumSeqSteps = 8;
constexpr int kNumChannels = 2;

// ---------------------------------------------------------------------------
// Musical tables. All lengths are in quarter-note beats.
// ---------------------------------------------------------------------------
struct NoteDiv {
    const char* name;
    double beats;
};

inline constexpr NoteDiv kDivisions[] = {
    {"1/64", 0.0625},    {"1/32T", 1.0 / 12.0}, {"1/32", 0.125},   {"1/16T", 1.0 / 6.0},
    {"1/32D", 0.1875},   {"1/16", 0.25},        {"1/8T", 1.0 / 3.0}, {"1/16D", 0.375},
    {"1/8", 0.5},        {"1/4T", 2.0 / 3.0},   {"1/8D", 0.75},   {"1/4", 1.0},
    {"5/16", 1.25},      {"1/2T", 4.0 / 3.0},   {"1/4D", 1.5},    {"7/16", 1.75},
    {"1/2", 2.0},        {"5/8", 2.5},          {"1/1T", 8.0 / 3.0}, {"1/2D", 3.0},
    {"7/8", 3.5},        {"1/1", 4.0},
};
inline constexpr int kNumDivisions = static_cast<int>(sizeof(kDivisions) / sizeof(kDivisions[0]));

// Longer periods for LFOs and filter motion.
inline constexpr NoteDiv kLfoPeriods[] = {
    {"1/16", 0.25}, {"1/8", 0.5},      {"1/4", 1.0},      {"1/2", 2.0},
    {"1/2.", 3.0},  {"1 bar", 4.0},    {"1.5 bars", 6.0}, {"2 bars", 8.0},
    {"3 bars", 12.0}, {"4 bars", 16.0}, {"8 bars", 32.0},
};
inline constexpr int kNumLfoPeriods = static_cast<int>(sizeof(kLfoPeriods) / sizeof(kLfoPeriods[0]));

// Rate ratios a spectral band can take relative to the base slice rate.
inline constexpr double kRateLadder[11] = {0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1.0,
                                           4.0 / 3.0, 1.5, 2.0, 3.0, 4.0};
inline constexpr int kLadderSize = 11;
inline constexpr int kLadderCenter = 5;

inline constexpr int kSnapBars[] = {0, 1, 2, 4, 8, 16, 32};
inline constexpr const char* kSnapNames[] = {"Off", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars"};
inline constexpr int kNumSnapChoices = 7;

inline int divisionIndex(const char* name) {
    for (int i = 0; i < kNumDivisions; ++i)
        if (std::strcmp(kDivisions[i].name, name) == 0) return i;
    return 0;
}
inline int lfoPeriodIndex(const char* name) {
    for (int i = 0; i < kNumLfoPeriods; ++i)
        if (std::strcmp(kLfoPeriods[i].name, name) == 0) return i;
    return 0;
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
template <typename T>
inline T clampT(T v, T lo, T hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
// Deterministic pseudo-random value in [-1, 1] for an integer index.
inline float hashBipolar(int32_t n) {
    return static_cast<float>(hash32(static_cast<uint32_t>(n)) & 0xFFFFFFu) / 8388607.5f - 1.0f;
}

inline int nextPow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

// Transparent below 0.9, smoothly saturating to 1.0 above.
inline float softLimit(float x) {
    const float t = 0.9f;
    const float a = std::fabs(x);
    if (a <= t) return x;
    const float y = t + (1.0f - t) * std::tanh((a - t) / (1.0f - t));
    return x < 0.0f ? -y : y;
}

// One-pole smoother.
struct Smoother {
    float v = 0.0f;
    float a = 1.0f;
    void setTime(float ms, double sr) {
        a = ms <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (0.001f * ms * static_cast<float>(sr)));
    }
    void reset(float x) { v = x; }
    float tick(float target) {
        v += a * (target - v);
        return v;
    }
};

// Smooth bounded random in [-1, 1]: cosine-interpolated value noise.
class ValueNoise {
public:
    void seed(uint32_t s) {
        state_ = s ? s : 1u;
        a_ = next();
        b_ = next();
        phase_ = 0.0;
    }
    float tick(double advanceCycles) {
        phase_ += advanceCycles;
        while (phase_ >= 1.0) {
            phase_ -= 1.0;
            a_ = b_;
            b_ = next();
        }
        const float w = 0.5f - 0.5f * std::cos(static_cast<float>(phase_ * kPi));
        return a_ + (b_ - a_) * w;
    }

private:
    float next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return static_cast<float>(state_ & 0xFFFFFFu) / 8388607.5f - 1.0f;
    }
    uint32_t state_ = 1u;
    float a_ = 0.0f, b_ = 0.0f;
    double phase_ = 0.0;
};

}  // namespace rd
