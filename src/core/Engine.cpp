// Reality Desync engine: tempo-locked rhythm, drifting texture, bar-line snap.
#include "Engine.h"

#include <climits>
#include <cmath>

#include "Dsp.h"
#include "Fft.h"
#include "SpectralSlicer.h"

namespace rd {

namespace {

// Fixed per-band offsets used by the "scatter" rate pattern.
constexpr float kScatter[kMaxBands] = {0.7f, -0.4f, 0.1f, -0.9f, 0.5f, 0.9f, -0.2f, -0.7f,
                                       0.3f, 0.8f,  -0.6f, 0.0f, -0.3f, 0.6f, -0.8f, 0.4f};

float lfoShapeValue(int shape, double x) {
    const double fl = std::floor(x);
    const float f = static_cast<float>(x - fl);
    const int n = static_cast<int>(static_cast<long long>(fl) & 0x7fffffff);
    switch (shape) {
        default:
        case 0: return std::sin(static_cast<float>(kTwoPi) * f);                                     // sine
        case 1: return f < 0.25f ? 4.0f * f : (f < 0.75f ? 2.0f - 4.0f * f : 4.0f * f - 4.0f);       // triangle
        case 2: return 2.0f * f - 1.0f;                                                              // saw up
        case 3: return 1.0f - 2.0f * f;                                                              // saw down
        case 4: return f < 0.5f ? 1.0f : -1.0f;                                                      // square
        case 5: return hashBipolar(n);                                                               // stepped random
        case 6: {                                                                                    // glided random
            const float a = hashBipolar(n), b = hashBipolar(n + 1);
            const float w = 0.5f - 0.5f * std::cos(static_cast<float>(kPi) * f);
            return a + (b - a) * w;
        }
    }
}

}  // namespace

struct Engine::Impl {
    // --- static
    double sr = 48000.0;
    Fft fft;
    std::vector<float> window;
    float norm = 1.0f;

    // --- audio blocks
    SlicerChannel slicer[kNumChannels];
    SliceControl slice[kNumChannels];
    DelayLine delay[kNumChannels];
    DelayLine dry[kNumChannels];
    PitchShifter pitch[kNumChannels];
    Svf svf[kNumChannels];
    DcBlocker dcb[kNumChannels];

    // --- drift noise
    ValueNoise nTime[kNumChannels], nPitch[kNumChannels], nBand[kNumChannels][kMaxBands];

    // --- clock
    bool started = false;
    bool wasHostPlaying = false;
    bool hostSynced = false;
    double ppq = 0.0, lastPpqEnd = 0.0;
    double bpm = 120.0, beatsPerSample = 120.0 / 60.0 / 48000.0;
    double beatsPerBar = 4.0, barOffset = 0.0;

    // --- cycle / snap state
    double driftStartBars = -1.0e6;
    double phaseOriginPpq = 0.0;
    int lastSnapBars = -1;
    long long lastCycleIndex = LLONG_MIN;
    double snapAge = 1.0e9;
    float dSnap = 0.0f, dEff = 0.0f;

    // --- band clocks
    double bandPhase[kNumChannels][kMaxBands] = {};
    float sliceMixS = 0.5f;

    // --- control-rate targets, smoothed per sample
    int ctrlCounter = 0;
    double dTarget[kNumChannels] = {1000.0, 1000.0};
    double dSm[kNumChannels] = {1000.0, 1000.0};
    float semiBase[kNumChannels] = {0.0f, 0.0f};
    float seqTarget = 0.0f;
    float gCur = 0.1f, gStep = 0.0f, fk = 1.0f;
    Smoother semiSm[kNumChannels], seqSm, fbSm, mixSm, outSm;
    double dCoef = 0.01;
    bool firstSamples = true;

