#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace pattcat;

namespace
{
    const char* layerSuffix[] = { "A", "B", "C", "D" };

    // A note-on arriving this long after the previous one starts a new chord, even if keys of
    // the previous chord are still held (legato playing would otherwise merge both chords).
    constexpr double newChordGapSeconds = 0.05;

    // A chord that arrives up to this long after a hit started (e.g. played slightly late, or
    // routed through Cubase with a little jitter) still gets that hit.
    constexpr double lateChordCatchSeconds = 0.08;
}

juce::AudioProcessorValueTreeState::ParameterLayout PattCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "inputMode", 1 }, "Input Mode",
                                                        StringArray { "Hold Chord", "Advance Chords" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "latch", 1 }, "Latch", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "followGroove", 1 }, "Follow Drum Groove", false));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "globalSwing", 1 }, "Global Swing", 0, 100, 0,
                                                     AudioParameterIntAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gateRandom", 1 }, "Gate Random",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.5f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "useInputVel", 1 }, "Use Input Velocity", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "runStopped", 1 }, "Run When Stopped", true));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "inChannel", 1 }, "Input Channel", 0, 16, 0,
                                                     AudioParameterIntAttributes().withStringFromValueFunction (
                                                         [] (int v, int) { return v == 0 ? String ("Omni") : String (v); })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "thru", 1 }, "Chord Thru", false));

    // Bounce
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bounceMode", 1 }, "Bounce", StringArray { "Off", "Pattern Notes", "Keys" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bounceSync", 1 }, "Bounce Rate Mode", StringArray { "Sync", "Free" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bounceRate", 1 }, "Bounce Rate", getBounceRateNames(), 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bounceMs", 1 }, "Bounce Time",
                                                       NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f), 300.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "bounceNotes", 1 }, "Bounce Notes", 1, 32, 8));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bounceCurve", 1 }, "Bounce Curve",
                                                       NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 40.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bounceFade", 1 }, "Bounce Fade Out",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f, AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "bounceOctave", 1 }, "Bounce Octave", -2, 2, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bounceOctRnd", 1 }, "Bounce Octave Random", StringArray { "Off", "+-1 Oct", "+-2 Oct" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bounceDur", 1 }, "Bounce Duration",
                                                       NormalisableRange<float> (0.02f, 2.0f, 0.01f, 0.5f), 0.25f, AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bounceHuman", 1 }, "Bounce Human",
                                                       NormalisableRange<float> (0.0f, 50.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bounceHold", 1 }, "Bounce Play Mode", StringArray { "One Shot", "While Held" }, 0));

    for (int l = 0; l < 4; ++l)
    {
        const String s (layerSuffix[l]);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "mute" + s, 1 }, "Mute " + s, false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "solo" + s, 1 }, "Solo " + s, false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vol" + s, 1 }, "Volume " + s,
                                                           NormalisableRange<float> (0.0f, 1.5f, 0.01f), l == 0 ? 1.0f : 0.9f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (
                                                               [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; })));
    }
    return layout;
}

PattCatProcessor::PattCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                      ),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    {
        cat::LinkClient::Caps caps;
        caps.receiveChords = true;
        caps.receiveChordsText = "Play Chord-Cat's chords";
        caps.grooveParam = "followGroove";
        presetManager.enableLink (apvts, caps);
    }
    cat::followDrumGroove (presetManager, apvts, [this] (const cat::CatLink::Groove& g)
    {
        cat::SurpriseControls::setParam (*this, "globalSwing", (float) juce::roundToInt (g.swing));
    });

    scheduler.monitor = &noteMonitor;
    bounce = { apvts.getRawParameterValue ("bounceMode"), apvts.getRawParameterValue ("bounceSync"),
               apvts.getRawParameterValue ("bounceRate"), apvts.getRawParameterValue ("bounceMs"),
               apvts.getRawParameterValue ("bounceNotes"), apvts.getRawParameterValue ("bounceCurve"),
               apvts.getRawParameterValue ("bounceFade"), apvts.getRawParameterValue ("bounceOctave"),
               apvts.getRawParameterValue ("bounceOctRnd"), apvts.getRawParameterValue ("bounceDur"),
               apvts.getRawParameterValue ("bounceHuman"), apvts.getRawParameterValue ("bounceHold") };
    held.reserve (64);
    heldKeys.reserve (128);
    chord.reserve (64);
    voiceScratch.reserve (64);
    activeHits.reserve (1024);
    memory.reserve (16);
    lastChordForUi.reserve (64);
    lastChordForUi = { 60, 64, 67, 71 };
}

