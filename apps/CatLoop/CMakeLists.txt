# Cat Loop: a 5-track live looper program (like a Boss RC-505): tempo, click, a beat, MIDI pads, keys, sessions.
juce_add_gui_app(CatLoop
    PRODUCT_NAME "Cat-Loop"
    COMPANY_NAME "Cat Suite"
    BUNDLE_ID    com.catsuite.catloop
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png"
    VERSION      1.0.0
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Cat Loop records loops from your microphone or sound card.")

target_sources(CatLoop PRIVATE
    Source/Main.cpp
    Source/LoopEngine.cpp
    Source/LoopView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/Source/CatSounds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../midi/DrumCat/Source/DrumCatEngine.cpp)

target_include_directories(CatLoop PRIVATE Source ${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/Source ${CMAKE_CURRENT_SOURCE_DIR}/../../midi/DrumCat/Source)
target_compile_features(CatLoop PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(CatLoop PRIVATE /utf-8)
endif()

target_compile_definitions(CatLoop PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CatLoop,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CatLoop,JUCE_VERSION>")

target_link_libraries(CatLoop
    PRIVATE
        cat_fonts
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
