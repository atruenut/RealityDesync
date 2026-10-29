# JUCE-free build of Reality Desync on the Steinberg VST3 SDK (MIT licensed).
# Builds with MinGW-w64 gcc on Windows, MSVC, clang, and gcc on Linux/macOS.
#   cmake -S vst3-plain -B build-plain -DVST3_SDK_DIR=/path/to/vst3sdk
cmake_minimum_required(VERSION 3.15)
project(RealityDesyncPlain VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(VST3_SDK_DIR "" CACHE PATH "Path to the VST3 SDK (the folder containing base/, pluginterfaces/, public.sdk/)")
if(NOT EXISTS "${VST3_SDK_DIR}/public.sdk")
    message(FATAL_ERROR "Set -DVST3_SDK_DIR=<path to vst3sdk>  (git clone --recurse-submodules https://github.com/steinbergmedia/vst3sdk)")
endif()
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(SDK_SOURCES
    ${VST3_SDK_DIR}/base/source/baseiids.cpp
    ${VST3_SDK_DIR}/base/source/fbuffer.cpp
    ${VST3_SDK_DIR}/base/source/fdebug.cpp
    ${VST3_SDK_DIR}/base/source/fobject.cpp
    ${VST3_SDK_DIR}/base/source/fstreamer.cpp
    ${VST3_SDK_DIR}/base/source/fstring.cpp
    ${VST3_SDK_DIR}/base/source/timer.cpp
    ${VST3_SDK_DIR}/base/source/updatehandler.cpp
    ${VST3_SDK_DIR}/base/thread/source/fcondition.cpp
    ${VST3_SDK_DIR}/base/thread/source/flock.cpp
    ${VST3_SDK_DIR}/pluginterfaces/base/conststringtable.cpp
    ${VST3_SDK_DIR}/pluginterfaces/base/coreiids.cpp
    ${VST3_SDK_DIR}/pluginterfaces/base/funknown.cpp
    ${VST3_SDK_DIR}/pluginterfaces/base/ustring.cpp
    ${VST3_SDK_DIR}/public.sdk/source/common/commoniids.cpp
    ${VST3_SDK_DIR}/public.sdk/source/common/pluginview.cpp
    ${VST3_SDK_DIR}/public.sdk/source/main/pluginfactory.cpp
    ${VST3_SDK_DIR}/public.sdk/source/vst/vstaudioeffect.cpp
    ${VST3_SDK_DIR}/public.sdk/source/vst/vstbus.cpp
    ${VST3_SDK_DIR}/public.sdk/source/vst/vstcomponent.cpp
    ${VST3_SDK_DIR}/public.sdk/source/vst/vstcomponentbase.cpp
    ${VST3_SDK_DIR}/public.sdk/source/vst/vsteditcontroller.cpp
    ${VST3_SDK_DIR}/public.sdk/source/vst/vstinitiids.cpp
    ${VST3_SDK_DIR}/public.sdk/source/vst/vstparameters.cpp)
if(WIN32)
    list(APPEND SDK_SOURCES ${VST3_SDK_DIR}/public.sdk/source/main/dllmain.cpp)
elseif(APPLE)
    list(APPEND SDK_SOURCES ${VST3_SDK_DIR}/public.sdk/source/main/macmain.cpp)
else()
    list(APPEND SDK_SOURCES ${VST3_SDK_DIR}/public.sdk/source/main/linuxmain.cpp)
endif()

add_library(RealityDesync MODULE
    ${SDK_SOURCES}
    ${ROOT}/src/core/Engine.cpp
    ${ROOT}/src/core/ParamTable.cpp
    ${ROOT}/src/vst3/RdProcessor.cpp
    ${ROOT}/src/vst3/RdController.cpp
    ${ROOT}/src/vst3/RdFactory.cpp)
target_include_directories(RealityDesync PRIVATE ${VST3_SDK_DIR} ${ROOT}/src/core)
target_compile_definitions(RealityDesync PRIVATE RELEASE=1)
set_target_properties(RealityDesync PROPERTIES PREFIX "" SUFFIX ".vst3" OUTPUT_NAME "Reality Desync")
if(MSVC)
    target_compile_options(RealityDesync PRIVATE /O2 /fp:fast)
else()
    target_compile_options(RealityDesync PRIVATE -O3 -fvisibility=hidden)
endif()
if(MINGW)
    # one self-contained DLL: no MinGW runtime DLLs needed on the user's machine
    target_link_options(RealityDesync PRIVATE -static -static-libgcc -static-libstdc++)
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(RealityDesync PRIVATE dl pthread)
endif()
