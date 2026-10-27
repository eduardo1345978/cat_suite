# Cat Box 2: the Cat Suite groovebox (8 tracks: engines, steps, note FX, mixer) for the PC and the Raspberry Pi in the box.
juce_add_gui_app(CatBox
    PRODUCT_NAME "Cat-Box"
    COMPANY_NAME "Cat Suite"
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/../CatHub/Resources/icon.png"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/../CatHub/Resources/icon_small.png"
    VERSION      2.0.0)

target_sources(CatBox PRIVATE
    Source/Main.cpp
    Source/Params.cpp
    Source/Engine.cpp
    Source/EngineAnalog.cpp
    Source/EngineAcid.cpp
    Source/EngineDrums.cpp
    Source/EngineKeys.cpp
    Source/EngineGlide.cpp
    Source/EngineFM.cpp
    Source/EngineWavetable.cpp
    Source/EngineChoir.cpp
    Source/EngineSampler.cpp
    Source/EngineGranular.cpp
    Source/Samples.cpp
    Source/Templates.cpp
    Source/TrackFx.cpp
    Source/PresetsExtra.cpp
    Source/NoteFx.cpp
    Source/Project.cpp
    Source/MixFx.cpp
    Source/Audio.cpp
    Source/Controller.cpp
    Source/View.cpp
    Source/BoxLink.cpp
    Source/LinkPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source/CatSounds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source/GlideSynth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source/GlideFX.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source/GlideData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../OrbitCat/Source/OrbitEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SingCat/Source/ChoirEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SingCat/Source/VoiceModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SingCat/Source/Lyrics.cpp)

target_include_directories(CatBox PRIVATE
    Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shared/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../GlideCat/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../OrbitCat/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../SingCat/Source)

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
