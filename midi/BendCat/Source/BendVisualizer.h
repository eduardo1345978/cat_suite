#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class BendCatProcessor;

/** Real-time oscilloscope of the pitch bend sent on each voice / channel. */
class BendVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit BendVisualizer (BendCatProcessor& p);
    void paint (juce::Graphics&) override;

    /** Compact mode draws only the traces (used in the header strip). */
    void setCompact (bool c) { compact = c; repaint(); }

private:
    void timerCallback() override;

    static constexpr int historySize = 300;
    BendCatProcessor& processor;
    std::array<std::array<float, historySize>, 16> history {};
    std::array<bool, 16> wasActive {};
    int writePos = 0;
    bool compact = false;
};