    // ----------------------------------------------------------------------
    void prepare(double sampleRate) {
        sr = sampleRate;
        fft.init(kFftSize);
        window.assign(kFftSize, 0.0f);
        for (int n = 0; n < kFftSize; ++n)
            window[static_cast<size_t>(n)] = static_cast<float>(0.5 - 0.5 * std::cos(kTwoPi * n / kFftSize));
        // Hann analysis * Hann synthesis: overlap-add of w^2 is constant; normalise it away
        double sum = 0.0;
        for (int k = 0; k < kOverlap; ++k) {
            const double w = window[static_cast<size_t>(kHop / 3 + k * kHop)];
            sum += w * w;
        }
        norm = static_cast<float>(1.0 / sum);

        const int maxDelay = static_cast<int>(12.5 * sr) + 64;
        for (int c = 0; c < kNumChannels; ++c) {
            slicer[c].prepare(sr, &fft, window.data(), norm);
            delay[c].prepare(maxDelay);
            dry[c].prepare(kFftSize + 8);
            pitch[c].prepare(sr);
            dcb[c].prepare(sr);
            nTime[c].seed(0x1234567u + 7919u * static_cast<uint32_t>(c));
            nPitch[c].seed(0x2345678u + 6007u * static_cast<uint32_t>(c));
            for (int b = 0; b < kMaxBands; ++b)
                nBand[c][b].seed(0x3456789u + 104729u * static_cast<uint32_t>(b) + 15485863u * static_cast<uint32_t>(c));
        }
        reset();
    }

    void reset() {
        for (int c = 0; c < kNumChannels; ++c) {
            slicer[c].reset();
            delay[c].clear();
            dry[c].clear();
            pitch[c].reset();
            svf[c].reset();
            dcb[c].reset();
            slice[c] = SliceControl();
            for (int b = 0; b < kMaxBands; ++b) bandPhase[c][b] = 0.0;
        }
        started = false;
        wasHostPlaying = false;
        ppq = lastPpqEnd = 0.0;
        driftStartBars = -1.0e6;
        phaseOriginPpq = 0.0;
        lastSnapBars = -1;
        lastCycleIndex = LLONG_MIN;
        snapAge = 1.0e9;
        dSnap = dEff = 0.0f;
        ctrlCounter = 0;
        firstSamples = true;
    }

    // ----------------------------------------------------------------------
    // Period (in beats) of one spectral band's retrigger clock.
    static int ladderIndex(const Params& p, int b, int nb) {
        const double pos = nb > 1 ? static_cast<double>(b) / (nb - 1) - 0.5 : 0.0;
        double off = 0.0;
        switch (p.slicePattern) {
            default:
            case 0: off = p.sliceSpread * 10.0 * pos; break;
            case 1: off = p.sliceSpread * 5.0 * ((b & 1) ? 1.0 : -1.0); break;
            case 2: off = p.sliceSpread * 5.0 * kScatter[b]; break;
        }
        return clampT(kLadderCenter + static_cast<int>(std::lround(off)), 0, kLadderSize - 1);
    }

    static double bandPeriod(const Params& p, int c, int b, int nb, double skewRatio) {
        const double base = kDivisions[clampT(p.sliceRate, 0, kNumDivisions - 1)].beats;
        double ratio = kRateLadder[ladderIndex(p, b, nb)];
        if (c == 1) ratio *= skewRatio;
        return base / ratio;
    }

    // ----------------------------------------------------------------------
    // Sets all drift back to zero and re-fires every spectral band together.
    void doSnap(const Params& p, Telemetry& tel, double snapBarPos) {
        dSnap = dEff;
        snapAge = 0.0;
        driftStartBars = snapBarPos;
        phaseOriginPpq = snapBarPos * beatsPerBar + barOffset;
        const int nb = clampT(p.sliceBands, 2, kMaxBands);
        const double skewRatio = std::pow(1.5, static_cast<double>(p.sliceSkew));
        for (int c = 0; c < kNumChannels; ++c)
            for (int b = 0; b < nb; ++b) {
                const double per = bandPeriod(p, c, b, nb, skewRatio);
                bandPhase[c][b] = clampT((ppq - phaseOriginPpq) / per, 0.0, 0.999);
                slice[c].trig[b] = true;
                tel.bandTrig[c][b].fetch_add(1u, std::memory_order_relaxed);
            }
        tel.snapCount.fetch_add(1u, std::memory_order_relaxed);
    }

