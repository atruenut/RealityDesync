#include "ParamTable.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rd {
namespace pt {

namespace {

std::vector<std::string> divisionNames() {
    std::vector<std::string> a;
    for (int i = 0; i < kNumDivisions; ++i) a.push_back(kDivisions[i].name);
    return a;
}
std::vector<std::string> lfoPeriodNames() {
    std::vector<std::string> a;
    for (int i = 0; i < kNumLfoPeriods; ++i) a.push_back(kLfoPeriods[i].name);
    return a;
}
std::vector<std::string> snapNames() {
    std::vector<std::string> a;
    for (int i = 0; i < kNumSnapChoices; ++i) a.push_back(kSnapNames[i]);
    return a;
}

std::vector<Spec> buildTable() {
    std::vector<Spec> t;
    auto F = [&](const char* id, const char* name, float mn, float mx, float def, float step, float centre, const char* unit) {
        t.push_back({id, name, Kind::Float, mn, mx, def, step, centre, unit, {}});
    };
    auto C = [&](const char* id, const char* name, const std::vector<std::string>& ch, int def) {
        t.push_back({id, name, Kind::Choice, 0, static_cast<float>(ch.size() - 1), static_cast<float>(def), 1, 0, "", ch});
    };
    auto B = [&](const char* id, const char* name, bool def) { t.push_back({id, name, Kind::Bool, 0, 1, def ? 1.0f : 0.0f, 1, 0, "", {}}); };

    // --- output
    F("mix", "Mix", 0, 1, 0.8f, 0.001f, 0, "%");
    F("outGain", "Output", -24, 12, 0, 0.1f, 0, " dB");
    C("clock", "Clock Source", {"Host", "Internal"}, 0);
    F("bpm", "Internal BPM", 40, 240, 120, 0.1f, 0, " bpm");

    // --- desync / snap
    F("desync", "Desync", 0, 1, 0.6f, 0.001f, 0, "%");
    F("driftRate", "Drift Rate", 0.02f, 3.0f, 0.3f, 0, 0.3f, " Hz");
    C("snap", "Snap Every", snapNames(), 3);
    F("driftBuild", "Build Time", 0.1f, 1.0f, 0.8f, 0.001f, 0, "%");
    F("driftCurve", "Build Curve", -1, 1, 0, 0.001f, 0, "");
    F("snapGlide", "Snap Glide", 0, 500, 60, 1, 60, " ms");
    B("snapNow", "Snap Now", false);

    // --- time split
    C("timeL", "Left Time", divisionNames(), 8);
    C("timeR", "Right Time", divisionNames(), 11);
    F("feedback", "Feedback", 0, 1.1f, 0.35f, 0.001f, 0, "%");
    F("cross", "Cross", 0, 1, 0.4f, 0.001f, 0, "%");
    F("glide", "Time Glide", 1, 1000, 80, 1, 100, " ms");
    F("timeDrift", "Time Wobble", 0, 1, 0.35f, 0.001f, 0, "%");

    // --- pitch
    F("pitch", "Pitch", -24, 24, 0, 0.01f, 0, " st");
    F("cents", "Fine", -100, 100, 0, 0.1f, 0, " ct");
    F("pitchSplit", "L/R Split", -12, 12, 0, 0.01f, 0, " st");
    C("pitchPlace", "Pitch Stage", {"Pre-delay", "In loop", "Post-delay"}, 2);
    F("pitchMix", "Pitch Amount", 0, 1, 1, 0.001f, 0, "%");
    F("grain", "Grain", 20, 150, 60, 1, 60, " ms");
    F("pitchDrift", "Pitch Drift", 0, 1, 0.3f, 0.001f, 0, "%");

    // --- motion
    C("lfoShape", "LFO Shape", {"Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random Step", "Random Glide"}, 0);
    C("lfoRate", "LFO Length", lfoPeriodNames(), 5);
    F("lfoDepth", "LFO Depth", 0, 12, 0, 0.01f, 0, " st");
    F("lfoPhase", "LFO L/R Phase", -180, 180, 0, 1, 0, " deg");
    F("seqAmount", "Seq Amount", 0, 1, 0, 0.001f, 0, "%");
    C("seqDiv", "Seq Step", divisionNames(), 5);
    F("seqGlide", "Seq Glide", 0, 200, 20, 1, 0, " ms");
    for (int i = 0; i < kNumSeqSteps; ++i) {
        char id[8], nm[16];
        std::snprintf(id, sizeof id, "seq%d", i + 1);
        std::snprintf(nm, sizeof nm, "Seq Step %d", i + 1);
        F(id, nm, -12, 12, 0, 1, 0, " st");
    }

    // --- spectral slices
    F("sliceMix", "Slice Amount", 0, 1, 0.5f, 0.001f, 0, "%");
    F("sliceBands", "Bands", 2, 16, 8, 1, 0, "");
    C("sliceRate", "Slice Rate", divisionNames(), 8);
    F("sliceSpread", "Rate Spread", -1, 1, 0.5f, 0.001f, 0, "");
    C("slicePattern", "Spread Pattern", {"Slope", "Alternate", "Scatter"}, 0);
    F("sliceSkew", "L/R Skew", -1, 1, 0, 0.001f, 0, "");
    F("sliceDecay", "Slice Decay", 0, 1, 0.3f, 0.001f, 0, "%");
    F("sliceDrift", "Slice Slip", 0, 1, 0.4f, 0.001f, 0, "%");
    F("sliceLow", "Slice Low", 40, 1000, 100, 1, 200, " Hz");
    F("sliceHigh", "Slice High", 2000, 16000, 8000, 1, 6000, " Hz");

    // --- filter
    C("filterType", "Filter", {"Off", "Low-pass", "Band-pass", "High-pass", "Notch"}, 0);
    C("filterPlace", "Filter Position", {"In feedback", "On wet out"}, 0);
    F("cutoff", "Cutoff", 40, 16000, 2500, 0, 1000, " Hz");
    F("resonance", "Resonance", 0, 1, 0.3f, 0.001f, 0, "%");
    F("filterMod", "Filter Mod", 0, 3, 0, 0.001f, 0, " oct");
    C("filterModRate", "Mod Length", lfoPeriodNames(), 5);
    return t;
}

float skewOf(const Spec& s) {
    if (s.skewCentre > s.min && s.skewCentre < s.max) return std::log(0.5f) / std::log((s.skewCentre - s.min) / (s.max - s.min));
    return 1.0f;
}

}  // namespace

const std::vector<Spec>& specs() {
    static const std::vector<Spec> t = buildTable();
    return t;
}

int indexOf(const std::string& id) {
    const auto& t = specs();
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].id == id) return static_cast<int>(i);
    return -1;
}

