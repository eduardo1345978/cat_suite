cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Negative: a photo becomes sound (and its spectrogram shows the photo). A VST3 instrument (play the photo on MIDI
# notes, or in time with Cubase) and a standalone program (open photos and sounds, draw, save .wav / pictures).
set(_variants Negative)
if(CAT_BUILD_PRO)
    list(APPEND _variants NegativePro)
endif()
foreach(_t IN LISTS _variants)
    if(_t STREQUAL "Negative")
        set(_name "Negative")
        set(_code Ngt1)
        set(_fmt VST3 Standalone)
        set(_pro 0)
    else()
        set(_name "Negative Pro")
        set(_code Ngtp)
        set(_fmt VST3)
        set(_pro 1)
    endif()
    juce_add_plugin(${_t}
        VERSION                    1.0.0
        COMPANY_NAME               "Cat Suite"
        BUNDLE_ID                  "com.catsuite.${_t}"
        PLUGIN_MANUFACTURER_CODE   CatS
        PLUGIN_CODE                ${_code}
        PRODUCT_NAME               "${_name}"
        DESCRIPTION                "A photo you can hear: photo to spectrogram to sound"
        FORMATS                    ${_fmt}
        IS_SYNTH                   TRUE
        IS_MIDI_EFFECT             FALSE
        NEEDS_MIDI_INPUT           TRUE
        NEEDS_MIDI_OUTPUT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES            Instrument Synth
        COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

    target_sources(${_t} PRIVATE Source/NegativeCore.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_features(${_t} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${_t} PRIVATE /utf-8)
    endif()
    target_compile_definitions(${_t}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            CAT_INSTRUMENT_MODE=1
            CAT_PRO=${_pro})
    target_link_libraries(${_t}
        PRIVATE
            cat_shared
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endforeach()
