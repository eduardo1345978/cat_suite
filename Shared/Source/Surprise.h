#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CatLookAndFeel.h"

namespace cat
{
/**
    "Surprise" button with one-step undo, shared by all plug-ins.

    Surprise saves the plug-in's complete state, then calls the plug-in's randomiser.
    The undo button restores exactly that state (sound, pattern, progression, mappings).
    The snapshot lives in the processor, so it survives closing and reopening the editor.
*/
class SurpriseControls : public juce::Component, private juce::Timer
{
public:
    SurpriseControls (juce::AudioProcessor& processor, juce::MemoryBlock& undoSnapshot, std::function<void()> randomise);
    void resized() override;

    /** Helper for randomisers: sets a parameter to a real (unnormalised) value as one host gesture. */
    static void setParam (juce::AudioProcessor& processor, const juce::String& paramID, float value);

private:
    struct UndoIcon : public juce::Button
    {
        UndoIcon() : Button ("Undo Surprise") {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    void timerCallback() override;

    juce::AudioProcessor& processor;
    juce::MemoryBlock& snapshot;
    std::function<void()> randomise;
    juce::TextButton surpriseButton { "Surprise" };
    UndoIcon undoButton;
};

} // namespace cat
