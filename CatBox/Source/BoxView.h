#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "BoxController.h"

namespace catbox
{
/**
    The Cat Box on screen. Kept simple: the box is played with its knobs and pads, the screen
    only shows what they do and what you hear.

    Kiosk mode (on the Raspberry Pi): only the 5" display, 800 x 480, full screen.
    Desktop mode (on the PC): the display plus the front panel underneath it (12 knobs,
    16 pads in two rows of 8, 8 buttons). Drag or scroll the knobs, click pads and buttons,
    or use the computer keyboard:
      pads   1 2 3 4 5 6 7 8 / Q W E R T Y U I   (Z works as Y)
      Space = Play, B = Steps, Shift = Shift, N = Scene, Tab = Instr, , . = Oct- Oct+, Enter = Rec, C = Chaos
*/
class BoxView : public juce::Component, private juce::Timer
{
public:
    BoxView (BoxController&, bool kioskMode);
    ~BoxView() override;

    std::function<void()> onSettings;      // right-click menu: sound card / MIDI
    std::function<void()> onToggleFullScreen;
    std::function<juce::String()> linkInfo;   // e.g. "Link: 192.168.1.23" (the box shows it with Shift)

    /** The simple 5" screen of the box (always in kiosk mode; on the PC from the right-click menu). */
    void setBoxScreen (bool b) { boxPreview = b; repaint(); }
    bool isBoxScreen() const noexcept { return kiosk || boxPreview; }

    static constexpr int screenW = 800, screenH = 480, panelH = 440;
    juce::Rectangle<int> logicalBounds() const { return { 0, 0, screenW, kiosk ? screenH : screenH + panelH }; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

    static juce::Colour pageColour (int page);

    struct Palette
    {
        juce::Colour bg, screen, cell, text, dim, faint;
    };

private:
    void timerCallback() override;
    void updateSpectrum();
    juce::Point<float> toLogical (juce::Point<float>) const;
    const Palette& pal() const;

    void paintScreen (juce::Graphics&, juce::Rectangle<float>);
    void paintBoxScreen (juce::Graphics&, juce::Rectangle<float>);
    void paintScope (juce::Graphics&, juce::Rectangle<float>);
    void paintEngineGrid (juce::Graphics&, juce::Rectangle<float>);
    void paintOrbit (juce::Graphics&, juce::Rectangle<float>);
    void paintSlotGrid (juce::Graphics&, juce::Rectangle<float>);
    void paintStrip (juce::Graphics&, juce::Rectangle<float>);
    juce::String loadedName() const;
    void paintKnobCell (juce::Graphics&, juce::Rectangle<float>, int knob);
    void paintBottom (juce::Graphics&, juce::Rectangle<float>);
    void paintSteps (juce::Graphics&, juce::Rectangle<float>);
    void paintPerfGrid (juce::Graphics&, juce::Rectangle<float>);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float>);
    void paintPanel (juce::Graphics&, juce::Rectangle<float>);
    void paintPad (juce::Graphics&, juce::Rectangle<float>, int pad);

    void paintHelp (juce::Graphics&, juce::Rectangle<float>);
    void paintChaosBar (juce::Graphics&, juce::Rectangle<float>, float progress);
    float chaosProgress() const;
    void setButtonPressed (int button, bool down);
    int knobAt (juce::Point<float>) const;

    juce::Colour padColour (int pad, bool& lit, bool& outlined, bool& thick) const;
    juce::String padLabel (int pad) const;
    juce::String presetTitle() const;

    juce::Rectangle<float> tabArea (int page) const;
    juce::Rectangle<float> knobArea (int knob) const;
    juce::Rectangle<float> padArea (int pad) const;
    juce::Rectangle<float> buttonArea (int button) const;

    BoxController& box;
    const bool kiosk;
    float scale = 1.0f;
    juce::Point<float> offset;

    std::array<float, numPads> flashLevel {};
    std::array<float, orbitcat::maxBodies> planetGlow {};
    uint32_t pressedPads = 0;          // mouse / keyboard presses, for instant feedback
    std::array<bool, numPads> keyPadDown {};
    std::array<bool, NumButtons> keyButtonDown {};

    static constexpr int fftOrder = 10, numBands = 40;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> fftData;
    std::array<float, numBands> bands {};

    enum class Drag { None, Knob, Pad, Button } drag = Drag::None;
    int dragIndex = -1;
    float lastDragY = 0.0f;
    bool mouseShiftLatched = false;
    bool boxPreview = false;
    bool helpShown = false;            // F1: the keys and controls
    int hoverKnob = -1;                // the PC panel shows this knob's value instead of its name
    uint32_t pressedButtons = 0;       // held buttons look pressed
    juce::uint32 chaosPressedAt = 0;   // linked to a box, the bar is timed here

public:
    void showHelp (bool b) { helpShown = b; repaint(); }
    bool isHelpShown() const noexcept { return helpShown; }
};
} // namespace catbox
