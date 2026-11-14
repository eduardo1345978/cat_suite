cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Vortex: the sound spins up (or down) forever - a Shepard / Risset spiral (an audio effect)
cat_add_fx_plugin(Vortex
    PRODUCT_NAME "Vortex"
    PLUGIN_CODE  Vtx1
    DESCRIPTION  "The sound spins up or down forever: an endless Shepard spiral"
    CATEGORIES   "Fx;Pitch Shift"
    SOURCES
        Source/VortexEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
