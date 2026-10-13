#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

/**
    Vowel chart (front/back x close/open). Drag the filled dot (A) and the ring (B); in Path mode
    draw a line with the mouse and the choir follows it. The glowing cursor shows where the
    choir is singing right now.
*/
class VowelPad : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit VowelPad (SingCatProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> chartArea() const;
    juce::Point<float> toScreen (juce::Point<float> chart) const;
    juce::Point<float> toChart (juce::Point<float> screen) const;
    void setPoint (int which, juce::Point<float> chart);
    bool pathMode() const;

    SingCatProcessor& processor;
    int dragging = -1;                         // 0 = A, 1 = B, 2 = drawing a path
    std::vector<juce::Point<float>> drawing;
    float pulse = 0.0f;
};

/** The stage: one face per singer, mouths moving with their voices. Drag faces to move them. */
class StageView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit StageView (SingCatProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> floorArea() const;
    juce::Point<float> facePosition (int index, float& scale) const;
    void drawRoom (juce::Graphics&, juce::Rectangle<float>) const;
    void drawFace (juce::Graphics&, int index, juce::Point<float> c, float size) const;
    void drawFace3D (juce::Graphics&, int index, juce::Point<float> c, float size) const;
    void drawLips (juce::Graphics&, int index, juce::Rectangle<float> cell) const;
    void drawMics (juce::Graphics&) const;
    juce::Point<float> stageToScreen (float x, float y, float& scale) const;
    juce::Point<float> screenToStage (juce::Point<float> p) const;
    juce::Point<float> micPosition() const;
    void resized() override;
    int numSingers() const;

    juce::TextButton viewButtons[3] { juce::TextButton ("Stage"), juce::TextButton ("Lips"), juce::TextButton ("3D") };

    SingCatProcessor& processor;
    int dragging = -1;
    float time = 0.0f;
    std::array<float, singcat::maxSingers> blink {};
};

/** The lyric line as syllables; the one being sung lights up. Click to restart from the top. */
class LyricStrip : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit LyricStrip (SingCatProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

private:
    void timerCallback() override;
    SingCatProcessor& processor;
    std::vector<juce::String> syllables;
    int shown = -2;
};

class SingCatEditor : public juce::Component
{
public:
    explicit SingCatEditor (SingCatProcessor&);
    ~SingCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SingCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<cat::Stepper> singers, unison, bendRange;
    std::unique_ptr<cat::PresetBar> presetStepper;
    cat::ParamCombo mode, cast, style, onset, move, room, quality;
    cat::BarSlider moveRate, spread, roomMix, micWidth, micDist;
    cat::BarSlider attack, release, vibrato, breath, rasp, glide, drift, human, purr;
    cat::BarSlider neck, jaw, lips, nose, chest, volume;
    cat::ParamToggle mpe, words;
    cat::BarSlider diction;
    juce::TextEditor lyricsBox;
    LyricStrip lyricStrip;
    juce::TextButton dice { "Dice" }, arrange { "Arrange" };

    VowelPad pad;
    StageView stage;
    cat::CatVisualizer catView;
    cat::SurpriseControls surpriseControls;
    cat::BootScreen bootScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SingCatEditor)
};
