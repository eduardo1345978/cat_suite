# Poly Board: a pedalboard for a synth (Behringer Poly D): drive, compressor, filter, phaser, chorus, delay, reverb
cat_add_fx_plugin(PolyBoard
    PRODUCT_NAME "Poly-Board"
    PLUGIN_CODE  Pbd1
    DESCRIPTION  "A pedalboard for your synth: drive, compressor, filter, phaser, chorus, delay, reverb - in any order"
    CATEGORIES   "Fx"
    SOURCES
        Source/BoardEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
cat_plugin_targets(_targets PolyBoard)
foreach(_t IN LISTS _targets)
    target_link_libraries(${_t} PRIVATE juce::juce_dsp)
endforeach()
