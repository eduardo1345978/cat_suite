#include "HubEngine.h"

namespace hub
{
const char* catName (int id)
{
    static const char* names[] = { "Chord-Cat", "Bass-Cat", "Drum-Cat", "Life-Cat", "Glide-Cat" };
    return names[juce::jlimit (0, (int) NumCats - 1, id)];
}

const char* partName (int part)
{
    static const char* names[] = { "Chords", "Bass", "Drums", "Life" };
    return names[juce::jlimit (0, (int) NumParts - 1, part)];
}

namespace
{
    juce::AudioProcessorParameter* findParam (juce::AudioProcessor& p, const juce::String& name)
    {
        for (auto* prm : p.getParameters())
            if (prm->getName (100) == name) return prm;
        return nullptr;
    }

    template <typename Synth>
    void renderWithMidi (Synth& s, const juce::MidiBuffer& m, float* l, float* r, int n)
    {
        int pos = 0;
        for (const auto meta : m)
        {
            const int p = juce::jlimit (0, n, meta.samplePosition);
            if (p > pos) { s.render (l + pos, r + pos, p - pos); pos = p; }
            s.handle (meta.getMessage());
        }
        if (pos < n) s.render (l + pos, r + pos, n - pos);
    }

    // copies without giving up the destination's memory (no allocation on the audio thread)
    void copyMidi (juce::MidiBuffer& dst, const juce::MidiBuffer& src)
    {
        dst.clear();
        dst.addEvents (src, 0, -1, 0);
    }

