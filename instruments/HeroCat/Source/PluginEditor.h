#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/** The game screen: the chord's name, the notes falling as pixels, an "MP" bar for the level. */
class ScreenView : public juce::Component
{
public:
    explicit ScreenView (HeroCatProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    HeroCatProcessor& processor;
    struct Pixel { float x, y, vy, life; juce::Colour c; int size; };
    std::vector<Pixel> pixels;
    struct Star { float x, y; int phase; };
    std::vector<Star> stars;
    int seenLog = 0, frame = 0;
    float meter = 0.0f;
};

/** The drawn arp pattern: 16 pixel columns, drag up / down for loudness, double-click on / off. */
class PatternView : public juce::Component
{
public:
    explicit PatternView (HeroCatProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void tick() { repaint(); }

private:
    void setFrom (juce::Point<float>);
    HeroCatProcessor& processor;
};

class HeroCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit HeroCatEditor (HeroCatProcessor&);
    ~HeroCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    HeroCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "" };
    ScreenView screen { processor };
    PatternView pattern { processor };
    juce::TextButton dice { "DICE" }, clear { "CLEAR" };

    cat::ParamCombo srcA { processor.apvts, "srcA", "A", nullptr, 22 }, srcB { processor.apvts, "srcB", "B", nullptr, 22 },
                    octB { processor.apvts, "octB", "oct", nullptr, 30 },
                    chordMode { processor.apvts, "chordMode", "chord", nullptr, 50 }, key { processor.apvts, "key", "key", nullptr, 34 },
                    scale { processor.apvts, "scale", "scale", nullptr, 42 },
                    arpMode { processor.apvts, "arpMode", "arp", nullptr, 50 }, rate { processor.apvts, "rate", "rate", nullptr, 34 },
                    octaves { processor.apvts, "octaves", "oct", nullptr, 30 },
                    texture { processor.apvts, "texture", "texture", nullptr, 56 }, lfoShape { processor.apvts, "lfoShape", "lfo", nullptr, 34 },
                    lfoTarget { processor.apvts, "lfoTarget", "to", nullptr, 22 };
    cat::LineSlider mix { processor.apvts, "mix", "A / B" }, detune { processor.apvts, "detune", "DETUNE" },
                    cutoff { processor.apvts, "cutoff", "CUTOFF" }, reso { processor.apvts, "reso", "RESO" },
                    envAmt { processor.apvts, "envAmt", "F.ENV" }, vibrato { processor.apvts, "vibrato", "VIBRATO" },
                    attack { processor.apvts, "attack", "ATTACK" }, decay { processor.apvts, "decay", "DECAY" },
                    sustain { processor.apvts, "sustain", "SUSTAIN" }, release { processor.apvts, "release", "RELEASE" },
                    glide { processor.apvts, "glide", "GLIDE" },
                    strum { processor.apvts, "strum", "STRUM" }, gate { processor.apvts, "gate", "GATE" },
                    swing { processor.apvts, "swing", "SWING" }, chaos { processor.apvts, "chaos", "CHAOS" },
                    texLevel { processor.apvts, "texLevel", "TEXTURE" }, lfoRate { processor.apvts, "lfoRate", "LFO RATE" },
                    lfoDepth { processor.apvts, "lfoDepth", "LFO DEPTH" }, drive { processor.apvts, "drive", "DRIVE" },
                    crush { processor.apvts, "crush", "CRUSH" }, chorus { processor.apvts, "chorus", "CHORUS" },
                    delay { processor.apvts, "delay", "DELAY" }, reverb { processor.apvts, "reverb", "REVERB" },
                    width { processor.apvts, "width", "WIDTH" }, output { processor.apvts, "output", "OUTPUT" };
    cat::ParamToggle voiceLead { processor.apvts, "voiceLead", "VOICE LEAD" }, latch { processor.apvts, "latch", "LATCH" },
                     midiOut { processor.apvts, "midiOut", "MIDI OUT" };
    juce::MidiKeyboardComponent keys { processor.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeroCatEditor)
};
