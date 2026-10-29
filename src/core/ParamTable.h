// Shared, framework-independent parameter table and factory presets.
// Used by both plugin front-ends (JUCE and plain VST3 SDK) so they behave identically.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Engine.h"

namespace rd {
namespace pt {

enum class Kind { Float, Choice, Bool };

struct Spec {
    std::string id, name;
    Kind kind;
    float min, max, def;  // Float: range + default. Choice: 0..N-1, def = index. Bool: 0/1.
    float step;           // Float: interval (0 = continuous)
    float skewCentre;     // Float: value that sits at the middle of the control (0 = linear)
    std::string unit;
    std::vector<std::string> choices;
};

const std::vector<Spec>& specs();
int indexOf(const std::string& id);  // -1 if unknown

float plainToNorm(const Spec&, float plain);
float normToPlain(const Spec&, float norm);
std::string format(const Spec&, float plain);  // value text incl. unit
float parse(const Spec&, const std::string& text);
int stepCount(const Spec&);  // 0 = continuous

// values[i] is the plain value of specs()[i]
Params toParams(const float* values);

struct PresetDef {
    std::string name, category, description;
    std::vector<std::pair<std::string, std::string>> values;  // param id -> number or choice label
};
const std::vector<PresetDef>& presets();

// Plain values for every parameter: defaults, then the preset's overrides.
std::vector<float> resolvePreset(const PresetDef&);

}  // namespace pt
}  // namespace rd
