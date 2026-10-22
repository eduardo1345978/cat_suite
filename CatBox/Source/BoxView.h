#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BoxController.h"

namespace catbox
{
/**
    The Cat Box on screen.

    Kiosk mode (on the Raspberry Pi): only the 5" display, 800 x 480, full screen; the real
    knobs, pads and buttons do the rest.
    Desktop mode (on the PC): the display plus the whole front panel underneath it - drag or
    scroll the knobs, click the pads and buttons, or use the computer keyboard:
      pads   1 2 3 4 / Q W E R / A S D F / Z X C V   (Y works as Z)
      Space = Play, B = Steps, Shift = Shift, T = Tap, Tab = Instrument, , . = Oct- Oct+, Enter = Rec
*/
class BoxView : public juce::Component, private juce::Timer
{
public:
    BoxView (BoxController&, bool kioskMode);
    ~BoxView() override;

    std::function<void()> onSettings;      // right-click menu: sound card / MIDI
    std::function<void()> onToggleFullScreen;

    static constexpr int screenW = 800, screenH = 480, panelH = 450;
    juce::Rectangle<int> logicalBounds() const { return { 0, 0, screenW, kiosk ? screenH : screenH + panelH }; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

    static juce::Colour instrumentColour (int instrument);

    struct Palette
    {
        juce::Colour bg, screen, cell, text, dim, faint, padOff;
    };

private:
    void timerCallback() override;
    juce::Point<float> toLogical (juce::Point<float>) const;
    const Palette& pal() const;

    void paintScreen (juce::Graphics&, juce::Rectangle<float>);
    void paintKnobCell (juce::Graphics&, juce::Rectangle<float>, int knob);
    void paintBottom (juce::Graphics&, juce::Rectangle<float>);
    void paintStepRow (juce::Graphics&, juce::Rectangle<float>, int instrument);
    void paintScope (juce::Graphics&, juce::Rectangle<float>);
    void paintPanel (juce::Graphics&, juce::Rectangle<float>);
    void paintPad (juce::Graphics&, juce::Rectangle<float>, int pad, bool big);

    juce::Colour padColour (int pad, bool& lit, bool& outlined, bool& thick) const;
    juce::String padLabel (int pad) const;

    juce::Rectangle<float> knobArea (int knob) const;
    juce::Rectangle<float> padArea (int pad) const;
    juce::Rectangle<float> buttonArea (int button) const;

    BoxController& box;
    const bool kiosk;
    float scale = 1.0f;
    juce::Point<float> offset;

    std::array<float, numPads> flashLevel {};
    uint32_t pressedPads = 0;          // mouse / keyboard presses, for instant feedback
    std::array<bool, numPads> keyPadDown {};
    std::array<bool, NumButtons> keyButtonDown {};

    enum class Drag { None, Knob, Pad, Button } drag = Drag::None;
    int dragIndex = -1;
    float lastDragY = 0.0f;
    bool mouseShiftLatched = false;
};
} // namespace catbox
