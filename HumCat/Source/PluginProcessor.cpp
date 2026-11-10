#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

using namespace juce;

namespace
{
const StringArray gridNames { "Off", "1/8", "1/16", "1/32" };
const double gridBeats[] = { 0.0, 0.5, 0.25, 0.125 };
StringArray keyChoices()
{
    StringArray k { "Auto" };
    for (int i = 0; i < 12; ++i) k.add (humcat::keyName (i));
    return k;
}
}

AudioProcessorValueTreeState::ParameterLayout HumCatProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", humcat::styleNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "chordRate", 1 }, "Chords every", StringArray { "Bar", "Half bar" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keyChoices(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", StringArray { "Auto", "Major", "Minor" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "snap", 1 }, "Snap to key", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "grid", 1 }, "Quantize", gridNames, 2));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "leadOct", 1 }, "Lead octave", -2, 2, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sensitivity", 1 }, "Sensitivity", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "trim", 1 }, "Start at first note", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo (no DAW)", NormalisableRange<float> (50.0f, 200.0f, 1.0f), 110.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " BPM"; })));
    for (int part = 0; part < humcat::NumParts; ++part)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "out" + humcat::partNames()[part], 1 }, humcat::partNames()[part], true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "ownSound", 1 }, "Own sound", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "withHost", 1 }, "Play with DAW", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "leadSound", 1 }, "Lead sound", hub::KeysSynth::soundNames(), 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "chordSound", 1 }, "Chord sound", hub::KeysSynth::soundNames(), 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "kit", 1 }, "Drum kit", hub::DrumKit::soundNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (-40.0f, 6.0f), -6.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return layout;
}

const std::vector<HumCatProcessor::Preset>& HumCatProcessor::presets()
{
    static const std::vector<Preset> list {
        { "Pop Song",        { { "style", 0 }, { "chordRate", 0 }, { "grid", 2 }, { "leadSound", 2 }, { "chordSound", 1 }, { "kit", 0 } } },
        { "Piano Ballad",    { { "style", 1 }, { "chordRate", 0 }, { "grid", 1 }, { "leadSound", 5 }, { "chordSound", 6 }, { "kit", 3 } } },
        { "Hip-Hop Beat",    { { "style", 2 }, { "chordRate", 0 }, { "grid", 2 }, { "leadSound", 4 }, { "chordSound", 1 }, { "kit", 1 } } },
        { "Rock Band",       { { "style", 3 }, { "chordRate", 0 }, { "grid", 1 }, { "leadSound", 2 }, { "chordSound", 3 }, { "kit", 3 } } },
        { "Lo-Fi Sketch",    { { "style", 4 }, { "chordRate", 0 }, { "grid", 2 }, { "leadSound", 7 }, { "chordSound", 1 }, { "kit", 2 } } },
        { "House Hum",       { { "style", 5 }, { "chordRate", 1 }, { "grid", 2 }, { "leadSound", 4 }, { "chordSound", 0 }, { "kit", 1 } } },
        { "Just the Melody", { { "outChords", 0 }, { "outBass", 0 }, { "outDrums", 0 }, { "grid", 2 } } },
    };
    return list;
}

void HumCatProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    // every part back on unless the preset says otherwise
    for (int part = 0; part < humcat::NumParts; ++part)
        if (auto* prm = apvts.getParameter ("out" + humcat::partNames()[part])) prm->setValueNotifyingHost (1.0f);
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String HumCatProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

HumCatProcessor::HumCatProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HumCat", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (prm))
            apvts.addParameterListener (wp->getParameterID(), this);
}

HumCatProcessor::~HumCatProcessor()
{
    cancelPendingUpdate();
    pool.removeAllJobs (true, 5000);
}

