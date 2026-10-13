#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace cat::midiexport
{
/**
    Writes a Standard MIDI File (type 1, 960 PPQ) to the temp folder.
    The sequence timestamps are in quarter notes (ppq). Text / marker meta events
    in the sequence are kept, which Cubase imports as markers / chord names.
*/
juce::File writeMidiFile (const juce::MidiMessageSequence& sequenceInPpq,
                          double bpm,
                          const juce::String& baseName,
                          int ticksPerQuarter = 960);

/** Starts an OS drag of the file (drop it onto a Cubase track or the project window). */
bool dragFileOut (const juce::File& file, juce::Component* sourceComponent);

/** Reads a MIDI file and returns all tracks merged, timestamps converted to ppq. */
juce::MidiMessageSequence readMidiFileAsPpq (const juce::File& file);

} // namespace cat::midiexport