float plainToNorm(const Spec& s, float v) {
    if (s.max <= s.min) return 0.0f;
    float p = (std::min(std::max(v, s.min), s.max) - s.min) / (s.max - s.min);
    const float k = skewOf(s);
    if (s.kind == Kind::Float && k != 1.0f) p = std::pow(p, k);
    return p;
}

float normToPlain(const Spec& s, float n) {
    n = std::min(std::max(n, 0.0f), 1.0f);
    const float k = skewOf(s);
    if (s.kind == Kind::Float && k != 1.0f && n > 0.0f) n = std::exp(std::log(n) / k);
    float v = s.min + (s.max - s.min) * n;
    if (s.kind != Kind::Float) return std::floor(v + 0.5f);
    if (s.step > 0.0f) v = s.min + std::floor((v - s.min) / s.step + 0.5f) * s.step;
    return std::min(std::max(v, s.min), s.max);
}

int stepCount(const Spec& s) {
    if (s.kind != Kind::Float) return static_cast<int>(s.max - s.min + 0.5f);
    if (s.step >= 1.0f) return static_cast<int>((s.max - s.min) / s.step + 0.5f);
    return 0;
}

std::string format(const Spec& s, float v) {
    char buf[64];
    if (s.kind == Kind::Choice) {
        const int i = std::min(std::max(static_cast<int>(v + 0.5f), 0), static_cast<int>(s.choices.size()) - 1);
        return s.choices[static_cast<size_t>(i)];
    }
    if (s.kind == Kind::Bool) return v > 0.5f ? "On" : "Off";
    const std::string& u = s.unit;
    if (u == "%") {
        std::snprintf(buf, sizeof buf, "%d%%", static_cast<int>(std::lround(v * 100.0f)));
        return buf;
    }
    if (u == " Hz") {
        if (v >= 1000.0f) std::snprintf(buf, sizeof buf, "%.2f kHz", v / 1000.0f);
        else std::snprintf(buf, sizeof buf, v < 100.0f ? "%.1f Hz" : "%.0f Hz", v);
        return buf;
    }
    const float span = s.max - s.min;
    int dp = s.step >= 1.0f ? 0 : (span <= 50.0f ? 2 : 1);
    if (u == " ms" || u == " deg") dp = 0;
    if (u == " ct" || u == " dB" || u == " bpm") dp = 1;
    std::snprintf(buf, sizeof buf, "%s%.*f%s", ((u == " st" || u == " ct") && v > 0.0f) ? "+" : "", dp, v, u.c_str());
    return buf;
}

