cmake_minimum_required(VERSION 3.22)

# Allows configuring Life-Cat on its own:  cmake -S LifeCat -B build-lifecat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LifeCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(LifeCat SOUND
    PRODUCT_NAME "Life-Cat"
    PLUGIN_CODE  Lif1
    DESCRIPTION  "A tiny ecosystem that makes music"
    SOURCES
        Source/LifeWorld.cpp
        Source/LifeMusic.cpp
        Source/LifeSynth.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
