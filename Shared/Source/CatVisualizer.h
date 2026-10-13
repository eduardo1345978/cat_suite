#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CatLookAndFeel.h"
#include "MidiScheduler.h"

namespace cat
{
/** Each plug-in has its own cat: same body, its own accessory. */
enum class CatStyle { Plain, Chord, Patt, Bend, Drum, Orbit, Talk, Bass, Glide, Sing };

/**
    Music-reactive cat silhouette (pure vector, stays sharp at any UI scale).

    Reacts to what the plug-in is playing (read lock-free from its NoteMonitor) and to the
    host's beat:
      - head bobs on every beat while the transport (or free-running clock) is running
      - ears twitch and the tail flicks on each new note, harder for higher velocities
      - the eyes glow with the overall playing energy
      - the head tilts towards the pitch of the notes being played
      - soft rings ripple out on note-ons
    When nothing plays it breathes, sways its tail and blinks now and then.
*/
class CatVisualizer : public juce::Component, private juce::Timer
{
public:
    CatVisualizer (NoteMonitor& monitor, std::function<double()> getPpq, std::function<bool()> isRunning,
                   CatStyle style = CatStyle::Plain);

    void paint (juce::Graphics&) override;

    /** Silhouette colour (defaults to a soft grey that sits quietly in the UI). */
    void setBodyColour (juce::Colour c) { bodyColour = c; }
    void setShowRings (bool shouldShow) { showRings = shouldShow; }

private:
    void timerCallback() override;
    void noteOn (int note, int velocity);
    void drawAccessory (juce::Graphics&, const juce::AffineTransform& toScreen, juce::Point<float> headCentre, float tailSway) const;

    NoteMonitor& monitor;
    CatStyle style;
    std::function<double()> ppq;
    std::function<bool()> running;

    std::array<uint8_t, 128> previous {};
    juce::Random rng;
    double lastTime = 0.0;
    float time = 0.0f;
    float energy = 0.0f, earKick = 0.0f, tailKick = 0.0f, bounce = 0.0f;
    float tailPhase = 0.0f, tilt = 0.0f, beatBob = 0.0f;
    float blink = 0.0f, nextBlink = 3.0f;
    double lastRingTime = -1.0;

    struct Ring { float age, strength; };
    std::vector<Ring> rings;

    juce::Colour bodyColour = Colours::raised.brighter (0.45f);
    bool showRings = true;
};

/**
    Boot screen shown over the editor when it opens: big reactive cat + logo, fades out after
    a moment (click to skip).
*/
class BootScreen : public juce::Component, private juce::Timer
{
public:
    BootScreen (NoteMonitor& monitor, std::function<double()> getPpq, std::function<bool()> isRunning,
                juce::String logoFirst, juce::String logoSecond, juce::String subtitle, CatStyle style = CatStyle::Plain);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    CatVisualizer cat;
    juce::String first, second, sub;
    double startTime = 0.0;
};

} // namespace cat
