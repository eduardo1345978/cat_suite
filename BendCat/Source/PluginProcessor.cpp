#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace bendcat;

juce::AudioProcessorValueTreeState::ParameterLayout BendCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto f = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto pct = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (
                                                               [] (float v, int) { return String (v * 100.0f, 1) + "%"; })));
    };
    auto b = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto c = [&] (const char* id, const char* name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };
    auto i = [&] (const char* id, const char* name, int lo, int hi, int def)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
    };

    c ("mode", "Bend Mode", getModeNames(), ModeEnvelope);
    f ("range", "Bend Range", NormalisableRange<float> (1.0f, 48.0f, 1.0f), 2.0f, "st");
    i ("bendAmount", "Bend Amount", 1, 24, 2);

    f ("glideTime", "Glide Time", NormalisableRange<float> (0.0f, 1000.0f, 1.0f, 0.5f), 120.0f, "ms");
    c ("glideCurve", "Glide Curve", getGlideCurveNames(), 0);

    f ("lfoRate", "LFO Rate", NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.4f), 2.0f, "Hz");
    b ("lfoSync", "LFO Sync", false);
    c ("lfoDivision", "LFO Division", getDivisionNames(), 3);
    pct ("lfoDepth", "LFO Depth", 0.3f);
    c ("lfoWave", "LFO Wave", getLfoWaveNames(), 0);
    pct ("lfoVoiceRandom", "LFO Voice Random", 0.0f);
    b ("lfoPhaseRandom", "LFO Phase Random", false);

    pct ("envDepth", "Envelope Depth", 1.0f);
    pct ("envRandom", "Envelope Random", 0.0f);
    pct ("envChance", "Envelope Chance", 1.0f);
    i ("envCycles", "Envelope Cycles", 1, 32, 1);
    c ("envLength", "Envelope Length", getEnvelopeLengthNames(), 0);
    f ("envDuration", "Envelope Duration", NormalisableRange<float> (10.0f, 5000.0f, 1.0f, 0.4f), 250.0f, "ms");
    c ("envPlayMode", "Envelope Play Mode", { "Retrigger", "Global" }, 0);
    b ("envLoop", "Envelope Loop", false);

    c ("patDivision", "Pattern Division", getDivisionNames(), 4);
    pct ("patSmoothing", "Pattern Smoothing", 0.3f);

    f ("detune", "Human Detune", NormalisableRange<float> (0.0f, 25.0f, 0.1f), 0.0f, "ct");
    f ("driftCents", "Drift Amount", NormalisableRange<float> (0.0f, 25.0f, 0.1f), 0.0f, "ct");
    f ("driftRate", "Drift Rate", NormalisableRange<float> (0.01f, 1.0f, 0.001f, 0.5f), 0.05f, "Hz");

    b ("mpe", "MPE", true);
    i ("mpeVoices", "MPE Voices", 1, 15, 15);
    b ("sendConfig", "Send MPE/RPN Config", true);
    c ("resolution", "Bend Resolution", { "32 samples", "64 samples", "128 samples", "256 samples" }, 2);
    b ("scalaEnabled", "Microtuning", false);
    i ("scalaRef", "Scala Reference Note", 0, 127, 60);
    return layout;
}

BendCatProcessor::BendCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                      ),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    presetManager.enableLink (apvts, {});
    for (auto* v : { &heldPitches, &previousChord, &groupUsed, &lastReleasedChord })
        v->reserve (128);
    leadVoiceForChannel.fill (-1);
    engine.envelope.points.reserve (128);
    engine.envelope.curves.reserve (128);
}

BendCatProcessor::~BendCatProcessor() = default;

bool BendCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
#endif
}

void BendCatProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    hostSync.prepare (sr);
    outBuffer.ensureSize ((size_t) juce::jmax (4096, samplesPerBlock * 8));
    sampleCounter = 0;
    allocator.reset();
    for (auto& v : voices) v = {};
    leadVoiceForChannel.fill (-1);
    heldPitches.clear();
    configPending = true;
}