PattCatProcessor::~PattCatProcessor() = default;

bool PattCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void PattCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSync.prepare (sampleRate);
    scheduler.prepare (8192);
    outBuffer.ensureSize ((size_t) juce::jmax (4096, samplesPerBlock * 4));
    sampleCounter = 0;
    held.clear();
    heldKeys.clear();
    chord.clear();
    activeHits.clear();
    keysChanged = noteOnInGroup = false;
    lastNoteOnTime = chordGroupStart = -1000000;
    memoryPendingTime = -1;
}

void PattCatProcessor::syncMixerFromParams()
{
    const juce::SpinLock::ScopedLockType lock (engineLock);
    for (int l = 0; l < 4; ++l)
    {
        const juce::String s (layerSuffix[l]);
        auto& layer = engine.layers[(size_t) l];
        layer.muted = apvts.getRawParameterValue ("mute" + s)->load() > 0.5f;
        layer.solo = apvts.getRawParameterValue ("solo" + s)->load() > 0.5f;
        layer.volume = apvts.getRawParameterValue ("vol" + s)->load();
    }
}

void PattCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    presetManager.linkClient->processInput (midi, buffer.getNumSamples());     // Cat Link: Chord-Cat's chord
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    hostSync.beginBlock (getPlayHead(), numSamples);
    lastBpm = hostSync.get().bpm;
    uiWallClockAtBlock = juce::Time::getMillisecondCounterHiRes();
    midiLearn.processMidi (midi);
    syncMixerFromParams();

    if (clearMemoryRequest.exchange (false))
    {
        memory.clear();
        memorySize = 0;
    }

    // Note events that share a sample are applied as one group, so a chord change arriving as
    // "note-offs + note-ons on the same sample" (Chord-Cat) is a change, not stop-then-start.
    int lastPos = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
        if (pos != lastPos && keysChanged)
            applyHeldKeys (lastPos);
        renderPattern (lastPos, pos);
        handleMessage (meta.getMessage(), pos);
        lastPos = pos;
    }
    if (keysChanged)
        applyHeldKeys (lastPos);
    renderPattern (lastPos, numSamples);

    if (memoryPendingTime >= 0 && memoryPendingTime < sampleCounter + numSamples)
    {
        memoryPendingTime = -1;
        storeChordInMemory();
    }

    if (chord.empty() && hostSync.isFreeRunning())
        hostSync.stopFreeRun();

    activeHits.erase (std::remove_if (activeHits.begin(), activeHits.end(),
                                      [this, numSamples] (const ActiveHit& h) { return h.end < sampleCounter + numSamples; }),
                      activeHits.end());

    outBuffer.clear();
    scheduler.emit (outBuffer, sampleCounter, numSamples);
    midi.swapWith (outBuffer);

    uiPpq = hostSync.ppqAt (0);
    uiBarLength = hostSync.get().barLengthPpq;
    uiRunning = hostSync.isRunning() && ! chord.empty();
    uiTransportRunning = hostSync.isRunning();

    hostSync.endBlock();
    sampleCounter += numSamples;
}

