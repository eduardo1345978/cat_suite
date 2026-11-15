#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The sky: the choir's halo in the middle (the chord's name inside), four planets on their orbits. */
class SkyView : public juce::Component
{
public:
    explicit SkyView (HaloCatProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    HaloCatProcessor& processor;
    struct Star { float x, y, size, twinkle; };
    std::vector<Star> stars;
    float glow = 0.0f, time = 0.0f;
};

class HaloCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit HaloCatEditor (HaloCatProcessor&);
    ~HaloCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    HaloCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "" };
    SkyView sky { processor };
    cat::ParamCombo progression { processor.apvts, "progression", "chords", nullptr, 50 },
                    change { processor.apvts, "change", "every", nullptr, 40 },
                    orbits { processor.apvts, "orbits", "orbits", nullptr, 46 };
    cat::LineSlider choir { processor.apvts, "choir", "CHOIR" }, vowel { processor.apvts, "vowel", "VOWEL" },
                    glide { processor.apvts, "glide", "GLIDE" }, bells { processor.apvts, "bells", "BELLS" },
                    spin { processor.apvts, "spin", "SPIN" }, dust { processor.apvts, "dust", "STAR DUST" },
                    shimmer { processor.apvts, "shimmer", "SHIMMER" }, brightness { processor.apvts, "brightness", "BRIGHT" },
                    output { processor.apvts, "output", "OUTPUT" };
    juce::MidiKeyboardComponent keys { processor.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaloCatEditor)
};
