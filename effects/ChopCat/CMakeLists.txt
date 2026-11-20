cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Chop-Cat: glitches drums in time - repeats, rolls, reverse, tape stop, pitch, scatter, freeze, mute;
# from 12 live pads (mouse or MIDI keys), a 16-step sequencer and CHAOS (random, but locked to the groove)
cat_add_fx_plugin(ChopCat
    PRODUCT_NAME "Chop-Cat"
    PLUGIN_CODE  Chp1
    DESCRIPTION  "Glitches drums in time: stutter, rolls, reverse, tape stop, scatter - live pads, a step sequencer and chaos"
    CATEGORIES   "Fx;Delay"
    MIDI_IN
    SOURCES
        Source/ChopEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
