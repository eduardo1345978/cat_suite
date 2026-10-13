# Headless host that loads the three built VST3 bundles, chains them like Cubase
# would (Chord-Cat -> Patt-Cat -> Bend-Cat) and checks the MIDI they produce.
#   cmake -B build -DCAT_BUILD_TESTS=ON && cmake --build build && ./build/Tests/CatSuiteHostTest
juce_add_console_app(CatSuiteHostTest PRODUCT_NAME "CatSuiteHostTest")

target_sources(CatSuiteHostTest PRIVATE HostTest.cpp)

target_compile_definitions(CatSuiteHostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    CHORDCAT_VST3="$<TARGET_FILE_DIR:ChordCat_VST3>/../../../Chord-Cat.vst3"
    PATTCAT_VST3="$<TARGET_FILE_DIR:PattCat_VST3>/../../../Patt-Cat.vst3"
    BENDCAT_VST3="$<TARGET_FILE_DIR:BendCat_VST3>/../../../Bend-Cat.vst3"
    DRUMCAT_VST3="$<TARGET_FILE_DIR:DrumCat_VST3>/../../../Drum-Cat.vst3"
    ORBITCAT_VST3="$<TARGET_FILE_DIR:OrbitCat_VST3>/../../../Orbit-Cat.vst3"
    SINGCAT_VST3="$<TARGET_FILE_DIR:SingCat_VST3>/../../../Sing-Cat.vst3"
    LIFECAT_VST3="$<TARGET_FILE_DIR:LifeCat_VST3>/../../../Life-Cat.vst3"
    BASSCAT_VST3="$<TARGET_FILE_DIR:BassCat_VST3>/../../../Bass-Cat.vst3"
    GLIDECAT_VST3="$<TARGET_FILE_DIR:GlideCat_VST3>/../../../Glide-Cat.vst3"
    TALKCAT_VST3="$<TARGET_FILE_DIR:TalkCat_VST3>/../../../Talk-Cat.vst3")

target_link_libraries(CatSuiteHostTest PRIVATE
    juce::juce_audio_processors
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

add_dependencies(CatSuiteHostTest ChordCat_VST3 PattCat_VST3 BendCat_VST3 DrumCat_VST3 OrbitCat_VST3 SingCat_VST3 BassCat_VST3 GlideCat_VST3 TalkCat_VST3 LifeCat_VST3)

if(MSVC)
    target_compile_options(CatSuiteHostTest PRIVATE /utf-8)
endif()
