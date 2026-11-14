cmake_minimum_required(VERSION 3.22)

# Allows configuring Hum-Cat on its own:  cmake -S HumCat -B build-humcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(HumCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_fx_plugin(HumCat
    MIDI_OUT
    PRODUCT_NAME "Hum-Cat"
    PLUGIN_CODE  Hum1
    DESCRIPTION  "Sing or hum - get the melody, chords, bass and drums as MIDI"
    SOURCES
        Source/HumAnalysis.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/Source/CatSounds.cpp)

# mp3 recordings can be dropped in too
cat_plugin_targets(_targets HumCat)
foreach(_t IN LISTS _targets)
    target_compile_definitions(${_t} PRIVATE JUCE_USE_MP3AUDIOFORMAT=1)
endforeach()
