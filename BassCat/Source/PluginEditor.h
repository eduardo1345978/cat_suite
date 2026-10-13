#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

/**
    The bass line as chord degrees over time: rows Root / 3rd / 5th / 7th / Oct / Appr / Up /
    Down / Ghost, one column per 16th. Click an empty cell to add a note, click a note to remove
    it, drag up/down on a note to change its velocity. The playhead follows the host.
*/
class DegreeGrid : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit DegreeGrid (BassCatProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static juce::Colour degreeColour (int degree);

private:
    void timerCallback() override;
    juce::Rectangle<float> gridArea() const;
    int rowFor (int degree) const;
    int degreeForRow (int row) const;

    BassCatProcessor& processor;
    basscat::Pattern pattern;
    uint32_t seenVersion = 0;
    int dragNote = -1, dragStartVel = 100;
    float dragStartY = 0.0f;
};

class BassCatEditor : public juce::Component
{
public:
    explicit BassCatEditor (BassCatProcessor&);
    ~BassCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BassCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    cat::PresetBar presetBar;
    juce::TextButton generateButton { "Generate" };
    std::unique_ptr<cat::Stepper> variationStepper, lowNote, octave, channel;
    cat::DragButton dragButton { "Drag MIDI" };

    cat::ParamCombo style, length;
    cat::BarSlider density, fills, ghosts, slides, swing, human, gate, velocity, velRandom, chance;
    cat::ParamToggle followGroove, latch, thru, keyTrigger;
    juce::Label chordLabel;

    DegreeGrid grid;
    cat::PianoDisplay keyboard;
    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    struct ChordTimer : public juce::Timer
    {
        std::function<void()> fn;
        void timerCallback() override { fn(); }
    } chordTimer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassCatEditor)
};
