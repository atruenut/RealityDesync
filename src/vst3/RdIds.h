// Class IDs and parameter numbering for the JUCE-free VST3 build of Reality Desync.
#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace rdv {

static const Steinberg::FUID kProcessorUID(0x5E1A17D1, 0x9C3B4A27, 0xB6D0E4F1, 0x2A7C3D55);
static const Steinberg::FUID kControllerUID(0x8B2F6C04, 0x3D1E4F9A, 0xA1C7D2E8, 0x55B90F13);

constexpr Steinberg::Vst::ParamID kParamBase = 1000;       // spec index i  ->  parameter id 1000 + i
constexpr Steinberg::Vst::ProgramListID kProgramListId = 100;  // doubles as the program-change parameter id
constexpr Steinberg::int32 kStateVersion = 1;

#define RD_VERSION_STRING "1.0.0"

}  // namespace rdv
