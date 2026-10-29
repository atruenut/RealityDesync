#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace rdp {

struct Preset {
    juce::String name;
    juce::String category;
    juce::String description;
    int index = 0;  // index into rd::pt::presets()
};

const std::vector<Preset>& factoryPresets();

// Resets every parameter (except Snap Now) to its default, then applies the preset's values.
// Every change goes through the host-visible parameter objects so the GUI and the host follow.
void applyPreset(juce::AudioProcessorValueTreeState& apvts, const Preset& preset);

// User presets stored on disk.
juce::File userPresetFolder();
bool saveUserPreset(juce::AudioProcessorValueTreeState& apvts, const juce::String& name, const juce::String& description);
juce::Array<juce::File> listUserPresets();
bool loadUserPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file, juce::String* nameOut = nullptr,
                    juce::String* descOut = nullptr);

}  // namespace rdp
