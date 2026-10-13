# Makes JUCE 8 available exactly once, whether configured from the suite root
# or from one of the plug-in sub-projects on its own.
include_guard(GLOBAL)

if(NOT COMMAND juce_add_plugin)
    set(CAT_JUCE_DIR "" CACHE PATH "Path to a local JUCE 8 checkout (empty = download JUCE 8.0.15)")

    if(CAT_JUCE_DIR)
        add_subdirectory(${CAT_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
    else()
        # Downloaded as a release archive with CMake's built-in downloader, so
        # git does not have to be installed or on PATH (e.g. in PowerShell).
        include(FetchContent)
        FetchContent_Declare(JUCE
            URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/CatPlugin.cmake)
