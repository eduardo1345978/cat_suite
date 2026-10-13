cmake_minimum_required(VERSION 3.22)

# Allows configuring Orbit-Cat on its own:  cmake -S OrbitCat -B build-chordcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(OrbitCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(OrbitCat
    PRODUCT_NAME "Orbit-Cat"
    PLUGIN_CODE  Orc1
    DESCRIPTION  "Orbital physics arpeggiator"
    SOURCES
        Source/OrbitEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
