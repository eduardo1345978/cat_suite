cmake_minimum_required(VERSION 3.22)

# Allows configuring Bass-Cat on its own:  cmake -S BassCat -B build-basscat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BassCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(BassCat
    PRODUCT_NAME "Bass-Cat"
    PLUGIN_CODE  Bss1
    DESCRIPTION  "Bass lines that follow your chords"
    SOURCES
        Source/BassEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
