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
add_subdirectory(BassCat)
add_subdirectory(GlideCat)
add_subdirectory(TalkCat)

option(CAT_BUILD_TESTS "Build the headless VST3 host test (loads the built .vst3 files)" OFF)
if(CAT_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
