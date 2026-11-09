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

    // The factory presets are published as host programs, so the host's own preset menu lists them too.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}
    int currentProgram = 0;

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
