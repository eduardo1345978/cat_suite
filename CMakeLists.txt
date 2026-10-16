cmake_minimum_required(VERSION 3.22)

project(CatSuite VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    # Static runtime: the .vst3 bundles load in Cubase without a VC++ redistributable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/JuceSetup.cmake)

add_subdirectory(Shared)
add_subdirectory(ChordCat)
add_subdirectory(PattCat)
add_subdirectory(BendCat)
add_subdirectory(DrumCat)
add_subdirectory(OrbitCat)
add_subdirectory(SingCat)
add_subdirectory(LifeCat)
add_subdirectory(BassCat)
add_subdirectory(GlideCat)
add_subdirectory(TalkCat)
add_subdirectory(CatHub)

option(CAT_BUILD_TESTS "Build the headless VST3 host test (loads the built .vst3 files)" OFF)
if(CAT_BUILD_TESTS)
    add_subdirectory(Tests)
endif()

# Stress test: each plug-in's shared code + the real editor, random UI + audio at once.
# Build with -DCAT_BUILD_STRESS=ON -DCMAKE_CXX_FLAGS="-fsanitize=address -D_GLIBCXX_ASSERTIONS" and run under Xvfb.
option(CAT_BUILD_STRESS "Build the per-plug-in stress test executables" OFF)
if(CAT_BUILD_STRESS)
    foreach(_cat ChordCat PattCat BendCat DrumCat OrbitCat SingCat LifeCat BassCat GlideCat TalkCat)
        add_executable(${_cat}Stress Tests/PluginStress.cpp)
        target_compile_definitions(${_cat}Stress PRIVATE $<TARGET_PROPERTY:${_cat},COMPILE_DEFINITIONS>)
        target_include_directories(${_cat}Stress PRIVATE $<TARGET_PROPERTY:${_cat},INCLUDE_DIRECTORIES>)
        target_compile_features(${_cat}Stress PRIVATE cxx_std_20)
        target_link_libraries(${_cat}Stress PRIVATE ${_cat})
    endforeach()
    foreach(_cat SingCat GlideCat TalkCat LifeCat)
        add_executable(${_cat}Sound Tests/SoundCheck.cpp)
        target_compile_definitions(${_cat}Sound PRIVATE $<TARGET_PROPERTY:${_cat},COMPILE_DEFINITIONS>)
        target_include_directories(${_cat}Sound PRIVATE $<TARGET_PROPERTY:${_cat},INCLUDE_DIRECTORIES>)
        target_compile_features(${_cat}Sound PRIVATE cxx_std_20)
        target_link_libraries(${_cat}Sound PRIVATE ${_cat})
    endforeach()
endif()

# The composer's check (music theory rules, regenerate, endless, MIDI, player, demo renders).
option(CAT_BUILD_COMPOSE_CHECK "Build the composer check" ${CAT_BUILD_STRESS})
if(CAT_BUILD_COMPOSE_CHECK)
    juce_add_console_app(ComposeCheck PRODUCT_NAME "ComposeCheck")
    target_sources(ComposeCheck PRIVATE Tests/ComposeCheck.cpp Shared/Source/Composer.cpp
                   Shared/Source/SongRenderer.cpp Shared/Source/CatSounds.cpp)
    target_include_directories(ComposeCheck PRIVATE Shared/Source)
    target_compile_definitions(ComposeCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_compile_features(ComposeCheck PRIVATE cxx_std_20)
    target_link_libraries(ComposeCheck PRIVATE juce::juce_audio_formats juce::juce_audio_basics juce::juce_gui_basics
                          juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
