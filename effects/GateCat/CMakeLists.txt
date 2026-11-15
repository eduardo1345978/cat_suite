cmake_minimum_required(VERSION 3.22)

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Gate-Cat: a rhythmic gate - 16 steps chop any sound into a rhythm (trance pads, stutters)
cat_add_fx_plugin(GateCat
    PRODUCT_NAME "Gate-Cat"
    PLUGIN_CODE  Gte1
    DESCRIPTION  "Chops any sound into a rhythm: 16 steps, each with its own level and filter"
    CATEGORIES   "Fx;Dynamics"
    SOURCES
        Source/GateEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