void BendCatProcessor::readParams()
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [this] (const char* id) { return juce::roundToInt (apvts.getRawParameterValue (id)->load()); };

    engine.mode = (BendMode) i ("mode");
    engine.rangeSemitones = f ("range");
    engine.bendAmountSemitones = f ("bendAmount");
    engine.glideTimeMs = f ("glideTime");
    engine.glideCurve = i ("glideCurve");
    engine.lfoRateHz = f ("lfoRate");
    engine.lfoSync = f ("lfoSync") > 0.5f;
    engine.lfoDivision = i ("lfoDivision");
    engine.lfoDepth = f ("lfoDepth");
    engine.lfoWave = i ("lfoWave");
    engine.lfoVoiceRandom = f ("lfoVoiceRandom");
    engine.lfoPhaseRandom = f ("lfoPhaseRandom") > 0.5f;
    engine.envDepth = f ("envDepth");
    engine.envRandom = f ("envRandom");
    engine.envChance = f ("envChance");
    engine.envCycles = i ("envCycles");
    engine.envelope.syncDivision = i ("envLength");
    engine.envelope.durationMs = f ("envDuration");
    engine.envPlayMode = i ("envPlayMode");
    engine.envelope.loop = f ("envLoop") > 0.5f;
    engine.patternDivision = i ("patDivision");
    engine.patternSmoothing = f ("patSmoothing");
    engine.humanDetuneCents = f ("detune");
    engine.driftCents = f ("driftCents");
    engine.driftRateHz = f ("driftRate");
    engine.scalaEnabled = f ("scalaEnabled") > 0.5f && scala.isLoaded();

    mpeEnabled = f ("mpe") > 0.5f;
    engine.mpeEnabled = mpeEnabled;
    mpeVoices = i ("mpeVoices");
    sendConfig = f ("sendConfig") > 0.5f;
    static const int intervals[] = { 32, 64, 128, 256 };
    controlInterval = intervals[juce::jlimit (0, 3, i ("resolution"))];

    if (scala.getReferenceNote() != i ("scalaRef"))
    {
        scala.setReferenceNote (i ("scalaRef"));
        tuningCache = scala.tuningTableCents;
    }

    if (! juce::approximatelyEqual (engine.rangeSemitones, lastRange) || mpeVoices != lastMpeVoices || mpeEnabled != lastMpeEnabled)
    {
        // Mode change: release everything so no note is left on a stale channel
        if (mpeEnabled != lastMpeEnabled && lastRange > 0.0f)
        {
            for (int ch = 1; ch <= 16; ++ch)
            {
                outBuffer.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
                outBuffer.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), 0);
            }
            for (auto& v : voices) v = {};
            allocator.reset();
            noteMonitor.clear();
        }
        lastRange = engine.rangeSemitones;
        lastMpeVoices = mpeVoices;
        lastMpeEnabled = mpeEnabled;
        allocator.setNumChannels (mpeVoices);
        configPending = true;
    }
}

void BendCatProcessor::sendConfiguration (int pos)
{
    const int range = juce::roundToInt (engine.rangeSemitones);
    auto rpn = [this, pos] (int ch, int number, int value)
    {
        outBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 0), pos);
        outBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 100, number), pos);
        outBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 6, value), pos);
        outBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 38, 0), pos);
        outBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 127), pos);
        outBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 100, 127), pos);
    };

    if (mpeEnabled)
    {
        rpn (1, 6, allocator.getNumChannels());   // MPE Configuration Message: lower zone
        rpn (1, 0, 2);                             // master channel bend range (MPE default)
        for (int ch = MPEAllocator::firstChannel; ch < MPEAllocator::firstChannel + allocator.getNumChannels(); ++ch)
            rpn (ch, 0, range);                    // member channel pitch-bend sensitivity
    }
    else
    {
        for (int ch = 1; ch <= 16; ++ch)
            rpn (ch, 0, range);
    }
}

void BendCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    outBuffer.clear();
    hostSync.beginBlock (presetManager.linkClient->playHead (getPlayHead(), numSamples, getSampleRate()), numSamples);   // Cat Link: shared tempo in the apps
    midiLearn.processMidi (midi);
    blockSize = numSamples;

    // Re-send the bend range when the transport starts / jumps: the copy sent when the plug-in
    // was loaded is easily lost (e.g. the instrument track was not monitoring yet).
    if (hostSync.get().jumped && hostSync.get().hostPlaying)
        configPending = true;

    {
        // Held for the whole block; the editor only takes it for short copies.
        const juce::SpinLock::ScopedLockType lock (dataLock);
        readParams();

        if (configPending)
        {
            if (sendConfig) sendConfiguration (0);
            configPending = false;
            lastConfigSample = sampleCounter;
        }

        int lastPos = 0;
        for (const auto meta : midi)
        {
            const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
            renderControl (lastPos, pos);
            handleMessage (meta.getMessage(), pos);
            lastPos = pos;
        }
        renderControl (lastPos, numSamples);
    }

    // Free-running clock (for synced LFO / envelope / pattern) only while notes are held
    bool anyActive = false;
    for (auto& v : voices) anyActive = anyActive || v.active;
    if (! anyActive && hostSync.isFreeRunning())
        hostSync.stopFreeRun();

    // Visualiser feed
    float envPhase = -1.0f;
    int64_t newest = -1;
    for (size_t k = 0; k < voices.size(); ++k)
    {
        const auto& v = voices[k];
        const bool show = v.active || (v.releasing && sampleCounter - v.releaseSample < (int64_t) sampleRate);
        voiceViews[k].active = show;
        voiceViews[k].bend = v.currentBend;
        voiceViews[k].note = v.inNote;
        if (v.active && v.startSample > newest) { newest = v.startSample; envPhase = v.envPhase; }
    }
    uiEnvPhase = engine.mode == ModeEnvelope ? envPhase : -1.0f;

    uiPpq = hostSync.ppqAt (0);
    uiTransportRunning = hostSync.isRunning();

    midi.swapWith (outBuffer);
    hostSync.endBlock();
    sampleCounter += numSamples;
}

int BendCatProcessor::findVoice (int noteId) const
{
    for (size_t k = 0; k < voices.size(); ++k)
        if (voices[k].active && voices[k].noteId == noteId)
            return (int) k;
    return -1;
}

float BendCatProcessor::glideSourceFor (int note)
{
    if (previousChord.empty())
        return 0.0f;

    // Nearest unused note of the previous chord (greedy voice pairing)
    int best = -1, bestDist = 1000;
    for (int p : previousChord)
    {
        if (std::find (groupUsed.begin(), groupUsed.end(), p) != groupUsed.end() && groupUsed.size() < previousChord.size())
            continue;
        const int d = std::abs (p - note);
        if (d < bestDist) { bestDist = d; best = p; }
    }
    if (best < 0) return 0.0f;
    groupUsed.push_back (best);
    return (float) (best - note);
}