    void addPanic (juce::MidiBuffer& m, int pos)
    {
        for (int ch = 1; ch <= 16; ++ch)
            m.addEvent (juce::MidiMessage::allNotesOff (ch), pos);
    }
}

//==============================================================================
juce::Optional<juce::AudioPlayHead::PositionInfo> HubEngine::PlayHead::getPosition() const
{
    PositionInfo i;
    i.setBpm (bpm);
    i.setTimeSignature (TimeSignature { 4, 4 });
    i.setIsPlaying (playing);
    i.setIsRecording (false);
    i.setPpqPosition (ppq);
    i.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
    i.setTimeInSamples (samples);
    i.setTimeInSeconds ((double) samples / sampleRate);
    i.setIsLooping (false);
    return i;
}

//==============================================================================
HubEngine::HubEngine()
{
    formats.addFormat (new juce::VST3PluginFormat());
    parts[ChordPart].reverb = 0.22f;
    parts[BassPart].reverb = 0.03f;   parts[BassPart].volume = 0.75f;
    parts[DrumPart].reverb = 0.08f;   parts[DrumPart].volume = 0.8f;
    parts[LifePart].reverb = 0.0f;    parts[LifePart].volume = 0.7f;
    writerThread.startThread();
}

HubEngine::~HubEngine()
{
    stopRecording();
    releaseCats();
    writerThread.stopThread (2000);
}

juce::Array<juce::File> HubEngine::defaultSearchFolders (const juce::File& extraFolder)
{
    juce::Array<juce::File> dirs;
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    if (extraFolder != juce::File()) dirs.add (extraFolder);
    dirs.add (exeDir.getChildFile ("Hub Cats"));        // the copies installed with the Hub (same version)
    dirs.add (exeDir);
   #if JUCE_WINDOWS
    dirs.add (juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("Programs").getChildFile ("Common").getChildFile ("VST3"));
    dirs.add (juce::File (juce::SystemStats::getEnvironmentVariable ("CommonProgramFiles", "C:\\Program Files\\Common Files")).getChildFile ("VST3"));
   #elif JUCE_MAC
    dirs.add (juce::File ("~/Library/Audio/Plug-Ins/VST3"));
    dirs.add (juce::File ("/Library/Audio/Plug-Ins/VST3"));
   #else
    dirs.add (juce::File ("~/.vst3"));
    dirs.add (juce::File ("/usr/lib/vst3"));
   #endif
    return dirs;
}

juce::String HubEngine::loadCats (const juce::Array<juce::File>& folders)
{
    juce::StringArray report;
    for (int id = 0; id < NumCats; ++id)
    {
        if (cats[(size_t) id] != nullptr) continue;
        for (const auto& dir : folders)
        {
            const auto file = dir.getChildFile (juce::String (catName (id)) + ".vst3");
            if (! file.exists()) continue;
            juce::OwnedArray<juce::PluginDescription> types;
            for (auto* f : formats.getFormats()) f->findAllTypesForFile (types, file.getFullPathName());
            if (types.isEmpty()) continue;
            juce::String error;
            auto instance = formats.createPluginInstance (*types[0], sampleRate, maxBlockSize, error);
            if (instance == nullptr) { report.add (juce::String (catName (id)) + ": " + error); continue; }
            instance->enableAllBuses();
            instance->setPlayHead (&playHead);
            catFiles[(size_t) id] = file;
            cats[(size_t) id] = std::move (instance);
            break;
        }
        if (cats[(size_t) id] == nullptr && id != GlideCat) report.add (juce::String (catName (id)) + " not found");
    }

    if (auto* chord = getCat (ChordCat))
    {
        // Pads are played chromatically from the trigger note (C3 by default)
        if (auto* mode = findParam (*chord, "Trigger Mode")) mode->setValueNotifyingHost (0.0f);
        pollPluginSettings();
    }
    if (prepared) prepare (sampleRate, maxBlockSize);
    return report.joinIntoString ("\n");
}

void HubEngine::releaseCats()
{
    const juce::ScopedLock sl (renderLock);
    for (auto& c : cats)
        if (c != nullptr) { c->releaseResources(); c.reset(); }
    prepared = false;
}

void HubEngine::pollPluginSettings()
{
    if (auto* chord = getCat (ChordCat))
    {
        if (auto* pads = findParam (*chord, "Pads"))  numPads = juce::jlimit (4, 16, 4 + juce::roundToInt (pads->getValue() * 12.0f));
        if (auto* low = findParam (*chord, "Trigger Low")) triggerLow = juce::jlimit (0, 112, juce::roundToInt (low->getValue() * 127.0f));
    }
}

//==============================================================================
void HubEngine::setPlaying (bool shouldPlay)
{
    if (shouldPlay) { stopRequested = false; startRequested = true; }
    else            { startRequested = false; stopRequested = true; }
}

void HubEngine::prepare (double newRate, int maxBlock)
{
    const juce::ScopedLock sl (renderLock);
    sampleRate = newRate > 0.0 ? newRate : 48000.0;
    maxBlockSize = juce::jmax (32, maxBlock);
    playHead.sampleRate = sampleRate;
    for (auto& c : cats)
    {
        if (c == nullptr) continue;
        c->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
        c->prepareToPlay (sampleRate, maxBlockSize);
    }
    keys.prepare (sampleRate);
    bass.prepare (sampleRate);
    drums.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.62f; rp.damping = 0.45f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
    reverb.setParameters (rp);
    reverb.reset();
    limiter.prepare (sampleRate);
    catAudio.setSize (8, maxBlockSize);
    partAudio.setSize (2, maxBlockSize);
    sendAudio.setSize (2, maxBlockSize);
    mixAudio.setSize (2, maxBlockSize);
    songMidi.ensureSize (512); chordOut.ensureSize (2048); scratchMidi.ensureSize (2048);
    heldTrigger = -1;
    prepared = true;
}

void HubEngine::processCat (int id, juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    auto* p = cats[(size_t) id].get();
    if (p == nullptr) { midi.clear(); return; }
    const int n = audio.getNumSamples();
    const int chans = juce::jlimit (2, 8, juce::jmax (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels()));
    catAudio.setSize (chans, n, false, false, true);
    catAudio.clear();
    {
        const juce::ScopedLock sl (p->getCallbackLock());
        if (p->isSuspended()) midi.clear();
        else p->processBlock (catAudio, midi);
    }
    audio.clear();
    for (int ch = 0; ch < juce::jmin (2, p->getTotalNumOutputChannels()); ++ch)
        audio.copyFrom (ch, 0, catAudio, ch, 0, n);
    if (p->getTotalNumOutputChannels() == 1) audio.copyFrom (1, 0, catAudio, 0, 0, n);
}

void HubEngine::render (float* outL, float* outR, int numSamples)
{
    const juce::ScopedTryLock sl (renderLock);
    if (! sl.isLocked() || ! prepared)
    {
        juce::FloatVectorOperations::clear (outL, numSamples);
        juce::FloatVectorOperations::clear (outR, numSamples);
        return;
    }
    for (int done = 0; done < numSamples;)
    {
        const int n = juce::jmin (maxBlockSize, numSamples - done);
        renderBlock (outL + done, outR + done, n);
        done += n;
    }
}

void HubEngine::renderBlock (float* outL, float* outR, int n)
{
    const double bpmNow = juce::jlimit (30.0, 300.0, bpm.load());
    const double samplesPerBeat = sampleRate * 60.0 / bpmNow;
    songMidi.clear();
    bool panic = false;

    // ---- transport
    if (startRequested.exchange (false))
    {
        playing = true;
        playHead.ppq = 0.0;
        playHead.samples = 0;
    }
    if (stopRequested.exchange (false) && playing)
    {
        playing = false;
        panic = true;
    }
    playHead.bpm = bpmNow;
    playHead.playing = playing.load();

    // ---- which Chord-Cat pad is held (song or your fingers)
    auto switchPad = [this] (int pad, int pos)
    {
        const int note = pad >= 0 ? juce::jlimit (0, 127, triggerLow.load() + pad) : -1;
        if (note == heldTrigger) return;
        if (heldTrigger >= 0) songMidi.addEvent (juce::MidiMessage::noteOff (1, heldTrigger), pos);
        if (note >= 0) songMidi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), pos);
        heldTrigger = note;
        currentPad = pad;
    };

    const int live = livePad.load();
    const int pads = juce::jmax (1, numPads.load());
    if (panic)
    {
        switchPad (-1, 0);
        addPanic (songMidi, 0);
    }
    else if (live >= 0)
        switchPad (live % 16, 0);
    else if (playing && autoChords)
    {
        const double chordLen = 4.0 * juce::jlimit (1, 8, barsPerChord.load());
        const double ppq = playHead.ppq;
        const int slot = (int) std::floor (ppq / chordLen + 1.0e-9);
        switchPad (slot % pads, 0);
        const double nextBoundary = (slot + 1) * chordLen;
        const int at = (int) std::ceil ((nextBoundary - ppq) * samplesPerBeat);
        if (at >= 0 && at < n) switchPad ((slot + 1) % pads, at);
    }
    else
        switchPad (-1, 0);

    mixAudio.setSize (2, n, false, false, true);
    sendAudio.setSize (2, n, false, false, true);
    partAudio.setSize (2, n, false, false, true);
    mixAudio.clear();
    sendAudio.clear();

    bool anySolo = false;
    for (auto& p : parts) anySolo = anySolo || p.solo.load();
    auto mixPart = [&] (int part)
    {
        auto& s = parts[(size_t) part];
        const float v = s.volume.load();
        const float gain = (s.mute || (anySolo && ! s.solo)) ? 0.0f : v * v * 1.4f;
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            mixAudio.addFrom (ch, 0, partAudio, ch, 0, n, gain);
            sendAudio.addFrom (ch, 0, partAudio, ch, 0, n, gain * s.reverb.load());
            peak = juce::jmax (peak, partAudio.getMagnitude (ch, 0, n) * gain);
            const auto* d = partAudio.getReadPointer (ch);
            double e = 0.0;
            for (int i = 0; i < n; ++i) e += (double) d[i] * d[i];
            s.energy = s.energy.load() + e;
        }
        s.meter = juce::jmax (peak, s.meter.load() * 0.85f);
    };

    // ---- Chord-Cat turns the pad into chords
    copyMidi (chordOut, songMidi);
    if (hasCat (ChordCat)) processCat (ChordCat, partAudio, chordOut);
    if (panic) addPanic (chordOut, 0);

    // ---- chords: built-in keys, or Glide-Cat
    partAudio.clear();
    const int chordSound = parts[ChordPart].sound.load();
    keys.setSound (chordSound);
    if (chordSound >= keysGlideIndex() && hasCat (GlideCat))
    {
        copyMidi (scratchMidi, chordOut);
        processCat (GlideCat, partAudio, scratchMidi);
    }
    else
        renderWithMidi (keys, chordOut, partAudio.getWritePointer (0), partAudio.getWritePointer (1), n);
    mixPart (ChordPart);

    // ---- Bass-Cat follows the chords -> bass synth
    partAudio.clear();
    copyMidi (scratchMidi, chordOut);
    if (hasCat (BassCat)) processCat (BassCat, partAudio, scratchMidi); else scratchMidi.clear();
    if (panic) addPanic (scratchMidi, 0);
    partAudio.clear();
    bass.setSound (parts[BassPart].sound.load());
    renderWithMidi (bass, scratchMidi, partAudio.getWritePointer (0), partAudio.getWritePointer (1), n);
    mixPart (BassPart);

    // ---- Drum-Cat -> drum kit
    scratchMidi.clear();
    if (panic) addPanic (scratchMidi, 0);
    if (hasCat (DrumCat)) processCat (DrumCat, partAudio, scratchMidi); else scratchMidi.clear();
    partAudio.clear();
    drums.setSound (parts[DrumPart].sound.load());
    renderWithMidi (drums, scratchMidi, partAudio.getWritePointer (0), partAudio.getWritePointer (1), n);
    mixPart (DrumPart);

    // ---- Life-Cat makes its own sound and hears the chords
    copyMidi (scratchMidi, chordOut);
    if (hasCat (LifeCat)) processCat (LifeCat, partAudio, scratchMidi); else partAudio.clear();
    mixPart (LifePart);

    // ---- reverb send, master, safety limiter
    reverb.processStereo (sendAudio.getWritePointer (0), sendAudio.getWritePointer (1), n);
    mixAudio.addFrom (0, 0, sendAudio, 0, 0, n, 0.5f);
    mixAudio.addFrom (1, 0, sendAudio, 1, 0, n, 0.5f);
    const float master = masterVolume.load();
    mixAudio.applyGain (master * master * 1.3f);
    auto* l = mixAudio.getWritePointer (0);
    auto* r = mixAudio.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (l[i])) l[i] = 0.0f;
        if (! std::isfinite (r[i])) r[i] = 0.0f;
    }
    limiter.process (l, r, n);
    juce::FloatVectorOperations::copy (outL, l, n);
    juce::FloatVectorOperations::copy (outR, r, n);

    if (recording)
    {
        const juce::ScopedTryLock wl (writerLock);
        if (wl.isLocked() && writer != nullptr)
        {
            const float* chans[2] = { l, r };
            writer->write (chans, n);
        }
    }

    if (playing)
    {
        playHead.ppq += n / samplesPerBeat;
        playHead.samples += n;
    }
    ppqShown = playHead.ppq;
}

