#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"
#include "ScaledEditor.h"

/** The cat meows on the left; its echoes travel right - spaced by the echo time, fading with the feedback,
    bouncing left / right with ping-pong, darker with less tone, wobbling with wobble, glowing with space. */
class EchoView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit EchoView (EchoCatProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void drawCat (juce::Graphics&, juce::Point<float> centre, float size, float open, bool frozen) const;
    EchoCatProcessor& processor;
    float in = 0.0f, echo = 0.0f, space = 0.0f, duck = 0.0f;
    double clock = 0.0;
};

/** A titled glass group with its controls. */
class EchoSection : public juce::Component
{
public:
    EchoSection (juce::String title, juce::Colour colour) : titleText (std::move (title)), tint (colour) {}
    void addTop (juce::Component* c) { top.push_back (c); addAndMakeVisible (c); }
    void addKnob (juce::Component* c) { knobs.push_back (c); addAndMakeVisible (c); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::String titleText;
    juce::Colour tint;
    std::vector<juce::Component*> top, knobs;
};

class EchoCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit EchoCatEditor (EchoCatProcessor&);
    ~EchoCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    cat::Knob* knob (const juce::String& id, const juce::String& caption, juce::Colour c);

    EchoCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "Preset" };
    cat::ParamToggle freezeButton { processor.apvts, "freeze", "Freeze" }, syncButton { processor.apvts, "sync", "In time with Cubase" };
    cat::ParamCombo divisionBox { processor.apvts, "division", "Echo", nullptr, 44 };
    EchoView view { processor };
    EchoSection timeSection { "TIME", cat::Colours::accent }, colourSection { "EACH ECHO", cat::Colours::red },
                spaceSection { "SPACE", cat::Colours::purple }, mixSection { "MIX", cat::Colours::green };
    juce::OwnedArray<cat::Knob> knobs;
    cat::Knob* timeKnob = nullptr;
    juce::Rectangle<int> logoArea, infoArea;
    juce::String info;
    bool shownSync = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoCatEditor)
};