    // Transport jumped (play pressed, loop, seek): align everything to the grid at this position.
    void relock(const Params& p, Telemetry& tel) {
        const double barPos = (ppq - barOffset) / beatsPerBar;
        const int bars = kSnapBars[clampT(p.snapIdx, 0, kNumSnapChoices - 1)];
        lastSnapBars = bars;
        if (bars > 0) {
            const long long idx = static_cast<long long>(std::floor(barPos / bars));
            lastCycleIndex = idx;
            driftStartBars = static_cast<double>(idx) * bars;
            phaseOriginPpq = driftStartBars * beatsPerBar + barOffset;
        } else {
            lastCycleIndex = 0;
            driftStartBars = barPos - 1.0e6;
            phaseOriginPpq = barOffset;
        }
        snapAge = 1.0e9;
        dSnap = 0.0f;
        const int nb = clampT(p.sliceBands, 2, kMaxBands);
        const double skewRatio = std::pow(1.5, static_cast<double>(p.sliceSkew));
        for (int c = 0; c < kNumChannels; ++c)
            for (int b = 0; b < nb; ++b) {
                const double per = bandPeriod(p, c, b, nb, skewRatio);
                double ph = (ppq - phaseOriginPpq) / per;
                ph -= std::floor(ph);
                bandPhase[c][b] = ph;
                slice[c].trig[b] = true;
                tel.bandTrig[c][b].fetch_add(1u, std::memory_order_relaxed);
            }
    }

