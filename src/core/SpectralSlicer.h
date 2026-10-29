// STFT-based spectral slice retriggering.
//
// The spectrum is divided into log-spaced bands. Every band owns a retrigger clock
// (driven by the engine, locked to tempo). On each trigger the band's current spectral
// frame is captured and then "frozen" (phase-advanced with per-bin instantaneous
// frequency, so tonal content keeps ringing coherently) until the next trigger.
// Different bands retrigger at different rates, which is what makes the sound feel
// like several time-streams running at once.
#pragma once

#include <cmath>
#include <vector>

#include "Common.h"
#include "Fft.h"

namespace rd {

struct SliceControl {
    int numBands = 8;
    float lowHz = 100.0f;
    float highHz = 8000.0f;
    float mix = 0.5f;    // 0 = live spectrum, 1 = fully held slices
    float decay = 0.3f;  // 0 = hold until next trigger, 1 = fast fade inside each slice
    bool trig[kMaxBands] = {};
    float phase[kMaxBands] = {};  // 0..1 progress since the band's last trigger
};

class SlicerChannel {
public:
    void prepare(double sampleRate, const Fft* fft, const float* window, float norm) {
        sr_ = sampleRate;
        fft_ = fft;
        win_ = window;
        norm_ = norm;
        inRing_.assign(kFftSize, 0.0f);
        outAccum_.assign(kFftSize, 0.0f);
        fifo_.assign(kHop, 0.0f);
        re_.assign(kFftSize, 0.0f);
        im_.assign(kFftSize, 0.0f);
        prevPhase_.assign(kNumBins, 0.0f);
        heldMag_.assign(kNumBins, 0.0f);
        heldPhase_.assign(kNumBins, 0.0f);
        heldFreq_.assign(kNumBins, 0.0f);
        heldValid_.assign(kNumBins, 0);
        binBand_.assign(kNumBins, 0);
        reset();
    }

    void reset() {
        std::fill(inRing_.begin(), inRing_.end(), 0.0f);
        std::fill(outAccum_.begin(), outAccum_.end(), 0.0f);
        std::fill(fifo_.begin(), fifo_.end(), 0.0f);
        std::fill(prevPhase_.begin(), prevPhase_.end(), 0.0f);
        std::fill(heldMag_.begin(), heldMag_.end(), 0.0f);
        std::fill(heldValid_.begin(), heldValid_.end(), static_cast<unsigned char>(0));
        wpos_ = 0;
        outPos_ = 0;
        fifoRead_ = 0;
        hopCount_ = 0;
        cachedBands_ = -1;
    }

    // One sample in, one sample out. Latency is exactly kFftSize samples.
    inline float process(float x, SliceControl& ctl) {
        const float y = fifo_[static_cast<size_t>(fifoRead_)];
        if (fifoRead_ < kHop - 1) ++fifoRead_;
        inRing_[static_cast<size_t>(wpos_)] = x;
        wpos_ = (wpos_ + 1) & (kFftSize - 1);
        if (++hopCount_ >= kHop) {
            hopCount_ = 0;
            processFrame(ctl);
            fifoRead_ = 0;
        }
        return y;
    }

private:
    void rebuildBandMap(const SliceControl& ctl) {
        const int nb = clampT(ctl.numBands, 1, kMaxBands);
        const float lo = clampT(ctl.lowHz, 20.0f, 2000.0f);
        const float hi = clampT(ctl.highHz, lo * 1.5f, static_cast<float>(sr_) * 0.45f);
        const double binHz = sr_ / kFftSize;
        for (int k = 0; k < kNumBins; ++k) {
            const double f = k * binHz;
            int b;
            if (f <= lo) b = 0;
            else if (f >= hi) b = nb - 1;
            else b = static_cast<int>(nb * std::log(f / lo) / std::log(hi / lo));
            binBand_[static_cast<size_t>(k)] = clampT(b, 0, nb - 1);
        }
        cachedBands_ = nb;
        cachedLo_ = ctl.lowHz;
        cachedHi_ = ctl.highHz;
    }

    static inline float wrapPi(float x) {
        const float twoPi = static_cast<float>(kTwoPi);
        x -= twoPi * std::floor(x / twoPi + 0.5f);
        return x;
    }

