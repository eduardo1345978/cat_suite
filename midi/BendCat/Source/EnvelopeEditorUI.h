#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BendCatEngine.h"

class BendCatProcessor;

/** 3 x 9 grid of bend shapes (Bendimist-style palette). Click loads the shape into the envelope. */
class ShapePalette : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ShapePalette (BendCatProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int index) const;
    BendCatProcessor& processor;
    std::vector<bendcat::BendEnvelope> shapes;
    int hover = -1, selected = -1;
};

/**
    Draggable multi-node envelope editor. The shape is drawn repeated `cycles` times;
    editing any repetition edits the shared shape.
      drag node            move it (first/last nodes stay at the ends)
      double-click         add a node / remove a node
      right-click node     remove it
      drag between nodes   bend the segment's curve
*/
class EnvelopeEditor : public juce::Component, private juce::Timer
{
public:
    explicit EnvelopeEditor (BendCatProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refresh();

private:
    void timerCallback() override;
    int cycles() const;
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float t, float v, int cycle) const;
    std::pair<float, float> fromScreen (juce::Point<float> p) const; // (local time, value)
    int hitNode (juce::Point<float> p) const;
    int segmentAt (float localT) const;
    void commit();

    BendCatProcessor& processor;
    bendcat::BendEnvelope env;
    uint32_t shownVersion = 0;
    int dragNode = -1, dragSegment = -1;
    float dragStartCurve = 0.0f, dragStartY = 0.0f;
    float playhead = -1.0f;
};

/** 16-step bend pattern (-50..+50 cents per step). */
class BendPatternEditor : public juce::Component, private juce::Timer
{
public:
    explicit BendPatternEditor (BendCatProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setFromMouse (juce::Point<float> p);
    BendCatProcessor& processor;
    std::array<float, 16> cents {};
    uint32_t shownVersion = 0;
};

/** Draws the glide curve or LFO wave for the current settings. */
class CurvePreview : public juce::Component, private juce::Timer
{
public:
    enum class Kind { Glide, LFO };
    CurvePreview (BendCatProcessor& p, Kind k);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    BendCatProcessor& processor;
    Kind kind;
};
