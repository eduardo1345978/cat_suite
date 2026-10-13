#pragma once

#include "PluginProcessor.h"
#include "EnvelopeEditorUI.h"
#include "BendVisualizer.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

/** Envelope mode: play mode, shape palette, length / editor / cycle, depth, random, chance. */
class EnvelopePanel : public juce::Component
{
public:
    explicit EnvelopePanel (BendCatProcessor& p);
    void resized() override;

private:
    cat::ParamCombo playMode, length;
    ShapePalette palette;
    EnvelopeEditor editor;
    std::unique_ptr<cat::Stepper> cycles;
    cat::BarSlider depth, random, chance;
    cat::ParamToggle loop;
    cat::Knob duration;
};

class GlidePanel : public juce::Component
{
public:
    explicit GlidePanel (BendCatProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    cat::Knob time;
    cat::ParamCombo curve;
    CurvePreview preview;
};

class LfoPanel : public juce::Component
{
public:
    explicit LfoPanel (BendCatProcessor& p);
    void resized() override;

private:
    cat::Knob rate, depth, voiceRandom;
    cat::ParamToggle sync, phaseRandom;
    cat::ParamCombo division, wave;
    CurvePreview preview;
};

class PatternPanel : public juce::Component
{
public:
    explicit PatternPanel (BendCatProcessor& p);
    void resized() override;

private:
    BendPatternEditor editor;
    cat::ParamCombo division;
    cat::Knob smoothing;
};

/** BEND tab: mode selector + the panel for the active mode. */
class BendTab : public juce::Component
{
public:
    explicit BendTab (BendCatProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void showMode (int mode);

    BendCatProcessor& processor;
    std::array<juce::TextButton, 5> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    EnvelopePanel envelopePanel;
    GlidePanel glidePanel;
    LfoPanel lfoPanel;
    PatternPanel patternPanel;
    int currentMode = 0;
};

/** DETUNE tab: human detune, drift, Scala microtuning. */
class DetuneTab : public juce::Component, private juce::Timer
{
public:
    explicit DetuneTab (BendCatProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    BendCatProcessor& processor;
    cat::Knob detune, drift, driftRate;
    cat::ParamToggle scalaEnabled;
    juce::TextButton loadButton { "Load .scl..." }, clearButton { "Clear" };
    std::unique_ptr<cat::Stepper> reference;
    std::unique_ptr<juce::FileChooser> chooser;
    uint32_t shownVersion = 0;
    juce::String description;
    int numDegrees = 0;
    std::array<float, 128> table {};
};

class BendCatEditor : public juce::Component
{
public:
    explicit BendCatEditor (BendCatProcessor&);
    ~BendCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BendCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    cat::ParamToggle mpe, sendConfig;
    std::unique_ptr<cat::Stepper> range, amount, voices;
    std::unique_ptr<cat::PresetBar> presetStepper;
    cat::ParamCombo resolution;
    BendVisualizer miniScope;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    cat::PianoDisplay keyboard;


    // Music-reactive cat, boot screen, Surprise / undo
    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BendCatEditor)
};
