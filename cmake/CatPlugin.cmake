include_guard(GLOBAL)

# Cubase only offers Steinberg's own MIDI plug-ins in MIDI Insert slots. Third-party
# MIDI generators are loaded as instruments and their "MIDI Out" is routed to the
# next track. ON  = VST3 instrument with MIDI in/out and a silent stereo output (Cubase).
#                OFF = pure MIDI effect (isMidiEffect = true, no audio buses).
option(CAT_CUBASE_INSTRUMENT_MODE "Build as VST3 instrument with MIDI output (required for Cubase)" ON)
option(CAT_COPY_AFTER_BUILD "Copy the plug-ins to the system plug-in folder after building" OFF)
set(CAT_PLUGIN_FORMATS VST3 Standalone CACHE STRING "Plug-in formats to build")
# The plug-ins that make sound also become standalone apps (play them with the computer keyboard).
option(CAT_SOUND_STANDALONE "Always build the sound-making plug-ins as standalone apps too" ON)

function(_cat_formats out_var makes_sound)
    set(_f ${CAT_PLUGIN_FORMATS})
    if(makes_sound AND CAT_SOUND_STANDALONE AND NOT "Standalone" IN_LIST _f)
        list(APPEND _f Standalone)
    endif()
    set(${out_var} ${_f} PARENT_SCOPE)
endfunction()

# SOUND: the plug-in also makes its own sound (Life-Cat) -> gets a standalone app.
function(cat_add_midi_plugin target)
    cmake_parse_arguments(ARG "SOUND" "PRODUCT_NAME;PLUGIN_CODE;DESCRIPTION" "SOURCES" ${ARGN})
    _cat_formats(_formats ${ARG_SOUND})

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
        FORMATS                    ${_formats}
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
    _cat_formats(_formats TRUE)

    juce_add_plugin(${target}
        VERSION                    1.0.0
        COMPANY_NAME               "Cat Suite"
        BUNDLE_ID                  "com.catsuite.${target}"
        PLUGIN_MANUFACTURER_CODE   CatS
        PLUGIN_CODE                ${ARG_PLUGIN_CODE}
        PRODUCT_NAME               "${ARG_PRODUCT_NAME}"
        DESCRIPTION                "${ARG_DESCRIPTION}"
        FORMATS                    ${_formats}
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

# An audio effect (Echo-Cat): audio in -> audio out, no MIDI.
function(cat_add_fx_plugin target)
    cmake_parse_arguments(ARG "" "PRODUCT_NAME;PLUGIN_CODE;DESCRIPTION;CATEGORIES" "SOURCES" ${ARGN})
    _cat_formats(_formats FALSE)

    juce_add_plugin(${target}
        VERSION                    1.0.0
        COMPANY_NAME               "Cat Suite"
        BUNDLE_ID                  "com.catsuite.${target}"
        PLUGIN_MANUFACTURER_CODE   CatS
        PLUGIN_CODE                ${ARG_PLUGIN_CODE}
        PRODUCT_NAME               "${ARG_PRODUCT_NAME}"
        DESCRIPTION                "${ARG_DESCRIPTION}"
        FORMATS                    ${_formats}
        IS_SYNTH                   FALSE
        IS_MIDI_EFFECT             FALSE
        NEEDS_MIDI_INPUT           FALSE
        NEEDS_MIDI_OUTPUT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES            Fx Delay Reverb
        COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

    target_sources(${target} PRIVATE ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()

    target_compile_definitions(${target}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            CAT_INSTRUMENT_MODE=0)

    target_link_libraries(${target}
        PRIVATE
            cat_shared
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
