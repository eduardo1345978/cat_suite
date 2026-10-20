# Cat Ocean: an endless pixel-art ocean for YouTube live streams. Viewers claim fish through the
# chat, fish have babies and mutate, and their genes steer a never-ending song. Streams by itself
# through ffmpeg (downloaded by the app on first use).
juce_add_gui_app(CatOcean
    PRODUCT_NAME "Cat-Ocean"
    COMPANY_NAME "Cat Suite"
    BUNDLE_ID    com.catsuite.catocean
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png"
    VERSION      1.0.0)

target_sources(CatOcean PRIVATE
    Source/Main.cpp
    Source/OceanGenes.cpp
    Source/OceanSim.cpp
    Source/OceanCommands.cpp
    Source/OceanAdmin.cpp
    Source/OceanArt.cpp
    Source/OceanMusic.cpp
    Source/OceanEngine.cpp
    Source/YouTubeChat.cpp
    Source/Streamer.cpp
    Source/OceanTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatSounds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/Composer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/SongRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatLookAndFeel.cpp)

# the text for the stream's YouTube description (the app copies it to the clipboard)
juce_add_binary_data(CatOceanData SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../packaging/Cat-Ocean-YouTube-Description.txt)

target_include_directories(CatOcean PRIVATE Source ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source)
target_compile_features(CatOcean PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(CatOcean PRIVATE /utf-8)
endif()

target_compile_definitions(CatOcean PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CatOcean,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CatOcean,JUCE_VERSION>")

target_link_libraries(CatOcean
    PRIVATE
        CatOceanData
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