bool HumCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void HumCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    if (! recording.load())
    {
        recRate = sr;
        recBuffer.assign ((size_t) (maxRecordSeconds * sr), 0.0f);
    }
    leadSynth.prepare (sr); chordSynth.prepare (sr); bassSynth.prepare (sr); drumKit.prepare (sr);
    leadSound = chordSound = kitSound = -1;
    synthBuffer.setSize (2, juce::jmax (1, samplesPerBlock), false, false, true);
    partEvents.ensureSize (2048);
    limiter.prepare (sr);
    for (auto& h : held) h.clear();
}

//==============================================================================
void HumCatProcessor::startRecording()
{
    if (recording.load()) return;
    recPos = 0;
    recStartPpq = -1.0;
    sawHostPpq = false;
    recording = true;
}

void HumCatProcessor::stopRecording()
{
    if (! recording.exchange (false)) return;
    const int n = recPos.load();
    if (n < (int) (0.3 * recRate)) return;
    std::vector<float> audio (recBuffer.begin(), recBuffer.begin() + n);
    const double startPpq = recStartPpq.load();
    recordedWithTransport = sawHostPpq.load() && startPpq >= 0.0;
    analyse (std::move (audio), recRate, recordedWithTransport ? startPpq : 0.0);
}

bool HumCatProcessor::loadFile (const File& file, String& error)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Hum-Cat can't read " + file.getFileName(); return false; }
    // a broken or hostile file can claim anything in its header: only believe sane numbers
    if (reader->numChannels < 1 || reader->numChannels > 32 || ! (reader->sampleRate >= 4000.0 && reader->sampleRate <= 768000.0)
        || reader->lengthInSamples <= 0)
    { error = file.getFileName() + " looks broken"; return false; }
    const auto maxSamples = (int64) (reader->sampleRate * maxRecordSeconds);
    const int n = (int) jmin (reader->lengthInSamples, maxSamples);
    if (n < (int) (0.3 * reader->sampleRate)) { error = "That recording is too short"; return false; }
    AudioBuffer<float> buf ((int) jmax (1u, reader->numChannels), n);
    reader->read (&buf, 0, n, 0, true, true);
    std::vector<float> mono ((size_t) n, 0.0f);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* d = buf.getReadPointer (ch);
        for (int i = 0; i < n; ++i) mono[(size_t) i] += d[i] / (float) buf.getNumChannels();
    }
    recordedWithTransport = false;
    analyse (std::move (mono), reader->sampleRate, 0.0);
    return true;
}

void HumCatProcessor::loadDemo()
{
    // Ode to Joy, "sung": harmonics, vibrato, a scoop into each note, breaths between them
    const int tune[] = { 64, 64, 65, 67, 67, 65, 64, 62, 60, 60, 62, 64, 64, 62, 62,
                         64, 64, 65, 67, 67, 65, 64, 62, 60, 60, 62, 64, 62, 60, 60 };
    const double beats[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5, 0.5, 2,
                             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5, 0.5, 2 };
    const double rate = 44100.0, beat = 0.5;
    std::vector<float> audio ((size_t) (rate * 0.25), 0.0f);
    Random rng (11);
    double phase = 0.0;
    for (size_t i = 0; i < std::size (tune); ++i)
    {
        const double len = beats[i] * beat - 0.04;
        const int n = (int) (len * rate);
        for (int k = 0; k < n; ++k)
        {
            const double t = k / rate;
            const double cents = (t < 0.05 ? -60.0 * (1.0 - t / 0.05) : 0.0) + (t > 0.15 ? 22.0 * std::sin (MathConstants<double>::twoPi * 5.3 * t) : 0.0);
            phase += 440.0 * std::pow (2.0, (tune[i] - 69 + cents / 100.0) / 12.0) / rate;
            const double env = jmin (1.0, t / 0.03) * jmin (1.0, (len - t) / 0.05);
            double v = 0.0;
            for (int h = 1; h <= 7; ++h) v += std::sin (MathConstants<double>::twoPi * phase * h) * (h == 1 ? 1.0 : 0.5 / h);
            audio.push_back ((float) (0.25 * env * v) + 0.004f * (rng.nextFloat() * 2.0f - 1.0f));
        }
        for (int k = 0; k < (int) (0.04 * rate); ++k) audio.push_back (0.004f * (rng.nextFloat() * 2.0f - 1.0f));
    }
    recordedWithTransport = false;
    analyse (std::move (audio), rate, 0.0);
}

