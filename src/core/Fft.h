// Small radix-2 complex FFT (float). Self-contained so the DSP core has no dependencies.
#pragma once

#include <cmath>
#include <vector>

#include "Common.h"

namespace rd {

class Fft {
public:
    void init(int n) {
        n_ = n;
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        rev_.assign(static_cast<size_t>(n), 0);
        for (int i = 0; i < n; ++i) {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev_[static_cast<size_t>(i)] = r;
        }
        cos_.assign(static_cast<size_t>(n / 2), 0.0f);
        sin_.assign(static_cast<size_t>(n / 2), 0.0f);
        for (int k = 0; k < n / 2; ++k) {
            const double ang = -kTwoPi * k / n;
            cos_[static_cast<size_t>(k)] = static_cast<float>(std::cos(ang));
            sin_[static_cast<size_t>(k)] = static_cast<float>(std::sin(ang));
        }
    }

    // In-place transform. Inverse includes the 1/N scale.
    void transform(float* re, float* im, bool inverse) const {
        const int n = n_;
        for (int i = 0; i < n; ++i) {
            const int j = rev_[static_cast<size_t>(i)];
            if (j > i) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        for (int size = 2; size <= n; size <<= 1) {
            const int half = size >> 1;
            const int step = n / size;
            for (int start = 0; start < n; start += size) {
                for (int k = 0; k < half; ++k) {
                    const float wr = cos_[static_cast<size_t>(k * step)];
                    const float wi = inverse ? -sin_[static_cast<size_t>(k * step)] : sin_[static_cast<size_t>(k * step)];
                    const int i = start + k;
                    const int j = i + half;
                    const float tr = re[j] * wr - im[j] * wi;
                    const float ti = re[j] * wi + im[j] * wr;
                    re[j] = re[i] - tr;
                    im[j] = im[i] - ti;
                    re[i] += tr;
                    im[i] += ti;
                }
            }
        }
        if (inverse) {
            const float s = 1.0f / static_cast<float>(n);
            for (int i = 0; i < n; ++i) {
                re[i] *= s;
                im[i] *= s;
            }
        }
    }

private:
    int n_ = 0;
    std::vector<int> rev_;
    std::vector<float> cos_, sin_;
};

}  // namespace rd
