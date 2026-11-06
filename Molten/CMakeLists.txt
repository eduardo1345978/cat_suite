cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Molten: the sound melts (warm valve dirt, falling echoes, tape wobble) - an audio effect
cat_add_fx_plugin(Molten
    PRODUCT_NAME "Molten"
    PLUGIN_CODE  Mlt1
    DESCRIPTION  "The sound melts: warm valve dirt, dripping echoes that fall, tape wobble"
    CATEGORIES   "Fx;Distortion"
    SOURCES
        Source/MoltenEngine.cpp
        Source/LavaSim.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
target_link_libraries(Molten PRIVATE juce::juce_dsp)