void HumCatProcessor::clear()
{
    {
        const ScopedLock l (sungLock);
        sung.clear();
    }
    {
        const SpinLock::ScopedLockType l (songLock);
        pitchTrack.reset();
    }
    rebuild();
}

void HumCatProcessor::analyse (std::vector<float> audio, double rate, double offsetBeats)
{
    analysing = true;
    const float sens = p ("sensitivity");
    pool.addJob ([this, audio = std::move (audio), rate, offsetBeats, sens]
    {
        auto track = std::make_shared<humcat::PitchTrack> (humcat::trackPitch (audio.data(), (int) audio.size(), rate, sens));
        auto notes = humcat::segment (*track);
        {
            const ScopedLock l (sungLock);
            sung = std::move (notes);
            startOffsetBeats = offsetBeats;
        }
        {
            const SpinLock::ScopedLockType l (songLock);
            pitchTrack = track;
        }
        analysing = false;
        triggerAsyncUpdate();
    });
}

double HumCatProcessor::songBpm() const
{
    const double h = lastBpm.load();
    return h > 20.0 ? h : (double) p ("tempo");
}

std::array<bool, humcat::NumParts> HumCatProcessor::partsOn() const
{
    std::array<bool, humcat::NumParts> on {};
    for (int part = 0; part < humcat::NumParts; ++part)
        on[(size_t) part] = apvts.getRawParameterValue ("out" + humcat::partNames()[part])->load() > 0.5f;
    return on;
}

void HumCatProcessor::rebuild()
{
    humcat::SongSettings s;
    s.bpm = songBpm();
    s.grid = gridBeats[jlimit (0, 3, roundToInt (p ("grid")))];
    s.key = roundToInt (p ("key")) - 1;
    s.scale = roundToInt (p ("scale")) - 1;
    s.snapToScale = p ("snap") > 0.5f;
    s.leadOctave = roundToInt (p ("leadOct"));
    s.style = roundToInt (p ("style"));
    s.chordBeats = roundToInt (p ("chordRate")) == 1 ? 2.0 : 4.0;
    std::vector<humcat::Note> notes;
    {
        const ScopedLock l (sungLock);
        notes = sung;
        s.trimStart = p ("trim") > 0.5f || ! recordedWithTransport;
        s.startOffsetBeats = startOffsetBeats;
    }
    auto made = std::make_shared<humcat::Song> (humcat::makeSong (notes, s));
    {
        const SpinLock::ScopedLockType l (songLock);
        song = made;
    }
    ++songVersion;
}

juce::MidiFile HumCatProcessor::midiFile() const
{
    if (auto s = getSong()) return humcat::toMidiFile (*s, partsOn());
    return {};
}

//==============================================================================
void HumCatProcessor::allNotesOff (MidiBuffer& out, int samplePos)
{
    for (int part = 0; part < humcat::NumParts; ++part)
    {
        for (int pitch : held[(size_t) part])
        {
            const auto off = MidiMessage::noteOff (humcat::partChannel (part), pitch);
            out.addEvent (off, samplePos);
            partEvents.addEvent (off, samplePos);
        }
        held[(size_t) part].clear();
    }
}

