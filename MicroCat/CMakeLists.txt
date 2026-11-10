cmake_minimum_required(VERSION 3.22)

# Allows configuring Micro-Cat on its own:  cmake -S MicroCat -B build-microcat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MicroCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

cat_add_midi_plugin(MicroCat
    PRODUCT_NAME "Micro-Cat"
    PLUGIN_CODE  Mic1
    DESCRIPTION  "Microtonal tunings: its own piano and pads, or any MPE / MTS-ESP synth"
    SOURCES
        Source/Tunings.cpp
        Source/MicroEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/MtsEsp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatSounds.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/MTS-ESP/Master/libMTSMaster.cpp)
# ODDSound's file as it is: no warnings from it
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/MTS-ESP/Master/libMTSMaster.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
if(UNIX AND NOT APPLE)
    cat_plugin_targets(_targets MicroCat)
    foreach(_t IN LISTS _targets)
        target_link_libraries(${_t} PRIVATE ${CMAKE_DL_LIBS})
    endforeach()
endif()
