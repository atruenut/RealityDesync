#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../core/Engine.h"
#include "ParamDefs.h"

class RealityDesyncProcessor : public juce::AudioProcessor {
public:
    RealityDesyncProcessor();
    ~RealityDesyncProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Reality Desync"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 14.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    rd::Engine engine;

    // Preset display info (kept in the plugin state so the name survives save/load)
    juce::String currentPresetName = "Init - Aligned";
    juce::String currentPresetDescription;

private:
    float lastSnapParam = 0.0f;
    bool snapInit = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RealityDesyncProcessor)
};
