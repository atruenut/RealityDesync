// JUCE glue for the shared parameter table (src/core/ParamTable.h).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../core/Engine.h"
#include "../core/ParamTable.h"

namespace rdp {

namespace pt = rd::pt;

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : pt::specs()) {
        const juce::ParameterID pid{s.id, 1};
        switch (s.kind) {
            case pt::Kind::Float: {
                juce::NormalisableRange<float> r(s.min, s.max, s.step);
                if (s.skewCentre > s.min && s.skewCentre < s.max) r.setSkewForCentre(s.skewCentre);
                const pt::Spec* sp = &s;  // the table lives for the whole program
                auto attrs = juce::AudioParameterFloatAttributes()
                                 .withStringFromValueFunction([sp](float v, int) { return juce::String(pt::format(*sp, v)); })
                                 .withValueFromStringFunction([sp](const juce::String& t) { return pt::parse(*sp, t.toStdString()); });
                layout.add(std::make_unique<juce::AudioParameterFloat>(pid, s.name, r, s.def, attrs));
                break;
            }
            case pt::Kind::Choice: {
                juce::StringArray a;
                for (auto& c : s.choices) a.add(c);
                layout.add(std::make_unique<juce::AudioParameterChoice>(pid, s.name, a, static_cast<int>(s.def)));
                break;
            }
            case pt::Kind::Bool:
                layout.add(std::make_unique<juce::AudioParameterBool>(pid, s.name, s.def > 0.5f));
                break;
        }
    }
    return layout;
}

// Reads every parameter into the engine struct.
inline rd::Params readParams(juce::AudioProcessorValueTreeState& apvts) {
    static thread_local std::vector<float> values;
    const auto& t = pt::specs();
    values.resize(t.size());
    static std::vector<std::atomic<float>*> raw;
    if (raw.empty())
        for (const auto& s : t) raw.push_back(apvts.getRawParameterValue(s.id));
    for (size_t i = 0; i < t.size(); ++i) values[i] = raw[i]->load();
    return pt::toParams(values.data());
}

}  // namespace rdp
