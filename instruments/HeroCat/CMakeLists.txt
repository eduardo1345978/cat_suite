cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Hero-Cat: a lead synth for 90s game / anime melodies - two sounds per note (synth waves or small physical models:
# guitar, strings, flute, choir, bell), Chord Lock with strum and voice leading, a waterfall and a drawn arp,
# CHAOS that improvises, textures (rain, birds, chimes ...), an LFO and an effect chain; chords go out as MIDI too
cat_add_synth_plugin(HeroCat
    PRODUCT_NAME "Hero-Cat"
    PLUGIN_CODE  Hro1
    DESCRIPTION  "A lead synth for game and anime melodies: chord lock, waterfall arp, physical instruments, textures"
    MIDI_OUT
    SOURCES
        Source/HeroEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
cat_plugin_targets(_targets HeroCat)
foreach(_t IN LISTS _targets)
    target_link_libraries(${_t} PRIVATE juce::juce_dsp)
endforeach()