    // ----------------------------------------------------------------------
    void controlTick(const Params& p, Telemetry& tel, std::atomic<bool>& snapReq) {
        const double beatsAdvance = kCtrlBlock * beatsPerSample;
        const double barPos = (ppq - barOffset) / beatsPerBar;
        const int bars = kSnapBars[clampT(p.snapIdx, 0, kNumSnapChoices - 1)];

        // Snap-length setting changed on the fly: re-seat the cycle without firing a snap.
        if (bars != lastSnapBars) {
            lastSnapBars = bars;
            if (bars > 0) {
                const long long idx = static_cast<long long>(std::floor(barPos / bars));
                lastCycleIndex = idx;
                driftStartBars = static_cast<double>(idx) * bars;
                phaseOriginPpq = driftStartBars * beatsPerBar + barOffset;
            } else {
                driftStartBars = barPos - 1.0e6;
            }
        }
        // Bar-boundary snap
        if (bars > 0) {
            const long long idx = static_cast<long long>(std::floor(barPos / bars));
            if (idx != lastCycleIndex) {
                lastCycleIndex = idx;
                doSnap(p, tel, static_cast<double>(idx) * bars);
            }
        }
        // Manual snap button
        if (snapReq.exchange(false)) doSnap(p, tel, barPos);

        // --- drift envelope (0 right after a snap, builds over the cycle)
        const double elapsed = barPos - driftStartBars;
        const double cycleLen = bars > 0 ? static_cast<double>(bars) : 8.0;
        const double cyclePos = clampT(elapsed / cycleLen, 0.0, 1.0);
        const double ramp = std::max(0.02, static_cast<double>(p.driftBuild));
        const double t = clampT(cyclePos / ramp, 0.0, 1.0);
        const double expo = std::pow(2.0, static_cast<double>(clampT(p.driftCurve, -1.0f, 1.0f)) * 2.0);
        const float dT = p.desync * static_cast<float>(std::pow(t, expo));
        const double glide = static_cast<double>(p.snapGlideMs) * 0.001 * sr;
        if (snapAge < glide) {
            float rel = 1.0f - static_cast<float>(snapAge / glide);
            rel *= rel;
            dEff = dT + (dSnap - dT) * rel;
        } else {
            dEff = dT;
        }
        dEff = clampT(dEff, 0.0f, 1.0f);
        snapAge += kCtrlBlock;

        const double noiseAdv = static_cast<double>(p.driftRateHz) * kCtrlBlock / sr;

        // --- spectral band clocks
        const int nb = clampT(p.sliceBands, 2, kMaxBands);
        const double skewRatio = std::pow(1.5, static_cast<double>(p.sliceSkew));
        const float detuneAmt = p.sliceDrift * 0.25f * dEff;
        for (int c = 0; c < kNumChannels; ++c) {
            SliceControl& sc = slice[c];
            sc.numBands = nb;
            sc.lowHz = p.sliceLowHz;
            sc.highHz = p.sliceHighHz;
            sc.decay = p.sliceDecay;
            for (int b = 0; b < nb; ++b) {
                const double per = bandPeriod(p, c, b, nb, skewRatio);
                const float n = nBand[c][b].tick(noiseAdv * (0.6 + 0.07 * b + 0.03 * c));
                const double detune = static_cast<double>(n * detuneAmt);
                double ph = bandPhase[c][b] + beatsAdvance / per * (1.0 + detune);
                if (ph >= 1.0) {
                    ph -= std::floor(ph);
                    sc.trig[b] = true;
                    tel.bandTrig[c][b].fetch_add(1u, std::memory_order_relaxed);
                }
                bandPhase[c][b] = ph;
                sc.phase[b] = static_cast<float>(ph);
            }
        }
        sliceMixS += (p.sliceMix - sliceMixS) * (1.0f - std::exp(-static_cast<float>(kCtrlBlock) / (0.03f * static_cast<float>(sr))));
        slice[0].mix = slice[1].mix = sliceMixS;

        // --- delay targets (tempo locked), drift only adds wobble around the grid
        const double spb = 1.0 / beatsPerSample;
        const double maxD = static_cast<double>(delay[0].capacity() - 24);
        const int idxs[2] = {clampT(p.timeL, 0, kNumDivisions - 1), clampT(p.timeR, 0, kNumDivisions - 1)};
        const double driftSamples = static_cast<double>(p.timeDrift) * 0.030 * sr * dEff;
        for (int c = 0; c < kNumChannels; ++c) {
            const float n = nTime[c].tick(noiseAdv * (c == 0 ? 1.0 : 1.13));
            dTarget[c] = clampT(kDivisions[idxs[c]].beats * spb + static_cast<double>(n) * driftSamples, 4.0, maxD);
        }

        // --- pitch targets
        const double lfoPeriod = kLfoPeriods[clampT(p.lfoRate, 0, kNumLfoPeriods - 1)].beats;
        const double lx = (ppq - phaseOriginPpq) / lfoPeriod;
        const float lfo[2] = {lfoShapeValue(p.lfoShape, lx), lfoShapeValue(p.lfoShape, lx + p.lfoPhaseLR / 360.0)};
        for (int c = 0; c < kNumChannels; ++c) {
            const float n = nPitch[c].tick(noiseAdv * (c == 0 ? 0.8 : 0.93));
            float semi = p.pitchSemi + p.pitchCents * 0.01f + (c == 0 ? -0.5f : 0.5f) * p.pitchStereo;
            semi += p.lfoDepth * lfo[c];
            semi += p.pitchDrift * 2.0f * n * dEff;
            semiBase[c] = clampT(semi, -36.0f, 36.0f);
        }
        {
            const double stepBeats = kDivisions[clampT(p.seqDiv, 0, kNumDivisions - 1)].beats;
            const double sp = (ppq - phaseOriginPpq) / stepBeats;
            long long si = static_cast<long long>(std::floor(sp)) % kNumSeqSteps;
            if (si < 0) si += kNumSeqSteps;
            seqTarget = p.seq[si] * p.seqAmount;
        }

        // --- filter
        {
            float cut = p.cutoffHz;
            if (p.filterModDepth > 0.0f) {
                const double fx = (ppq - phaseOriginPpq) / kLfoPeriods[clampT(p.filterModRate, 0, kNumLfoPeriods - 1)].beats;
                cut *= std::pow(2.0f, p.filterModDepth * std::sin(static_cast<float>(kTwoPi * fx)));
            }
            cut = clampT(cut, 20.0f, static_cast<float>(sr) * 0.45f);
            const float gT = std::tan(static_cast<float>(kPi) * cut / static_cast<float>(sr));
            gStep = (gT - gCur) / static_cast<float>(kCtrlBlock);
            if (firstSamples) {
                gCur = gT;
                gStep = 0.0f;
            }
            fk = 2.0f - 1.95f * clampT(p.resonance, 0.0f, 1.0f);
        }

        // --- telemetry
        const double sMs = 1000.0 / sr;
        tel.drift.store(dEff, std::memory_order_relaxed);
        tel.cyclePos.store(static_cast<float>(cyclePos), std::memory_order_relaxed);
        tel.barsToSnap.store(bars > 0 ? static_cast<float>((static_cast<double>(lastCycleIndex) + 1.0) * bars - barPos) : -1.0f,
                             std::memory_order_relaxed);
        for (int c = 0; c < kNumChannels; ++c) {
            tel.delayMs[c].store(static_cast<float>(dSm[c] * sMs), std::memory_order_relaxed);
            tel.pitchSemi[c].store(semiBase[c], std::memory_order_relaxed);
            for (int b = 0; b < nb; ++b) tel.bandPhase[c][b].store(slice[c].phase[b], std::memory_order_relaxed);
        }
        tel.bpm.store(static_cast<float>(bpm), std::memory_order_relaxed);
    }

