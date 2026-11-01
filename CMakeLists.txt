cmake_minimum_required(VERSION 3.22)
project(AbbaKhaos VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(ABAKOS_DSP_SOURCES
    src/dsp/AbakosTypes.h
    src/dsp/SetupFileIO.h
    src/dsp/SetupFileIO.cpp
    src/dsp/Arpeggiator.h
    src/dsp/Arpeggiator.cpp
    src/dsp/AbakosSynthEngine.h
    src/dsp/AbakosSynthEngine.cpp
)

juce_add_plugin(AbbaKhaos
    COMPANY_NAME "GlitchyGloomy"
    COMPANY_WEBSITE "https://github.com/GlitchyGloomy/AbbaKhaos"
    COMPANY_EMAIL "info@glitchygloomy.com"
    BUNDLE_ID "com.glitchygloomy.abbakhaos"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE GlGl
    PLUGIN_CODE AbKh
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "AbbaKhaos"
)

target_sources(AbbaKhaos
    PRIVATE
        ${ABAKOS_DSP_SOURCES}
        src/PluginProcessor.h
        src/PluginProcessor.cpp
        src/PluginEditor.h
        src/PluginEditor.cpp
)

target_include_directories(AbbaKhaos
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(AbbaKhaos
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        ABAKOS_DEFAULT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/HERCs Abakos PRO"
)

target_link_libraries(AbbaKhaos
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Unit & Integration Test Runner
add_executable(AbbaKhaosTests
    tests/AbakosTests.cpp
)

target_include_directories(AbbaKhaosTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(AbbaKhaosTests
    PRIVATE
        ABAKOS_DEFAULT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/HERCs Abakos PRO"
)

target_link_libraries(AbbaKhaosTests
    PRIVATE
        AbbaKhaos
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
)
