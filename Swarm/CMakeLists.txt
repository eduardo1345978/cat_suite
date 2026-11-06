cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Swarm: a granular organism - the sound becomes a living cloud of grains (an audio effect)
cat_add_fx_plugin(Swarm
    PRODUCT_NAME "Swarm"
    PLUGIN_CODE  Swm1
    DESCRIPTION  "A granular organism: the sound becomes a living cloud of grains"
    CATEGORIES   "Fx;Modulation"
    SOURCES
        Source/SwarmEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