float parse(const Spec& s, const std::string& text) {
    if (s.kind == Kind::Choice) {
        for (size_t i = 0; i < s.choices.size(); ++i) {
            std::string a = s.choices[i], b = text;
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);
            if (a == b) return static_cast<float>(i);
        }
        return s.def;
    }
    if (s.kind == Kind::Bool) return (text == "On" || text == "on" || text == "1") ? 1.0f : 0.0f;
    float v = static_cast<float>(std::atof(text.c_str()));
    if (text.find('k') != std::string::npos || text.find('K') != std::string::npos) v *= 1000.0f;
    if (s.unit == "%") v *= 0.01f;
    return v;
}

Params toParams(const float* f) {
    static const int ix[] = {
        indexOf("mix"), indexOf("outGain"), indexOf("clock"), indexOf("bpm"), indexOf("desync"), indexOf("driftRate"), indexOf("snap"),
        indexOf("driftBuild"), indexOf("driftCurve"), indexOf("snapGlide"), indexOf("timeL"), indexOf("timeR"), indexOf("feedback"),
        indexOf("cross"), indexOf("glide"), indexOf("timeDrift"), indexOf("pitch"), indexOf("cents"), indexOf("pitchSplit"),
        indexOf("pitchPlace"), indexOf("pitchMix"), indexOf("grain"), indexOf("pitchDrift"), indexOf("lfoShape"), indexOf("lfoRate"),
        indexOf("lfoDepth"), indexOf("lfoPhase"), indexOf("seqAmount"), indexOf("seqDiv"), indexOf("seqGlide"), indexOf("sliceMix"),
        indexOf("sliceBands"), indexOf("sliceRate"), indexOf("sliceSpread"), indexOf("slicePattern"), indexOf("sliceSkew"),
        indexOf("sliceDecay"), indexOf("sliceDrift"), indexOf("sliceLow"), indexOf("sliceHigh"), indexOf("filterType"),
        indexOf("filterPlace"), indexOf("cutoff"), indexOf("resonance"), indexOf("filterMod"), indexOf("filterModRate"), indexOf("seq1")};
    int k = 0;
    auto v = [&]() { return f[ix[k++]]; };
    auto i = [&]() { return static_cast<int>(std::lround(f[ix[k++]])); };
    Params p;
    p.mix = v();
    p.outGainDb = v();
    p.clockSource = i();
    p.internalBpm = v();
    p.desync = v();
    p.driftRateHz = v();
    p.snapIdx = i();
    p.driftBuild = v();
    p.driftCurve = v();
    p.snapGlideMs = v();
    p.timeL = i();
    p.timeR = i();
    p.feedback = v();
    p.cross = v();
    p.glideMs = v();
    p.timeDrift = v();
    p.pitchSemi = v();
    p.pitchCents = v();
    p.pitchStereo = v();
    p.pitchPlace = i();
    p.pitchMix = v();
    p.grainMs = v();
    p.pitchDrift = v();
    p.lfoShape = i();
    p.lfoRate = i();
    p.lfoDepth = v();
    p.lfoPhaseLR = v();
    p.seqAmount = v();
    p.seqDiv = i();
    p.seqGlideMs = v();
    p.sliceMix = v();
    p.sliceBands = i();
    p.sliceRate = i();
    p.sliceSpread = v();
    p.slicePattern = i();
    p.sliceSkew = v();
    p.sliceDecay = v();
    p.sliceDrift = v();
    p.sliceLowHz = v();
    p.sliceHighHz = v();
    p.filterType = i();
    p.filterPlace = i();
    p.cutoffHz = v();
    p.resonance = v();
    p.filterModDepth = v();
    p.filterModRate = i();
    const int s0 = ix[k];
    for (int n = 0; n < kNumSeqSteps; ++n) p.seq[n] = f[s0 + n];  // seq1..seq8 are consecutive in the table
    return p;
}

