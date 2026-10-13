#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace cat
{
/**
    MIDI Learn for every automatable parameter.

    Right-click a control -> "MIDI Learn", then move a controller. CCs reach the
    plug-in either directly or via Cubase's IMidiMapping (Quick Controls / remote
    devices), which JUCE converts back to MIDI CC before processBlock.
    Learned CCs are consumed (not forwarded to the next plug-in in the chain).
    Parameter changes are applied on the message thread with proper gestures,
    so Cubase records them as automation.
*/
class MidiLearnManager : private juce::Timer
{
public:
    explicit MidiLearnManager (juce::AudioProcessor& processor);
    ~MidiLearnManager() override;

    void startLearning (const juce::String& paramID);
    void cancelLearning();
    bool isLearning (const juce::String& paramID) const;
    void clearMapping (const juce::String& paramID);
    int getCCForParam (const juce::String& paramID) const;

    /** Audio thread: consumes learned CCs from the buffer. */
    void processMidi (juce::MidiBuffer& midi);

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

    static inline const juce::Identifier treeId { "MIDILEARN" };

private:
    void timerCallback() override;
    int indexOf (const juce::String& paramID) const;

    juce::AudioProcessor& processor;
    juce::StringArray paramIDs;
    juce::Array<juce::RangedAudioParameter*> params;

    std::array<std::atomic<int>, 128> ccToParam;
    std::atomic<int> learningParam { -1 };

    juce::AbstractFifo fifo { 1024 };
    std::array<std::pair<int, float>, 1024> fifoData {};
    juce::MidiBuffer scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};

} // namespace cat