void BendCatProcessor::noteOn (int channel, int note, int velocity, int pos)
{
    const int64_t now = sampleCounter + pos;

    // Group note-ons that arrive together (a chord, possibly strummed) for glide pairing
    if (now - groupStartSample > (int64_t) (0.06 * sampleRate))
    {
        groupStartSample = now;
        groupUsed.clear();
        previousChord = heldPitches.empty() ? lastReleasedChord : heldPitches;
    }
    heldPitches.push_back (note);

    if (! hostSync.isRunning())
        hostSync.startFreeRunAt (pos);

    const int noteId = MPEAllocator::makeNoteId (channel, note);
    const float glideFrom = engine.mode == ModeGlide ? glideSourceFor (note) : 0.0f;
    const double ppq = hostSync.ppqAt (pos);

    int voiceIndex = -1, outChannel = channel;

    // The initial bend goes out one sample before the note-on, so the instrument has the
    // voice's pitch before the note starts even if the host reorders same-sample events.
    const int bendPos = pos > 0 ? pos - 1 : 0;
    const int notePos = pos > 0 ? pos : juce::jmin (1, juce::jmax (0, blockSize - 1));

    // First note after a pause: repeat the MPE / bend-range set-up so the instrument is
    // guaranteed to use the same range as Bend-Cat (otherwise bends land in the wrong octave).
    if (sendConfig && now - lastConfigSample > (int64_t) (0.5 * sampleRate))
    {
        bool anySounding = false;
        for (const auto& existing : voices)
            anySounding = anySounding || existing.active || existing.releasing;
        if (! anySounding)
        {
            sendConfiguration (bendPos);
            lastConfigSample = now;
        }
    }

    if (mpeEnabled)
    {
        int stolen = -1;
        outChannel = allocator.allocateChannel (noteId, &stolen);
        voiceIndex = outChannel - 1;
        auto& existing = voices[(size_t) voiceIndex];
        if (existing.active || existing.releasing)
        {
            if (existing.active)
                outBuffer.addEvent (juce::MidiMessage::noteOff (outChannel, existing.inNote), bendPos);
            noteMonitor.noteOff (existing.inNote);
        }
    }
    else
    {
        for (size_t k = 0; k < voices.size(); ++k)
            if (! voices[k].active) { voiceIndex = (int) k; break; }
        if (voiceIndex < 0) voiceIndex = 0;
        leadVoiceForChannel[(size_t) (channel - 1)] = voiceIndex;
    }

    auto& v = voices[(size_t) voiceIndex];
    engine.initVoice (v, channel, note, outChannel, now, ppq, glideFrom, rng);
    v.noteId = noteId;

    sendBend (voiceIndex, bendPos, true);
    outBuffer.addEvent (juce::MidiMessage::noteOn (outChannel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), notePos);
    noteMonitor.noteOn (outChannel, note, velocity);
}

void BendCatProcessor::noteOff (int channel, int note, int pos)
{
    const int noteId = MPEAllocator::makeNoteId (channel, note);
    heldPitches.erase (std::remove (heldPitches.begin(), heldPitches.end(), note), heldPitches.end());

    const int voiceIndex = findVoice (noteId);
    if (voiceIndex < 0)
    {
        // In MPE mode every note goes out on a member channel; a voice that is gone was
        // stolen and already ended there, so nothing must be sent on the input channel.
        if (! mpeEnabled)
            outBuffer.addEvent (juce::MidiMessage::noteOff (channel, note), pos);
        return;
    }

    auto& v = voices[(size_t) voiceIndex];

    // A note-on at the very start of a block goes out one sample late (after its initial
    // bend); a zero-length note must keep its note-off after that note-on.
    if (pos == 0 && v.startSample == sampleCounter)
        pos = juce::jmin (1, juce::jmax (0, blockSize - 1));

    outBuffer.addEvent (juce::MidiMessage::noteOff (v.outChannel, note), pos);
    noteMonitor.noteOff (note);
    v.active = false;
    v.releasing = true;
    v.releaseSample = sampleCounter + pos;
    if (mpeEnabled)
        allocator.releaseChannel (noteId);

    if (heldPitches.empty())
    {
        lastReleasedChord.clear();
        for (const auto& other : voices)
            if (other.releasing && sampleCounter + pos - other.releaseSample < (int64_t) (0.1 * sampleRate))
                lastReleasedChord.push_back (other.inNote);
    }
}

