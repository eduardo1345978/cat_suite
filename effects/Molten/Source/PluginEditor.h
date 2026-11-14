#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"
#include "LavaSim.h"

/** The lava: drag up / down on it to set HEAT, click to poke it. */
class LavaView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LavaView (MoltenProcessor&);
    ~LavaView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick (float dt);
    LavaSim sim;

private:
    juce::Rectangle<float> fieldArea() const;
    MoltenProcessor& processor;
    juce::RangedAudioParameter& heat;
    float dragStartHeat = 0.0f;
    bool dragging = false;
};

class MoltenEditor : public juce::Component, private juce::Timer
{
public:
    explicit MoltenEditor (MoltenProcessor&);
    ~MoltenEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MoltenProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "" };
    LavaView lava { processor };
    cat::LineSlider drip { processor.apvts, "drip", "DRIP" }, wobble { processor.apvts, "wobble", "WOBBLE" },
                    body { processor.apvts, "body", "BODY" }, mix { processor.apvts, "mix", "MIX" };
    cat::ParamCombo dripTime { processor.apvts, "dripTime", "drip time", nullptr, 66 };
    cat::ParamToggle monoBass { processor.apvts, "monoBass", "mono bass" };
    juce::uint32 lastTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoltenEditor)
};
