#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/**
    The nebula: the whole window is space. Four clouds of dust (DREAM, PULSE, DARK, WILD), your point (drag it),
    the point LIFE moves round it, and a spark for every grain the swarm plays.
*/
class NebulaView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit NebulaView (SwarmProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick (float dt);

    static juce::Colour cornerColour (int corner, bool dust);
    static juce::Colour colourAt (float x, float y, bool dust);

private:
    void setPoint (juce::Point<float>);
    juce::Rectangle<float> field() const;
    struct Spark { float x, y, vx, vy, age, life, size; juce::Colour col; };
    SwarmProcessor& processor;
    juce::RangedAudioParameter &px, &py;
    juce::Image stars, dust;
    std::vector<Spark> sparks;
    std::vector<juce::Point<float>> trail;
    juce::Random rng { 0x5eed };
    float time = 0.0f, energySmooth = 0.0f;
    bool dragging = false;
    std::array<swarm::GrainEvent, 256> evBuf {};
};

/** A thin line slider: caption, a hairline, a dot, the value under it. */
class DustSlider : public juce::Slider
{
public:
    DustSlider (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& caption);
    void paint (juce::Graphics&) override;
    juce::String suffix;            // extra text after the value (PITCH shows its notes)

private:
    juce::String caption;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class SwarmEditor : public juce::Component, private juce::Timer
{
public:
    explicit SwarmEditor (SwarmProcessor&);
    ~SwarmEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Pill : juce::TextButton
    {
        using juce::TextButton::TextButton;
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };
    struct Dice : juce::Button
    {
        Dice() : juce::Button ("dice") {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    SwarmProcessor& processor;
    NebulaView nebula { processor };
    cat::PresetBar presetBar { processor.presetManager, "" };
    Dice dice;
    cat::ParamCombo grid { processor.apvts, "sync", "grid", nullptr, 34 }, notes { processor.apvts, "scale", "notes", nullptr, 42 };
    std::vector<std::unique_ptr<DustSlider>> sliders;
    Pill freeze { "FREEZE" };
    juce::AudioProcessorValueTreeState::ButtonAttachment freezeAttachment { processor.apvts, "freeze", freeze };
    juce::uint32 lastTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwarmEditor)
};
