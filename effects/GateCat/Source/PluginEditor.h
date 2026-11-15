#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The 16 steps: drag up / down to draw the levels, double-click a step to turn it fully on / off. */
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (GateCatProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int stepAtX (float x) const;
    void setStep (int k, float v);
    GateCatProcessor& processor;
    std::array<juce::RangedAudioParameter*, gate::maxSteps> params {};
    juce::Point<float> last;
    std::array<bool, gate::maxSteps> grabbed {};
};

class GateCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit GateCatEditor (GateCatProcessor&);
    ~GateCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    GateCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "" };
    StepGrid grid { processor };
    cat::LineSlider swing { processor.apvts, "swing", "SWING" }, length { processor.apvts, "length", "LENGTH" },
                    attack { processor.apvts, "attack", "ATTACK" }, release { processor.apvts, "release", "RELEASE" },
                    depth { processor.apvts, "depth", "DEPTH" }, filter { processor.apvts, "filter", "FILTER" },
                    mix { processor.apvts, "mix", "MIX" }, steps { processor.apvts, "steps", "STEPS" };
    cat::ParamCombo rate { processor.apvts, "rate", "rate", nullptr, 40 };
    juce::TextButton clearB { "Clear" }, fillB { "Fill" }, randomB { "Random" }, shiftB { "Shift >" };
    float shownGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateCatEditor)
};
