# Crypt FX: any sound into a dark 8-bit one (the crypt of the Crypt synth as an effect)
cat_add_fx_plugin(CryptFX
    PRODUCT_NAME "Crypt-FX"
    PLUGIN_CODE  Crx1
    DESCRIPTION  "Any sound into a dark 8-bit one: bits, rate, drive, gloom, a stone room, an echo"
    CATEGORIES   "Fx;Distortion"
    SOURCES
        ../Crypt/Source/CryptDsp.cpp
        ../Crypt/Source/CryptLook.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
foreach(_t IN ITEMS CryptFX CryptFXPro)
    if(TARGET ${_t})
        target_include_directories(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Crypt/Source)
    endif()
endforeach()
