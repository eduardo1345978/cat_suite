cmake_minimum_required(VERSION 3.22)

# Allows configuring Glide-Cat on its own:  cmake -S GlideCat -B build-basscat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(GlideCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_synth_plugin(GlideCat
    PRODUCT_NAME "Glide-Cat"
    PLUGIN_CODE  Gld1
    DESCRIPTION  "Voice-leading poly glide synth"
    SOURCES
        Source/GlideSynth.cpp
        Source/GlideFX.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
