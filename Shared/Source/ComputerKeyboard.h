#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace cat
{
/**
    A processor that can be played from the computer keyboard (in the standalone apps).
    The processor merges `keyboardState` into its MIDI input at the start of processBlock;
    inside a DAW nothing ever presses these keys, so it costs nothing.
*/
struct ComputerKeyboardTarget
{
    virtual ~ComputerKeyboardTarget() = default;
    juce::MidiKeyboardState keyboardState;

    void mergeComputerKeyboard (juce::MidiBuffer& midi, int numSamples)
    {
        keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    }
};

/**
    Strip shown at the bottom of a standalone app: turns the computer keyboard into a piano.

        W E   T Y U   O P          black keys   (Y and Z both work, for QWERTZ keyboards)
       A S D F G H J K L           white keys   C D E F G A B C D
       Left / Right  octave,  Down / Up  velocity
*/
class ComputerKeyboardBar : public juce::Component, public juce::SettableTooltipClient, public juce::KeyListener, private juce::Timer
{
public:
    explicit ComputerKeyboardBar (juce::MidiKeyboardState&);
    ~ComputerKeyboardBar() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged (bool isKeyDown, juce::Component*) override;

    static constexpr int height = 26;

    /** Semitone offset (from the octave's C) for a typed character, or -1. */
    static int semitoneFor (juce::juce_wchar c) noexcept;

private:
    void timerCallback() override;
    void releaseAll();
    static bool typingIntoText();

    juce::MidiKeyboardState& state;
    int octave = 5;                     // A = MIDI 60 (Cubase C3)
    int velocity = 100;
    struct Held { int keyCode; int note; };
    std::vector<Held> held;
};

} // namespace cat