void PattCatProcessor::handleMessage (const juce::MidiMessage& m, int pos)
{
    const int inChannel = juce::roundToInt (apvts.getRawParameterValue ("inChannel")->load());
    const bool thru = apvts.getRawParameterValue ("thru")->load() > 0.5f;

    if (m.isNoteOnOrOff() && (inChannel == 0 || m.getChannel() == inChannel))
    {
        if (thru)
            scheduler.schedule (sampleCounter + pos, m);

        const int note = m.getNoteNumber();
        heldKeys.erase (std::remove_if (heldKeys.begin(), heldKeys.end(), [note] (const HeldKey& k) { return k.note == note; }),
                        heldKeys.end());

        if (juce::roundToInt (bounce.mode->load()) == 2)
        {
            if (m.isNoteOn())
                spawnBounces (m.getChannel(), note, m.getVelocity(), sampleCounter + pos, true, note);
            else if (juce::roundToInt (bounce.hold->load()) == 1)
                stopKeyBounces (note, sampleCounter + pos);
        }

        if (m.isNoteOn())
        {
            heldKeys.push_back ({ note, m.getVelocity(), sampleCounter + pos });
            noteOnInGroup = true;

            const bool runStopped = apvts.getRawParameterValue ("runStopped")->load() > 0.5f;
            if (! hostSync.isRunning() && runStopped)
                hostSync.startFreeRunAt (pos);

            memoryPendingTime = sampleCounter + pos + (int64_t) (0.03 * hostSync.getSampleRate());
        }

        keysChanged = true;
        return;
    }

    if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        held.clear();
        heldKeys.clear();
        chord.clear();
        keysChanged = noteOnInGroup = false;
        releaseActiveNotes (pos);
    }

    scheduler.schedule (sampleCounter + pos, m);
}

void PattCatProcessor::applyHeldKeys (int pos)
{
    const int64_t now = sampleCounter + pos;
    const auto gap = (int64_t) (newChordGapSeconds * hostSync.getSampleRate());
    const bool latch = apvts.getRawParameterValue ("latch")->load() > 0.5f;
    const bool hadNoteOn = noteOnInGroup;

    if (hadNoteOn)
    {
        if (now - lastNoteOnTime > gap)
            chordGroupStart = now;       // new chord: keys still held from the previous one are left out
        lastNoteOnTime = now;
    }
    keysChanged = noteOnInGroup = false;

    held.clear();
    int maxVelocity = 0;
    for (const auto& k : heldKeys)
        if (k.time >= chordGroupStart)
        {
            held.push_back (k.note);
            maxVelocity = juce::jmax (maxVelocity, k.velocity);
        }

    if (held.empty())
    {
        if (latch)
        {
            chordFromLatch = true;
        }
        else if (! chord.empty())
        {
            chord.clear();
            releaseActiveNotes (pos);
        }
        return;
    }

    if (! hadNoteOn && (latch || chordFromLatch))
        return;                          // latched chord does not shrink when keys come up

    chordFromLatch = false;
    std::vector<int> newChord = held;
    std::sort (newChord.begin(), newChord.end());

    // One velocity for the whole chord (the loudest key), not whichever key happened to arrive last
    if (hadNoteOn)
        chordVelocity = maxVelocity;

    if (newChord != chord)
    {
        chord = newChord;
        onChordChanged (pos);

        const juce::SpinLock::ScopedLockType lock (chordLock);
        lastChordForUi = chord;
    }
}

void PattCatProcessor::playHitNotes (ActiveHit& hit, const std::vector<int>& source, int64_t from)
{
    if (source.empty() || hit.end <= from)
        return;

    const bool useInputVel = apvts.getRawParameterValue ("useInputVel")->load() > 0.5f;
    const float base = useInputVel ? (float) chordVelocity / 127.0f : 0.8f;
    const int vel = juce::jlimit (1, 127, juce::roundToInt (hit.velocity * base * 127.0f));
    const auto& layer = engine.layers[(size_t) hit.layer];

    PattCatEngine::selectVoices (source, hit.voiceIndex, voiceScratch);

    for (int note : voiceScratch)
    {
        const int n = note + 12 * hit.octave;
        if (n < 0 || n > 127 || hit.numNotes >= (int) hit.notes.size())
            continue;

        bool already = false;
        for (int i = 0; i < hit.numNotes; ++i)
            already = already || hit.notes[(size_t) i].second == n;
        if (already)
            continue;

        const int ch = layer.mpe ? 2 + (mpeRoundRobin++ % 15) : layer.midiChannel;
        scheduler.scheduleNote (from, hit.end, ch, n, vel);
        hit.notes[(size_t) hit.numNotes++] = { ch, n };

        if (juce::roundToInt (bounce.mode->load()) == 1 && from == hit.start)
            spawnBounces (ch, n, vel, from, false, -1);
    }
}

