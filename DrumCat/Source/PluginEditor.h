#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

/** The groove grid: one row per drum, hits drawn where they are really played (micro-timing). */
class DrumGrid : public juce::Component, private juce::Timer
{
public:
    explicit DrumGrid (DrumCatProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static juce::Colour laneColour (int lane);

private:
    void timerCallback() override;
    juce::Rectangle<float> rowBounds (int lane) const;
    juce::Rectangle<float> hitBounds (const drumcat::Hit& h) const;
    int hitAt (juce::Point<float> p) const;
    int laneAt (float y) const;

    DrumCatProcessor& processor;
    drumcat::Pattern pattern;
    uint32_t shownVersion = 0;
    double playhead = -1.0;
    int dragHit = -1, hoverHit = -1;
    bool dragged = false;
    int dragStartVelocity = 100;
};

/** Lane name + mute, flashing when the lane plays. */
class LaneHeader : public juce::Component
{
public:
    LaneHeader (DrumCatProcessor& p, int lane);
    void paint (juce::Graphics&) override;
    void resized() override;
    void flash() { glow = 1.0f; }
    void decay() { if (glow > 0.0f) { glow = juce::jmax (0.0f, glow - 0.12f); repaint(); } }

private:
    const int lane;
    cat::ParamToggle mute;
    float glow = 0.0f;
};

class DrumCatEditor : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::Timer
{
public:
    explicit DrumCatEditor (DrumCatProcessor&);
    ~DrumCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DrumCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::OwnedArray<juce::TextButton> styleButtons;
    juce::TextButton generateButton { "Generate" };
    std::unique_ptr<cat::Stepper> variationStepper;
    std::unique_ptr<cat::PresetBar> presetStepper;
    cat::DragButton dragButton { "Drag MIDI" };

    cat::BarSlider swing, human, velocity, velRandom, chance, open, density, fills;
    cat::ParamCombo grid, length, map;
    std::unique_ptr<cat::Stepper> channel;
    cat::ParamToggle keyTrigger;

    juce::OwnedArray<LaneHeader> lanes;
    DrumGrid drumGrid;
    std::array<uint32_t, drumcat::NumLanes> seenHits {};

    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumCatEditor)
};