void BendCatProcessor::handleMessage (const juce::MidiMessage& m, int pos)
{
    if (m.isNoteOn())
    {
        noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity(), pos);
        return;
    }
    if (m.isNoteOff())
    {
        noteOff (m.getChannel(), m.getNoteNumber(), pos);
        return;
    }

    if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (size_t k = 0; k < voices.size(); ++k)
            if (voices[k].active)
                outBuffer.addEvent (juce::MidiMessage::noteOff (voices[k].outChannel, voices[k].inNote), pos);
        for (auto& v : voices) v = {};
        allocator.reset();
        heldPitches.clear();
        noteMonitor.clear();
        outBuffer.addEvent (m, pos);
        return;
    }

    if (mpeEnabled)
    {
        if (m.isAftertouch())   // poly pressure -> channel pressure on the voice's own channel (MPE)
        {
            const int k = findVoice (MPEAllocator::makeNoteId (m.getChannel(), m.getNoteNumber()));
            if (k >= 0)
                outBuffer.addEvent (juce::MidiMessage::channelPressureChange (voices[(size_t) k].outChannel, m.getAfterTouchValue()), pos);
            return;
        }

        if (m.isPitchWheel() || m.isController() || m.isChannelPressure() || m.isProgramChange())
        {
            // Zone-wide messages go to the MPE master channel
            auto copy = m;
            copy.setChannel (1);
            outBuffer.addEvent (copy, pos);
            return;
        }
    }

    outBuffer.addEvent (m, pos);
}

void BendCatProcessor::sendBend (int voiceIndex, int pos, bool force)
{
    auto& v = voices[(size_t) voiceIndex];
    if (! mpeEnabled && leadVoiceForChannel[(size_t) (v.outChannel - 1)] != voiceIndex)
        return; // channel-wide fallback: only the newest voice drives the channel's bend

    const float semis = engine.computeBend (v, sampleCounter + pos, hostSync.ppqAt (pos), hostSync.get().bpm, sampleRate,
                                            hostSync.get().hostPlaying, tuningCache);
    const int value = BendCatEngine::bendToPitchWheel (semis, engine.rangeSemitones);

    if (force || value != v.lastSentValue)
    {
        outBuffer.addEvent (juce::MidiMessage::pitchWheel (v.outChannel, value), pos);
        v.lastSentValue = value;
    }
}

void BendCatProcessor::renderControl (int fromSample, int toSample)
{
    if (toSample <= fromSample)
        return;

    const int64_t releaseTail = (int64_t) (1.5 * sampleRate);
    int64_t t = ((sampleCounter + fromSample + controlInterval - 1) / controlInterval) * controlInterval;

    for (; t < sampleCounter + toSample; t += controlInterval)
    {
        const int pos = (int) (t - sampleCounter);
        for (size_t k = 0; k < voices.size(); ++k)
        {
            auto& v = voices[k];
            if (v.releasing && t - v.releaseSample > releaseTail)
                v.releasing = false;
            if (v.active || v.releasing)
                sendBend ((int) k, pos, false);
        }
    }
}

