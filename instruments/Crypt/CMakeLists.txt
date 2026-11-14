# Crypt: dark chip music (an instrument) + a sound-effect maker that saves .wav files for a game
cat_add_synth_plugin(Crypt
    PRODUCT_NAME "Crypt"
    PLUGIN_CODE  Cry1
    DESCRIPTION  "Dark chip music and sound effects for a dungeon game"
    SOURCES
        Source/CryptDsp.cpp
        Source/CryptSynth.cpp
        Source/CryptDark.cpp
        Source/CryptSfx.cpp
        Source/CryptLook.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
foreach(_t IN ITEMS Crypt CryptPro)
    if(TARGET ${_t})
        target_link_libraries(${_t} PRIVATE juce::juce_audio_formats)
    endif()
endforeach()
