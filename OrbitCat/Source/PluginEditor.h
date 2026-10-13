#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

/** Animated orbit display: sun, tilted orbits, planets with trails, spokes, trigger flashes. */
class OrbitView : public juce::Component, private juce::Timer
{
public:
    explicit OrbitView (OrbitCatProcessor& p);
    void paint (juce::Graphics&) override;
    static juce::Colour slotColour (int slot);

private:
    void timerCallback() override;
    juce::Point<float> toScreen (float x, float y) const;

    OrbitCatProcessor& processor;
    std::vector<juce::Point<float>> stars;
    static constexpr int trailLength = 48;
    std::array<std::array<juce::Point<float>, trailLength>, orbitcat::maxBodies> trails {};
    std::array<int, orbitcat::maxBodies> trailCount {};
    std::array<int, orbitcat::maxBodies> trailHead {};
    float pxPerAu = 10.0f, tiltCos = 0.85f;
    juce::Point<float> centre;
    float time = 0.0f;
};

class OrbitCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit OrbitCatEditor (OrbitCatProcessor&);
    ~OrbitCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectSlot (int slot);

    OrbitCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<cat::Stepper> spokes, channel;
    std::unique_ptr<cat::PresetBar> presetStepper;
    cat::ParamToggle latch, followGroove;
    cat::ParamCombo quantize;
    cat::BarSlider gravity, drag, turbulence, align, accent, tilt, glow, swing;

    juce::OwnedArray<juce::TextButton> slotButtons;
    std::array<std::unique_ptr<cat::BarSlider>, 8> slotSliders;
    int selectedSlot = 0;

    OrbitView view;
    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitCatEditor)
};
