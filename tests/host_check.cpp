// Loads the built .vst3 through JUCE's VST3 host path, asks it for its editor and processes audio.
// (It does not draw the editor on screen: a JUCE host loading a JUCE plugin in one process is not a faithful GUI test.)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf("usage: host_check <plugin.vst3>\n"); return 2; }
    juce::AudioPluginFormatManager fm;
    fm.addFormat(new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> found;
    fm.getFormat(0)->findAllTypesForFile(found, argv[1]);
    std::printf("plugins found in file: %d\n", found.size());
    if (found.isEmpty()) return 1;
    juce::String err;
    auto inst = fm.createPluginInstance(*found[0], 48000.0, 512, err);
    if (!inst) { std::printf("LOAD FAILED: %s\n", err.toRawUTF8()); return 1; }
    std::printf("loaded: %s | params %d | programs %d | latency %d | hasEditor %d\n", inst->getName().toRawUTF8(),
                inst->getParameters().size(), inst->getNumPrograms(), inst->getLatencySamples(), (int)inst->hasEditor());
    inst->setPlayConfigDetails(2, 2, 48000.0, 512);
    inst->prepareToPlay(48000.0, 512);
    inst->setCurrentProgram(4);  // Bar Line Cliff, set by the host
    std::printf("program 4 = %s\n", inst->getProgramName(4).toRawUTF8());

    std::unique_ptr<juce::AudioProcessorEditor> ed(inst->createEditorIfNeeded());
    if (!ed) { std::printf("NO EDITOR RETURNED\n"); return 1; }
    std::printf("editor size: %d x %d\n", ed->getWidth(), ed->getHeight());
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    float peak = 0;
    for (int blk = 0; blk < 400; ++blk) {
        for (int i = 0; i < 512; ++i) { const float x = 0.3f * std::sin(0.04f * (blk * 512 + i)); buf.setSample(0, i, x); buf.setSample(1, i, x); }
        inst->processBlock(buf, midi);
        for (int i = 0; i < 512; ++i) peak = std::max(peak, std::fabs(buf.getSample(0, i)));
        if (blk % 40 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    }
    std::printf("audio peak %.3f\nHOST CHECK PASSED\n", peak);
    ed.reset();
    return 0;
}
