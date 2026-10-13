#include "MidiExport.h"

namespace cat::midiexport
{
juce::File writeMidiFile (const juce::MidiMessageSequence& sequenceInPpq, double bpm,
                          const juce::String& baseName, int ticksPerQuarter)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CatSuite");
    dir.createDirectory();

    auto file = dir.getChildFile (juce::File::createLegalFileName (baseName) + ".mid");
    file.deleteFile();

    juce::MidiMessageSequence tempoTrack;
    auto trackName = juce::MidiMessage::textMetaEvent (3, baseName);
    trackName.setTimeStamp (0.0);
    tempoTrack.addEvent (trackName);
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (1.0, bpm)));
    tempo.setTimeStamp (0.0);
    tempoTrack.addEvent (tempo);
    auto sig = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    sig.setTimeStamp (0.0);
    tempoTrack.addEvent (sig);

    juce::MidiMessageSequence noteTrack;
    auto name2 = juce::MidiMessage::textMetaEvent (3, baseName);
    name2.setTimeStamp (0.0);
    noteTrack.addEvent (name2);

    double lastTick = 0.0;
    for (const auto* holder : sequenceInPpq)
    {
        auto m = holder->message;
        const double tick = std::round (m.getTimeStamp() * ticksPerQuarter);
        m.setTimeStamp (tick);
        lastTick = juce::jmax (lastTick, tick);

        // Markers belong on the tempo track so Cubase creates marker-track events.
        if (m.isTextMetaEvent() && m.getMetaEventType() == 6)
            tempoTrack.addEvent (m);
        else
            noteTrack.addEvent (m);
    }

    noteTrack.sort();
    noteTrack.updateMatchedPairs();
    tempoTrack.sort();
    tempoTrack.addEvent (juce::MidiMessage::endOfTrack(), lastTick + 1);
    noteTrack.addEvent (juce::MidiMessage::endOfTrack(), lastTick + 1);

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (ticksPerQuarter);
    midiFile.addTrack (tempoTrack);
    midiFile.addTrack (noteTrack);

    if (auto stream = file.createOutputStream())
    {
        midiFile.writeTo (*stream, 1);
        stream->flush();
    }

    return file;
}

bool dragFileOut (const juce::File& file, juce::Component* sourceComponent)
{
    if (! file.existsAsFile())
        return false;

    return juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, sourceComponent);
}

juce::MidiMessageSequence readMidiFileAsPpq (const juce::File& file)
{
    juce::MidiMessageSequence result;
    juce::FileInputStream in (file);
    if (! in.openedOk())
        return result;

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (in))
        return result;

    const int tpq = midiFile.getTimeFormat();
    if (tpq <= 0) // SMPTE timing: convert seconds -> beats at 120 bpm
    {
        midiFile.convertTimestampTicksToSeconds();
        for (int t = 0; t < midiFile.getNumTracks(); ++t)
            result.addSequence (*midiFile.getTrack (t), 0.0);
        for (auto* h : result)
            h->message.setTimeStamp (h->message.getTimeStamp() * 2.0);
    }
    else
    {
        for (int t = 0; t < midiFile.getNumTracks(); ++t)
            result.addSequence (*midiFile.getTrack (t), 0.0);
        for (auto* h : result)
            h->message.setTimeStamp (h->message.getTimeStamp() / (double) tpq);
    }

    result.sort();
    result.updateMatchedPairs();
    return result;
}

} // namespace cat::midiexport