    // ----------------------------------------------------------------------
    void renderChunk(const Params& p, float* L, float* R, int n, float outGain) {
        const SvfType ftype = static_cast<SvfType>(clampT(p.filterType, 0, 4));
        const bool filterInFb = p.filterPlace == 0;
        const double lp = pitch[0].latency();
        const float pmix = p.pitchMix;
        const float cross = clampT(p.cross, 0.0f, 1.0f);
        const float fbTarget = clampT(p.feedback, 0.0f, 1.1f);
        const float mixTarget = clampT(p.mix, 0.0f, 1.0f);

        for (int i = 0; i < n; ++i) {
            const float in[2] = {L[i], R[i]};
            float dryS[2];
            float x[2];
            for (int c = 0; c < 2; ++c) {
                dryS[c] = dry[c].readInt(kFftSize);
                dry[c].write(in[c]);
                x[c] = slicer[c].process(in[c], slice[c]);
            }

            const float fb = fbSm.tick(fbTarget);
            const float mix = mixSm.tick(mixTarget);
            const float og = outSm.tick(outGain);
            const float seqS = seqSm.tick(seqTarget);
            gCur += gStep;

            float yOut[2], fbSig[2], wIn[2];
            for (int c = 0; c < 2; ++c) {
                dSm[c] += dCoef * (dTarget[c] - dSm[c]);
                const float semi = semiSm[c].tick(semiBase[c]) + seqS;
                const float ratio = std::exp2(semi * (1.0f / 12.0f));
                const double D = dSm[c];
                const double dComp = std::max(2.0, D - lp);

                float fbRaw;
                wIn[c] = x[c];
                switch (p.pitchPlace) {
                    case 0:  // pitch before the delay
                        wIn[c] = pitch[c].process(x[c], ratio, pmix);
                        yOut[c] = delay[c].read(dComp);
                        fbRaw = delay[c].read(D);
                        break;
                    case 2: {  // pitch after the delay
                        const float t = delay[c].read(dComp);
                        yOut[c] = pitch[c].process(t, ratio, pmix);
                        fbRaw = delay[c].read(D);
                        break;
                    }
                    default: {  // pitch inside the feedback loop: every repeat shifts again
                        yOut[c] = delay[c].read(D);
                        const float t = delay[c].read(dComp);
                        fbRaw = pitch[c].process(t, ratio, pmix);
                        break;
                    }
                }
                fbRaw = dcb[c].process(fbRaw);
                if (filterInFb && ftype != SvfType::Off) fbRaw = svf[c].process(fbRaw, ftype, gCur, fk);
                fbSig[c] = fbRaw;
                if (!filterInFb && ftype != SvfType::Off) yOut[c] = svf[c].process(yOut[c], ftype, gCur, fk);
            }

            const float w0 = wIn[0] + fb * ((1.0f - cross) * fbSig[0] + cross * fbSig[1]);
            const float w1 = wIn[1] + fb * ((1.0f - cross) * fbSig[1] + cross * fbSig[0]);
            delay[0].write(softLimit(w0));
            delay[1].write(softLimit(w1));

            const float wetG = std::sin(mix * static_cast<float>(kPi * 0.5));
            const float dryG = std::cos(mix * static_cast<float>(kPi * 0.5));
            L[i] = (dryS[0] * dryG + yOut[0] * wetG) * og;
            R[i] = (dryS[1] * dryG + yOut[1] * wetG) * og;
            firstSamples = false;
        }
    }

