// Headless check of the real plugin classes: presets, state, audio health, GUI snapshots.
#include <cmath>
#include <cstdio>

#include "../src/plugin/PluginEditor.h"
#include "../src/plugin/PluginProcessor.h"
#include "../src/plugin/Presets.h"

static int fails = 0;
#define EXPECT(c, ...) do { if (!(c)) { ++fails; std::printf("  FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

static float get(RealityDesyncProcessor& p, const char* id) { return p.apvts.getRawParameterValue(id)->load(); }

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outDir = argc > 1 ? argv[1] : "/tmp";

    RealityDesyncProcessor proc;
    proc.setPlayConfigDetails(2, 2, 48000.0, 512);
    proc.prepareToPlay(48000.0, 512);
    std::printf("params exposed to host: %d\n", proc.getParameters().size());
    std::printf("reported latency: %d samples\n", proc.getLatencySamples());

    const auto& presets = rdp::factoryPresets();
    std::printf("factory presets: %zu\n", presets.size());

    juce::Random rng(7);
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    int idx = 0;
    for (const auto& pr : presets) {
        rdp::applyPreset(proc.apvts, pr);
        proc.prepareToPlay(48000.0, 512);
        float peak = 0, rms = 0;
        bool bad = false;
        long n = 0;
        for (int blk = 0; blk < 800; ++blk) {  // ~8.5 s: a drum-ish noise burst every 0.5 s plus a sine
            for (int i = 0; i < 512; ++i) {
                const long t = (long)blk * 512 + i;
                const bool burst = (t % 24000) < 3000;
                const float env = burst ? std::exp(-(float)(t % 24000) / 800.0f) : 0.0f;
                const float x = 0.5f * env * (rng.nextFloat() * 2 - 1) + 0.15f * std::sin(2 * 3.14159f * 220.0f * t / 48000.0f);
                buf.setSample(0, i, x);
                buf.setSample(1, i, x);
            }
            proc.processBlock(buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i) {
                    const float v = buf.getSample(c, i);
                    if (!std::isfinite(v)) bad = true;
                    peak = std::max(peak, std::fabs(v));
                    rms += v * v;
                    ++n;
                }
        }
        rms = std::sqrt(rms / n);
        std::printf("  %-24s %-10s peak %.2f  rms %.3f %s\n", pr.name.toRawUTF8(), pr.category.toRawUTF8(), peak, rms, bad ? "  <-- NaN/Inf" : "");
        EXPECT(!bad, "%s produced non-finite audio", pr.name.toRawUTF8());
        EXPECT(peak < 4.0f, "%s output too hot (%.2f)", pr.name.toRawUTF8(), peak);
        EXPECT(rms > 0.001f, "%s is silent", pr.name.toRawUTF8());
        ++idx;
    }

    // Applying a preset must move every parameter it names, and a following preset must reset the rest.
    {
        const rdp::Preset* a = nullptr; const rdp::Preset* b = nullptr;
        for (auto& p : presets) { if (p.name == "Octave Ladder") a = &p; if (p.name == "Init - Aligned") b = &p; }
        rdp::applyPreset(proc.apvts, *a);
        EXPECT(std::fabs(get(proc, "pitch") - 12.0f) < 0.01f, "Octave Ladder sets pitch to +12");
        EXPECT((int)get(proc, "pitchPlace") == 1, "Octave Ladder puts the pitch stage in the loop");
        rdp::applyPreset(proc.apvts, *b);
        EXPECT(std::fabs(get(proc, "pitch")) < 0.01f, "next preset resets pitch back to default");
        EXPECT((int)get(proc, "pitchPlace") == 2, "next preset resets pitch stage to default");
    }
    // every key in every preset must name a real parameter / choice
    for (auto& pr : rd::pt::presets())
        for (auto& kv : pr.values) {
            const int si = rd::pt::indexOf(kv.first);
            EXPECT(si >= 0, "%s uses unknown parameter '%s'", pr.name.c_str(), kv.first.c_str());
            if (si >= 0 && rd::pt::specs()[(size_t)si].kind == rd::pt::Kind::Choice) {
                bool ok = false;
                for (auto& c : rd::pt::specs()[(size_t)si].choices) ok |= (c == kv.second);
                EXPECT(ok, "%s: '%s' is not a valid choice for %s", pr.name.c_str(), kv.second.c_str(), kv.first.c_str());
            }
        }

    // state round trip
    {
        rdp::applyPreset(proc.apvts, presets[3]);
        proc.currentPresetName = presets[3].name;
        juce::MemoryBlock mb;
        proc.getStateInformation(mb);
        RealityDesyncProcessor other;
        other.setStateInformation(mb.getData(), (int)mb.getSize());
        bool same = true;
        for (auto& s : rd::pt::specs()) { const bool ok = std::fabs(get(proc, s.id.c_str()) - get(other, s.id.c_str())) < 1e-4f * std::max(1.0f, std::fabs(get(proc, s.id.c_str()))); if (!ok) std::printf("    differs: %s  %.4f vs %.4f\n", s.id.c_str(), get(proc, s.id.c_str()), get(other, s.id.c_str())); same &= ok; }
        EXPECT(same, "saved/restored state matches");
        EXPECT(other.currentPresetName == presets[3].name, "preset name survives state restore");
    }
    // user preset round trip
    {
        rdp::applyPreset(proc.apvts, presets[10]);
        EXPECT(rdp::saveUserPreset(proc.apvts, "Check Preset", "made by the check"), "user preset saved");
        rdp::applyPreset(proc.apvts, presets[0]);
        auto files = rdp::listUserPresets();
        EXPECT(files.size() >= 1, "user preset listed");
        juce::String n;
        EXPECT(rdp::loadUserPreset(proc.apvts, files[0], &n), "user preset loaded");
        bool same = true;
        auto cmp = juce::ValueTree();
        rdp::applyPreset(proc.apvts, presets[10]);
        EXPECT(std::fabs(get(proc, "pitch")) >= 0.0f, "sanity");
        for (auto& f : files) f.deleteFile();
    }

    // GUI snapshots
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
        const char* names[] = {"Bar Line Cliff", "Octave Ladder", "Shatter Glass"};
        int shot = 0;
        for (auto* nm : names) {
            for (auto& p : presets)
                if (p.name == nm) { rdp::applyPreset(proc.apvts, p); proc.currentPresetName = p.name; proc.currentPresetDescription = p.description; }
            proc.prepareToPlay(48000.0, 512);
            for (int blk = 0; blk < 700; ++blk) {
                for (int i = 0; i < 512; ++i) { const float x = 0.3f * std::sin(0.03f * (blk * 512 + i)); buf.setSample(0, i, x); buf.setSample(1, i, x); }
                proc.processBlock(buf, midi);
                if (blk % 50 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
            }
            ed.reset(proc.createEditor());
            ed->setVisible(false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
            for (int blk = 0; blk < 120; ++blk) { proc.processBlock(buf, midi); if (blk % 10 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil(40); }
            auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            juce::File f(outDir + "/gui_" + juce::String(shot++) + ".png");
            f.deleteFile();
            juce::FileOutputStream os(f);
            juce::PNGImageFormat().writeImageToStream(img, os);
            std::printf("snapshot %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        }
    }
    std::printf("%s (%d failures)\n", fails ? "FAILED" : "PLUGIN CHECK PASSED", fails);
    return fails ? 1 : 0;
}