void HumCatProcessor::emit (double from, double to, int sampleStart, double samplesPerBeat, MidiBuffer& out, const humcat::Song& s)
{
    const auto on = partsOn();
    for (int part = 0; part < humcat::NumParts; ++part)
    {
        const int ch = humcat::partChannel (part);
        auto& h = held[(size_t) part];
        for (const auto& nn : s.parts[(size_t) part])
        {
            const double end = nn.start + nn.length;
            if (end >= from && end < to)
                if (auto it = std::find (h.begin(), h.end(), nn.pitch); it != h.end())
                {
                    const int at = sampleStart + jlimit (0, 1 << 20, (int) ((end - from) * samplesPerBeat));
                    const auto off = MidiMessage::noteOff (ch, nn.pitch);
                    out.addEvent (off, at); partEvents.addEvent (off, at);
                    h.erase (it);
                }
            if (on[(size_t) part] && nn.start >= from && nn.start < to)
            {
                const int at = sampleStart + jlimit (0, 1 << 20, (int) ((nn.start - from) * samplesPerBeat));
                if (std::find (h.begin(), h.end(), nn.pitch) != h.end())
                {
                    const auto off = MidiMessage::noteOff (ch, nn.pitch);
                    out.addEvent (off, at); partEvents.addEvent (off, at);
                }
                else h.push_back (nn.pitch);
                const auto msg = MidiMessage::noteOn (ch, nn.pitch, nn.velocity);
                out.addEvent (msg, at); partEvents.addEvent (msg, at);
            }
        }
    }
}

void HumCatProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    midi.clear();
    partEvents.clear();
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    for (int ch = inCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    // host
    double ppq = 0.0; bool hostPlaying = false; double bpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) bpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) { ppq = *q; hostPlaying = pos->getIsPlaying(); }
        }
    if (bpm > 20.0 && std::abs (bpm - lastBpm.load()) > 0.01) { lastBpm = bpm; triggerAsyncUpdate(); }

    // the voice in: level meter and recording
    {
        float peak = 0.0f;
        const int use = jmin (inCh, buffer.getNumChannels());
        if (recording.load())
        {
            int pos = recPos.load();
            if (pos == 0 && hostPlaying) { recStartPpq = ppq; sawHostPpq = true; }
            for (int i = 0; i < n && pos < (int) recBuffer.size(); ++i, ++pos)
            {
                float v = 0.0f;
                for (int ch = 0; ch < use; ++ch) v += buffer.getSample (ch, i);
                recBuffer[(size_t) pos] = use > 0 ? v / (float) use : 0.0f;
            }
            recPos = pos;
        }
        for (int ch = 0; ch < use; ++ch) peak = jmax (peak, buffer.getMagnitude (ch, 0, n));
        inputLevel = jmax (peak, inputLevel.load() * 0.9f);
    }

    std::shared_ptr<const humcat::Song> s;
    {
        const SpinLock::ScopedTryLockType l (songLock);
        if (l.isLocked()) s = song;
    }

    const bool withHost = p ("withHost") > 0.5f && hostPlaying && ! recording.load();
    const bool preview = previewPlaying.load() && ! withHost;
    const bool playing = (withHost || preview) && s != nullptr && ! s->parts[humcat::Lead].empty();
    if (! playing)
    {
        if (wasPlaying) allNotesOff (midi, 0);
        wasPlaying = false;
        playPosition = -1.0;
        if (! previewPlaying.load()) previewBeat = 0.0;
    }
    else
    {
        const double beatsPerSample = (bpm > 20.0 ? bpm : songBpm()) / 60.0 / sr;
        const double len = jmax (4.0, s->lengthBeats);
        double from;
        if (withHost)
        {
            const double origin = (recordedWithTransport && p ("trim") < 0.5f) ? std::floor (startOffsetBeats / 4.0) * 4.0 : 0.0;
            from = std::fmod (jmax (0.0, ppq - origin), len);
            if (! wasPlaying) allNotesOff (midi, 0);
        }
        else from = previewBeat;
        const double blockBeats = n * beatsPerSample;
        const double spb = 1.0 / beatsPerSample;
        if (from + blockBeats <= len) emit (from, from + blockBeats, 0, spb, midi, *s);
        else
        {
            const int split = jlimit (0, n, (int) ((len - from) * spb));
            emit (from, len + 1.0e-9, 0, spb, midi, *s);
            allNotesOff (midi, split);
            emit (0.0, from + blockBeats - len, split, spb, midi, *s);
        }
        previewBeat = std::fmod (from + blockBeats, len);
        playPosition = from;
        wasPlaying = true;
    }

    // its own sound
    if (p ("ownSound") > 0.5f && buffer.getNumChannels() > 0)
    {
        if (const int ls = roundToInt (p ("leadSound")); ls != leadSound) { leadSound = ls; leadSynth.setSound (ls); }
        if (const int cs = roundToInt (p ("chordSound")); cs != chordSound) { chordSound = cs; chordSynth.setSound (cs); }
        if (const int ks = roundToInt (p ("kit")); ks != kitSound) { kitSound = ks; drumKit.setSound (ks); }
        synthBuffer.setSize (2, n, false, false, true);
        synthBuffer.clear();
        auto* l = synthBuffer.getWritePointer (0);
        auto* r = synthBuffer.getWritePointer (1);
        int pos = 0;
        auto renderTo = [&] (int at)
        {
            if (at <= pos) return;
            leadSynth.render (l + pos, r + pos, at - pos);
            chordSynth.render (l + pos, r + pos, at - pos);
            bassSynth.render (l + pos, r + pos, at - pos);
            drumKit.render (l + pos, r + pos, at - pos);
            pos = at;
        };
        for (const auto meta : partEvents)
        {
            renderTo (jlimit (0, n, meta.samplePosition));
            const auto m = meta.getMessage();
            switch (m.getChannel())
            {
                case 1: chordSynth.handle (m); break;
                case 2: bassSynth.handle (m); break;
                case 3: leadSynth.handle (m); break;
                case 10: drumKit.handle (m); break;
                default: break;
            }
        }
        renderTo (n);
        const float g = Decibels::decibelsToGain (p ("volume"));
        synthBuffer.applyGain (g * 1.2f);
        limiter.process (l, r, n);
        buffer.addFrom (0, 0, synthBuffer, 0, 0, n);
        if (buffer.getNumChannels() > 1) buffer.addFrom (1, 0, synthBuffer, 1, 0, n);
    }
    else if (! partEvents.isEmpty() || ! playing)
    {
        leadSynth.allOff(); chordSynth.allOff(); bassSynth.allOff(); drumKit.allOff();
    }
}

