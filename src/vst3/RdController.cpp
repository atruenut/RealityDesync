#include "RdController.h"

#include <algorithm>
#include <cmath>

#include "../core/ParamTable.h"
#include "RdIds.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstcomponent.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace rdv {

namespace {

void asciiToString128(const std::string& s, String128 out) {
    UString128 u;
    u.fromAscii(s.c_str());
    u.copyTo(out, 128);
}

// One parameter of the shared table. Presents values exactly like the JUCE build does.
class RdParameter : public Parameter {
public:
    RdParameter(const rd::pt::Spec& s, int index) : spec_(s) {
        asciiToString128(s.name, info.title);
        asciiToString128(s.name, info.shortTitle);
        info.units[0] = 0;  // the value text already carries its unit
        info.id = kParamBase + static_cast<ParamID>(index);
        info.unitId = kRootUnitId;
        info.stepCount = rd::pt::stepCount(s);
        info.defaultNormalizedValue = rd::pt::plainToNorm(s, s.def);
        info.flags = ParameterInfo::kCanAutomate;
        if (s.kind == rd::pt::Kind::Choice) info.flags |= ParameterInfo::kIsList;
        valueNormalized = info.defaultNormalizedValue;
    }
    void toString(ParamValue norm, String128 string) const SMTG_OVERRIDE {
        asciiToString128(rd::pt::format(spec_, rd::pt::normToPlain(spec_, static_cast<float>(norm))), string);
    }
    bool fromString(const TChar* string, ParamValue& norm) const SMTG_OVERRIDE {
        char buf[128];
        UString128 u(string, 128);
        u.toAscii(buf, 128);
        norm = rd::pt::plainToNorm(spec_, rd::pt::parse(spec_, buf));
        return true;
    }
    ParamValue toPlain(ParamValue norm) const SMTG_OVERRIDE { return rd::pt::normToPlain(spec_, static_cast<float>(norm)); }
    ParamValue toNormalized(ParamValue plain) const SMTG_OVERRIDE { return rd::pt::plainToNorm(spec_, static_cast<float>(plain)); }

private:
    const rd::pt::Spec& spec_;
};

}  // namespace

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
    tresult r = EditControllerEx1::initialize(context);
    if (r != kResultOk) return r;

    // Presets appear in the host's own preset menu through a program list.
    addUnit(new Unit(STR16("Reality Desync"), kRootUnitId, kNoParentUnitId, kProgramListId));
    auto* list = new ProgramList(STR16("Presets"), kProgramListId, kRootUnitId);
    for (const auto& p : rd::pt::presets()) {
        String128 name;
        asciiToString128(p.name, name);
        list->addProgram(name);
    }
    addProgramList(list);
    parameters.addParameter(list->getParameter());

    const auto& t = rd::pt::specs();
    for (size_t i = 0; i < t.size(); ++i) parameters.addParameter(new RdParameter(t[i], static_cast<int>(i)));
    return kResultOk;
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0, count = 0, program = 0;
    if (!s.readInt32(version) || version != kStateVersion) return kResultFalse;
    if (!s.readInt32(count)) return kResultFalse;
    const auto& t = rd::pt::specs();
    for (int32 i = 0; i < count; ++i) {
        float f = 0.0f;
        if (!s.readFloat(f)) break;
        if (i < static_cast<int32>(t.size()))
            EditControllerEx1::setParamNormalized(kParamBase + static_cast<ParamID>(i), rd::pt::plainToNorm(t[static_cast<size_t>(i)], f));
    }
    if (s.readInt32(program)) {
        const int n = static_cast<int>(rd::pt::presets().size());
        EditControllerEx1::setParamNormalized(kProgramListId, n > 1 ? static_cast<double>(std::min(std::max(program, 0), n - 1)) / (n - 1) : 0.0);
    }
    return kResultOk;
}

tresult PLUGIN_API Controller::setParamNormalized(ParamID tag, ParamValue value) {
    const tresult r = EditControllerEx1::setParamNormalized(tag, value);
    if (tag == kProgramListId && !applyingProgram_) {
        // Choosing a preset moves every other parameter too: mirror what the processor does, then tell the host.
        applyingProgram_ = true;
        const auto& list = rd::pt::presets();
        const int n = static_cast<int>(list.size());
        const int program = std::min(std::max(static_cast<int>(std::lround(value * std::max(1, n - 1))), 0), n - 1);
        const auto resolved = rd::pt::resolvePreset(list[static_cast<size_t>(program)]);
        const auto& t = rd::pt::specs();
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i].id == "snapNow") continue;
            EditControllerEx1::setParamNormalized(kParamBase + static_cast<ParamID>(i), rd::pt::plainToNorm(t[i], resolved[i]));
        }
        if (componentHandler) componentHandler->restartComponent(kParamValuesChanged);
        applyingProgram_ = false;
    }
    return r;
}

}  // namespace rdv
