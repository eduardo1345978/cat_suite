cmake_minimum_required(VERSION 3.22)

# Allows configuring Bend-Cat on its own:  cmake -S BendCat -B build-bendcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BendCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(BendCat
    PRODUCT_NAME "Bend-Cat"
    PLUGIN_CODE  Bnc1
    DESCRIPTION  "MPE pitch-bend expression engine"
    SOURCES
        Source/BendCatEngine.cpp
        Source/MPEAllocator.cpp
        Source/ScalaParser.cpp
        Source/EnvelopeEditorUI.cpp
        Source/BendVisualizer.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