void PattCatProcessor::onChordChanged (int pos)
{
    if (juce::roundToInt (apvts.getRawParameterValue ("inputMode")->load()) == 1)
        return; // Advance mode: each step plays its own remembered chord

    const int64_t now = sampleCounter + pos;
    const double sr = hostSync.getSampleRate();
    const double spp = hostSync.samplesPerPpq();
    std::vector<int> wanted;

    const juce::SpinLock::ScopedLockType lock (engineLock);
    for (auto& hit : activeHits)
    {
        if (hit.end <= now)
            continue;

        // Re-voice the hit with the new chord when the chord came just after the hit started
        // (late chord) or when a long note still has a good part of its length to go.
        const auto catchWindow = (int64_t) juce::jmin (lateChordCatchSeconds * sr, 0.5 * (double) (hit.end - hit.start));
        const bool revoice = now - hit.start <= catchWindow || (double) (hit.end - now) >= 0.25 * spp;

        PattCatEngine::selectVoices (chord, hit.voiceIndex, voiceScratch);
        wanted.clear();
        for (int note : voiceScratch)
            wanted.push_back (note + 12 * hit.octave);

        // End the notes that are not part of the new chord; common tones keep ringing
        int kept = 0;
        for (int i = 0; i < hit.numNotes; ++i)
        {
            const auto cn = hit.notes[(size_t) i];
            if (std::find (wanted.begin(), wanted.end(), cn.second) != wanted.end())
                hit.notes[(size_t) kept++] = cn;
            else
                scheduler.releaseNote (cn.first, cn.second, now);
        }
        hit.numNotes = kept;

        if (revoice)
            playHitNotes (hit, chord, juce::jmax (now, hit.start));
    }
}

//==============================================================================
juce::StringArray PattCatProcessor::getBounceRateNames()
{
    return { "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4D", "1/8D" };
}

