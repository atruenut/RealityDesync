cmake_minimum_required(VERSION 3.22)
project(RealityDesync VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------------------------
# JUCE: either point JUCE_DIR at a checkout, or let CMake find an installed JUCE package.
#   cmake -B build -DJUCE_DIR=/path/to/JUCE          (a JUCE source checkout)
#   cmake -B build -DCMAKE_PREFIX_PATH=/path/prefix  (an installed JUCE package)
# If neither is given, JUCE 8 is downloaded with FetchContent.
# ---------------------------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Path to a JUCE source checkout")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" juce EXCLUDE_FROM_ALL)
else()
    find_package(JUCE CONFIG QUIET)
    if(NOT JUCE_FOUND)
        include(FetchContent)
        FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW ON)
        FetchContent_MakeAvailable(juce)
    endif()
endif()

# ---------------------------------------------------------------------------------------------
# Portable DSP core. No JUCE, no OS calls: it can be reused in any other plugin format.
# ---------------------------------------------------------------------------------------------
add_library(rd_core STATIC src/core/Engine.cpp src/core/ParamTable.cpp)
target_include_directories(rd_core PUBLIC src/core)
set_target_properties(rd_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(rd_core PRIVATE /O2 /fp:fast)
else()
    target_compile_options(rd_core PRIVATE -O3 -ffp-contract=fast)
endif()

# ---------------------------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------------------------
set(RD_FORMATS VST3)
option(RD_BUILD_STANDALONE "Also build a standalone application" OFF)
if(RD_BUILD_STANDALONE)
    list(APPEND RD_FORMATS Standalone)
endif()

juce_add_plugin(RealityDesync
    COMPANY_NAME "Groundwork NB"
    PLUGIN_MANUFACTURER_CODE Gwnb
    PLUGIN_CODE Rdsy
    FORMATS ${RD_FORMATS}
    PRODUCT_NAME "Reality Desync"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Modulation Delay
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(RealityDesync PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/Presets.cpp)

target_compile_definitions(RealityDesync PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(RealityDesync
    PRIVATE
        rd_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(MINGW)
    # Self-contained DLL: no libstdc++/libgcc/winpthread runtime DLLs needed on the user's machine.
    target_link_options(RealityDesync PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------------------------
# Tests (DSP only; no JUCE needed)
# ---------------------------------------------------------------------------------------------
add_executable(rd_dsp_tests tests/dsp_tests.cpp)
target_link_libraries(rd_dsp_tests PRIVATE rd_core)
enable_testing()
add_test(NAME dsp COMMAND rd_dsp_tests)

# ---------------------------------------------------------------------------------------------
# Headless plugin check (loads every preset, renders audio, snapshots the GUI)
# ---------------------------------------------------------------------------------------------
option(RD_BUILD_CHECK "Build the headless plugin check tool" OFF)
if(RD_BUILD_CHECK)
    juce_add_console_app(RDPluginCheck PRODUCT_NAME "RDPluginCheck")
    target_sources(RDPluginCheck PRIVATE
        tests/plugin_check.cpp
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/Presets.cpp)
    target_compile_definitions(RDPluginCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(RDPluginCheck PRIVATE rd_core juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
