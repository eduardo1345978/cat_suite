#pragma once

#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>

namespace cat
{
/**
    Cat Link: lets the Cats loaded in the same host talk to each other.

    Every Cat is its own plug-in binary, so they share a small XML file in the temp folder
    (guarded by an inter-process lock) and poll it 4x a second on the message thread:
      - GROOVE: Drum-Cat publishes swing / human timing / grid; Cats with "Follow Drum" take it over.
      - CHAIN:  picking a chain preset in one Cat makes every other Cat load its part of it.
*/
class CatLink : private juce::Timer
{
public:
    struct Groove
    {
        float swing = 0.0f;   // 0..100 %: off-beat 16ths move later by swing/100 * half a 16th
        float human = 0.0f;   // ms of timing looseness
        int grid = 1;         // 0 = 1/8, 1 = 1/16, 2 = 1/32
        juce::int64 stamp = 0;
        bool valid = false;
    };

    explicit CatLink (juce::String pluginName);
    ~CatLink() override;

    void publishGroove (float swing, float human, int grid);
    void publishChain (const juce::String& chainName);

    /** Most recent groove published by any Drum-Cat (valid == false if none yet). */
    Groove latestGroove() const { return groove; }

    std::function<void (const Groove&)> onGroove;           // a new groove arrived (from another instance)
    std::function<void (const juce::String&)> onChain;      // another Cat picked a chain
    std::function<void()> onTick;                           // called on every poll (for publishers)

    static juce::File linkFile();

private:
    void timerCallback() override;
    void write (const std::function<void (juce::XmlElement&)>& modify);

    juce::String plugin, instanceId;
    juce::int64 lastChainStamp = 0, lastGrooveStamp = 0;
    Groove groove;
};

//==============================================================================
/** Chain presets: one name, one factory preset per Cat (matched by preset name). */
struct ChainPreset
{
    const char* name;
    std::vector<std::pair<const char*, const char*>> parts;   // (plug-in, factory preset name)
};

const std::vector<ChainPreset>& chainPresets();
juce::String chainPartFor (const juce::String& chain, const juce::String& plugin);

} // namespace cat
