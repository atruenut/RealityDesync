// Offline verification of the Reality Desync DSP core. Exit code 0 = all passed.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

#include "../src/core/Engine.h"

using namespace rd;

static int g_fail = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            ++g_fail;                                     \
            std::printf("  FAIL: " __VA_ARGS__);          \
            std::printf("\n");                            \
        } else {                                          \
            std::printf("  ok:   " __VA_ARGS__);          \
            std::printf("\n");                            \
        }                                                 \
    } while (0)

static constexpr double SR = 48000.0;

static Params cleanParams() {
    Params p;
    p.mix = 1.0f;
    p.feedback = 0.0f;
    p.desync = 0.0f;
    p.timeDrift = 0.0f;
    p.pitchDrift = 0.0f;
    p.sliceDrift = 0.0f;
    p.sliceMix = 0.0f;
    p.glideMs = 0.0f;
    p.pitchSemi = 0.0f;
    p.filterType = 0;
    p.internalBpm = 120.0f;
    p.clockSource = 1;
    return p;
}

static void run(Engine& e, std::vector<float>& L, std::vector<float>& R, int block = 512) {
    HostInfo h;
    const int n = static_cast<int>(L.size());
    for (int i = 0; i < n; i += block) {
        const int m = std::min(block, n - i);
        e.process(L.data() + i, R.data() + i, m, h);
    }
}

static int peakIndex(const std::vector<float>& v, int from = 0) {
    int best = from;
    float bv = 0;
    for (int i = from; i < static_cast<int>(v.size()); ++i)
        if (std::fabs(v[static_cast<size_t>(i)]) > bv) {
            bv = std::fabs(v[static_cast<size_t>(i)]);
            best = i;
        }
    return best;
}

// Frequency of a sine via zero-crossing count in a window.
static double estimateFreq(const std::vector<float>& v, int a, int b) {
    int zc = 0;
    int first = -1, last = -1;
    for (int i = a + 1; i < b; ++i)
        if (v[static_cast<size_t>(i - 1)] <= 0.0f && v[static_cast<size_t>(i)] > 0.0f) {
            if (first < 0) first = i;
            last = i;
            ++zc;
        }
    if (zc < 3) return 0.0;
    return (zc - 1) * SR / (last - first);
}

static void testDelaysAreMusicalAndIndependent() {
    std::printf("[1] L/R musical delays, sample accurate\n");
    Engine e;
    e.prepare(SR);
    Params p = cleanParams();
    p.timeL = divisionIndex("1/8");   // 0.25 s @120 -> 12000 samples
    p.timeR = divisionIndex("1/8D");  // 0.375 s -> 18000 samples
    p.pitchMix = 0.0f;
    e.setParams(p);
    const int N = 48000;
    std::vector<float> L(N, 0.0f), R(N, 0.0f);
    L[1000] = 1.0f;
    R[1000] = 1.0f;
    run(e, L, R);
    const int lat = e.latencySamples();
    // search beyond the (zeroed) dry impulse position: dry is muted at mix=1, wet peak is the echo
    const int pl = peakIndex(L), pr = peakIndex(R);
    CHECK(std::abs(pl - (1000 + lat + 12000)) <= 1, "left echo at %d (expected %d)", pl, 1000 + lat + 12000);
    CHECK(std::abs(pr - (1000 + lat + 18000)) <= 1, "right echo at %d (expected %d)", pr, 1000 + lat + 18000);
}

static void testTempoFollowsBpm() {
    std::printf("[2] delays track tempo\n");
    Engine e;
    e.prepare(SR);
    Params p = cleanParams();
    p.internalBpm = 90.0f;
    p.timeL = p.timeR = divisionIndex("1/4");  // 60/90 = 0.6667 s = 32000 samples
    p.pitchMix = 0.0f;
    e.setParams(p);
    std::vector<float> L(60000, 0.0f), R(60000, 0.0f);
    L[500] = 1.0f;
    run(e, L, R);
    CHECK(std::abs(peakIndex(L) - (500 + e.latencySamples() + 32000)) <= 1, "1/4 @ 90 BPM = 32000 samples");
}

