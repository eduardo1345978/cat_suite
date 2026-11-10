cmake_minimum_required(VERSION 3.22)

# Allows configuring Meow-Cat on its own:  cmake -S MeowCat -B build-meowcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MeowCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_synth_plugin(MeowCat
    PRODUCT_NAME "Meow-Cat"
    PLUGIN_CODE  Mew1
    DESCRIPTION  "Every note is a cat: meows, a cat choir, purring and hissing"
    SOURCES
        Source/MeowEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