//==============================================================================
void HumCatProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    ValueTree notes ("SUNG");
    {
        const ScopedLock l (sungLock);
        notes.setProperty ("offset", startOffsetBeats, nullptr);
        notes.setProperty ("transport", recordedWithTransport, nullptr);
        for (const auto& nn : sung)
        {
            ValueTree t ("N");
            t.setProperty ("s", nn.start, nullptr);
            t.setProperty ("l", nn.length, nullptr);
            t.setProperty ("p", nn.pitch, nullptr);
            t.setProperty ("v", nn.velocity, nullptr);
            notes.appendChild (t, nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("SUNG"), nullptr);
    state.appendChild (notes, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void HumCatProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = ValueTree::fromXml (*xml);
    currentPreset = (int) state.getProperty ("preset", -1);
    state.removeProperty ("preset", nullptr);
    if (auto notes = state.getChildWithName ("SUNG"); notes.isValid())
    {
        const ScopedLock l (sungLock);
        sung.clear();
        startOffsetBeats = (double) notes.getProperty ("offset", 0.0);
        recordedWithTransport = (bool) notes.getProperty ("transport", false);
        for (const auto& t : notes)
            sung.push_back ({ (double) t.getProperty ("s"), (double) t.getProperty ("l"), (int) t.getProperty ("p"), (float) t.getProperty ("v", 0.8f) });
        state.removeChild (notes, nullptr);
    }
    apvts.replaceState (state);
    triggerAsyncUpdate();
}

AudioProcessorEditor* HumCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<HumCatEditor> (*this), "Hum-Cat");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HumCatProcessor();
}