double PattCatProcessor::bounceRateToPpq (int index)
{
    static const double v[] = { 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
    return v[juce::jlimit (0, 9, index)];
}

void PattCatProcessor::pushBounceEvent (const BounceEvent& e)
{
    const auto scope = bounceFifo.write (1);
    if (scope.blockSize1 > 0) bounceEvents[(size_t) scope.startIndex1] = e;
    else if (scope.blockSize2 > 0) bounceEvents[(size_t) scope.startIndex2] = e;
}

void PattCatProcessor::spawnBounces (int channel, int note, int velocity, int64_t start, bool includeFirst, int sourceKey)
{
    const double sr = hostSync.getSampleRate();
    const int count = juce::roundToInt (bounce.notes->load());
    const float curve = bounce.curve->load() / 100.0f;
    const float fade = bounce.fade->load() / 100.0f;
    const int octave = juce::roundToInt (bounce.octave->load());
    const int octRnd = juce::roundToInt (bounce.octRnd->load());
    const double humanSamples = bounce.human->load() * 0.001 * sr;
    const auto dur = (int64_t) juce::jmax (1.0, (double) bounce.dur->load() * sr);

    // First gap: a note value at the host tempo, or milliseconds
    const double firstGap = juce::roundToInt (bounce.sync->load()) == 0
                                ? bounceRateToPpq (juce::roundToInt (bounce.rate->load())) * hostSync.samplesPerPpq()
                                : bounce.ms->load() * 0.001 * sr;
    // Curve > 0: each gap is shorter than the last (a real ball); < 0: the bounces slow down
    const double ratio = curve >= 0.0f ? 1.0 - 0.45 * curve : 1.0 - 0.6 * curve;

    auto& echoes = keyEchoes[(size_t) juce::jlimit (0, 127, sourceKey < 0 ? 0 : sourceKey)];
    if (sourceKey >= 0) { echoes.count = 0; echoes.channel = channel; }
    auto remember = [&echoes, sourceKey] (int n)
    {
        if (sourceKey >= 0 && echoes.count < (int) echoes.notes.size())
            echoes.notes[(size_t) echoes.count++] = (uint8_t) n;
    };

    if (includeFirst)
    {
        scheduler.scheduleNote (start, start + dur, channel, note, velocity);
        remember (note);
    }

    double t = (double) start, gap = firstGap;
    const double maxSpan = 32.0 * hostSync.samplesPerPpq();   // never schedule more than 8 bars ahead
    for (int k = 1; k <= count; ++k)
    {
        t += gap;
        gap = juce::jlimit (0.002 * sr, 8.0 * firstGap, gap * ratio);
        if (t - (double) start > maxSpan) break;

        int n = note + 12 * octave;
        if (octRnd > 0) n += 12 * (rng.nextInt (2 * octRnd + 1) - octRnd);
        n = juce::jlimit (0, 127, n);
        const float v = (float) velocity * (1.0f - fade * (float) k / (float) count);
        const int vel = juce::jlimit (1, 127, juce::roundToInt (v));
        const auto when = (int64_t) (t + (rng.nextFloat() * 2.0f - 1.0f) * humanSamples);
        scheduler.scheduleNote (juce::jmax (start + 1, when), juce::jmax (start + 1, when) + dur, channel, n, vel);
        remember (n);
    }

    BounceEvent e;
    e.note = note;
    e.count = count;
    e.velocity = (float) velocity / 127.0f;
    e.firstGapMs = (float) (firstGap / sr * 1000.0);
    e.ratio = (float) ratio;
    e.fade = fade;
    e.startMs = uiWallClockAtBlock + (double) (start - sampleCounter) / sr * 1000.0;
    pushBounceEvent (e);
}

void PattCatProcessor::stopKeyBounces (int key, int64_t now)
{
    auto& echoes = keyEchoes[(size_t) juce::jlimit (0, 127, key)];
    for (int i = 0; i < echoes.count; ++i)
        scheduler.releaseNote (echoes.channel, echoes.notes[(size_t) i], now);
    echoes.count = 0;

    BounceEvent e;
    e.note = key;
    e.count = 0;   // tells the display to drop this cat
    e.startMs = uiWallClockAtBlock;
    pushBounceEvent (e);
}

void PattCatProcessor::storeChordInMemory()
{
    if (juce::roundToInt (apvts.getRawParameterValue ("inputMode")->load()) != 1 || chord.empty())
        return;

    if (! memory.empty() && memory.back() == chord)
        return;

    if (memory.size() >= 16)
        memory.erase (memory.begin());
    memory.push_back (chord);
    memorySize = (int) memory.size();
}

void PattCatProcessor::renderPattern (int fromSample, int toSample)
{
    if (toSample <= fromSample || ! hostSync.isRunning())
        return;

    const int mode = juce::roundToInt (apvts.getRawParameterValue ("inputMode")->load());
    const int swing = juce::roundToInt (apvts.getRawParameterValue ("globalSwing")->load());
    const float gateRandom = apvts.getRawParameterValue ("gateRandom")->load() / 100.0f;
    const double spp = hostSync.samplesPerPpq();
    const double ppqFrom = hostSync.ppqAt (fromSample);
    const double ppqTo = hostSync.ppqAt (toSample);
    const double bar = hostSync.get().barLengthPpq;

    const juce::SpinLock::ScopedLockType lock (engineLock);
    engine.collectHits (ppqFrom, ppqTo, bar, swing, gateRandom, rng, true, [&] (const PatternHit& h)
    {
        const auto& source = (mode == 1 && ! memory.empty()) ? memory[(size_t) h.step % memory.size()] : chord;

        ActiveHit hit;
        hit.start = sampleCounter + (int64_t) std::llround (hostSync.sampleForPpq (h.ppq));
        hit.end = hit.start + juce::jmax ((int64_t) 1, (int64_t) (h.lengthPpq * spp));
        hit.layer = h.layer;
        hit.step = h.step;
        hit.voiceIndex = h.voiceIndex;
        hit.octave = h.octaveOffset;
        hit.velocity = h.velocity;

        // Hits are remembered even when no chord is held yet, so a chord that arrives a few
        // milliseconds late still plays this step (see onChordChanged).
        playHitNotes (hit, source, hit.start);
        if (activeHits.size() < activeHits.capacity())
            activeHits.push_back (hit);
    });
}

void PattCatProcessor::releaseActiveNotes (int pos)
{
    const int64_t now = sampleCounter + pos;
    for (auto& hit : activeHits)
    {
        if (hit.end > now)
            for (int i = 0; i < hit.numNotes; ++i)
                scheduler.releaseNote (hit.notes[(size_t) i].first, hit.notes[(size_t) i].second, now);
        hit.numNotes = 0;   // kept (without notes) so a chord arriving just after can still catch it
    }
}

//==============================================================================
PattCatEngine PattCatProcessor::getEngineCopy() const
{
    const juce::SpinLock::ScopedLockType lock (engineLock);
    return engine;
}

void PattCatProcessor::editEngine (const std::function<void (PattCatEngine&)>& fn)
{
    auto copy = getEngineCopy();
    fn (copy);
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine = copy;
    }
    engineVersion.fetch_add (1);
}

