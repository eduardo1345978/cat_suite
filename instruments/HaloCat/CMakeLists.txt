cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Halo-Cat: a singing choir with planets around it - hold a chord (or one key: it plays a progression),
# the choir sings it and glides to the next one, four planets orbit and ring bells on the chord's notes
cat_add_synth_plugin(HaloCat
    PRODUCT_NAME "Halo-Cat"
    PLUGIN_CODE  Hal1
    DESCRIPTION  "A singing choir with planets that ring bells: chords, glides, orbits and star dust"
    SOURCES
        Source/HaloEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
cat_plugin_targets(_targets HaloCat)
foreach(_t IN LISTS _targets)
    target_link_libraries(${_t} PRIVATE juce::juce_dsp)
endforeach()
