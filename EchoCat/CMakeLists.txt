cmake_minimum_required(VERSION 3.22)

# Allows configuring Echo-Cat on its own:  cmake -S EchoCat -B build-echocat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(EchoCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_fx_plugin(EchoCat
    PRODUCT_NAME "Echo-Cat"
    PLUGIN_CODE  Ech1
    DESCRIPTION  "Tape echo, ping-pong and shimmer space"
    SOURCES
        Source/EchoEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
