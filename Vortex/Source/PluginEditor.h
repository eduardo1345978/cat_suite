#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The vortex: rings fly out of (or into) the middle forever. Drag up / down on it to set SPIN. */
class VortexView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VortexView (VortexProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick (float dt);

private:
    VortexProcessor& processor;
    juce::RangedAudioParameter& spin;
    float dragStart = 0.0f;
    struct Star { float angle, u, speed, size; };
    std::vector<Star> stars;
    float swirl = 0.0f, level = 0.0f, shownPos = 0.0f;
};

/** The staircase: where the copies (PITCH) or the filters (FILTER) are now, and how loud each one is. */
class LadderView : public juce::Component
{
public:
    explicit LadderView (VortexProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;

private:
    VortexProcessor& processor;
};

class VortexEditor : public juce::Component, private juce::Timer
{
public:
    explicit VortexEditor (VortexProcessor&);
    ~VortexEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    VortexProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "" };
    VortexView vortex { processor };
    LadderView ladder { processor };
    cat::LineSlider spin { processor.apvts, "spin", "SPIN" }, focus { processor.apvts, "focus", "FOCUS" },
                    feedback { processor.apvts, "feedback", "FEEDBACK" }, width { processor.apvts, "width", "WIDTH" },
                    tone { processor.apvts, "tone", "TONE" }, gen { processor.apvts, "gen", "GEN" }, mix { processor.apvts, "mix", "MIX" };
    cat::ParamCombo mode { processor.apvts, "mode", "mode", nullptr, 44 }, sync { processor.apvts, "sync", "sync", nullptr, 40 };
    juce::uint32 lastTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VortexEditor)
};