    void processFrame(SliceControl& ctl) {
        if (ctl.numBands != cachedBands_ || ctl.lowHz != cachedLo_ || ctl.highHz != cachedHi_) rebuildBandMap(ctl);

        float* re = re_.data();
        float* im = im_.data();
        for (int n = 0; n < kFftSize; ++n) {
            re[n] = inRing_[static_cast<size_t>((wpos_ + n) & (kFftSize - 1))] * win_[n];
            im[n] = 0.0f;
        }
        fft_->transform(re, im, false);

        const float twoPiOverN = static_cast<float>(kTwoPi / kFftSize);
        const float hop = static_cast<float>(kHop);
        const float mix = ctl.mix;
        const float decayExp = ctl.decay * 4.0f;

        float env[kMaxBands];
        for (int b = 0; b < kMaxBands; ++b) {
            const float p = clampT(ctl.phase[b], 0.0f, 1.0f);
            env[b] = decayExp > 0.0f ? std::pow(std::max(0.0f, 1.0f - p), decayExp) : 1.0f;
        }

        for (int k = 0; k < kNumBins; ++k) {
            float xr = re[k], xi = im[k];
            const float mag = std::sqrt(xr * xr + xi * xi);
            const float ph = std::atan2(xi, xr);
            const float omega = twoPiOverN * static_cast<float>(k);
            const float dphi = wrapPi(ph - prevPhase_[static_cast<size_t>(k)] - omega * hop);
            const float inst = omega + dphi / hop;
            prevPhase_[static_cast<size_t>(k)] = ph;

            const int b = binBand_[static_cast<size_t>(k)];
            if (ctl.trig[b]) {
                heldMag_[static_cast<size_t>(k)] = mag;
                heldPhase_[static_cast<size_t>(k)] = ph;
                heldFreq_[static_cast<size_t>(k)] = inst;
                heldValid_[static_cast<size_t>(k)] = 1;
            } else if (heldValid_[static_cast<size_t>(k)]) {
                heldPhase_[static_cast<size_t>(k)] = wrapPi(heldPhase_[static_cast<size_t>(k)] + heldFreq_[static_cast<size_t>(k)] * hop);
            }

            if (mix > 0.0f && heldValid_[static_cast<size_t>(k)]) {
                const float hm = heldMag_[static_cast<size_t>(k)] * env[b];
                const float hr = hm * std::cos(heldPhase_[static_cast<size_t>(k)]);
                const float hi2 = hm * std::sin(heldPhase_[static_cast<size_t>(k)]);
                xr += mix * (hr - xr);
                xi += mix * (hi2 - xi);
            }
            re[k] = xr;
            im[k] = xi;
        }
        for (int b = 0; b < kMaxBands; ++b) ctl.trig[b] = false;

        // Hermitian symmetry -> real output
        im[0] = 0.0f;
        im[kFftSize / 2] = 0.0f;
        for (int k = 1; k < kFftSize / 2; ++k) {
            re[kFftSize - k] = re[k];
            im[kFftSize - k] = -im[k];
        }
        fft_->transform(re, im, true);

        for (int n = 0; n < kFftSize; ++n) {
            const size_t idx = static_cast<size_t>((outPos_ + n) & (kFftSize - 1));
            outAccum_[idx] += re[n] * win_[n] * norm_;
        }
        for (int i = 0; i < kHop; ++i) {
            const size_t idx = static_cast<size_t>((outPos_ + i) & (kFftSize - 1));
            fifo_[static_cast<size_t>(i)] = outAccum_[idx];
            outAccum_[idx] = 0.0f;
        }
        outPos_ = (outPos_ + kHop) & (kFftSize - 1);
    }

    double sr_ = 44100.0;
    const Fft* fft_ = nullptr;
    const float* win_ = nullptr;
    float norm_ = 1.0f;

    std::vector<float> inRing_, outAccum_, fifo_, re_, im_;
    std::vector<float> prevPhase_, heldMag_, heldPhase_, heldFreq_;
    std::vector<unsigned char> heldValid_;
    std::vector<int> binBand_;
    int wpos_ = 0, outPos_ = 0, fifoRead_ = 0, hopCount_ = 0;
    int cachedBands_ = -1;
    float cachedLo_ = 0.0f, cachedHi_ = 0.0f;
};

}  // namespace rd
