#pragma once

#include "PluginProcessor.h"
#include "PresetBar.h"

/** The knobs and the footswitch drawn like a boutique pedal (cream knobs, a chrome switch). */
class BoardLaf : public juce::LookAndFeel_V4
{
public:
    BoardLaf();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
};

/** One pedal: its own colour, hammered paint, two knobs, a picture that moves with the sound, an LED and a footswitch. */
class PedalView : public juce::Component, public juce::SettableTooltipClient
{
public:
    PedalView (PolyBoardProcessor&, int pedal, std::function<void (PedalView&, const juce::MouseEvent&, int phase)> onDrag);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    const int pedal;
    float time = 0.0f, work = 0.0f;

private:
    struct Footswitch : juce::Button
    {
        Footswitch() : juce::Button ("footswitch") {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };
    PolyBoardProcessor& processor;
    juce::Slider knobA, knobB;
    Footswitch footswitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attA, attB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attOn;
    std::function<void (PedalView&, const juce::MouseEvent&, int)> onDrag;
    void drawPicture (juce::Graphics&, juce::Rectangle<float>, juce::Colour) const;
};

class PolyBoardEditor : public juce::Component, private juce::Timer
{
public:
    explicit PolyBoardEditor (PolyBoardProcessor&);
    ~PolyBoardEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutPedals (bool animate);
    void dragPedal (PedalView&, const juce::MouseEvent&, int phase);
    juce::Rectangle<int> slotBounds (int slot) const;

    PolyBoardProcessor& processor;
    BoardLaf laf;
    juce::TooltipWindow tips { this, 500 };
    cat::PresetBar presetBar { processor.presetManager, "" };
    juce::OwnedArray<PedalView> pedals;
    juce::Slider input, output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inA, outA;
    PedalView* dragging = nullptr;
    int dragFrom = -1;
    std::array<int, board::numPedals> shownOrder {};
    float time = 0.0f, inLevel = 0.0f, outLevel = 0.0f, tunerHz = 0.0f;
    int tunerCountdown = 0;
    juce::uint32 lastTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyBoardEditor)
};
