#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"

/**
    Bounce display: every bouncing note is a little cat on a stick above its key. The cat
    jumps along the same gaps the notes are played on (shorter / longer with Curve, lower as
    the velocity fades) and the key lights up each time it lands.
*/
class BounceView : public juce::Component, private juce::Timer
{
public:
    explicit BounceView (PattCatProcessor& p);
    void paint (juce::Graphics&) override;

    static constexpr int lowNote = 24, highNote = 96;   // C0 .. C6 (Cubase naming)

private:
    struct Cat
    {
        PattCatProcessor::BounceEvent e;
        double endMs = 0.0;
    };

    void timerCallback() override;
    juce::Rectangle<float> keyBounds (int note) const;
    static bool isBlack (int note);
    static juce::Colour colourFor (int note);
    /** Height (0..1) of a cat at a time, and whether it just landed. */
    static float heightAt (const PattCatProcessor::BounceEvent& e, double nowMs, bool& landed, double& endMs);
    void drawCat (juce::Graphics&, juce::Point<float> centre, float size, juce::Colour) const;

    PattCatProcessor& processor;
    std::vector<Cat> cats;
};
