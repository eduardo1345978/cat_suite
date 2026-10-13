cmake_minimum_required(VERSION 3.22)

# Allows configuring Chord-Cat on its own:  cmake -S ChordCat -B build-chordcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ChordCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(ChordCat
    PRODUCT_NAME "Chord-Cat"
    PLUGIN_CODE  Chc1
    DESCRIPTION  "One-finger chord progression engine"
    SOURCES
        Source/ChordFormulas.cpp
        Source/Scales.cpp
        Source/ChordEngine.cpp
        Source/ProgressionGenerator.cpp
        Source/PatternPlayer.cpp
        Source/FactoryPresets.cpp
        Source/StrumEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