std::vector<float> resolvePreset(const PresetDef& pr) {
    const auto& t = specs();
    std::vector<float> v(t.size());
    for (size_t i = 0; i < t.size(); ++i) v[i] = t[i].def;
    for (const auto& kv : pr.values) {
        const int i = indexOf(kv.first);
        if (i < 0) continue;
        v[static_cast<size_t>(i)] = parse(t[static_cast<size_t>(i)], kv.second);
    }
    return v;
}

using V = std::vector<std::pair<std::string, std::string>>;

const std::vector<PresetDef>& presets() {
    static const std::vector<PresetDef> list = {
        // ------------------------------------------------------------------ INIT
        {"Init - Aligned", "Utility", "Everything on the grid. Nothing drifts. A clean starting point.",
         V{{"mix", "0.4"}, {"desync", "0"}, {"timeDrift", "0"}, {"pitchDrift", "0"}, {"sliceDrift", "0"}, {"sliceMix", "0"},
           {"feedback", "0.3"}, {"timeL", "1/8"}, {"timeR", "1/8D"}}},

        // ------------------------------------------------------------------ RHYTHM
        {"Two Clocks", "Rhythm", "Left runs straight eighths, right runs dotted eighths. They meet again every 3 beats.",
         V{{"timeL", "1/8"}, {"timeR", "1/8D"}, {"feedback", "0.38"}, {"cross", "0.45"}, {"desync", "0.4"}, {"snap", "2 bars"},
           {"sliceMix", "0.2"}, {"pitchMix", "0"}}},
        {"Three Against Two", "Rhythm", "Left plays in triplet quarters, right in straight quarters. Classic 3:2 polyrhythm.",
         V{{"timeL", "1/4T"}, {"timeR", "1/4"}, {"feedback", "0.5"}, {"cross", "0.65"}, {"desync", "0.3"}, {"snap", "1 bar"},
           {"sliceMix", "0.15"}, {"pitchMix", "0"}, {"filterType", "Low-pass"}, {"cutoff", "4500"}}},
        {"Slipping Grid", "Rhythm", "Starts locked, then the grid slides out from under you until the 4-bar snap.",
         V{{"timeL", "1/8"}, {"timeR", "1/16D"}, {"desync", "0.9"}, {"timeDrift", "1"}, {"sliceDrift", "0.8"}, {"sliceBands", "10"},
           {"sliceRate", "1/8"}, {"snap", "4 bars"}, {"driftBuild", "0.9"}, {"feedback", "0.4"}, {"mix", "0.75"}}},
        {"Bar Line Cliff", "Rhythm", "Tension builds quietly, then everything falls apart in the last bar and snaps shut.",
         V{{"desync", "1"}, {"driftBuild", "0.95"}, {"driftCurve", "0.85"}, {"snap", "2 bars"}, {"snapGlide", "15"}, {"timeDrift", "1"},
           {"pitchDrift", "0.8"}, {"sliceDrift", "1"}, {"sliceMix", "0.55"}, {"sliceRate", "1/8"}, {"sliceSpread", "0.8"},
           {"timeL", "1/8"}, {"timeR", "1/4"}, {"feedback", "0.5"}}},
        {"Metronome Ghost", "Rhythm", "A dark, steady quarter-note echo that gently ages with every repeat.",
         V{{"timeL", "1/4"}, {"timeR", "1/4"}, {"feedback", "0.55"}, {"cross", "0.2"}, {"desync", "0.12"}, {"pitchDrift", "0.2"},
           {"filterType", "Low-pass"}, {"cutoff", "1800"}, {"filterPlace", "In feedback"}, {"sliceMix", "0.1"}, {"mix", "0.7"}}},
        {"Syncopation Engine", "Rhythm", "Odd-length echoes in each ear that shuffle against the beat.",
         V{{"timeL", "5/16"}, {"timeR", "7/16"}, {"feedback", "0.45"}, {"cross", "0.7"}, {"desync", "0.35"}, {"snap", "2 bars"},
           {"sliceMix", "0.3"}, {"sliceRate", "1/8D"}, {"sliceSpread", "0.6"}}},

        // ------------------------------------------------------------------ PITCH
        {"Octave Ladder", "Pitch", "Each repeat climbs another octave. Shimmer with a clock.",
         V{{"pitch", "12"}, {"pitchPlace", "In loop"}, {"grain", "80"}, {"feedback", "0.62"}, {"cross", "0.5"}, {"timeL", "1/4"},
           {"timeR", "1/4D"}, {"filterType", "Low-pass"}, {"cutoff", "6500"}, {"desync", "0.25"}, {"mix", "0.6"}, {"sliceMix", "0.1"}}},
        {"Falling Floor", "Pitch", "Every repeat sinks an octave deeper into the floor.",
         V{{"pitch", "-12"}, {"pitchPlace", "In loop"}, {"grain", "90"}, {"feedback", "0.6"}, {"timeL", "1/8"}, {"timeR", "1/8D"},
           {"filterType", "Low-pass"}, {"cutoff", "2200"}, {"sliceMix", "0.2"}, {"desync", "0.3"}}},
        {"Fifth Wheel", "Pitch", "Left stays home, right sits a perfect fifth above. Instant parallel harmony.",
         V{{"pitch", "3.5"}, {"pitchSplit", "7"}, {"pitchPlace", "Post-delay"}, {"timeL", "1/8"}, {"timeR", "1/8"}, {"feedback", "0.3"},
           {"cross", "0.3"}, {"desync", "0.2"}, {"sliceMix", "0.1"}, {"grain", "45"}}},
        {"Seasick Choir", "Pitch", "Slow, out-of-phase pitch waves in each ear. Notes lean like a boat.",
         V{{"lfoShape", "Sine"}, {"lfoRate", "2 bars"}, {"lfoDepth", "0.9"}, {"lfoPhase", "120"}, {"pitchDrift", "0.5"}, {"pitchPlace", "Post-delay"},
           {"timeL", "1/4"}, {"timeR", "1/4D"}, {"feedback", "0.45"}, {"sliceMix", "0.3"}, {"sliceRate", "1/4"}, {"desync", "0.5"}}},
        {"Arpeggio Collapse", "Pitch", "The 8-step pitch sequencer plays an arpeggio that the delays smear across time.",
         V{{"seqAmount", "1"}, {"seqDiv", "1/16"}, {"seqGlide", "4"}, {"seq1", "0"}, {"seq2", "4"}, {"seq3", "7"}, {"seq4", "12"},
           {"seq5", "7"}, {"seq6", "4"}, {"seq7", "0"}, {"seq8", "-5"}, {"timeL", "1/8"}, {"timeR", "1/8D"}, {"feedback", "0.5"},
           {"cross", "0.6"}, {"grain", "35"}, {"desync", "0.3"}, {"snap", "1 bar"}, {"sliceMix", "0.1"}}},
        {"Pitch Cathedral", "Pitch", "Huge, slow, rising echoes in a stone room that does not exist.",
         V{{"pitch", "12"}, {"pitchPlace", "In loop"}, {"timeL", "1/2"}, {"timeR", "1/2D"}, {"feedback", "0.72"}, {"cross", "0.7"},
           {"grain", "110"}, {"filterType", "Low-pass"}, {"cutoff", "5000"}, {"mix", "0.55"}, {"sliceMix", "0.35"}, {"sliceRate", "1/2"},
           {"sliceBands", "12"}, {"desync", "0.4"}, {"snap", "4 bars"}}},
        {"Detune Twins", "Pitch", "Short offset echoes pitched a few cents apart. Wide, liquid doubling.",
         V{{"timeL", "1/64"}, {"timeR", "1/32"}, {"pitchSplit", "0.5"}, {"cents", "6"}, {"pitchPlace", "Post-delay"}, {"grain", "40"},
           {"feedback", "0.15"}, {"cross", "0.2"}, {"mix", "0.5"}, {"desync", "0.2"}, {"timeDrift", "0.2"}, {"sliceMix", "0"}}},

        // ------------------------------------------------------------------ SPECTRAL
        {"Frozen Prism", "Spectral", "Frequency bands hold their last moment and restart half a bar later. A lit-up freeze.",
         V{{"sliceMix", "1"}, {"sliceBands", "12"}, {"sliceRate", "1/2"}, {"sliceSpread", "0.3"}, {"sliceDecay", "0"}, {"mix", "0.9"},
           {"timeL", "1/16"}, {"timeR", "1/16D"}, {"feedback", "0.2"}, {"desync", "0.3"}}},
        {"Shatter Glass", "Spectral", "Sixteen bands retrigger at scattered rates. Sounds like a window breaking in slow motion.",
         V{{"sliceMix", "0.85"}, {"sliceBands", "16"}, {"sliceRate", "1/16"}, {"sliceSpread", "1"}, {"slicePattern", "Scatter"},
           {"sliceDecay", "0.7"}, {"sliceHigh", "12000"}, {"sliceLow", "150"}, {"timeL", "1/16"}, {"timeR", "1/16T"}, {"feedback", "0.3"},
           {"desync", "0.5"}}},
        {"Rainfall Spectrum", "Spectral", "Lows repeat slowly, highs repeat fast. Every band is on a different clock.",
         V{{"sliceMix", "0.8"}, {"sliceBands", "14"}, {"sliceRate", "1/8"}, {"sliceSpread", "1"}, {"slicePattern", "Slope"},
           {"sliceDecay", "0.5"}, {"sliceSkew", "0.6"}, {"sliceDrift", "0.6"}, {"timeL", "1/8D"}, {"timeR", "1/4"}, {"desync", "0.5"}}},
        {"Mirror Slices", "Spectral", "Odd bands race ahead, even bands lag. The right side runs 3:2 against the left.",
         V{{"sliceMix", "0.8"}, {"sliceBands", "8"}, {"sliceRate", "1/8"}, {"sliceSpread", "0.8"}, {"slicePattern", "Alternate"},
           {"sliceSkew", "1"}, {"sliceDecay", "0.4"}, {"timeL", "1/8"}, {"timeR", "1/8"}, {"feedback", "0.3"}, {"desync", "0.4"}}},
        {"Vowel Drift", "Spectral", "A band-pass sweeps slowly over retriggering mid bands. Speech-like and unsettling.",
         V{{"sliceMix", "0.75"}, {"sliceBands", "6"}, {"sliceLow", "300"}, {"sliceHigh", "3500"}, {"sliceRate", "1/4"}, {"sliceSpread", "0.4"},
           {"filterType", "Band-pass"}, {"filterPlace", "On wet out"}, {"cutoff", "900"}, {"resonance", "0.65"}, {"filterMod", "1.5"},
           {"filterModRate", "2 bars"}, {"timeL", "1/8D"}, {"timeR", "1/4"}, {"desync", "0.45"}, {"mix", "0.8"}}},

        // ------------------------------------------------------------------ CINEMATIC
        {"Time Dilation", "Cinematic", "Long, sinking repeats where the high and low bands slowly lose track of each other.",
         V{{"timeL", "1/2"}, {"timeR", "1/1"}, {"pitch", "-5"}, {"pitchPlace", "In loop"}, {"feedback", "0.68"}, {"cross", "0.6"},
           {"grain", "100"}, {"filterType", "Low-pass"}, {"cutoff", "3200"}, {"desync", "0.7"}, {"sliceMix", "0.5"}, {"sliceRate", "1/2"},
           {"sliceSpread", "0.4"}, {"snap", "4 bars"}, {"mix", "0.6"}}},
        {"Event Horizon", "Cinematic", "Everything falls in. Repeats darken, sink and slowly stretch for eight bars before the snap.",
         V{{"timeL", "1/2D"}, {"timeR", "1/1"}, {"pitch", "-12"}, {"pitchPlace", "In loop"}, {"feedback", "0.86"}, {"cross", "0.8"},
           {"grain", "120"}, {"filterType", "Low-pass"}, {"cutoff", "1200"}, {"resonance", "0.5"}, {"filterMod", "0.8"}, {"filterModRate", "4 bars"},
           {"sliceMix", "0.6"}, {"sliceRate", "1/1"}, {"sliceDecay", "0.2"}, {"desync", "1"}, {"snap", "8 bars"}, {"driftBuild", "1"}, {"mix", "0.55"}}},
        {"Parallel Hallway", "Cinematic", "Two corridors of different length. The second one sounds a fifth higher.",
         V{{"timeL", "1/4D"}, {"timeR", "1/2"}, {"pitch", "3.5"}, {"pitchSplit", "7"}, {"pitchPlace", "In loop"}, {"feedback", "0.6"},
           {"cross", "1"}, {"grain", "85"}, {"sliceMix", "0.3"}, {"sliceDecay", "0.5"}, {"desync", "0.5"}, {"mix", "0.6"}}},
        {"Memory Leak", "Cinematic", "Feedback that never quite dies. A slight detune makes the tail spiral upward.",
         V{{"feedback", "0.97"}, {"cross", "0.8"}, {"cents", "14"}, {"pitchPlace", "In loop"}, {"grain", "90"}, {"filterType", "High-pass"},
           {"cutoff", "380"}, {"filterPlace", "In feedback"}, {"timeL", "1/4D"}, {"timeR", "1/2T"}, {"desync", "0.5"}, {"mix", "0.5"},
           {"sliceMix", "0.2"}}},
        {"Sleepwalker", "Cinematic", "Slow, loose and dreamy. Time and pitch both wander, then the whole thing wakes up on the bar line.",
         V{{"driftRate", "0.1"}, {"desync", "0.8"}, {"snap", "8 bars"}, {"driftBuild", "0.9"}, {"timeL", "1/4D"}, {"timeR", "1/2T"},
           {"pitchDrift", "0.6"}, {"timeDrift", "0.8"}, {"sliceMix", "0.4"}, {"sliceRate", "1/2"}, {"sliceDrift", "0.7"}, {"feedback", "0.5"},
           {"mix", "0.65"}}},
        {"Haunted Elevator", "Cinematic", "A sagging low-pass echo that rides up and down in pitch on every floor.",
         V{{"lfoShape", "Triangle"}, {"lfoRate", "4 bars"}, {"lfoDepth", "3"}, {"pitchPlace", "In loop"}, {"timeL", "1/4D"}, {"timeR", "1/4"},
           {"feedback", "0.6"}, {"filterType", "Low-pass"}, {"cutoff", "2000"}, {"filterMod", "0.6"}, {"filterModRate", "2 bars"},
           {"sliceMix", "0.35"}, {"sliceDecay", "0.5"}, {"desync", "0.5"}}},

        // ------------------------------------------------------------------ GLITCH
        {"Stutter Machine", "Glitch", "Fast frozen slices chopping the sound into a mechanical stutter.",
         V{{"sliceMix", "1"}, {"sliceBands", "8"}, {"sliceRate", "1/32"}, {"sliceSpread", "0.2"}, {"sliceDecay", "0.9"}, {"timeL", "1/16"},
           {"timeR", "1/16D"}, {"feedback", "0.25"}, {"desync", "0.25"}, {"mix", "0.85"}}},
        {"Buffer Overrun", "Glitch", "Tiny delays with near-runaway feedback and a resonant band-pass. Metallic and unstable.",
         V{{"timeL", "1/64"}, {"timeR", "1/32T"}, {"feedback", "0.92"}, {"cross", "1"}, {"filterType", "Band-pass"}, {"filterPlace", "In feedback"},
           {"cutoff", "3000"}, {"resonance", "0.8"}, {"filterMod", "1"}, {"filterModRate", "1/2"}, {"desync", "0.5"}, {"sliceMix", "0.3"},
           {"mix", "0.6"}}},
        {"Sample And Hold Rain", "Glitch", "A stepped-random pitch generator fires on every eighth note. Falling droplets on a grid.",
         V{{"lfoShape", "Random Step"}, {"lfoRate", "1/8"}, {"lfoDepth", "7"}, {"lfoPhase", "180"}, {"pitchPlace", "Post-delay"}, {"grain", "30"},
           {"timeL", "1/16"}, {"timeR", "1/8T"}, {"feedback", "0.4"}, {"cross", "0.5"}, {"sliceMix", "0.3"}, {"desync", "0.3"}}},
        {"Tape Stop Echo", "Glitch", "Every bar the pitch falls off a cliff and recovers. Locked to the tempo.",
         V{{"lfoShape", "Saw Down"}, {"lfoRate", "1 bar"}, {"lfoDepth", "12"}, {"pitchPlace", "In loop"}, {"grain", "70"}, {"timeL", "1/4"},
           {"timeR", "1/4"}, {"feedback", "0.6"}, {"cross", "0.4"}, {"desync", "0.15"}, {"sliceMix", "0"}}},

        // ------------------------------------------------------------------ SUBTLE
        {"Subtle Desync", "Subtle", "A faint haze of offset echoes. Makes a dry track feel slightly unreal.",
         V{{"mix", "0.3"}, {"timeL", "1/16"}, {"timeR", "1/16D"}, {"feedback", "0.15"}, {"timeDrift", "0.25"}, {"sliceMix", "0.2"},
           {"sliceDecay", "0.6"}, {"desync", "0.3"}, {"pitchMix", "0"}}},
        {"Stereo Slip", "Subtle", "Very short, slowly wobbling delays in each ear. Wide without sounding like an effect.",
         V{{"mix", "0.35"}, {"timeL", "1/64"}, {"timeR", "1/32"}, {"feedback", "0"}, {"timeDrift", "0.7"}, {"desync", "0.6"}, {"sliceMix", "0"},
           {"pitchSplit", "0.2"}, {"grain", "40"}, {"driftRate", "0.15"}}},
        {"Bar Line Reset (Demo)", "Subtle", "Every drift control maxed with a 1-bar snap. Watch the scope fall apart and re-align.",
         V{{"desync", "1"}, {"snap", "1 bar"}, {"driftBuild", "0.6"}, {"driftRate", "1"}, {"timeDrift", "1"}, {"pitchDrift", "1"}, {"sliceDrift", "1"},
           {"sliceMix", "0.5"}, {"sliceSpread", "1"}, {"pitchPlace", "Post-delay"}, {"timeL", "1/8"}, {"timeR", "1/4"}, {"feedback", "0.4"}}},
    };
    return list;
}

}  // namespace pt
}  // namespace rd
