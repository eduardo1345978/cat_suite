#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PattCatEngine.h"

class PattCatProcessor;

/** Shared state for the grid widgets (selected layer/step, playhead). */
struct GridContext
{
    PattCatProcessor& processor;
    pattcat::PattCatEngine engine;   // UI copy, refreshed when the processor's version changes
    int selectedStep = 0;
    std::vector<int> playheadSteps = std::vector<int> (4, -1);
    double playheadPpq = -1.0;
    double barLength = 4.0;
    std::function<void()> onSelectionChanged;

    explicit GridContext (PattCatProcessor& p) : processor (p) {}
};

//==============================================================================
/** Patternist-style overview: notes of all layers drawn on voice lanes with a playhead. */
class PatternOverview : public juce::Component
{
public:
    explicit PatternOverview (GridContext& context) : ctx (context) {}
    void paint (juce::Graphics&) override;
    void setChordSize (int n) { chordSize = juce::jlimit (1, 8, n); }

private:
    double displayLength() const;
    GridContext& ctx;
    int chordSize = 4;
};

//==============================================================================
/** 32-step grid for the current layer with velocity and gate lanes. */
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (GridContext& context) : ctx (context) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Lane { Steps, Velocity, Gate, None };
    Lane laneAt (float y) const;
    int stepAt (float x) const;
    juce::Rectangle<float> laneBounds (Lane) const;
    void showStepMenu (int step);
    void edit (const std::function<void (pattcat::PatternLayer&)>& fn);

    GridContext& ctx;
    Lane dragLane = Lane::None;
    bool paintValue = true;
    int lastPaintedStep = -1;
};

//==============================================================================
/** Grid of preset thumbnails. */
class PresetGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetGrid (GridContext& context);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cellBounds (int index) const;
    GridContext& ctx;
    std::vector<pattcat::PattCatEngine> thumbs;
    int hover = -1;
    static constexpr int columns = 10;
};