//==============================================================================
void BendCatProcessor::surprise()
{
    // New expression: random mode + shape / LFO / glide / pattern. Bend Range, Amount and the
    // MPE settings are left alone - they have to match the instrument.
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    const float pick = r.nextFloat();
    const int mode = pick < 0.4f ? ModeEnvelope : (pick < 0.65f ? ModeLFO : (pick < 0.85f ? ModePattern : ModeGlide));
    set ("mode", (float) mode);

    // Envelope
    setEnvelope (makeShape (1 + r.nextInt (getShapeNames().size() - 1)));
    set ("envDepth", 0.3f + r.nextFloat() * 0.7f);
    set ("envRandom", r.nextFloat() < 0.6f ? 0.0f : r.nextFloat() * 0.35f);
    set ("envChance", r.nextFloat() < 0.7f ? 1.0f : 0.5f + r.nextFloat() * 0.5f);
    set ("envCycles", (float) (r.nextFloat() < 0.75f ? 1 : 2 + r.nextInt (3)));
    const bool synced = r.nextBool();
    set ("envLength", synced ? (float) (2 + r.nextInt (4)) : 0.0f);
    set ("envDuration", 120.0f + r.nextFloat() * 800.0f);
    set ("envLoop", r.nextFloat() < 0.3f ? 1.0f : 0.0f);
    set ("envPlayMode", 0.0f);

    // LFO
    set ("lfoWave", (float) r.nextInt (getLfoWaveNames().size()));
    set ("lfoSync", r.nextFloat() < 0.6f ? 1.0f : 0.0f);
    set ("lfoDivision", (float) (2 + r.nextInt (4)));
    set ("lfoRate", 0.8f + r.nextFloat() * 6.0f);
    set ("lfoDepth", 0.08f + r.nextFloat() * 0.4f);
    set ("lfoVoiceRandom", r.nextFloat() * 0.5f);
    set ("lfoPhaseRandom", r.nextBool() ? 1.0f : 0.0f);

    // Glide
    set ("glideTime", 40.0f + r.nextFloat() * 360.0f);
    set ("glideCurve", (float) r.nextInt (getGlideCurveNames().size()));

    // Pattern: half the steps stay at pitch, the others move up to +-50 cents
    std::array<float, 16> cents {};
    for (auto& c : cents)
        c = r.nextBool() ? 0.0f : (float) juce::roundToInt ((r.nextFloat() * 2.0f - 1.0f) * 50.0f);
    setPatternCents (cents);
    set ("patDivision", (float) (3 + r.nextInt (2)));
    set ("patSmoothing", r.nextFloat() * 0.6f);

    // A touch of analogue life now and then
    set ("detune", r.nextFloat() < 0.3f ? r.nextFloat() * 4.0f : 0.0f);
    set ("driftCents", r.nextFloat() < 0.3f ? r.nextFloat() * 4.0f : 0.0f);
}

