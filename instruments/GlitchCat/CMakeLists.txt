cmake_minimum_required(VERSION 3.22)

# Allows configuring Glitch-Cat on its own:  cmake -S GlitchCat -B build-glitchcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(GlitchCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_synth_plugin(GlitchCat
    PRODUCT_NAME "Glitch-Cat"
    PLUGIN_CODE  Glc1
    DESCRIPTION  "Every note is a swarm: from a warm wobbly pad to rolls, stutters and chaos"
    SOURCES
        Source/GlitchEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/Source/CatSounds.cpp)

# mp3 voices can be dropped in too
cat_plugin_targets(_targets GlitchCat)
foreach(_t IN LISTS _targets)
    target_compile_definitions(${_t} PRIVATE JUCE_USE_MP3AUDIOFORMAT=1)
endforeach()