static void testPitchIndependentOfTime() {
    std::printf("[3] pitch shifts without changing timing\n");
    for (float semi : {12.0f, -12.0f, 7.0f}) {
        Engine e;
        e.prepare(SR);
        Params p = cleanParams();
        p.timeL = p.timeR = divisionIndex("1/16");
        p.pitchSemi = semi;
        p.pitchPlace = 2;
        p.grainMs = 40.0f;
        e.setParams(p);
        const int N = 96000;
        std::vector<float> L(N), R(N);
        for (int i = 0; i < N; ++i) L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = 0.5f * std::sin(2.0 * kPi * 440.0 * i / SR);
        run(e, L, R);
        const double f = estimateFreq(L, 40000, 90000);
        const double want = 440.0 * std::pow(2.0, semi / 12.0);
        CHECK(std::fabs(f / want - 1.0) < 0.03, "%+.0f st: measured %.1f Hz, wanted %.1f Hz", semi, f, want);
    }
    // timing unchanged: a noise burst keeps its energy centroid on the grid (an isolated impulse would be
    // smeared across the grain window, but sustained material averages over many grains)
    for (float semi : {0.0f, 7.0f, -5.0f, 12.0f}) {
        Engine e;
        e.prepare(SR);
        Params p = cleanParams();
        p.timeL = p.timeR = divisionIndex("1/8");
        p.pitchSemi = semi;
        p.pitchPlace = 2;
        p.grainMs = 40.0f;
        e.setParams(p);
        std::vector<float> L(48000, 0.0f), R(48000, 0.0f);
        uint32_t seed = 99;
        const int b0 = 4000, b1 = 14000;  // 208 ms burst
        for (int i = b0; i < b1; ++i) {
            seed = seed * 1664525u + 1013904223u;
            L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = (static_cast<float>(seed >> 8) / 8388608.0f - 1.0f) * 0.5f;
        }
        run(e, L, R);
        const double expect = 0.5 * (b0 + b1) + e.latencySamples() + 12000;
        double sw = 0, sx = 0;
        for (int i = 0; i < 48000; ++i) {
            const double v = L[static_cast<size_t>(i)] * L[static_cast<size_t>(i)];
            sw += v;
            sx += v * i;
        }
        const double centroid = sx / std::max(sw, 1e-12);
        CHECK(std::fabs(centroid - expect) < 0.0015 * SR, "burst stays on the grid at %+.0f st (off by %.1f samples = %.2f ms)",
              semi, centroid - expect, (centroid - expect) * 1000.0 / SR);
    }
}

static void testSliceReconstruction() {
    std::printf("[4] spectral engine is transparent when slice mix = 0\n");
    Engine e;
    e.prepare(SR);
    Params p = cleanParams();
    p.mix = 1.0f;  // wet only: slicer -> 1/64 delay -> pitch stage at 0 st
    p.timeL = p.timeR = divisionIndex("1/64");  // 1500 samples @120 BPM
    p.pitchMix = 0.0f;
    e.setParams(p);
    const int N = 48000;
    std::vector<float> L(N), R(N), ref(N);
    for (int i = 0; i < N; ++i) ref[static_cast<size_t>(i)] = L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] =
        0.4f * std::sin(2.0 * kPi * 330.0 * i / SR) + 0.2f * std::sin(2.0 * kPi * 2100.0 * i / SR);
    run(e, L, R);
    const int lat = e.latencySamples() + 1500;
    double err = 0;
    for (int i = lat + 4000; i < N; ++i) err = std::max(err, static_cast<double>(std::fabs(L[static_cast<size_t>(i)] - ref[static_cast<size_t>(i - lat)])));
    CHECK(err < 2e-3, "wet path reconstructs the input, max error %.2e", err);
}

static void testSlicesRetrigger() {
    std::printf("[5] slices freeze and retrigger\n");
    Engine e;
    e.prepare(SR);
    Params p = cleanParams();
    p.mix = 1.0f;
    p.timeL = p.timeR = divisionIndex("1/64");
    p.sliceMix = 1.0f;
    p.sliceDecay = 0.0f;
    p.sliceRate = divisionIndex("1/4");
    p.sliceSpread = 0.0f;
    p.pitchMix = 0.0f;
    e.setParams(p);
    // 1 s of tone, then silence: held slices should keep sounding into the silence until retriggered
    const int N = 96000;
    std::vector<float> L(N, 0.0f), R(N, 0.0f);
    for (int i = 0; i < 24000; ++i) L[static_cast<size_t>(i)] = R[static_cast<size_t>(i)] = 0.5f * std::sin(2.0 * kPi * 500.0 * i / SR);
    run(e, L, R);
    double energy = 0;
    for (int i = 32000; i < 44000; ++i) energy += L[static_cast<size_t>(i)] * L[static_cast<size_t>(i)];
    CHECK(energy > 1.0, "frozen slice keeps ringing after the source stops (energy %.2f)", energy);
}

