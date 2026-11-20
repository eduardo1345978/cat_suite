#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/**
    Chop-Cat looks like an old TV playing a worn VHS tape: the picture tears, shakes and splits into colours
    whenever the sound glitches. Inside: a monitor (the sound and this bar's glitches), the 16-step glitch sequencer
    with a brush palette, 12 live pads, and the controls.
*/
namespace chopui
{
juce::Colour fxColour (int fx);
}

/** The monitor: the output as a scrolling waveform coloured by the glitch, this bar's steps, a big OSD label. */
class Monitor : public juce::Component
{
public:
    explicit Monitor (ChopCatProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void tick();

private:
    void paintContent (juce::Graphics&);
    ChopCatProcessor& processor;
    juce::Image img;
    juce::Random rng;
    int frame = 0, labelFx = 0, labelFrames = 0;
};

/** The 16 steps: click to paint with the brush (click the same glitch again to erase), drag to paint many, right-click for a list. */
class SeqView : public juce::Component, public juce::SettableTooltipClient
{
public:
    SeqView (ChopCatProcessor& p, int& brushRef) : processor (p), brush (brushRef) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    int cellAt (juce::Point<float>) const;
    ChopCatProcessor& processor;
    int& brush;
    int paintWith = 0;
};

/** The brush palette: which glitch the sequencer paints (or the eraser). */
class BrushBar : public juce::Component
{
public:
    explicit BrushBar (int& brushRef) : brush (brushRef) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void()> onChange;

private:
    int& brush;
};

/** 12 pads: hold one and the glitch plays (also MIDI keys C..B). */
class PadGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PadGrid (ChopCatProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int padAt (juce::Point<float>) const;
    juce::Rectangle<int> padRect (int) const;
    ChopCatProcessor& processor;
    int held = -1;
};

/** A VHS on-screen-display button: white text in a box, inverted when pressed. */
class OsdButton : public juce::Button
{
public:
    OsdButton (const juce::String& name, float fontSize = 24.0f) : juce::Button (name), size (fontSize) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Colour accent { 0xfff2f2f2 };

private:
    float size;
};

/** The TV's screen: holds everything, and draws the scanlines, noise and OSD over it. It shakes when the sound glitches. */
class TvScreen : public juce::Component
{
public:
    explicit TvScreen (ChopCatProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    int frame = 0;
    float glitch = 0.0f;

private:
    ChopCatProcessor& processor;
    juce::Random rng;
};

class ChopCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit ChopCatEditor (ChopCatProcessor&);
    ~ChopCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    ChopCatProcessor& processor;
    int brush = chop::Rpt16;
    TvScreen screen { processor };
    cat::PresetBar presetBar { processor.presetManager, "" };
    Monitor monitor { processor };
    SeqView seq { processor, brush };
    BrushBar brushes { brush };
    PadGrid pads { processor };
    OsdButton craft { "CRAFT", 34.0f }, undo { "UNDO" }, dice { "DICE" }, clear { "CLEAR" };
    cat::ParamToggle seqOn { processor.apvts, "seqOn", "SEQUENCER" };
    cat::LineSlider chaos { processor.apvts, "chaos", "CHAOS" }, fill { processor.apvts, "fill", "FILL" },
                    seed { processor.apvts, "seed", "SEED" }, smooth { processor.apvts, "smooth", "SMOOTH" },
                    mix { processor.apvts, "mix", "MIX" }, output { processor.apvts, "output", "OUTPUT" };
    cat::ParamCombo lock { processor.apvts, "lock", "lock", nullptr, 38 }, quantize { processor.apvts, "quantize", "pads", nullptr, 38 };
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopCatEditor)
};
