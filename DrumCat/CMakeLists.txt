cmake_minimum_required(VERSION 3.22)

# Allows configuring Drum-Cat on its own:  cmake -S DrumCat -B build-chordcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DrumCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(DrumCat
    PRODUCT_NAME "Drum-Cat"
    PLUGIN_CODE  Drc1
    DESCRIPTION  "Hip-hop drum pattern generator"
    SOURCES
        Source/DrumCatEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
