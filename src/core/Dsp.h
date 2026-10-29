// Basic DSP building blocks: fractional delay line, granular pitch shifter, SVF, DC blocker.
#pragma once

#include <cmath>
#include <vector>

#include "Common.h"

namespace rd {

// ---------------------------------------------------------------------------
// Circular delay line with 4-point Hermite interpolation.
// Usage per sample: y = read(d); write(x);  -> y is x delayed by exactly d samples.
// ---------------------------------------------------------------------------
class DelayLine {
public:
    void prepare(int maxSamples) {
        size_ = nextPow2(maxSamples + 16);
        mask_ = size_ - 1;
        buf_.assign(static_cast<size_t>(size_), 0.0f);
        w_ = 0;
    }
    void clear() {
        std::fill(buf_.begin(), buf_.end(), 0.0f);
        w_ = 0;
    }
    int capacity() const { return size_; }

    inline void write(float x) {
        buf_[static_cast<size_t>(w_)] = x;
        w_ = (w_ + 1) & mask_;
    }

    inline float readInt(int d) const { return buf_[static_cast<size_t>((w_ - d) & mask_)]; }

    // d >= 2 (samples), fractional allowed.
    inline float read(double d) const {
        const double pos = static_cast<double>(w_) - d;
        const double fl = std::floor(pos);
        const float f = static_cast<float>(pos - fl);
        const int i = static_cast<int>(fl);
        const float y0 = buf_[static_cast<size_t>((i - 1) & mask_)];
        const float y1 = buf_[static_cast<size_t>(i & mask_)];
        const float y2 = buf_[static_cast<size_t>((i + 1) & mask_)];
        const float y3 = buf_[static_cast<size_t>((i + 2) & mask_)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

private:
    std::vector<float> buf_;
    int size_ = 0;
    int mask_ = 0;
    int w_ = 0;
};

// ---------------------------------------------------------------------------
// Dual-tap granular pitch shifter. Pitch changes without changing the playback
// speed of the surrounding audio (it works in real time on a short delay line).
// The module always has a constant average latency (latency()) so it can be
// compensated by the caller - even when it is bypassed or at ratio 1.
// ---------------------------------------------------------------------------
class PitchShifter {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        const int maxGrain = static_cast<int>(0.25 * sr_) + 8;
        buf_.assign(static_cast<size_t>(nextPow2(maxGrain * 2 + 32)), 0.0f);
        mask_ = static_cast<int>(buf_.size()) - 1;
        w_ = 0;
        phase_ = 0.0;
        activity_ = 0.0f;
        actCoef_ = 1.0f - std::exp(-1.0f / (0.010f * static_cast<float>(sr_)));
        setGrainMs(60.0f);
    }
    void reset() {
        std::fill(buf_.begin(), buf_.end(), 0.0f);
        phase_ = 0.0;
        activity_ = 0.0f;
    }

    void setGrainMs(float ms) { grain_ = clampT(static_cast<double>(ms) * 0.001 * sr_, 32.0, 0.25 * sr_); }

    // Average delay (samples) between input and output.
    double latency() const { return 2.0 + 0.5 * grain_; }

    // ratio: playback ratio of the grains (2.0 = one octave up). mix: 0 = clean delayed
    // signal (time aligned), 1 = fully pitch shifted.
    float process(float x, float ratio, float mix) {
        buf_[static_cast<size_t>(w_)] = x;
        w_ = (w_ + 1) & mask_;

        phase_ += (1.0 - static_cast<double>(ratio)) / grain_;
        phase_ -= std::floor(phase_);
        double p2 = phase_ + 0.5;
        p2 -= std::floor(p2);

        const float g1 = 0.5f - 0.5f * std::cos(static_cast<float>(kTwoPi * phase_));
        const float g2 = 0.5f - 0.5f * std::cos(static_cast<float>(kTwoPi * p2));
        const float s1 = readH(2.0 + phase_ * grain_);
        const float s2 = readH(2.0 + p2 * grain_);
        const float granular = g1 * s1 + g2 * s2;
        const float fixedTap = readH(latency());

        // Near ratio 1 the two taps would comb-filter; fade to the fixed tap instead.
        const float target = std::fabs(ratio - 1.0f) > 0.0035f ? 1.0f : 0.0f;
        activity_ += actCoef_ * (target - activity_);
        const float shifted = fixedTap + activity_ * (granular - fixedTap);
        return fixedTap + mix * (shifted - fixedTap);
    }

private:
    // d = age in samples of the value returned (0 = newest sample written).
    inline float readH(double d) const {
        const double pos = static_cast<double>(w_) - 1.0 - d;
        const double fl = std::floor(pos);
        const float f = static_cast<float>(pos - fl);
        const int i = static_cast<int>(fl);
        const float y0 = buf_[static_cast<size_t>((i - 1) & mask_)];
        const float y1 = buf_[static_cast<size_t>(i & mask_)];
        const float y2 = buf_[static_cast<size_t>((i + 1) & mask_)];
        const float y3 = buf_[static_cast<size_t>((i + 2) & mask_)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    std::vector<float> buf_;
    int mask_ = 0, w_ = 0;
    double sr_ = 44100.0, grain_ = 2646.0, phase_ = 0.0;
    float activity_ = 0.0f, actCoef_ = 0.01f;
};

// ---------------------------------------------------------------------------
// Zero-delay-feedback state variable filter (Cytomic / TPT), stable under modulation.
// ---------------------------------------------------------------------------
enum class SvfType { Off = 0, LowPass, BandPass, HighPass, Notch };

class Svf {
public:
    void reset() { ic1_ = ic2_ = 0.0f; }

    // g = tan(pi*fc/sr), k = damping (2 = no resonance, ~0.05 = near self-oscillation)
    inline float process(float x, SvfType type, float g, float k) {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x - ic2_;
        const float v1 = a1 * ic1_ + a2 * v3;
        const float v2 = ic2_ + a2 * ic1_ + a3 * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        switch (type) {
            case SvfType::LowPass: return v2;
            case SvfType::BandPass: return v1 * k;  // constant-peak band-pass
            case SvfType::HighPass: return x - k * v1 - v2;
            case SvfType::Notch: return x - k * v1;
            default: return x;
        }
    }

private:
    float ic1_ = 0.0f, ic2_ = 0.0f;
};

class DcBlocker {
public:
    void prepare(double sr) { r_ = 1.0f - static_cast<float>(kTwoPi * 20.0 / sr); }
    void reset() { x1_ = y1_ = 0.0f; }
    inline float process(float x) {
        const float y = x - x1_ + r_ * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }

private:
    float r_ = 0.997f, x1_ = 0.0f, y1_ = 0.0f;
};

}  // namespace rd
