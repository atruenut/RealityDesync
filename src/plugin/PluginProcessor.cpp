#include "PluginProcessor.h"

#include "PluginEditor.h"

RealityDesyncProcessor::RealityDesyncProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "RealityDesync", rdp::createLayout()) {}

bool RealityDesyncProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void RealityDesyncProcessor::prepareToPlay(double sampleRate, int) {
    engine.prepare(sampleRate);
    setLatencySamples(engine.latencySamples());
    snapInit = false;
}

void RealityDesyncProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();

    // Mono in -> stereo out: duplicate into the second channel.
    if (buffer.getNumChannels() >= 2 && inCh == 1) buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    if (buffer.getNumChannels() < 2) return;

    // Host transport
    rd::HostInfo host;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            host.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) {
                host.hasBpm = true;
                host.bpm = *bpm;
            }
            if (auto ppq = pos->getPpqPosition()) {
                host.hasPpq = true;
                host.ppq = *ppq;
            }
            if (auto ts = pos->getTimeSignature()) {
                host.tsNum = ts->numerator;
                host.tsDen = ts->denominator;
            }
            if (auto bs = pos->getPpqPositionOfLastBarStart()) {
                host.hasBarStart = true;
                host.barStartPpq = *bs;
            }
        }
    }

    // "Snap Now" is a trigger: any change of the parameter fires one realignment (works from automation too).
    const float snapNow = apvts.getRawParameterValue("snapNow")->load();
    if (!snapInit) {
        lastSnapParam = snapNow;
        snapInit = true;
    } else if (snapNow != lastSnapParam) {
        lastSnapParam = snapNow;
        engine.requestSnap();
    }

    engine.setParams(rdp::readParams(apvts));
    engine.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples, host);
}

juce::AudioProcessorEditor* RealityDesyncProcessor::createEditor() { return new RealityDesyncEditor(*this); }

void RealityDesyncProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty("presetName", currentPresetName, nullptr);
    state.setProperty("presetDescription", currentPresetDescription, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void RealityDesyncProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        if (xml->hasTagName(apvts.state.getType())) {
            auto tree = juce::ValueTree::fromXml(*xml);
            currentPresetName = tree.getProperty("presetName", "Init - Aligned").toString();
            currentPresetDescription = tree.getProperty("presetDescription", "").toString();
            apvts.replaceState(tree);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RealityDesyncProcessor(); }