//==============================================================================
const std::vector<BendCatProcessor::Preset>& BendCatProcessor::presets()
{
    // mode: 0 Off, 1 Glide, 2 Envelope, 3 LFO, 4 Pattern
    // shapes: 1 Ramp Up, 2 Curve Up, 3 Rise & Hold, 4 Ramp Down, 5 Curve Down, 6 Scoop, 9 Sine, 10 Bump,
    //         15 Square, 16 Square x2, 18 Ripple, 19 Step Down, 20 Step Up, 21 Wobble Decay, 22 Vibrato,
    //         23 Vibrato Swell, 24 Zigzag, 25 Spike, 26 Stutter
    static const std::vector<Preset> list = {
        { "Gentle Scoop",     6,  { { "mode", 2 }, { "bendAmount", 1 }, { "envDuration", 180 }, { "envDepth", 0.6f } }, {} },
        { "Guitar Bend",      3,  { { "mode", 2 }, { "bendAmount", 2 }, { "envDuration", 260 } }, {} },
        { "Dive Bomb",        4,  { { "mode", 2 }, { "bendAmount", 12 }, { "envDuration", 900 } }, {} },
        { "Riser Bar",        2,  { { "mode", 2 }, { "bendAmount", 12 }, { "envLength", 5 } }, {} },
        { "808 Drop",         5,  { { "mode", 2 }, { "bendAmount", 5 }, { "envDuration", 320 } }, {} },
        { "Vibrato Swell",    23, { { "mode", 2 }, { "bendAmount", 1 }, { "envDuration", 1400 }, { "envDepth", 0.5f } }, {} },
        { "Singer Vibrato",   -1, { { "mode", 3 }, { "bendAmount", 1 }, { "lfoWave", 0 }, { "lfoRate", 5.5f }, { "lfoDepth", 0.25f },
                                    { "lfoVoiceRandom", 0.3f }, { "lfoPhaseRandom", 1 } }, {} },
        { "Tape Wow",         -1, { { "mode", 3 }, { "bendAmount", 1 }, { "lfoWave", 0 }, { "lfoRate", 0.6f }, { "lfoDepth", 0.12f },
                                    { "lfoVoiceRandom", 0.6f }, { "lfoPhaseRandom", 1 }, { "driftCents", 3 } }, {} },
        { "Wobble 1/8",       -1, { { "mode", 3 }, { "bendAmount", 2 }, { "lfoWave", 1 }, { "lfoSync", 1 }, { "lfoDivision", 3 }, { "lfoDepth", 0.4f } }, {} },
        { "Seasick",          -1, { { "mode", 3 }, { "bendAmount", 1 }, { "lfoWave", 3 }, { "lfoRate", 0.4f }, { "lfoDepth", 0.35f },
                                    { "lfoVoiceRandom", 1.0f }, { "lfoPhaseRandom", 1 } }, {} },
        { "S&H Glitch",       -1, { { "mode", 3 }, { "bendAmount", 1 }, { "lfoWave", 4 }, { "lfoSync", 1 }, { "lfoDivision", 4 }, { "lfoDepth", 0.5f } }, {} },
        { "Trill",            16, { { "mode", 2 }, { "bendAmount", 2 }, { "envCycles", 4 }, { "envLoop", 1 }, { "envLength", 3 } }, {} },
        { "Portamento",       -1, { { "mode", 1 }, { "glideTime", 250 }, { "glideCurve", 1 } }, {} },
        { "Slow Glide",       -1, { { "mode", 1 }, { "glideTime", 700 }, { "glideCurve", 2 } }, {} },
        { "Snappy Glide",     -1, { { "mode", 1 }, { "glideTime", 60 }, { "glideCurve", 0 } }, {} },
        { "Stutter Pitch",    26, { { "mode", 2 }, { "bendAmount", 12 }, { "envCycles", 2 }, { "envDuration", 400 } }, {} },
        { "Zigzag Lead",      24, { { "mode", 2 }, { "bendAmount", 2 }, { "envLength", 2 }, { "envLoop", 1 } }, {} },
        { "Spike Pluck",      25, { { "mode", 2 }, { "bendAmount", 12 }, { "envDuration", 60 } }, {} },
        { "Staircase",        19, { { "mode", 2 }, { "bendAmount", 12 }, { "envLength", 5 } }, {} },
        { "Ripple",           18, { { "mode", 2 }, { "bendAmount", 1 }, { "envDuration", 600 } }, {} },
        { "Wobble Decay",     21, { { "mode", 2 }, { "bendAmount", 2 }, { "envDuration", 700 } }, {} },
        { "Random Scoops",    6,  { { "mode", 2 }, { "bendAmount", 2 }, { "envDuration", 200 }, { "envRandom", 0.35f }, { "envChance", 0.6f } }, {} },
        { "Microtonal Steps", -1, { { "mode", 4 }, { "patDivision", 4 }, { "patSmoothing", 0.1f } },
                                  { 0, 0, -50, 0, 25, 0, -25, 0, 0, 50, 0, -25, 0, 25, -50, 0 } },
        { "Blue Notes",       -1, { { "mode", 4 }, { "patDivision", 3 }, { "patSmoothing", 0.5f } },
                                  { 0, 0, 0, -30, 0, 0, -50, 0, 0, 0, -30, 0, 0, -50, 0, 0 } },
        { "Analog Choir",     -1, { { "mode", 0 }, { "detune", 8 }, { "driftCents", 6 }, { "driftRate", 0.08f } }, {} },
    };
    return list;
}

void BendCatProcessor::applyPreset (int index)
{
    const auto& list = presets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    currentPreset = index;
    const auto& preset = list[(size_t) index];

    // Expression parameters back to their defaults, then the preset's own values
    for (auto* id : { "mode", "bendAmount", "glideTime", "glideCurve", "lfoRate", "lfoSync", "lfoDivision", "lfoDepth", "lfoWave",
                      "lfoVoiceRandom", "lfoPhaseRandom", "envDepth", "envRandom", "envChance", "envCycles", "envLength",
                      "envDuration", "envPlayMode", "envLoop", "patDivision", "patSmoothing", "detune", "driftCents", "driftRate" })
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getDefaultValue());
            prm->endChangeGesture();
        }
    for (const auto& [id, v] : preset.values)
        cat::SurpriseControls::setParam (*this, id, v);

    if (preset.shape >= 0)
        setEnvelope (makeShape (preset.shape));
    std::array<float, 16> cents {};
    for (size_t i = 0; i < cents.size() && i < preset.pattern.size(); ++i)
        cents[i] = juce::jlimit (-50.0f, 50.0f, preset.pattern[i]);
    setPatternCents (cents);
}