static void testBarSnap() {
    std::printf("[6] bar-boundary snap re-aligns everything\n");
    Engine e;
    e.prepare(SR);
    Params p = cleanParams();
    p.desync = 1.0f;
    p.timeDrift = 1.0f;
    p.pitchDrift = 1.0f;
    p.sliceDrift = 1.0f;
    p.driftBuild = 1.0f;
    p.driftRateHz = 1.5f;
    p.snapIdx = 3;  // 4 bars; at 120 BPM a bar is 2 s, cycle is 8 s
    p.snapGlideMs = 40.0f;
    p.timeL = divisionIndex("1/8");
    p.timeR = divisionIndex("1/4");
    p.sliceBands = 8;
    p.sliceRate = divisionIndex("1/8");
    p.sliceSpread = 1.0f;
    e.setParams(p);

    HostInfo h;
    std::vector<float> big(4800), bigR(4800);
    float maxMidDrift = 0, maxDelayErrMid = 0, maxPitchMid = 0;
    // 0 .. 7.9 s in 100 ms blocks
    for (int blk = 0; blk < 79; ++blk) {
        for (size_t i = 0; i < big.size(); ++i) big[i] = bigR[i] = 0.3f * std::sin(0.05 * static_cast<double>(blk * 4800 + static_cast<int>(i)));
        e.process(big.data(), bigR.data(), static_cast<int>(big.size()), h);
        const Telemetry& t = e.telemetry();
        if (blk > 55) {
            maxMidDrift = std::max(maxMidDrift, t.drift.load());
            maxDelayErrMid = std::max(maxDelayErrMid, std::fabs(t.delayMs[0].load() - 250.0f));
            maxPitchMid = std::max(maxPitchMid, std::fabs(t.pitchSemi[0].load()));
        }
    }
    CHECK(maxMidDrift > 0.6f, "drift has built up before the bar line (%.2f)", maxMidDrift);
    CHECK(maxDelayErrMid > 3.0f, "delay time has wandered off the grid (up to %.1f ms)", maxDelayErrMid);
    CHECK(maxPitchMid > 0.3f, "pitch has drifted out of tune (up to %.2f st)", maxPitchMid);
    const uint32_t snapsBefore = e.telemetry().snapCount.load();

    // 7.9 s .. 8.4 s in 32-sample blocks so we can catch the exact moment of the snap
    std::vector<float> a(32), b(32);
    float spreadAtSnap = 9.0f;
    int snapBlock = -1;
    float driftEnd = 1, delayErrEnd = 1e9f, pitchErrEnd = 1e9f;
    for (int blk = 0; blk < 750; ++blk) {
        std::fill(a.begin(), a.end(), 0.1f);
        std::fill(b.begin(), b.end(), 0.1f);
        e.process(a.data(), b.data(), 32, h);
        const Telemetry& t = e.telemetry();
        if (snapBlock < 0 && t.snapCount.load() > snapsBefore) {
            snapBlock = blk;
            float mn = 1, mx = 0;
            for (int k = 0; k < 8; ++k) {
                mn = std::min(mn, t.bandPhase[0][k].load());
                mx = std::max(mx, t.bandPhase[0][k].load());
            }
            spreadAtSnap = mx - mn;
        }
        if (snapBlock >= 0 && blk == snapBlock + 4800 / 32 / 2) {  // ~50 ms after the snap (glide is 40 ms)
            driftEnd = t.drift.load();
            delayErrEnd = std::fabs(t.delayMs[0].load() - 250.0f);
            pitchErrEnd = std::fabs(t.pitchSemi[0].load());
        }
    }
    const int snapSample = 79 * 4800 + snapBlock * 32;
    CHECK(snapBlock >= 0, "snap fired by itself on the 4-bar line (t = %.3f s, expected 8.000 s)", snapSample / SR);
    CHECK(snapBlock >= 0 && std::fabs(snapSample / SR - 8.0) < 0.002, "snap lands within 2 ms of the bar line");
    CHECK(spreadAtSnap < 0.03f, "all 8 spectral band clocks restart together (phase spread %.4f)", spreadAtSnap);
    CHECK(driftEnd < 0.02f, "drift is back to ~0 after the snap (%.4f)", driftEnd);
    CHECK(delayErrEnd < 0.6f, "delay time is back on the musical grid (err %.3f ms)", delayErrEnd);
    CHECK(pitchErrEnd < 0.05f, "pitch is back in tune (%.3f st)", pitchErrEnd);
}