//==============================================================================
void HubEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void HubEngine::audioDeviceStopped() {}

void HubEngine::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples,
                                                  const juce::AudioIODeviceCallbackContext&)
{
    if (numOut <= 0) return;
    float* l = out[0];
    float* r = numOut > 1 ? out[1] : nullptr;
    if (r == nullptr)
    {
        // mono device: render stereo into a scratch, then sum
        juce::HeapBlock<float> tmp ((size_t) numSamples);
        render (l, tmp.get(), numSamples);
        juce::FloatVectorOperations::add (l, tmp.get(), numSamples);
        juce::FloatVectorOperations::multiply (l, 0.5f, numSamples);
    }
    else
        render (l, r, numSamples);
    for (int ch = 2; ch < numOut; ++ch)
        if (out[ch] != nullptr) juce::FloatVectorOperations::clear (out[ch], numSamples);
}

//==============================================================================
bool HubEngine::startRecording (const juce::File& file)
{
    stopRecording();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) return false;
    {
        const juce::ScopedLock sl (writerLock);
        writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w.release(), writerThread, 1 << 16);
    }
    recordFile = file;
    recording = true;
    return true;
}

void HubEngine::stopRecording()
{
    recording = false;
    const juce::ScopedLock sl (writerLock);
    writer.reset();
}

