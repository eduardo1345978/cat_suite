# Cat Box: the Cat Suite groovebox (Drums / Arp / Bass step sequencers) for the PC and the Raspberry Pi in the box.
juce_add_gui_app(CatBox
    PRODUCT_NAME "Cat-Box"
    COMPANY_NAME "Cat Suite"
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/../CatHub/Resources/icon.png"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/../CatHub/Resources/icon_small.png"
    VERSION      1.0.0)

target_sources(CatBox PRIVATE
    Source/Main.cpp
    Source/BoxModel.cpp
    Source/ArpEngine.cpp
    Source/BoxAudio.cpp
    Source/BoxController.cpp
    Source/BoxView.cpp
    Source/MasterFx.cpp
    Source/BoxLink.cpp
    Source/LinkPanel.cpp
    Source/Engines.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatSounds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source/GlideSynth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source/GlideFX.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source/GlideData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../OrbitCat/Source/OrbitEngine.cpp)

target_include_directories(CatBox PRIVATE
    Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../OrbitCat/Source)

target_compile_features(CatBox PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(CatBox PRIVATE /utf-8)
endif()

target_compile_definitions(CatBox PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CatBox,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CatBox,JUCE_VERSION>")

target_link_libraries(CatBox
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