    // ----------------------------------------------------------------------
    void process(const Params& p, Telemetry& tel, std::atomic<bool>& snapReq, float* L, float* R, int numSamples,
                 const HostInfo& host) {
        // Clock
        const bool hostMode = p.clockSource == 0;
        bpm = clampT((hostMode && host.hasBpm) ? host.bpm : static_cast<double>(p.internalBpm), 20.0, 400.0);
        beatsPerSample = bpm / (60.0 * sr);
        const double tsDen = host.tsDen > 0 ? host.tsDen : 4;
        beatsPerBar = clampT(host.tsNum > 0 ? host.tsNum * 4.0 / tsDen : 4.0, 0.5, 32.0);

        const bool useHost = hostMode && host.hasPpq && host.playing;
        bool jump = !started;
        if (useHost) {
            if (!wasHostPlaying || std::fabs(host.ppq - lastPpqEnd) > 0.05) jump = true;
            ppq = host.ppq;
            if (host.hasBarStart) {
                const double b = host.barStartPpq;
                barOffset = b - std::floor(b / beatsPerBar) * beatsPerBar;
            } else {
                barOffset = 0.0;
            }
        }
        wasHostPlaying = useHost;
        hostSynced = useHost;
        tel.hostSynced.store(useHost, std::memory_order_relaxed);
        if (jump) relock(p, tel);
        started = true;

        // Per-block smoother setup
        const float gl = std::max(p.glideMs, 0.0f);
        dCoef = gl <= 0.0f ? 1.0 : 1.0 - std::exp(-1.0 / (0.001 * gl * sr));
        for (int c = 0; c < 2; ++c) semiSm[c].setTime(3.0f, sr);
        seqSm.setTime(std::max(p.seqGlideMs, 0.5f), sr);
        fbSm.setTime(20.0f, sr);
        mixSm.setTime(20.0f, sr);
        outSm.setTime(20.0f, sr);
        for (int c = 0; c < 2; ++c) pitch[c].setGrainMs(p.grainMs);
        const float outGain = dbToGain(p.outGainDb);

        if (firstSamples) {
            for (int c = 0; c < 2; ++c) {
                semiSm[c].reset(0.0f);
                dSm[c] = 1000.0;
            }
            seqSm.reset(0.0f);
            fbSm.reset(p.feedback);
            mixSm.reset(p.mix);
            outSm.reset(outGain);
            sliceMixS = p.sliceMix;
            controlTick(p, tel, snapReq);  // establishes targets before the first sample
            for (int c = 0; c < 2; ++c) {
                dSm[c] = dTarget[c];
                semiSm[c].reset(semiBase[c]);
            }
            seqSm.reset(seqTarget);
            ctrlCounter = kCtrlBlock;
        }

        int pos = 0;
        while (pos < numSamples) {
            if (ctrlCounter <= 0) {
                controlTick(p, tel, snapReq);
                ctrlCounter = kCtrlBlock;
            }
            const int n = std::min(ctrlCounter, numSamples - pos);
            renderChunk(p, L + pos, R + pos, n, outGain);
            ppq += n * beatsPerSample;
            ctrlCounter -= n;
            pos += n;
        }
        lastPpqEnd = ppq;
    }
};

// ---------------------------------------------------------------------------
Engine::Engine() : impl_(new Impl()) {}
Engine::~Engine() = default;

void Engine::prepare(double sampleRate) { impl_->prepare(sampleRate); }
void Engine::reset() { impl_->reset(); }

void Engine::process(float* left, float* right, int numSamples, const HostInfo& host) {
    impl_->process(params_, telemetry_, snapRequested_, left, right, numSamples, host);
}

}  // namespace rd
