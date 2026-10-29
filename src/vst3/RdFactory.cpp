#include "RdController.h"
#include "RdIds.h"
#include "RdProcessor.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

bool InitModule() { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF("Groundwork NB", "", "")

DEF_CLASS2(INLINE_UID_FROM_FUID(rdv::kProcessorUID), PClassInfo::kManyInstances, kVstAudioEffectClass, "Reality Desync", kDistributable,
           PlugType::kFxDelay, RD_VERSION_STRING, kVstVersionString, rdv::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(rdv::kControllerUID), PClassInfo::kManyInstances, kVstComponentControllerClass, "Reality Desync Controller", 0, "",
           RD_VERSION_STRING, kVstVersionString, rdv::Controller::createInstance)

END_FACTORY
