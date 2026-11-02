#include "MtsEsp.h"
#include "../../ThirdParty/MTS-ESP/Master/libMTSMaster.h"

namespace micro
{
bool MtsMaster::libraryInstalled() { return MTS_LibraryLoaded(); }

void MtsMaster::setEnabled (bool on)
{
    if (on == enabled) return;
    enabled = on;
    if (on) poll();
    else if (registered)
    {
        MTS_DeregisterMaster();
        registered = false;
    }
}

void MtsMaster::poll()
{
    if (! enabled || registered || ! libraryInstalled()) return;
    if (! MTS_CanRegisterMaster()) return;
    MTS_RegisterMaster();
    registered = true;
    resend();
}

void MtsMaster::send (const std::array<double, 128>& hz, const std::array<bool, 128>& valid, const juce::String& scaleName)
{
    lastHz = hz;
    lastValid = valid;
    lastName = scaleName;
    haveTable = true;
    resend();
}

void MtsMaster::resend()
{
    if (! registered || ! haveTable) return;
    MTS_SetNoteTunings (lastHz.data());
    MTS_ClearNoteFilter();
    for (int k = 0; k < 128; ++k)
        if (! lastValid[(size_t) k]) MTS_FilterNote (true, (char) k, -1);
    MTS_SetScaleName (lastName.substring (0, 60).toRawUTF8());
}

MtsMaster::State MtsMaster::state() const
{
    if (! enabled) return State::Off;
    if (! libraryInstalled()) return State::NoLibrary;
    return registered ? State::Connected : State::OtherMaster;
}

int MtsMaster::numClients() const { return registered ? MTS_GetNumClients() : 0; }

bool MtsMaster::canReset() const { return state() == State::OtherMaster && MTS_HasIPC(); }

void MtsMaster::reset()
{
    if (! enabled || registered || ! libraryInstalled()) return;
    MTS_Reinitialize();
    poll();
}

juce::String MtsMaster::statusText() const
{
    switch (state())
    {
        case State::Off:         return "MTS-ESP is off";
        case State::NoLibrary:   return "MTS-ESP library not installed: run the Cat Suite installer again (it offers MTS-ESP), or install ODDSound's free MTS-ESP library";
        case State::OtherMaster: return "Another MTS-ESP master is running (e.g. another Micro-Cat or MTS-ESP Mini): close it to take over";
        case State::Connected:
        {
            const int n = numClients();
            return "MTS-ESP: tuning " + juce::String (n) + (n == 1 ? " synth" : " synths") + " (no MIDI routing needed)";
        }
    }
    return {};
}
} // namespace micro
