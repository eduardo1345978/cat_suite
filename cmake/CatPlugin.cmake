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
# Every plug-in also as "<Name> Pro" (target <Target>Pro): the same sound, a plain panel with every parameter,
# exact numbers, meters, CPU and A/B (Shared/Source/ProEditor.h). VST3 only.
option(CAT_BUILD_PRO "Also build the Pro version of every plug-in" ON)

# The targets of a plug-in: itself, and its Pro version when that is built (for per-plug-in extras in CMakeLists).
function(cat_plugin_targets out_var target)
    set(_t ${target})
    if(TARGET ${target}Pro)
        list(APPEND _t ${target}Pro)
    endif()
    set(${out_var} ${_t} PARENT_SCOPE)
endfunction()

# Name, code, formats and CAT_PRO for one variant (the plug-in, or its Pro version).
macro(_cat_variant base variant)
    if("${variant}" STREQUAL "${base}")
        set(_name "${ARG_PRODUCT_NAME}")
        set(_code ${ARG_PLUGIN_CODE})
        set(_pro 0)
    else()
        set(_name "${ARG_PRODUCT_NAME} Pro")
        string(SUBSTRING "${ARG_PLUGIN_CODE}" 0 3 _code)
        set(_code "${_code}p")
        set(_formats VST3)
        set(_pro 1)
    endif()
endmacro()

macro(_cat_variants out base)
    set(${out} ${base})
    if(CAT_BUILD_PRO)
        list(APPEND ${out} ${base}Pro)
    endif()
endmacro()

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

    _cat_variants(_variants ${target})
    set(_base_formats ${_formats})
    foreach(_t IN LISTS _variants)
        set(_formats ${_base_formats})
        _cat_variant(${target} ${_t})
        set(_fmt ${_formats})
        juce_add_plugin(${_t}
            VERSION                    1.0.0
            COMPANY_NAME               "Cat Suite"
            BUNDLE_ID                  "com.catsuite.${_t}"
            PLUGIN_MANUFACTURER_CODE   CatS
            PLUGIN_CODE                ${_code}
            PRODUCT_NAME               "${_name}"
            DESCRIPTION                "${ARG_DESCRIPTION}"
            FORMATS                    ${_fmt}
            IS_SYNTH                   ${_is_synth}
            IS_MIDI_EFFECT             ${_is_midi_fx}
            NEEDS_MIDI_INPUT           TRUE
            NEEDS_MIDI_OUTPUT          TRUE
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            VST3_CATEGORIES            ${_vst3_categories}
            COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

        target_sources(${_t} PRIVATE ${ARG_SOURCES})
        target_include_directories(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
        target_compile_features(${_t} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(${_t} PRIVATE /utf-8)   # sources are UTF-8 (Slovak / Japanese text)
        endif()

        target_compile_definitions(${_t}
            PUBLIC
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0
                JUCE_DISPLAY_SPLASH_SCREEN=0
                CAT_PRO=${_pro}
                # Keeps JUCE's IMidiMapping implementation: Cubase sends CCs / Quick Controls
                # through it and they arrive back in processBlock as MIDI CC (used by MIDI Learn).
                JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=1
                CAT_INSTRUMENT_MODE=$<BOOL:${CAT_CUBASE_INSTRUMENT_MODE}>)

        target_link_libraries(${_t}
            PRIVATE
                cat_shared
                juce::juce_audio_utils
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)
    endforeach()
endfunction()

# Sing-Cat makes sound: a real VST3 instrument (MIDI in, stereo audio out, no MIDI out).
function(cat_add_synth_plugin target)
    cmake_parse_arguments(ARG "" "PRODUCT_NAME;PLUGIN_CODE;DESCRIPTION" "SOURCES" ${ARGN})
    _cat_formats(_formats TRUE)

    _cat_variants(_variants ${target})
    set(_base_formats ${_formats})
    foreach(_t IN LISTS _variants)
        set(_formats ${_base_formats})
        _cat_variant(${target} ${_t})
        set(_fmt ${_formats})
        juce_add_plugin(${_t}
            VERSION                    1.0.0
            COMPANY_NAME               "Cat Suite"
            BUNDLE_ID                  "com.catsuite.${_t}"
            PLUGIN_MANUFACTURER_CODE   CatS
            PLUGIN_CODE                ${_code}
            PRODUCT_NAME               "${_name}"
            DESCRIPTION                "${ARG_DESCRIPTION}"
            FORMATS                    ${_fmt}
            IS_SYNTH                   TRUE
            IS_MIDI_EFFECT             FALSE
            NEEDS_MIDI_INPUT           TRUE
            NEEDS_MIDI_OUTPUT          FALSE
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            VST3_CATEGORIES            Instrument Synth
            COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

        target_sources(${_t} PRIVATE ${ARG_SOURCES})
        target_include_directories(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
        target_compile_features(${_t} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(${_t} PRIVATE /utf-8)   # sources are UTF-8 (Slovak / Japanese text)
        endif()

        target_compile_definitions(${_t}
            PUBLIC
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0
                JUCE_DISPLAY_SPLASH_SCREEN=0
                CAT_PRO=${_pro}
                JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=1
                CAT_INSTRUMENT_MODE=1)

        target_link_libraries(${_t}
            PRIVATE
                cat_shared
                juce::juce_audio_utils
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)
    endforeach()
endfunction()

# An audio effect (Echo-Cat): audio in -> audio out, no MIDI.
# MIDI_OUT: the effect also sends MIDI (Hum-Cat: the notes it heard).
function(cat_add_fx_plugin target)
    cmake_parse_arguments(ARG "MIDI_OUT" "PRODUCT_NAME;PLUGIN_CODE;DESCRIPTION;CATEGORIES" "SOURCES" ${ARGN})
    _cat_formats(_formats FALSE)
    if(ARG_MIDI_OUT)
        set(_midi_out TRUE)
        set(_cats Fx Analyzer)
    else()
        set(_midi_out FALSE)
        set(_cats Fx Delay Reverb)
    endif()
    if(ARG_CATEGORIES)
        set(_cats ${ARG_CATEGORIES})
    endif()

    _cat_variants(_variants ${target})
    set(_base_formats ${_formats})
    foreach(_t IN LISTS _variants)
        set(_formats ${_base_formats})
        _cat_variant(${target} ${_t})
        set(_fmt ${_formats})
        juce_add_plugin(${_t}
            VERSION                    1.0.0
            COMPANY_NAME               "Cat Suite"
            BUNDLE_ID                  "com.catsuite.${_t}"
            PLUGIN_MANUFACTURER_CODE   CatS
            PLUGIN_CODE                ${_code}
            PRODUCT_NAME               "${_name}"
            DESCRIPTION                "${ARG_DESCRIPTION}"
            FORMATS                    ${_fmt}
            IS_SYNTH                   FALSE
            IS_MIDI_EFFECT             FALSE
            NEEDS_MIDI_INPUT           FALSE
            NEEDS_MIDI_OUTPUT          ${_midi_out}
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            VST3_CATEGORIES            ${_cats}
            COPY_PLUGIN_AFTER_BUILD    ${CAT_COPY_AFTER_BUILD})

        target_sources(${_t} PRIVATE ${ARG_SOURCES})
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
                CAT_PRO=${_pro}
                CAT_INSTRUMENT_MODE=0)

        target_link_libraries(${_t}
            PRIVATE
                cat_shared
                juce::juce_audio_utils
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)
    endforeach()
endfunction()
