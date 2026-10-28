#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Controller.h"

/**
    Cat Box 2's screen (800 x 480, the 5" display in the box) and, on the PC, the front panel
    around it: 12 knobs, 20 buttons, 16 pads, the slider and the joystick. Calm and readable:
    grey on black, one accent colour, big type; the 12 knobs are always on screen with their
    names and values. Everything on the screen can be touched (tracks, modes, pages, knobs).
*/
namespace catbox
{
class View : public juce::Component, private juce::Timer
{
public:
    View (Controller&, bool kioskScreenOnly);
    ~View() override;

    static constexpr int screenW = 800, screenH = 480;
    static constexpr int panelW = 1300, panelH = 860;

    juce::Rectangle<int> logicalBounds() const { return kiosk ? juce::Rectangle<int> (screenW, screenH) : juce::Rectangle<int> (panelW, panelH); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

    std::function<void()> onSettings, onToggleFullScreen;
    std::function<juce::String()> linkInfo;

    /** For the self test: draws the screen alone. */
    juce::Image snapshotScreen();
    void setBigScope (bool b) { bigScope = b; }
    void setDrawMode (bool b) { drawMode = b; }
    bool isDrawMode() const noexcept { return drawMode; }
    /** The self test: a stroke drawn on the SCOPE app's pad (points in -1..1, y up), as the mouse would. */
    void drawStroke (const std::vector<juce::Point<float>>&);
    /** The box's screen (kiosk) without the hardware: the live SYNTH page for the self test. */

private:
    void timerCallback() override;

    // drawing (in screen coordinates, 800 x 480)
    void drawScreen (juce::Graphics&);
    void drawHeader (juce::Graphics&, juce::Rectangle<int>);
    void drawTabs (juce::Graphics&, juce::Rectangle<int>);
    void drawContent (juce::Graphics&, juce::Rectangle<int>);
    void drawKnobs (juce::Graphics&, juce::Rectangle<int>);
    void drawTracks (juce::Graphics&, juce::Rectangle<int>);
    void drawScope (juce::Graphics&, juce::Rectangle<int>);
    void drawSound (juce::Graphics&, juce::Rectangle<int>);
    void drawSampler (juce::Graphics&, juce::Rectangle<int>);
    void drawBrowser (juce::Graphics&, juce::Rectangle<int>);
    void drawSeq (juce::Graphics&, juce::Rectangle<int>);
    void drawFx (juce::Graphics&, juce::Rectangle<int>);
    void drawMix (juce::Graphics&, juce::Rectangle<int>);
    void drawProject (juce::Graphics&, juce::Rectangle<int>);
    void drawScenes (juce::Graphics&, juce::Rectangle<int>);
    void drawLearn (juce::Graphics&, juce::Rectangle<int>);
    void drawHelp (juce::Graphics&, juce::Rectangle<int>);
    void drawMenu (juce::Graphics&, juce::Rectangle<int>);
    void drawChain (juce::Graphics&, juce::Rectangle<int>);
    void drawMacros (juce::Graphics&, juce::Rectangle<int>);
    void drawMods (juce::Graphics&, juce::Rectangle<int>);
    void drawMy (juce::Graphics&, juce::Rectangle<int>);
    void drawReturns (juce::Graphics&, juce::Rectangle<int>);
    void drawSynthPanel (juce::Graphics&, juce::Rectangle<int>);
    // the box's own screen (the pots are labelled on the panel): the sound alive, the filter and envelopes, the knob turned
    void drawSynthLive (juce::Graphics&, juce::Rectangle<int>);
    void drawPolarScope (juce::Graphics&, juce::Rectangle<int>, juce::Colour);
    /** Oscilloscope graphics: left = x, right = y, on a green phosphor screen that glows and fades like a CRT. */
    void drawXYScope (juce::Graphics&, juce::Rectangle<int>);
    juce::Image phosphor;
    float xyScale = 1.0f;
    bool bigScope = false;
    juce::Rectangle<int> scopeHit;
    void drawSpectrum (juce::Graphics&, juce::Rectangle<int>, juce::Colour);
    void drawFilterCurve (juce::Graphics&, juce::Rectangle<int>, juce::Colour);
    void drawEnvelope (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, float a, float d, float s, float r, juce::Colour);
    void drawTouch (juce::Graphics&, juce::Rectangle<int>, bool big);
    std::vector<juce::Path> polarTrail;
    std::array<float, 48> spectrum {};
    void drawDrumMachine (juce::Graphics&, juce::Rectangle<int>);
    // the apps
    void drawHome (juce::Graphics&, juce::Rectangle<int>);
    void drawAppIcon (juce::Graphics&, int app, juce::Rectangle<float>, juce::Colour);
    void drawScopeApp (juce::Graphics&, juce::Rectangle<int>);
    void showTextDialog();
    static juce::Colour appColour (int app);
    static constexpr int ActHome = 100, ActApp0 = 200, ActMode0 = 300, ActAudioMidi = 400, ActPic0 = 500,
                         ActHold = 600, ActText = 601, ActDraw = 602, ActClear = 603, ActBig = 604, ActUndoStroke = 605;
    bool drawMode = false;                 // SCOPE: the mouse / finger draws on the scope
    juce::Rectangle<int> drawPad;
    std::vector<float> drawPts;
    bool synthPanelShown() const;

    // the PC panel (in panel coordinates)
    void drawPanel (juce::Graphics&);
    juce::Rectangle<float> knobRect (int k) const;
    juce::Rectangle<float> buttonRect (int b) const;
    juce::Rectangle<float> padRect (int p) const;
    juce::Rectangle<float> sliderRect() const;
    juce::Rectangle<float> joyRect() const;
    juce::Rectangle<int> screenRect() const;   // where the screen is, in panel coordinates

    // hit testing
    juce::Point<float> toLogical (juce::Point<float>) const;
    bool screenTouch (juce::Point<float> screenPos, bool down);
    int screenKnobAt (juce::Point<float> screenPos) const;

    Controller& box;
    const bool kiosk;
    float scale = 1.0f;
    juce::Point<float> offset;

    // the screen's touch areas (screen coordinates)
    juce::Array<juce::Rectangle<int>> trackHits, modeHits, pageHits, menuHits;
    struct BrowseHit { juce::Rectangle<int> r; int column, index; };
    juce::Array<BrowseHit> browseHits;
    std::array<juce::Rectangle<int>, 3> browseColumns;
    juce::Rectangle<int> soundNameHit, modelHit;
    struct PanelHit { juce::Rectangle<int> r; int param; };
    juce::Array<PanelHit> panelHits;
    struct SeqHit { juce::Rectangle<int> r; int lane, step; };
    juce::Array<SeqHit> seqHits;               // drum cells (lane >= 0), melodic steps (lane = -1), lane names (step = -1)
    struct ActionHit { juce::Rectangle<int> r; int action; };
    juce::Array<ActionHit> actionHits, screenHits;
    int panelActive = -1, panelHover = -1;
    bool panelMoved = false;
    int hoverColumn = -1, hoverIndex = -1;
    void mouseMove (const juce::MouseEvent&) override;
    juce::Rectangle<int> playHit, learnHit, skipHit, doneHit, knobArea;

    enum DragKind { DragNone, DragKnob, DragSlider, DragJoy, DragPad, DragButton, DragPanel, DragDraw };
    DragKind drag = DragNone;
    int dragIndex = -1;
    float dragLastY = 0.0f;
    bool shiftLatchedByTouch = false;
    std::array<bool, numPads> keyPadDown {};
    std::array<bool, NumButtons> keyButtonDown {};
    float sliderPos = 0.5f;
    juce::Point<float> joyPos;

    std::array<float, numTracks> meters {};
    std::array<float, numTracks> hitGlow {};
    std::array<float, 16> laneGlow {};
    float phase = 0.0f;
};
} // namespace catbox
