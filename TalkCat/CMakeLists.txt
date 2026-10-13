cmake_minimum_required(VERSION 3.22)

# Allows configuring Talk-Cat on its own:  cmake -S TalkCat -B build-basscat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(TalkCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_synth_plugin(TalkCat
    PRODUCT_NAME "Talk-Cat"
    PLUGIN_CODE  Sng1
    DESCRIPTION  "Vocaloid-style singer for people and cats"
    SOURCES
        Source/Phonetics.cpp
        Source/Vocal.cpp
        Source/VoiceEngine.cpp
        Source/Score.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
