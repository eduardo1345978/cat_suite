#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace micro
{
/**
    MTS-ESP master (ODDSound's microtuning standard, like Ableton's): Micro-Cat sends its tuning to every
    synth that supports MTS-ESP (Surge XT, Vital, Pigments, Serum 2, u-he, Kontakt ...), with no MIDI
    routing and no MPE - one tuning for the whole project. Needs the free MTS-ESP library (libMTS),
    installed with the Cat Suite installer. Only one master can run at a time. Message thread only.
*/
class MtsMaster
{
public:
    enum class State { Off, NoLibrary, OtherMaster, Connected };

    MtsMaster() = default;
    ~MtsMaster() { setEnabled (false); }

    /** On: registers as the master (if no other master runs; tries again later otherwise). */
    void setEnabled (bool on);
    bool isEnabled() const noexcept { return enabled; }
    /** Call now and then (e.g. from a timer): registers once the other master is gone. */
    void poll();
    /** Every key's frequency; keys with valid = false are filtered (the synths don't play them). */
    void send (const std::array<double, 128>& hz, const std::array<bool, 128>& valid, const juce::String& scaleName);

    State state() const;
    int numClients() const;
    /** Another master is stuck (a crashed host left it registered): reset MTS-ESP and take over. */
    bool canReset() const;
    void reset();
    juce::String statusText() const;
    static bool libraryInstalled();

private:
    void resend();
    bool enabled = false, registered = false, haveTable = false;
    std::array<double, 128> lastHz {};
    std::array<bool, 128> lastValid {};
    juce::String lastName;
};
} // namespace micro