//==============================================================================
juce::ValueTree HubEngine::saveState() const
{
    juce::ValueTree t ("CATHUB");
    t.setProperty ("bpm", bpm.load(), nullptr);
    t.setProperty ("barsPerChord", barsPerChord.load(), nullptr);
    t.setProperty ("autoChords", autoChords.load(), nullptr);
    t.setProperty ("master", masterVolume.load(), nullptr);
    for (int i = 0; i < NumParts; ++i)
    {
        juce::ValueTree p ("PART");
        const auto& s = parts[(size_t) i];
        p.setProperty ("index", i, nullptr);
        p.setProperty ("volume", s.volume.load(), nullptr);
        p.setProperty ("reverb", s.reverb.load(), nullptr);
        p.setProperty ("mute", s.mute.load(), nullptr);
        p.setProperty ("solo", s.solo.load(), nullptr);
        p.setProperty ("sound", s.sound.load(), nullptr);
        t.appendChild (p, nullptr);
    }
    for (int id = 0; id < NumCats; ++id)
    {
        if (auto* c = getCat (id))
        {
            juce::MemoryBlock mb;
            c->getStateInformation (mb);
            juce::ValueTree s ("CAT");
            s.setProperty ("name", catName (id), nullptr);
            s.setProperty ("state", mb.toBase64Encoding(), nullptr);
            t.appendChild (s, nullptr);
        }
    }
    return t;
}

