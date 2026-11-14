#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

/**
    Glide View: every voice drawn as a line over the last four seconds (pitch up, time right),
    with a vertical keyboard on the left and an arrow from each gliding voice to its target note.
*/
class GlideView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit GlideView (GlideCatProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float yFor (float pitch, juce::Rectangle<float> area) const;

    GlideCatProcessor& processor;
    float lowPitch = 48.0f, highPitch = 84.0f;
    std::array<float, glidecat::historyLength> scratch {};
};

/** A titled group: combo boxes / toggles stacked on the left, knobs in a row on the right. */
class GlideSection : public juce::Component
{
public:
    GlideSection (juce::String title, juce::Colour colour) : titleText (std::move (title)), tint (colour) {}
    void addLeft (juce::Component* c)  { left.push_back (c); addAndMakeVisible (c); }
    void addKnob (juce::Component* c)  { knobs.push_back (c); addAndMakeVisible (c); }
    void paint (juce::Graphics&) override;
    void resized() override;
    int leftWidth = 170, knobMaxHeight = 92;

private:
    juce::String titleText;
    juce::Colour tint;
    std::vector<juce::Component*> left, knobs;
};

/** Base for the tab pages: owns its widgets, makes knobs / combos / toggles bound to parameters. */
class GlidePage : public juce::Component
{
public:
    explicit GlidePage (GlideCatProcessor& p) : processor (p) {}

protected:
    cat::Knob* knob (const juce::String& id, const juce::String& caption, juce::Colour c = cat::Colours::accent);
    cat::ParamCombo* combo (const juce::String& id, const juce::String& caption, int captionW = 62);
    cat::ParamToggle* toggle (const juce::String& id, const juce::String& text);
    GlideSection* section (const juce::String& title, juce::Colour c);

    GlideCatProcessor& processor;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<GlideSection> sections;
};

class GlideCatEditor : public juce::Component
{
public:
    explicit GlideCatEditor (GlideCatProcessor&);
    ~GlideCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GlideCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    cat::PresetBar presetBar;
    std::unique_ptr<cat::Stepper> glideStepper, voices;
    juce::TextButton diceButton { "Dice" };
    cat::BarSlider volume;

    GlideView view;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlideCatEditor)
};
