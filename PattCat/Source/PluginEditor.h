#pragma once

#include "PluginProcessor.h"
#include "PatternGridUI.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"
#include "BounceView.h"

class PattCatEditor;

/** One layer of the left-hand mixer: name, mute, solo, channel, volume, MPE. */
class MixerStrip : public juce::Component
{
public:
    MixerStrip (PattCatEditor& owner, PattCatProcessor& p, GridContext& ctx, int layer);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

private:
    PattCatEditor& owner;
    GridContext& ctx;
    const int layer;
    juce::Label name;
    cat::ParamToggle mute, solo;
    std::unique_ptr<cat::Stepper> channel;
    cat::BarSlider volume;
    juce::TextButton mpeButton { "MPE" };
};

/** Right-hand inspector for the selected step. */
class StepInspector : public juce::Component
{
public:
    StepInspector (GridContext& ctx);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    void edit (const std::function<void (pattcat::Step&)>& fn);
    pattcat::Step& step();

    GridContext& ctx;
    juce::TextButton activeButton { "Active" }, tieButton { "Tie" }, restButton { "Rest" };
    juce::Slider velocity, gate, probability;
    std::unique_ptr<cat::Stepper> octave, ratchet;
    juce::ComboBox voice;
    bool updating = false;
};

class PattCatEditor : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::Timer
{
public:
    explicit PattCatEditor (PattCatProcessor&);
    ~PattCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void selectLayer (int layer);
    void editLayer (const std::function<void (pattcat::PatternLayer&)>& fn);
    void refreshAll();

private:
    void timerCallback() override;

    PattCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    GridContext ctx;

    // Top bar
    std::unique_ptr<cat::PresetBar> presetStepper;
    juce::TextButton generateButton { "Generate" }, clearChordsButton { "Clear Chords" };
    cat::ParamCombo inputMode;
    cat::ParamToggle latch, runStopped, thru, useInputVel, followGroove;
    cat::BarSlider globalSwing, gateRandom;
    cat::DragButton dragButton { "Drag MIDI" };

    // Layer / Euclid bar
    std::unique_ptr<cat::Stepper> fillStepper, lengthStepper, rotationStepper, swingStepper, velScaleStepper;
    juce::TextButton euclidButton { "Euclid" }, polyButton { "Poly" }, clearLayerButton { "Clear" };
    juce::ComboBox divisionBox;

    juce::OwnedArray<MixerStrip> strips;
    PatternOverview overview { ctx };
    StepGrid grid { ctx };
    StepInspector inspector { ctx };
    PresetGrid presets { ctx };
    cat::PianoDisplay keyboard;

    uint32_t shownVersion = 0;


    // Steps / Bounce view switch and the Bounce page
    void setBounceView (bool shouldShow);
    bool bounceVisible = false;
    juce::TextButton stepsViewButton { "Steps" }, bounceViewButton { "Bounce" };
    BounceView bounceView;
    cat::ParamCombo bounceMode, bounceSync, bounceRate, bounceOctRnd, bounceHold;
    cat::BarSlider bounceMs, bounceNotes, bounceCurve, bounceFade, bounceOctave, bounceDur, bounceHuman;

    // Music-reactive cat, boot screen, Surprise / undo
    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PattCatEditor)
};
