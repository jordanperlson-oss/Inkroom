cmake_minimum_required(VERSION 3.22)
project(Inkroom VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(NOT DEFINED INKROOM_FORMATS)
    set(INKROOM_FORMATS AU VST3 Standalone)
endif()

juce_add_plugin(Inkroom
    COMPANY_NAME "Imagine Plugins"
    BUNDLE_ID com.imagineplugins.inkroom
    PLUGIN_MANUFACTURER_CODE Imgn
    PLUGIN_CODE Inkr
    PRODUCT_NAME "Inkroom"
    FORMATS ${INKROOM_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Reverb)

target_sources(Inkroom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Inkroom PRIVATE Source)

target_compile_definitions(Inkroom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Only used for the headless Linux compile check, never for the Mac build.
if(INKROOM_LINUX_CHECK)
    target_compile_definitions(Inkroom PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(Inkroom
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline check: renders audio through the processor and snapshots the editor. Not part of the plugin.
option(INKROOM_TESTS "Build the offline check tool" OFF)
if(INKROOM_TESTS)
    juce_add_console_app(InkroomCheck PRODUCT_NAME "InkroomCheck")
    target_sources(InkroomCheck PRIVATE
        tests/InkroomCheck.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(InkroomCheck PRIVATE Source)
    target_compile_definitions(InkroomCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0)
    target_link_libraries(InkroomCheck PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
if(INKROOM_TESTS)
    juce_add_console_app(InkroomMeasure PRODUCT_NAME "InkroomMeasure")
    target_sources(InkroomMeasure PRIVATE tests/Measure.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(InkroomMeasure PRIVATE Source)
    target_compile_definitions(InkroomMeasure PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0)
    target_link_libraries(InkroomMeasure PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
