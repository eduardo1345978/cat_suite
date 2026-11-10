#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The rooftop at night: the cat sings what you play (mouth, eyes, ears, mane, purr, hiss), the choir sits beside it. */
class MeowStage : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MeowStage (MeowCatProcessor&);
    void paint (juce::Graphics&) override;
    void tick (float dt);

    struct Look
    {
        float mouth = 0.0f, anger = 0.0f, size = 0.4f, purr = 0.0f, hiss = 0.0f, blink = 0.0f, lookX = 0.0f, time = 0.0f;
        // the silly bits
        float tilt = 0.0f, squash = 0.0f;                 // head tilt (radians), stretch (> 0 tall, < 0 squashed)
        juce::Point<float> pupil[2];                      // where each googly pupil is (-1 .. 1)
        float eyeSize[2] { 1.0f, 1.0f };                  // one eye bigger than the other
        float wink = 0.0f, tongue = 0.0f, dizzy = 0.0f;   // left eye shut, a "blep", spiral eyes
        bool tooth = false;                               // one buck tooth
        juce::Colour fur { 0xffe89a4f }, stripes { 0xffb8642a }, eyes { 0xffc9ef5a };
    };
    static void drawCat (juce::Graphics&, juce::Point<float> centre, float s, const Look&, bool body = true);

private:
    MeowCatProcessor& processor;
    float time = 0.0f, mouth = 0.0f, hiss = 0.0f, purr = 0.0f, blink = 0.0f, nextBlink = 2.5f, lookX = 0.0f;
    int seenMeows = 0;
    struct Note { juce::Point<float> p, v; float life, spin; };
    std::vector<Note> flying;
    juce::Random random { 11 };
    juce::Point<float> mouthAt;
    struct Goofy
    {
        juce::Point<float> p[2], v[2];
        float squash = 0.0f, squashV = 0.0f, tongue = 0.0f, blepFor = 0.0f, nextBlep = 3.0f, spin = 0.0f;
    };
    std::array<Goofy, meow::maxCats> goofy;
    float mouthWidth = 60.0f;
};

class MeowCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit MeowCatEditor (MeowCatProcessor&);
    ~MeowCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MeowCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "" };
    MeowStage stage { processor };
    cat::LineSlider size { processor.apvts, "size", "SIZE" }, mood { processor.apvts, "mood", "MOOD" },
                    length { processor.apvts, "length", "LENGTH" }, octave { processor.apvts, "octave", "OCTAVE" },
                    choir { processor.apvts, "choir", "CHOIR" }, chaos { processor.apvts, "chaos", "CHAOS" },
                    purr { processor.apvts, "purr", "PURR" }, hiss { processor.apvts, "hiss", "HISS" }, alley { processor.apvts, "alley", "ALLEY" };
    cat::ParamCombo mode { processor.apvts, "mode", "mode", nullptr, 44 };
    juce::MidiKeyboardComponent keys { processor.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::uint32 lastTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeowCatEditor)
};
