cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Negative: a photo becomes sound (and its spectrogram shows the photo). An effect with MIDI in (play the photo on
# notes, or in time with Cubase, or hide it in the track), and a standalone program (open photos and sounds, draw,
# save .wav / pictures).
juce_add_plugin(Negative
    VERSION                    1.0.0
    COMPANY_NAME               "Cat Suite"
    BUNDLE_ID                  "com.catsuite.Negative"
    PLUGIN_MANUFACTURER_CODE   CatS
    PLUGIN_CODE                Ngt1
    PRODUCT_NAME               "Negative"
    DESCRIPTION                "A photo you can hear: photo to spectrogram to sound"
    FORMATS                    VST3 Standalone
    IS_SYNTH                   FALSE
    IS_MIDI_EFFECT             FALSE
    NEEDS_MIDI_INPUT           TRUE
    NEEDS_MIDI_OUTPUT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES            Fx Generator
    COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

target_sources(Negative PRIVATE Source/NegativeCore.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Negative PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_features(Negative PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(Negative PRIVATE /utf-8)
endif()
target_compile_definitions(Negative
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        CAT_INSTRUMENT_MODE=0)
target_link_libraries(Negative
    PRIVATE
        cat_shared
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
