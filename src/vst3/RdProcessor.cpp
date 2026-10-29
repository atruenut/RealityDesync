#include "RdProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "RdIds.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace rdv {

Processor::Processor() {
    setControllerClass(kControllerUID);
    engine_ = std::make_unique<rd::Engine>();
    const auto& t = rd::pt::specs();
    values_.resize(t.size());
    for (size_t i = 0; i < t.size(); ++i) values_[i] = t[i].def;
    snapIndex_ = rd::pt::indexOf("snapNow");
}

Processor::~Processor() = default;

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    tresult r = AudioEffect::initialize(context);
    if (r != kResultOk) return r;
    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) {
    if (numIns == 1 && numOuts == 1 && inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    return kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    sampleRate_ = setup.sampleRate;
    engine_->prepare(setup.sampleRate);
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
    if (state) {
        engine_->reset();
        snapSeen_ = false;
    }
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize) {
    return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

uint32 PLUGIN_API Processor::getLatencySamples() { return static_cast<uint32>(engine_->latencySamples()); }
uint32 PLUGIN_API Processor::getTailSamples() { return static_cast<uint32>(sampleRate_ * 14.0); }

void Processor::applyProgram(int program) {
    const auto& list = rd::pt::presets();
    if (program < 0 || program >= static_cast<int>(list.size())) return;
    program_ = program;
    const auto resolved = rd::pt::resolvePreset(list[static_cast<size_t>(program)]);
    for (size_t i = 0; i < values_.size(); ++i)
        if (static_cast<int>(i) != snapIndex_) values_[i] = resolved[i];
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
    const auto& specs = rd::pt::specs();

    // ---- parameter changes (last value in the block wins; the program change is applied first)
    if (data.inputParameterChanges) {
        const int32 n = data.inputParameterChanges->getParameterCount();
        for (int pass = 0; pass < 2; ++pass)
            for (int32 q = 0; q < n; ++q) {
                IParamValueQueue* queue = data.inputParameterChanges->getParameterData(q);
                if (!queue || queue->getPointCount() <= 0) continue;
                const ParamID id = queue->getParameterId();
                int32 offset = 0;
                ParamValue v = 0.0;
                if (queue->getPoint(queue->getPointCount() - 1, offset, v) != kResultTrue) continue;
                if (id == kProgramListId) {
                    if (pass == 0) {
                        const int count = static_cast<int>(rd::pt::presets().size());
                        applyProgram(static_cast<int>(std::lround(v * std::max(1, count - 1))));
                    }
                } else if (pass == 1 && id >= kParamBase && id < kParamBase + specs.size()) {
                    const size_t i = id - kParamBase;
                    values_[i] = rd::pt::normToPlain(specs[i], static_cast<float>(v));
                }
            }
    }

    // ---- "Snap Now" is a trigger: any change of the parameter fires one realignment
    if (snapIndex_ >= 0) {
        const float s = values_[static_cast<size_t>(snapIndex_)];
        if (!snapSeen_) {
            lastSnap_ = s;
            snapSeen_ = true;
        } else if (s != lastSnap_) {
            lastSnap_ = s;
            engine_->requestSnap();
        }
    }

    if (data.numSamples <= 0) return kResultOk;
    if (data.numInputs < 1 || data.numOutputs < 1 || data.inputs[0].numChannels < 2 || data.outputs[0].numChannels < 2) return kResultOk;

    // ---- transport
    rd::HostInfo host;
    if (data.processContext) {
        const ProcessContext& pc = *data.processContext;
        host.playing = (pc.state & ProcessContext::kPlaying) != 0;
        if (pc.state & ProcessContext::kTempoValid) {
            host.hasBpm = true;
            host.bpm = pc.tempo;
        }
        if (pc.state & ProcessContext::kProjectTimeMusicValid) {
            host.hasPpq = true;
            host.ppq = pc.projectTimeMusic;
        }
        if (pc.state & ProcessContext::kTimeSigValid) {
            host.tsNum = pc.timeSigNumerator;
            host.tsDen = pc.timeSigDenominator;
        }
        if (pc.state & ProcessContext::kBarPositionValid) {
            host.hasBarStart = true;
            host.barStartPpq = pc.barPositionMusic;
        }
    }

    // ---- audio (in place on the output buffers)
    float** in = data.inputs[0].channelBuffers32;
    float** out = data.outputs[0].channelBuffers32;
    for (int c = 0; c < 2; ++c)
        if (out[c] != in[c]) std::memcpy(out[c], in[c], sizeof(float) * static_cast<size_t>(data.numSamples));

    engine_->setParams(rd::pt::toParams(values_.data()));
    engine_->process(out[0], out[1], data.numSamples, host);
    data.outputs[0].silenceFlags = 0;
    return kResultOk;
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0, count = 0, program = 0;
    if (!s.readInt32(version) || version != kStateVersion) return kResultFalse;
    if (!s.readInt32(count)) return kResultFalse;
    for (int32 i = 0; i < count; ++i) {
        float f = 0.0f;
        if (!s.readFloat(f)) break;
        if (i < static_cast<int32>(values_.size())) values_[static_cast<size_t>(i)] = f;
    }
    if (s.readInt32(program)) program_ = program;
    snapSeen_ = false;  // restoring state must not count as a "Snap Now" press
    return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    s.writeInt32(kStateVersion);
    s.writeInt32(static_cast<int32>(values_.size()));
    for (float f : values_) s.writeFloat(f);
    s.writeInt32(program_);
    return kResultOk;
}

}  // namespace rdv
