cmake_minimum_required(VERSION 3.22)
project(WeztSuite VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(COPY_PLUGIN_AFTER_BUILD "Install VST3 to system folder after build" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# One VST3 per call. cat is a CMake list, e.g. "Fx" or "Instrument;Synth".
function(wezt_plugin target product code isSynth cat)
    juce_add_plugin(${target}
        COMPANY_NAME "Wezt"
        PLUGIN_MANUFACTURER_CODE Wezt
        PLUGIN_CODE ${code}
        FORMATS VST3
        PRODUCT_NAME "${product}"
        IS_SYNTH ${isSynth}
        NEEDS_MIDI_INPUT ${isSynth}
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD}
        VST3_CATEGORIES ${cat})
    target_sources(${target} PRIVATE ${ARGN})
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

# --- Batch 0 (original, own custom editor)
wezt_plugin(WeztOptoComp "Wezt Opto Comp" Wopc FALSE "Fx;Dynamics"
    Source/PluginProcessor.cpp Source/PluginEditor.cpp)

# --- Batch 1
wezt_plugin(WeztVariMu  "Wezt Vari-Mu"  Wvmu FALSE "Fx;Dynamics"          Source/Plugins/VariMu.cpp)
wezt_plugin(WeztTubeEQ  "Wezt Tube EQ"  Wteq FALSE "Fx;EQ"                Source/Plugins/TubeEQ.cpp)
wezt_plugin(WeztChorus  "Wezt Chorus"   Wchr FALSE "Fx;Modulation"        Source/Plugins/Chorus.cpp)
wezt_plugin(WeztPhaser  "Wezt Phaser"   Wphs FALSE "Fx;Modulation"        Source/Plugins/Phaser.cpp)
wezt_plugin(WeztVacuum  "Wezt Vacuum"   Wvac TRUE  "Instrument;Synth"     Source/Plugins/Vacuum.cpp)
