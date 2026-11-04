cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(PhaseCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(PhaseCat
    PRODUCT_NAME "Phase-Cat"
    PLUGIN_CODE  Pha1
    DESCRIPTION  "Experimental rhythm: phasing, polymeters, Risset rhythms, metric modulation"
    SOURCES
        Source/PhaseEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
