cmake_minimum_required(VERSION 3.22)

# Allows configuring Muse-Cat on its own:  cmake -S MuseCat -B build-musecat
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MuseCat VERSION 1.0.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JuceSetup.cmake)
endif()

if(NOT TARGET cat_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Shared ${CMAKE_BINARY_DIR}/Shared)
endif()

# Muse-Cat writes whole songs (or endless music): plays them with its own sounds and sends
# every part as MIDI (chords ch 1, bass ch 2, melody ch 3, counter-melody ch 4, drums ch 10).
cat_add_midi_plugin(MuseCat SOUND
    PRODUCT_NAME "Muse-Cat"
    PLUGIN_CODE  Mus1
    DESCRIPTION  "Writes songs: chords, melody, words, bass, drums"
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/Composer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/SongRenderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatSounds.cpp)
target_link_libraries(MuseCat PRIVATE juce::juce_audio_formats)
