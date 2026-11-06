#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"
#include "ScaledEditor.h"

/** One lane's steps: click = on / off, drag up / down = how loud. Steps past the lane's length are dim. */
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    StepGrid (PhaseCatProcessor&, int lane, juce::Colour);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    int stepAt (float x) const;
private:
    PhaseCatProcessor& processor;
    const int lane;
    juce::Colour colour;
    int dragStep = -1;
    float dragStartValue = 0.0f;
    bool painting = false;
    float paintValue = 1.0f;
};

/** The lanes as rings: every step a dot round its circle, a hand going round at the lane's speed.
    Different lengths = rings turning at different speeds; drift = one ring slipping past the other. */
class RingView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RingView (PhaseCatProcessor&);
    void paint (juce::Graphics&) override;
    std::array<float, phasecat::numLanes> flash {};
private:
    PhaseCatProcessor& processor;
};

class PhaseCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit PhaseCatEditor (PhaseCatProcessor&);
    ~PhaseCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PhaseCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "Preset" };
    juce::TextButton playButton { "Play" };
    cat::BarSlider channel { processor.apvts, "channel", "Ch", nullptr, 30 };

    struct LaneControls
    {
        LaneControls (PhaseCatProcessor& p, int l);
        std::unique_ptr<cat::ParamToggle> on, risset;
        std::unique_ptr<cat::BarSlider> note, length, drift;
        std::unique_ptr<cat::ParamCombo> rate;
        std::unique_ptr<StepGrid> grid;
    };
    std::vector<std::unique_ptr<LaneControls>> lanes;
    RingView rings { processor };

    juce::OwnedArray<juce::TextButton> modButtons;
    juce::TextButton resetModButton { "Back to x 1" }, realignButton { "Realign at the bar" };
    cat::BarSlider rissetBars { processor.apvts, "rissetBars", "Turn", nullptr, 44 }, gate { processor.apvts, "gate", "Gate", nullptr, 44 };
    cat::ParamToggle rissetDown { processor.apvts, "rissetDown", "Slowing down" };

    juce::Rectangle<int> logoArea, laneArea, modArea, rissetArea, infoArea;
    std::array<juce::Rectangle<int>, phasecat::numLanes> laneTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseCatEditor)
};