static void testStability() {
    std::printf("[7] stability, no NaN/Inf, bounded output at extreme settings\n");
    Engine e;
    e.prepare(44100.0);
    Params p;
    p.mix = 1.0f;
    p.feedback = 1.1f;
    p.cross = 1.0f;
    p.desync = 1.0f;
    p.timeDrift = 1.0f;
    p.pitchSemi = 19.0f;
    p.pitchPlace = 1;
    p.pitchDrift = 1.0f;
    p.lfoDepth = 12.0f;
    p.lfoShape = 5;
    p.seqAmount = 1.0f;
    for (int i = 0; i < kNumSeqSteps; ++i) p.seq[i] = static_cast<float>(i * 3 - 10);
    p.sliceMix = 1.0f;
    p.sliceBands = 16;
    p.sliceSpread = 1.0f;
    p.sliceDrift = 1.0f;
    p.filterType = 2;
    p.resonance = 1.0f;
    p.cutoffHz = 800.0f;
    p.filterModDepth = 3.0f;
    p.timeL = 0;
    p.timeR = kNumDivisions - 1;
    p.clockSource = 1;
    p.internalBpm = 300.0f;
    e.setParams(p);
    std::vector<float> L(4410), R(4410);
    float peak = 0;
    bool bad = false;
    uint32_t seed = 12345;
    for (int blk = 0; blk < 300; ++blk) {
        for (size_t i = 0; i < L.size(); ++i) {
            seed = seed * 1664525u + 1013904223u;
            const float n = (static_cast<float>(seed >> 8) / 8388608.0f - 1.0f) * 0.8f;
            L[i] = R[i] = n;
        }
        e.process(L.data(), R.data(), static_cast<int>(L.size()), HostInfo());
        for (size_t i = 0; i < L.size(); ++i) {
            if (!std::isfinite(L[i]) || !std::isfinite(R[i])) bad = true;
            peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
        }
    }
    CHECK(!bad, "all samples finite over 30 s of extreme settings");
    CHECK(peak < 8.0f, "output stays bounded (peak %.2f)", peak);
}

static void testHostSync() {
    std::printf("[8] host transport lock\n");
    Engine e;
    e.prepare(SR);
    Params p = cleanParams();
    p.clockSource = 0;
    p.timeL = p.timeR = divisionIndex("1/4");
    p.pitchMix = 0.0f;
    e.setParams(p);
    HostInfo h;
    h.playing = true;
    h.hasBpm = true;
    h.bpm = 100.0;  // quarter = 0.6 s = 28800 samples
    h.hasPpq = true;
    h.ppq = 8.0;
    std::vector<float> L(512, 0.0f), R(512, 0.0f);
    std::vector<float> out;
    for (int blk = 0; blk < 120; ++blk) {
        std::fill(L.begin(), L.end(), 0.0f);
        std::fill(R.begin(), R.end(), 0.0f);
        if (blk == 0) L[10] = 1.0f;
        e.process(L.data(), R.data(), 512, h);
        out.insert(out.end(), L.begin(), L.end());
        h.ppq += 512.0 * 100.0 / (60.0 * SR);
    }
    const int pk = peakIndex(out);
    CHECK(std::abs(pk - (10 + e.latencySamples() + 28800)) <= 1, "1/4 @ host 100 BPM echo at %d", pk);
    CHECK(e.telemetry().hostSynced.load(), "reports host sync");
}

int main() {
    testDelaysAreMusicalAndIndependent();
    testTempoFollowsBpm();
    testPitchIndependentOfTime();
    testSliceReconstruction();
    testSlicesRetrigger();
    testBarSnap();
    testStability();
    testHostSync();
    std::printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