void PattCatProcessor::applyPreset (int index)
{
    currentPreset = juce::jlimit (0, getPatternPresetNames().size() - 1, index);
    editEngine ([this] (PattCatEngine& e) { applyPatternPreset (e, currentPreset); });
}

void PattCatProcessor::surprise()
{
    // A random factory pattern, sometimes with a re-rolled extra layer, plus some groove
    auto& r = juce::Random::getSystemRandom();
    applyPreset (r.nextInt (getPatternPresetNames().size()));

    if (r.nextFloat() < 0.5f)
    {
        const int layer = 1 + r.nextInt (3);
        const auto seed = r.nextInt64();
        editEngine ([layer, seed] (PattCatEngine& e)
        {
            juce::Random local (seed);
            e.layers[(size_t) layer].muted = false;
            e.layers[(size_t) layer].length = e.layers[0].length;
            e.randomizeLayer (layer, local);
            e.layers[(size_t) layer].volume = 0.6f + local.nextFloat() * 0.3f;
        });
    }

    cat::SurpriseControls::setParam (*this, "globalSwing", (float) (r.nextFloat() < 0.5f ? 0 : r.nextInt (35)));
    cat::SurpriseControls::setParam (*this, "gateRandom", r.nextFloat() < 0.6f ? 0.0f : r.nextFloat() * 20.0f);
}

std::vector<int> PattCatProcessor::getLastChord() const
{
    const juce::SpinLock::ScopedLockType lock (chordLock);
    return lastChordForUi;
}

juce::MidiMessageSequence PattCatProcessor::buildPatternSequence() const
{
    juce::MidiMessageSequence seq;
    const auto e = getEngineCopy();
    auto chordNotes = getLastChord();
    if (chordNotes.empty()) chordNotes = { 60, 64, 67, 71 };

    double length = 4.0;
    for (int l = 0; l < 4; ++l)
        if (e.isLayerAudible (l))
            length = std::max (length, e.getCycleLengthPpq (l, 4.0));
    length = std::min (length, 32.0);

    juce::Random r;
    const int swing = juce::roundToInt (apvts.getRawParameterValue ("globalSwing")->load());
    std::vector<int> voices;

    e.collectHits (0.0, length, 4.0, swing, 0.0f, r, false, [&] (const PatternHit& h)
    {
        PattCatEngine::selectVoices (chordNotes, h.voiceIndex, voices);
        const int vel = juce::jlimit (1, 127, juce::roundToInt (h.velocity * 100.0f * h.probability));
        for (int note : voices)
        {
            const int n = note + 12 * h.octaveOffset;
            if (n < 0 || n > 127) continue;
            const int ch = e.layers[(size_t) h.layer].midiChannel;
            seq.addEvent (juce::MidiMessage::noteOn (ch, n, (juce::uint8) vel).withTimeStamp (h.ppq));
            seq.addEvent (juce::MidiMessage::noteOff (ch, n).withTimeStamp (std::min (length, h.ppq + h.lengthPpq)));
        }
    });

    seq.sort();
    seq.updateMatchedPairs();
    return seq;
}

//==============================================================================
void PattCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("PATTCAT");
    root.setProperty ("preset", currentPreset, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (getEngineCopy().toValueTree(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void PattCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("PATTCAT")) return;

    currentPreset = root.getProperty ("preset", 0);
    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);

    PattCatEngine loaded;
    loaded.fromValueTree (root.getChildWithName (PattCatEngine::treeId));
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine = loaded;
    }
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
    engineVersion.fetch_add (1);
}

juce::AudioProcessorEditor* PattCatProcessor::createEditor()
{
    // The editor is laid out at its design size and scaled as a whole (stays sharp at any size)
    return new cat::ScaledEditor (*this, std::make_unique<PattCatEditor> (*this), "Patt-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PattCatProcessor();
}
