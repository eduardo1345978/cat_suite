cmake_minimum_required(VERSION 3.22)

# Allows configuring Sing-Cat on its own:  cmake -S SingCat -B build-singcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SingCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_synth_plugin(SingCat
    PRODUCT_NAME "Sing-Cat"
    PLUGIN_CODE  Tlk1
    DESCRIPTION  "Physically modelled cat & human choir"
    SOURCES
        Source/VoiceModel.cpp
        Source/ChoirEngine.cpp
        Source/Lyrics.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
