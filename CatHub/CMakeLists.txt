# Cat Hub: a standalone app that loads Chord-Cat, Bass-Cat, Drum-Cat and Life-Cat (the
# real VST3s) and plays them together with built-in sounds - no Cubase needed.
juce_add_gui_app(CatHub
    PRODUCT_NAME "Cat-Hub"
    COMPANY_NAME "Cat Suite"
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png"
    VERSION      2.0.0)

target_sources(CatHub PRIVATE
    Source/Main.cpp
    Source/HubEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatSounds.cpp
    Source/HubWindow.cpp
    Source/HubLook.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatLookAndFeel.cpp)

target_include_directories(CatHub PRIVATE Source ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source)
target_compile_features(CatHub PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(CatHub PRIVATE /utf-8)
endif()

target_compile_definitions(CatHub PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CatHub,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CatHub,JUCE_VERSION>")

target_link_libraries(CatHub
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
