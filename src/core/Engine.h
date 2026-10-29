// Reality Desync - engine interface (portable C++17, no plugin-framework dependency).
#pragma once

#include <atomic>
#include <memory>

#include "Common.h"

namespace rd {

// ---------------------------------------------------------------------------
// Everything the user can set. The plugin layer fills this from its parameters once
// per block; the offline tests fill it directly.
// ---------------------------------------------------------------------------
struct Params {
    // Global
    float mix = 0.8f;
    float outGainDb = 0.0f;
    float internalBpm = 120.0f;
    int clockSource = 0;  // 0 = follow host (falls back to internal), 1 = always internal clock

    // Desync / snap
    float desync = 0.6f;      // master amount of drift (0..1)
    float driftRateHz = 0.3f; // how fast the texture wanders
    int snapIdx = 3;          // index into kSnapBars (3 = every 4 bars)
    float driftBuild = 0.8f;  // fraction of the cycle over which drift builds up
    float driftCurve = 0.0f;  // -1..1 shape of the build (exponent 0.25..4)
    float snapGlideMs = 60.0f;

    // Time split (left / right musical delays)
    int timeL = 8;   // index into kDivisions
    int timeR = 11;
    float feedback = 0.35f;  // 0..1.1
    float cross = 0.4f;      // 0 = parallel feedback, 1 = full ping-pong
    float glideMs = 80.0f;   // how fast delay times follow changes
    float timeDrift = 0.35f; // 0..1  (up to +-30 ms of wobble at full drift)

    // Pitch fracture
    float pitchSemi = 0.0f;
    float pitchCents = 0.0f;
    float pitchStereo = 0.0f;  // L/R pitch split in semitones (L -x/2, R +x/2)
    int pitchPlace = 2;        // 0 = pre-delay, 1 = in feedback loop (shifts repeats only), 2 = post-delay
    float pitchMix = 1.0f;
    float grainMs = 60.0f;
    float pitchDrift = 0.3f;   // 0..1 (up to +-2 semitones at full drift)
    int lfoShape = 0;
    int lfoRate = 5;           // index into kLfoPeriods
    float lfoDepth = 0.0f;     // semitones
    float lfoPhaseLR = 0.0f;   // degrees
    float seqAmount = 0.0f;
    int seqDiv = 5;            // index into kDivisions (step length)
    float seqGlideMs = 20.0f;
    float seq[kNumSeqSteps] = {0, 0, 0, 0, 0, 0, 0, 0};

    // Spectral slices
    float sliceMix = 0.5f;
    int sliceBands = 8;
    int sliceRate = 8;        // index into kDivisions (base retrigger period)
    float sliceSpread = 0.5f; // -1..1 how far band rates fan out
    int slicePattern = 0;     // 0 slope, 1 alternate, 2 scatter
    float sliceSkew = 0.0f;   // -1..1 right channel rate ratio (3:2 at the extremes)
    float sliceDecay = 0.3f;
    float sliceDrift = 0.4f;  // 0..1 detune of band clocks (they slip against the grid)
    float sliceLowHz = 100.0f;
    float sliceHighHz = 8000.0f;

    // Filter
    int filterType = 0;       // 0 off, 1 LP, 2 BP, 3 HP, 4 notch
    int filterPlace = 0;      // 0 in feedback, 1 on wet output
    float cutoffHz = 2500.0f;
    float resonance = 0.3f;   // 0..1
    float filterModDepth = 0.0f;  // octaves
    int filterModRate = 5;    // index into kLfoPeriods
};

// What the host tells us about the transport. Anything missing is simply "false".
struct HostInfo {
    bool playing = false;
    bool hasBpm = false;
    double bpm = 120.0;
    bool hasPpq = false;
    double ppq = 0.0;   // position in quarter notes at the start of the block
    int tsNum = 4;
    int tsDen = 4;
    bool hasBarStart = false;
    double barStartPpq = 0.0;
};

// Read-only state for the GUI. Written from the audio thread, read from the UI thread.
struct Telemetry {
    std::atomic<float> drift{0.0f};      // effective drift level 0..1
    std::atomic<float> cyclePos{0.0f};   // 0..1 progress to the next snap
    std::atomic<float> barsToSnap{-1.0f};
    std::atomic<float> delayMs[kNumChannels];
    std::atomic<float> pitchSemi[kNumChannels];
    std::atomic<float> bpm{120.0f};
    std::atomic<float> bandPhase[kNumChannels][kMaxBands];
    std::atomic<uint32_t> bandTrig[kNumChannels][kMaxBands];
    std::atomic<uint32_t> snapCount{0};
    std::atomic<bool> hostSynced{false};

    Telemetry() {
        for (int c = 0; c < kNumChannels; ++c) {
            delayMs[c].store(0.0f);
            pitchSemi[c].store(0.0f);
            for (int b = 0; b < kMaxBands; ++b) {
                bandPhase[c][b].store(0.0f);
                bandTrig[c][b].store(0u);
            }
        }
    }
};

class Engine {
public:
    Engine();
    ~Engine();

    void prepare(double sampleRate);
    void reset();

    int latencySamples() const { return kFftSize; }

    void setParams(const Params& p) { params_ = p; }

    // Request a manual realignment (the SNAP button). Safe to call from any thread.
    void requestSnap() { snapRequested_.store(true); }

    // Stereo, in place.
    void process(float* left, float* right, int numSamples, const HostInfo& host);

    const Telemetry& telemetry() const { return telemetry_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Params params_;
    std::atomic<bool> snapRequested_{false};
    Telemetry telemetry_;
};

}  // namespace rd