void HubEngine::restoreState (const juce::ValueTree& t)
{
    if (! t.hasType ("CATHUB")) return;
    bpm = juce::jlimit (30.0, 300.0, (double) t.getProperty ("bpm", 92.0));
    barsPerChord = juce::jlimit (1, 8, (int) t.getProperty ("barsPerChord", 1));
    autoChords = (bool) t.getProperty ("autoChords", true);
    masterVolume = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("master", 0.8f));
    for (const auto& p : t)
    {
        if (p.hasType ("PART"))
        {
            const int i = (int) p.getProperty ("index", -1);
            if (! juce::isPositiveAndBelow (i, (int) NumParts)) continue;
            auto& s = parts[(size_t) i];
            s.volume = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("volume", 0.8f));
            s.reverb = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("reverb", 0.1f));
            s.mute = (bool) p.getProperty ("mute", false);
            s.solo = (bool) p.getProperty ("solo", false);
            s.sound = juce::jlimit (0, 8, (int) p.getProperty ("sound", 0));
        }
        else if (p.hasType ("CAT"))
        {
            for (int id = 0; id < NumCats; ++id)
                if (p.getProperty ("name").toString() == catName (id))
                    if (auto* c = getCat (id))
                    {
                        juce::MemoryBlock mb;
                        if (mb.fromBase64Encoding (p.getProperty ("state").toString()) && mb.getSize() > 0)
                            c->setStateInformation (mb.getData(), (int) mb.getSize());
                    }
        }
    }
    pollPluginSettings();
}
} // namespace hub
