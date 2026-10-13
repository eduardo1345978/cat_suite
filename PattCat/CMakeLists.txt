cmake_minimum_required(VERSION 3.22)

# Allows configuring Patt-Cat on its own:  cmake -S PattCat -B build-pattcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(PattCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(PattCat
    PRODUCT_NAME "Patt-Cat"
    PLUGIN_CODE  Ptc1
    DESCRIPTION  "4-layer polyrhythmic chord pattern sequencer"
    SOURCES
        Source/EuclideanGenerator.cpp
        Source/PattCatEngine.cpp
        Source/PatternGridUI.cpp
        Source/PluginProcessor.cpp
        Source/BounceView.cpp
        Source/PluginEditor.cpp)
