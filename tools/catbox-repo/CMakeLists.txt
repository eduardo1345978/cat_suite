# Cat Box on its own (from github.com/eduardo1345978/cat_suite - the folders here are copied from there)
cmake_minimum_required(VERSION 3.22)
project(CatBox VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/JuceSetup.cmake)
option(CAT_BOX_ONLY "Kept for the scripts: this repository builds only Cat Box anyway" ON)
add_subdirectory(CatBox)