BendEnvelope BendCatProcessor::getEnvelope() const
{
    const juce::SpinLock::ScopedLockType lock (dataLock);
    auto env = engine.envelope;
    return env;
}

void BendCatProcessor::setEnvelope (const BendEnvelope& env)
{
    auto copy = env;
    copy.sortAndClamp();
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        engine.envelope.points = copy.points;
        engine.envelope.curves = copy.curves;
    }
    dataVersion.fetch_add (1);
}

std::array<float, 16> BendCatProcessor::getPatternCents() const
{
    const juce::SpinLock::ScopedLockType lock (dataLock);
    return engine.patternCents;
}

void BendCatProcessor::setPatternCents (const std::array<float, 16>& cents)
{
    const juce::SpinLock::ScopedLockType lock (dataLock);
    engine.patternCents = cents;
    dataVersion.fetch_add (1);
}

bool BendCatProcessor::loadScala (const juce::File& file, juce::String& error)
{
    ScalaParser parser;
    parser.setReferenceNote (juce::roundToInt (apvts.getRawParameterValue ("scalaRef")->load()));
    if (! parser.loadSclFile (file))
    {
        error = parser.getLastError();
        return false;
    }
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        scala = parser;
        tuningCache = scala.tuningTableCents;
    }
    if (auto* p = apvts.getParameter ("scalaEnabled"))
        p->setValueNotifyingHost (1.0f);
    dataVersion.fetch_add (1);
    return true;
}

void BendCatProcessor::clearScala()
{
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        scala.clear();
        tuningCache.fill (0.0f);
    }
    dataVersion.fetch_add (1);
}

juce::String BendCatProcessor::getScalaDescription() const
{
    const juce::SpinLock::ScopedLockType lock (dataLock);
    return scala.isLoaded() ? scala.getDescription() : juce::String();
}

int BendCatProcessor::getScalaNumDegrees() const
{
    const juce::SpinLock::ScopedLockType lock (dataLock);
    return scala.isLoaded() ? scala.getNumDegrees() : 0;
}

std::array<float, 128> BendCatProcessor::getTuningTable() const
{
    const juce::SpinLock::ScopedLockType lock (dataLock);
    return tuningCache;
}

//==============================================================================
void BendCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("BENDCAT");
    root.setProperty ("preset", currentPreset.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        auto data = engine.toValueTree();
        data.setProperty ("scala", scala.isLoaded() ? scala.getRawText() : juce::String(), nullptr);
        root.appendChild (data, nullptr);
    }
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void BendCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("BENDCAT")) return;
    currentPreset = (int) root.getProperty ("preset", -1);

    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);

    const auto bendData = root.getChildWithName (BendCatEngine::treeId);
    BendCatEngine loaded;
    loaded.fromValueTree (bendData);

    ScalaParser parser;
    parser.setReferenceNote (juce::roundToInt (apvts.getRawParameterValue ("scalaRef")->load()));
    const auto sclText = bendData.getProperty ("scala", "").toString();
    if (sclText.isNotEmpty()) parser.parse (sclText);

    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        engine.envelope.points = loaded.envelope.points;
        engine.envelope.curves = loaded.envelope.curves;
        engine.patternCents = loaded.patternCents;
        scala = parser;
        tuningCache = scala.tuningTableCents;
    }
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
    dataVersion.fetch_add (1);
}

juce::AudioProcessorEditor* BendCatProcessor::createEditor()
{
    // The editor is laid out at its design size and scaled as a whole (stays sharp at any size)
    return new cat::ScaledEditor (*this, std::make_unique<BendCatEditor> (*this), "Bend-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BendCatProcessor();
}
