include_guard(GLOBAL)

# Cubase only offers Steinberg's own MIDI plug-ins in MIDI Insert slots. Third-party
# MIDI generators are loaded as instruments and their "MIDI Out" is routed to the
# next track. ON  = VST3 instrument with MIDI in/out and a silent stereo output (Cubase).
#                OFF = pure MIDI effect (isMidiEffect = true, no audio buses).
option(CAT_CUBASE_INSTRUMENT_MODE "Build as VST3 instrument with MIDI output (required for Cubase)" ON)
option(CAT_COPY_AFTER_BUILD "Copy the plug-ins to the system plug-in folder after building" OFF)
set(CAT_PLUGIN_FORMATS VST3 Standalone CACHE STRING "Plug-in formats to build")

function(cat_add_midi_plugin target)
    cmake_parse_arguments(ARG "" "PRODUCT_NAME;PLUGIN_CODE;DESCRIPTION" "SOURCES" ${ARGN})

    if(CAT_CUBASE_INSTRUMENT_MODE)
        set(_is_synth TRUE)
        set(_is_midi_fx FALSE)
        set(_vst3_categories Instrument Generator)
    else()
        set(_is_synth FALSE)
        set(_is_midi_fx TRUE)
        set(_vst3_categories Fx Generator)
    endif()

    juce_add_plugin(${target}
        VERSION                    1.0.0
        COMPANY_NAME               "Cat Suite"
        BUNDLE_ID                  "com.catsuite.${target}"
        PLUGIN_MANUFACTURER_CODE   CatS
        PLUGIN_CODE                ${ARG_PLUGIN_CODE}
        PRODUCT_NAME               "${ARG_PRODUCT_NAME}"
        DESCRIPTION                "${ARG_DESCRIPTION}"
        FORMATS                    ${CAT_PLUGIN_FORMATS}
        IS_SYNTH                   ${_is_synth}
        IS_MIDI_EFFECT             ${_is_midi_fx}
        NEEDS_MIDI_INPUT           TRUE
        NEEDS_MIDI_OUTPUT          TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES            ${_vst3_categories}
        COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

    target_sources(${target} PRIVATE ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)   # sources are UTF-8 (Slovak / Japanese text)
    endif()

    target_compile_definitions(${target}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            # Keeps JUCE's IMidiMapping implementation: Cubase sends CCs / Quick Controls
            # through it and they arrive back in processBlock as MIDI CC (used by MIDI Learn).
            JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=1
            CAT_INSTRUMENT_MODE=$<BOOL:${CAT_CUBASE_INSTRUMENT_MODE}>)

    target_link_libraries(${target}
        PRIVATE
            cat_shared
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

# Sing-Cat makes sound: a real VST3 instrument (MIDI in, stereo audio out, no MIDI out).
function(cat_add_synth_plugin target)
    cmake_parse_arguments(ARG "" "PRODUCT_NAME;PLUGIN_CODE;DESCRIPTION" "SOURCES" ${ARGN})

    juce_add_plugin(${target}
        VERSION                    1.0.0
        COMPANY_NAME               "Cat Suite"
        BUNDLE_ID                  "com.catsuite.${target}"
        PLUGIN_MANUFACTURER_CODE   CatS
        PLUGIN_CODE                ${ARG_PLUGIN_CODE}
        PRODUCT_NAME               "${ARG_PRODUCT_NAME}"
        DESCRIPTION                "${ARG_DESCRIPTION}"
        FORMATS                    ${CAT_PLUGIN_FORMATS}
        IS_SYNTH                   TRUE
        IS_MIDI_EFFECT             FALSE
        NEEDS_MIDI_INPUT           TRUE
        NEEDS_MIDI_OUTPUT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES            Instrument Synth
        COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

    target_sources(${target} PRIVATE ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)   # sources are UTF-8 (Slovak / Japanese text)
    endif()

    target_compile_definitions(${target}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=1
            CAT_INSTRUMENT_MODE=1)

    target_link_libraries(${target}
        PRIVATE
            cat_shared
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
