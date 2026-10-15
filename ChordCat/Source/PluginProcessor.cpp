#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"
#include "FactoryPresets.h"
#include "MidiExport.h"
#include "StrumEngine.h"
#include <map>
#include <numeric>

using namespace chordcat;

namespace
{
    juce::String noteText (int v) { return juce::MidiMessage::getMidiNoteName (v, true, true, 3); }

    juce::StringArray keyNames()
    {
        juce::StringArray k;
        for (int i = 0; i < 12; ++i) k.add (pitchClassName (i, i == 3 || i == 8 || i == 10));
        return k;
    }

    bool isBlackKey (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    // Trigger keys pressed within this time of each other count as one press (last key wins)
    constexpr double simultaneousKeysSeconds = 0.03;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ChordCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto intParam = [&] (const char* id, const char* name, int lo, int hi, int def,
                         std::function<String (int, int)> toText = nullptr)
    {
        auto attrs = AudioParameterIntAttributes();
        if (toText) attrs = attrs.withStringFromValueFunction (toText);
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def, attrs));
    };
    auto floatParam = [&] (const char* id, const char* name, float lo, float hi, float def, float step, const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def,
                                                           AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto boolParam = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto choiceParam = [&] (const char* id, const char* name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };

    // Trigger / routing
    intParam ("inChannel", "Input Channel", 0, 16, 0, [] (int v, int) { return v == 0 ? String ("Omni") : String (v); });
    intParam ("outChannel", "Output Channel", 1, 16, 1);
    intParam ("triggerLow", "Trigger Low", 0, 127, 60, [] (int v, int) { return noteText (v); });
    intParam ("triggerHigh", "Trigger High", 0, 127, 83, [] (int v, int) { return noteText (v); });
    choiceParam ("triggerMode", "Trigger Mode", { "Chromatic", "White Keys" }, 0);
    intParam ("numPads", "Pads", 4, 16, 8);
    boolParam ("legato", "Legato", false);
    boolParam ("followGroove", "Follow Drum Groove", false);
    intParam ("velocity", "Fixed Velocity", 0, 127, 0, [] (int v, int) { return v == 0 ? String ("Input") : String (v); });

    // Voicing / harmony
    floatParam ("voiceLeading", "Voice Leading", 0.0f, 100.0f, 60.0f, 1.0f, "%");
    intParam ("transpose", "Transpose", -24, 24, 0, [] (int v, int) { return (v > 0 ? "+" : "") + String (v); });
    boolParam ("bassSplit", "Bass Split", false);
    intParam ("bassChannel", "Bass Channel", 1, 16, 2);
    choiceParam ("keyRoot", "Key", keyNames(), 0);
    choiceParam ("scale", "Scale", getScaleNames(), 0);
    boolParam ("scaleLock", "Scale Lock", false);
    boolParam ("chordTrackSync", "Chord Track Sync", false);

    // Performance
    // Strum: total time from first to last note. Up to 1 s (harp-style rolls); skewed so the
    // fine guitar range (5-60 ms) still has plenty of knob travel.
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "strumMs", 1 }, "Strum",
                                                       NormalisableRange<float> (0.0f, 1000.0f, 0.1f, 0.35f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    choiceParam ("strumSync", "Strum Sync", { "Free (ms)", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2" }, 0);
    floatParam ("strumShape", "Strum Shape", -100.0f, 100.0f, 0.0f, 1.0f, "%");
    choiceParam ("strumDir", "Strum Direction", getStrumDirectionNames(), 0);
    choiceParam ("strumShapeType", "Strum Shape Curve", getStrumShapeNames(), 0);
    intParam ("strumStack", "Strum Octave Stack", 0, 3, 0, [] (int v, int) { return v == 0 ? String ("Off") : "+" + String (v) + " oct"; });
    intParam ("strumTarget", "Strum Target Velocity", 0, 127, 0, [] (int v, int) { return v == 0 ? String ("Off") : String (v); });
    intParam ("strumWindow", "Strum Classic Window", 0, 6, 0, [] (int v, int) { return v < 2 ? String ("Off") : String (v) + " notes"; });
    intParam ("strumRepeats", "Strum Repeats", 1, 8, 1);
    boolParam ("strumRoundTrip", "Strum Round Trip", false);
    choiceParam ("strumBurst", "Strum Burst", getStrumBurstNames(), 0);
    floatParam ("strumMute", "Strum Muted Hits", 0.0f, 100.0f, 0.0f, 1.0f, "%");
    boolParam ("strumRelease", "Strum On Release", false);
    floatParam ("strumCurve", "Strum Velocity Curve", -100.0f, 100.0f, 0.0f, 1.0f, "%");
    floatParam ("humanTime", "Humanize Timing", 0.0f, 15.0f, 0.0f, 0.1f, "ms");
    floatParam ("humanVel", "Humanize Velocity", 0.0f, 15.0f, 0.0f, 0.1f, "%");
    intParam ("swing", "Swing", 50, 75, 50, [] (int v, int) { return String (v) + "%"; });
    choiceParam ("groove", "Groove", getGrooveNames(), 0);
    floatParam ("velSpread", "Velocity Spread", 0.0f, 50.0f, 0.0f, 0.5f, "%");
    intParam ("octSpread", "Octave Spread", 0, 2, 0);

    // Generator
    choiceParam ("genMood", "Generator Mood", getMoodNames(), 3);
    choiceParam ("genGenre", "Generator Genre", getGenreNames(), 0);
    choiceParam ("genComplexity", "Generator Complexity", getComplexityNames(), 2);

    return layout;
}

ChordCatProcessor::ChordCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                      ),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    {
        cat::LinkClient::Caps caps;
        caps.key = true;
        caps.sendChords = true;
        caps.grooveParam = "followGroove";
        auto& link = presetManager.enableLink (apvts, caps);
        link.getKey = [this]
        {
            const int root = juce::roundToInt (apvts.getRawParameterValue ("keyRoot")->load());
            const int sc = juce::roundToInt (apvts.getRawParameterValue ("scale")->load());
            return std::pair<int, juce::uint16> { root, cat::keys::rotate (chordcat::getScaleMask (sc, (uint16_t) customScaleMask.load()), root) };
        };
        link.applyKey = [this] (int root, juce::uint16 absMask)
        {
            std::vector<juce::uint16> masks;
            for (int i = 0; i < chordcat::getScaleNames().size(); ++i)
                masks.push_back (i == chordcat::getCustomScaleIndex() ? (juce::uint16) 0 : chordcat::getScaleMask (i, 0));
            const int sc = cat::keys::nearestScale (cat::keys::rotate (absMask, -root), masks);
            if (auto* k = apvts.getParameter ("keyRoot")) k->setValueNotifyingHost (k->convertTo0to1 ((float) root));
            if (auto* s = apvts.getParameter ("scale"); s != nullptr && sc >= 0) s->setValueNotifyingHost (s->convertTo0to1 ((float) sc));
        };
    }
    cat::followDrumGroove (presetManager, apvts, [this] (const cat::CatLink::Groove& g)
    {
        // Same swing as Drum-Cat: off-beat 16th later by swing% of half a 16th = MPC swing 50..75 %
        cat::SurpriseControls::setParam (*this, "swing", (float) juce::roundToInt (50.0f + g.swing * 0.25f));
        cat::SurpriseControls::setParam (*this, "humanTime", juce::jlimit (0.0f, 15.0f, g.human));
    });

    scheduler.monitor = &noteMonitor;
    heldTriggers.reserve (64);
    syncHeld.reserve (64);
    captureEvents.reserve (16384);
    current.notes.reserve (32);
}

ChordCatProcessor::~ChordCatProcessor() = default;

bool ChordCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ChordCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSync.prepare (sampleRate);
    scheduler.prepare (8192);
    outBuffer.ensureSize ((size_t) juce::jmax (2048, samplesPerBlock * 4));
    sampleCounter = 0;
    heldTriggers.clear();
    current = {};
    current.notes.reserve (32);
    syncHeld.clear();
    syncPendingTime = -1;
    playingSlot = -1;
}

void ChordCatProcessor::releaseResources() {}

//==============================================================================
ChordCatProcessor::Params ChordCatProcessor::readParams() const
{
    auto i = [this] (const char* id) { return juce::roundToInt (apvts.getRawParameterValue (id)->load()); };
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };

    Params p;
    p.transpose = i ("transpose");      p.inChannel = i ("inChannel");      p.outChannel = i ("outChannel");
    p.triggerLow = i ("triggerLow");    p.triggerHigh = i ("triggerHigh");  p.triggerMode = i ("triggerMode");
    p.numPads = i ("numPads");          p.bassChannel = i ("bassChannel");  p.strumDir = i ("strumDir");
    p.swing = i ("swing");              p.groove = i ("groove");            p.octSpread = i ("octSpread");
    p.keyRoot = i ("keyRoot");          p.scale = i ("scale");              p.fixedVelocity = i ("velocity");
    p.voiceLeading = f ("voiceLeading") / 100.0f;
    p.strumMs = f ("strumMs");          p.strumCurve = f ("strumCurve") / 100.0f;
    p.strumShape = f ("strumShape") / 100.0f;
    if (const int sync = i ("strumSync"); sync > 0)
    {
        // Tempo-synced strum: the whole roll lasts one note value at the current tempo
        static const double beats[] = { 0.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0, 2.0 };
        p.strumMs = (float) (beats[juce::jlimit (0, 6, sync)] * 60000.0 / juce::jmax (20.0, lastBpm.load()));
    }
    p.strumShapeType = i ("strumShapeType"); p.strumStack = i ("strumStack"); p.strumTarget = i ("strumTarget");
    p.strumWindow = i ("strumWindow");   p.strumRepeats = i ("strumRepeats"); p.strumBurst = i ("strumBurst");
    p.strumRoundTrip = b ("strumRoundTrip"); p.strumRelease = b ("strumRelease"); p.strumMute = f ("strumMute") / 100.0f;
    p.humanTime = f ("humanTime");      p.humanVel = f ("humanVel") / 100.0f;
    p.velSpread = f ("velSpread") / 100.0f;
    p.legato = b ("legato");            p.bassSplit = b ("bassSplit");
    p.scaleLock = b ("scaleLock");      p.chordTrackSync = b ("chordTrackSync");
    return p;
}

GlobalState ChordCatProcessor::makeGlobal (const Params& p) const
{
    GlobalState g;
    g.keyRoot = p.keyRoot;
    g.scaleIndex = p.scale;
    g.scale = getScales()[(size_t) juce::jlimit (0, getNumScales() - 1, p.scale)].name;
    g.scaleLock = p.scaleLock;
    g.swing = p.swing;
    g.groove = getGrooveNames()[p.groove];
    g.chordTrackSync = p.chordTrackSync;
    g.transpose = p.transpose;
    g.customScaleMask = (uint16_t) customScaleMask.load();
    return g;
}

GlobalState ChordCatProcessor::getGlobalState() const
{
    return makeGlobal (readParams());
}

//==============================================================================
void ChordCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const auto p = readParams();

    hostSync.beginBlock (presetManager.linkClient->playHead (getPlayHead(), numSamples, getSampleRate()), numSamples);   // Cat Link: shared tempo in the apps
    lastBpm = hostSync.get().bpm;
    midiLearn.processMidi (midi);

    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine.voiceLeadingStrength = p.voiceLeading;
    }

    // Chord-track sync toggled: release whatever was playing
    if (p.chordTrackSync != wasSyncing)
    {
        releaseChord (0);
        heldTriggers.clear();
        syncHeld.clear();
        syncPendingTime = -1;
        syncSlot = -1;
        wasSyncing = p.chordTrackSync;
    }

    // Pad auditions from the editor
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        auto handle = [&] (int start, int size)
        {
            for (int k = 0; k < size; ++k)
            {
                const auto [slot, on] = uiEvents[(size_t) (start + k)];
                if (on)
                {
                    auditionActive = true;
                    startChord (slot, p.fixedVelocity > 0 ? (float) p.fixedVelocity : 100.0f, 0, p.legato && current.active, p);
                }
                else if (auditionActive)
                {
                    auditionActive = false;
                    if (heldTriggers.empty()) releaseChord (0);
                }
            }
        };
        handle (scope.startIndex1, scope.blockSize1);
        handle (scope.startIndex2, scope.blockSize2);
    }

    // Sample-accurate: render the rhythm up to each incoming event, then apply the event
    int lastPos = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
        renderPattern (lastPos, pos, p);
        handleMessage (meta.getMessage(), pos, p);
        lastPos = pos;
    }
    renderPattern (lastPos, numSamples, p);

    if (p.chordTrackSync)
        evaluateChordTrackSync (numSamples, p);

    if (! current.active && ! capturing.load() && hostSync.isFreeRunning())
        hostSync.stopFreeRun();

    outBuffer.clear();
    scheduler.emit (outBuffer, sampleCounter, numSamples);
    midi.swapWith (outBuffer);
    presetManager.linkClient->processOutput (midi, numSamples, hostSync.getSampleRate());    // Cat Link: share the chord

    uiPpq = hostSync.ppqAt (0);
    uiRunning = hostSync.isRunning() && current.active;
    uiTransportRunning = hostSync.isRunning();
    uiPlayingRhythm = current.active ? current.rhythm : 0;

    hostSync.endBlock();
    sampleCounter += numSamples;
    uiNowSeconds = (double) sampleCounter / hostSync.getSampleRate();
}

void ChordCatProcessor::handleMessage (const juce::MidiMessage& m, int pos, const Params& p)
{
    if (m.isNoteOn() || m.isNoteOff())
    {
        const bool channelOk = p.inChannel == 0 || m.getChannel() == p.inChannel;
        if (! channelOk) { passThrough (m, pos); return; }

        const int note = m.getNoteNumber();

        if (p.chordTrackSync)
        {
            if (m.isNoteOn())
            {
                syncHeld.push_back (note);
                syncVelocity = m.getVelocity();
                syncPendingTime = sampleCounter + pos + (int64_t) (0.025 * hostSync.getSampleRate());
            }
            else
            {
                syncHeld.erase (std::remove (syncHeld.begin(), syncHeld.end(), note), syncHeld.end());
                if (syncHeld.size() < 2)
                {
                    releaseChord (pos);
                    syncPendingTime = -1;
                }
                else
                {
                    syncPendingTime = sampleCounter + pos + (int64_t) (0.025 * hostSync.getSampleRate());
                }
            }
            return;
        }

        if (m.isNoteOn())
        {
            const int slot = mapTrigger (note, p);
            if (slot == -1) { passThrough (m, pos); return; }
            if (slot >= 0) triggerOn (note, slot, p.fixedVelocity > 0 ? p.fixedVelocity : m.getVelocity(), pos, p);
        }
        else
        {
            const bool wasTrigger = std::any_of (heldTriggers.begin(), heldTriggers.end(), [note] (const Trigger& t) { return t.note == note; });
            if (wasTrigger) triggerOff (note, pos, p);
            else if (mapTrigger (note, p) == -1) passThrough (m, pos);
        }
        return;
    }

    if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldTriggers.clear();
        syncHeld.clear();
        releaseChord (pos);
    }

    // CC, pitch-bend, aftertouch, program changes (expression-map key-switch data) pass straight through
    passThrough (m, pos);
}

int ChordCatProcessor::mapTrigger (int note, const Params& p) const
{
    if (note < p.triggerLow || note > p.triggerHigh)
        return -1;

    int index = note - p.triggerLow;
    if (p.triggerMode == 1)
    {
        if (isBlackKey (note))
            return -1;
        index = 0;
        for (int n = p.triggerLow; n < note; ++n)
            if (! isBlackKey (n)) ++index;
    }

    const int slot = index % juce::jlimit (1, 16, p.numPads);
    bool active;
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        active = engine.progression[(size_t) slot].isActive;
    }
    return active ? slot : -2; // -2: inside the range but the pad is empty -> swallow
}

void ChordCatProcessor::triggerOn (int note, int slot, int velocity, int pos, const Params& p)
{
    heldTriggers.erase (std::remove_if (heldTriggers.begin(), heldTriggers.end(), [note] (const Trigger& t) { return t.note == note; }),
                        heldTriggers.end());
    heldTriggers.push_back ({ note, slot, velocity });
    startChord (slot, (float) velocity, pos, p.legato && current.active, p);
}

void ChordCatProcessor::triggerOff (int note, int pos, const Params& p)
{
    const bool wasCurrent = ! heldTriggers.empty() && heldTriggers.back().note == note;
    heldTriggers.erase (std::remove_if (heldTriggers.begin(), heldTriggers.end(), [note] (const Trigger& t) { return t.note == note; }),
                        heldTriggers.end());

    if (! wasCurrent)
        return;

    // The chord belongs to the key that started it: releasing that key ends the chord.
    // Keys that are still held from earlier chords are NOT re-triggered (that used to fire
    // extra chords - with those keys' old velocities - every time a key was released).
    // They are dropped so that releasing them later does nothing either.
    heldTriggers.clear();
    if (! auditionActive)
        releaseChord (pos, true, &p);
}

void ChordCatProcessor::startChord (int slot, float velocity, int pos, bool legato, const Params& p)
{
    const auto g = makeGlobal (p);
    ChordDefinition def;
    std::vector<int> voiced;
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        def = engine.progression[(size_t) juce::jlimit (0, 15, slot)].chord;
        voiced = engine.voiceSlot (slot, g);
    }
    if (voiced.empty())
        return;

    // Output note list
    std::vector<std::pair<int, int>> notes;
    notes.reserve (voiced.size() + 4);
    for (int n : voiced)
        notes.push_back ({ p.outChannel, n });

    for (int o = 1; o <= p.octSpread; ++o)
        if (voiced.back() + 12 * o <= 127)
            notes.push_back ({ p.outChannel, voiced.back() + 12 * o });

    // Strum octave stack: the whole chord again, one or more octaves higher
    const int baseTop = notes.back().second;
    for (int o = 1; o <= p.strumStack; ++o)
        for (int n : voiced)
            if (n + 12 * o <= 127 && std::find (notes.begin(), notes.end(), std::make_pair (p.outChannel, n + 12 * o)) == notes.end())
                notes.push_back ({ p.outChannel, n + 12 * o });

    if (p.bassSplit)
    {
        const auto resolved = ChordEngine::applyScaleLock (def, g);
        const int rootPc = ((resolved.rootNote + p.transpose) % 12 + 12) % 12;
        int bass = voiced.front() - 12;
        while (bass > 0 && ((bass % 12) + 12) % 12 != rootPc) --bass;
        if (bass >= 0)
            notes.push_back ({ p.bassChannel, bass });
    }

    const float strumMs = def.strumTimeMs >= 0.0f ? def.strumTimeMs : p.strumMs;
    const int rhythm = def.rhythmPattern;
    const int64_t now = sampleCounter + pos;

    if (legato && current.active && rhythm == 0 && current.rhythm == 0)
    {
        // Keep common tones ringing, move only the voices that change
        std::vector<std::pair<int, int>> added;
        for (const auto& old : current.notes)
            if (std::find (notes.begin(), notes.end(), old) == notes.end())
                scheduler.releaseNote (old.first, old.second, now);
        for (const auto& nn : notes)
            if (std::find (current.notes.begin(), current.notes.end(), nn) == current.notes.end())
                added.push_back (nn);

        if (capturing.load()) { recordCapture (current.notes, 0, false, pos); recordCapture (notes, (int) velocity, true, pos); }
        playNotes (added, velocity, now, -1, strumMs, p);
    }
    else
    {
        // Keys pressed (almost) together: the newest one wins outright. Cancelling the previous
        // chord's not-yet-sent notes avoids a few-millisecond blip of the chord being replaced.
        if (current.active && now - current.startTime < (int64_t) (simultaneousKeysSeconds * hostSync.getSampleRate()))
            for (const auto& [ch, note] : current.notes)
                scheduler.cancelPendingNoteOns (ch, note);

        releaseChord (pos);
        if (capturing.load()) recordCapture (notes, (int) velocity, true, pos);

        if (rhythm == 0)
        {
            playNotes (notes, velocity, now, -1, strumMs, p);
            if (p.strumBurst > 0 && ! hostSync.isRunning())
                hostSync.startFreeRunAt (pos);   // burst re-strums run from the key press
        }
        else if (! hostSync.isRunning())
        {
            hostSync.startFreeRunAt (pos); // rhythm starts on the key press
        }
        else
        {
            // Host is rolling: sound immediately until the next grid hit (pickup)
            const auto groove = PatternPlayer::makeGroove ((Groove) p.groove, p.swing);
            const double ppqNow = hostSync.ppqAt (pos);
            const double next = PatternPlayer::nextHitAfter (rhythm, ppqNow, groove);
            if (next < 0.0 || next - ppqNow > 0.125)
            {
                const auto offTime = next < 0.0 ? now + (int64_t) hostSync.samplesPerPpq()
                                                : sampleCounter + (int64_t) hostSync.sampleForPpq (next) - 1;
                playNotes (notes, velocity, now, juce::jmax (now + 1, offTime), strumMs, p);
            }
        }
    }

    current.startTime = now;
    current.baseTop = baseTop;
    current.rootPc = ((ChordEngine::applyScaleLock (def, g).rootNote + p.transpose) % 12 + 12) % 12;
    for (auto& b : uiChordNotes) b = false;
    for (const auto& [ch, nn] : notes) uiChordNotes[(size_t) juce::jlimit (0, 127, nn)] = true;
    uiBaseTop = baseTop;
    current.active = true;
    current.slot = slot;
    current.rhythm = rhythm;
    current.velocity = velocity;
    current.strumMs = strumMs;
    current.notes = notes;
    playingSlot = slot;
}

void ChordCatProcessor::releaseChord (int pos, bool strummedRelease, const Params* p)
{
    if (! current.active)
        return;

    const int64_t now = sampleCounter + pos;
    if (strummedRelease && p != nullptr && p->strumRelease && current.strumMs > 0.0f && current.notes.size() > 1)
    {
        // Strum on release: the notes let go one after the other, top to bottom
        auto sorted = current.notes;
        std::sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.second > b.second; });
        const double gap = current.strumMs * 0.001 * hostSync.getSampleRate() / (double) (sorted.size() - 1);
        for (size_t k = 0; k < sorted.size(); ++k)
            scheduler.releaseNote (sorted[k].first, sorted[k].second, now + (int64_t) (gap * (double) k));
    }
    else
        for (const auto& [ch, note] : current.notes)
            scheduler.releaseNote (ch, note, now);

    if (capturing.load())
        recordCapture (current.notes, 0, false, pos);

    current.active = false;
    current.notes.clear();
    playingSlot = -1;
}

void ChordCatProcessor::playNotes (const std::vector<std::pair<int, int>>& notes, float velocity, int64_t startTime,
                                   int64_t offTime, float strumMs, const Params& p, int counter, bool muted)
{
    if (notes.empty())
        return;

    const double sr = hostSync.getSampleRate();
    if (counter < 0)
        counter = strumCounter++;

    StrumSettings st;
    st.spreadMs = strumMs;
    // "Up / Down" also covers the old "Both" setting (index 2)
    st.direction = p.strumDir;
    st.shape = p.strumShapeType;
    st.curve = p.strumShape;
    st.tilt = p.strumCurve;
    st.target = p.strumTarget;
    st.window = p.strumWindow;
    st.repeats = strumMs > 0.0f ? p.strumRepeats : 1;
    st.roundTrip = p.strumRoundTrip;

    std::vector<int> pitches;
    pitches.reserve (notes.size());
    for (const auto& n : notes) pitches.push_back (n.second);
    auto events = buildStrum (pitches, st, velocity, counter, current.rootPc, rng);

    // Short hits (rhythm patterns, bursts): squeeze the whole strum into 70 % of the hit
    if (offTime >= 0 && ! events.empty())
    {
        const double total = events.back().timeMs;
        const double room = 0.7 * (double) (offTime - startTime) / sr * 1000.0;
        if (total > room && total > 0.0)
            for (auto& e : events) e.timeMs *= room / total;
    }

    std::array<bool, 128> struck {};
    for (const auto& e : events)
    {
        const auto& [ch, note] = notes[(size_t) e.index];
        double delayMs = e.timeMs;
        if (p.humanTime > 0.0f)
            delayMs += rng.nextFloat() * p.humanTime;   // zero-latency: humanise can only push later

        float v = e.velocity;
        if (p.velSpread > 0.0f) v *= 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * p.velSpread;
        if (p.humanVel > 0.0f)  v *= 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * p.humanVel;
        if (muted) v *= 0.55f;
        const int vel = juce::jlimit (1, 127, juce::roundToInt (v));

        const int64_t t = startTime + (int64_t) (delayMs * 0.001 * sr);
        const int64_t mutedOff = t + (int64_t) (0.035 * sr);
        const bool again = struck[(size_t) note];       // repeats / round trips strike a note again
        struck[(size_t) note] = true;

        if (offTime < 0 && ! muted)
        {
            if (again) scheduler.releaseNote (ch, note, t);
            scheduler.schedule (t, juce::MidiMessage::noteOn (ch, note, (juce::uint8) vel));
        }
        else
        {
            const int64_t end = muted ? juce::jmin (mutedOff, offTime < 0 ? mutedOff : offTime) : offTime;
            if (t >= end) continue;
            if (again) scheduler.releaseNote (ch, note, t);
            scheduler.scheduleNote (t, end, ch, note, vel);
        }

        // Live view
        const int w = uiStrumWrite.load (std::memory_order_relaxed);
        uiStrumEvents[(size_t) (w & 255)] = { (double) t / sr, note, (float) vel, note > current.baseTop };
        uiStrumWrite.store (w + 1, std::memory_order_release);
    }
}

void ChordCatProcessor::renderPattern (int fromSample, int toSample, const Params& p)
{
    if (! current.active || ! hostSync.isRunning() || toSample <= fromSample)
        return;

    // Burst: re-strum the held chord on every grid line (alternating strokes, optional muted chops)
    if (current.rhythm == 0)
    {
        const double grid = strumBurstToPpq (p.strumBurst);
        if (grid <= 0.0) return;
        const double ppqFrom = hostSync.ppqAt (fromSample), ppqTo = hostSync.ppqAt (toSample);
        const double spp = hostSync.samplesPerPpq();
        const double startPpq = hostSync.ppqAt ((int) juce::jlimit<int64_t> (0, toSample, current.startTime - sampleCounter));
        for (double t = std::ceil (ppqFrom / grid - 1.0e-9) * grid; t < ppqTo; t += grid)
        {
            const int64_t on = sampleCounter + (int64_t) std::llround (hostSync.sampleForPpq (t));
            if (on <= current.startTime + (int64_t) (0.02 * hostSync.getSampleRate()) || t < startPpq)
                continue;   // the key press itself already strummed
            const int64_t off = on + juce::jmax ((int64_t) 1, (int64_t) (grid * spp * 0.92));
            for (const auto& [ch, note] : current.notes)
                scheduler.releaseNote (ch, note, on);
            const auto index = (int) std::llround (t / grid);
            const bool muted = p.strumMute > 0.0f && rng.nextFloat() < p.strumMute;
            const float accent = index % 2 == 0 ? 1.0f : 0.85f;
            playNotes (current.notes, current.velocity * accent, on, off, juce::jmin (current.strumMs, (float) (grid * spp / hostSync.getSampleRate() * 1000.0 * 0.6)),
                       p, index, muted);
        }
        return;
    }

    const double ppqFrom = hostSync.ppqAt (fromSample);
    const double ppqTo = hostSync.ppqAt (toSample);
    const double spp = hostSync.samplesPerPpq();
    const auto groove = PatternPlayer::makeGroove ((Groove) p.groove, p.swing);

    PatternPlayer::forEachHit (current.rhythm, ppqFrom, ppqTo, groove, [&] (double t, double len, float velMul)
    {
        const int64_t on = sampleCounter + (int64_t) std::llround (hostSync.sampleForPpq (t));
        const int64_t off = on + juce::jmax ((int64_t) 1, (int64_t) (len * spp));
        playNotes (current.notes, current.velocity * velMul, on, off, current.strumMs, p);
    });
}

void ChordCatProcessor::passThrough (const juce::MidiMessage& m, int pos)
{
    scheduler.schedule (sampleCounter + pos, m);
}

void ChordCatProcessor::evaluateChordTrackSync (int numSamples, const Params& p)
{
    if (syncPendingTime < 0 || syncPendingTime >= sampleCounter + numSamples)
        return;

    const int pos = (int) juce::jlimit<int64_t> (0, numSamples - 1, syncPendingTime - sampleCounter);
    syncPendingTime = -1;

    if (syncHeld.size() < 2)
        return;

    const auto rec = recognizeChord (syncHeld);
    if (! rec.valid)
        return;

    int slot;
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        const int pads = juce::jlimit (1, 16, p.numPads);
        const bool same = syncSlot >= 0 && engine.progression[(size_t) syncSlot].chord.rootNote == rec.root
                          && engine.progression[(size_t) syncSlot].chord.chordType == rec.type;
        if (! same)
        {
            syncSlot = (syncSlot + 1) % pads;
            auto& s = engine.progression[(size_t) syncSlot];
            if (! s.isActive) s.chord = {};
            s.isActive = true;
            s.chord.rootNote = rec.root;
            s.chord.chordType = rec.type;
            s.chord.tension = {};
            s.chord.isBorrowed = ! isInScale (rec.root, p.keyRoot, p.scale, (uint16_t) customScaleMask.load());
        }
        slot = syncSlot;
    }
    bumpVersion();

    startChord (slot, p.fixedVelocity > 0 ? (float) p.fixedVelocity : (float) syncVelocity, pos, p.legato && current.active, p);
}

void ChordCatProcessor::recordCapture (const std::vector<std::pair<int, int>>& notes, int velocity, bool on, int pos)
{
    if (! hostSync.isRunning())
        hostSync.startFreeRunAt (pos);

    const double ppq = hostSync.ppqAt (pos);
    const juce::SpinLock::ScopedLockType lock (captureLock);
    for (const auto& [ch, note] : notes)
        if (captureEvents.size() < captureEvents.capacity())
            captureEvents.push_back ({ ppq, ch, note, velocity, on });
}

//==============================================================================
// Editor API
Progression ChordCatProcessor::getProgression() const
{
    const juce::SpinLock::ScopedLockType lock (engineLock);
    return engine.progression;
}

void ChordCatProcessor::pushUndo()
{
    undoStack.push_back (getProgression());
    if (undoStack.size() > 64) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void ChordCatProcessor::setProgression (const Progression& prog, bool undoable)
{
    if (undoable) pushUndo();
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine.progression = prog;
        engine.resetVoiceLeading();
    }
    bumpVersion();
}

void ChordCatProcessor::editSlot (int slot, const std::function<void (ProgressionSlot&)>& fn, bool undoable)
{
    if (! juce::isPositiveAndBelow (slot, 16)) return;
    if (undoable) pushUndo();
    auto prog = getProgression();
    fn (prog[(size_t) slot]);
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine.progression[(size_t) slot] = prog[(size_t) slot];
    }
    bumpVersion();
}

std::array<std::vector<int>, 16> ChordCatProcessor::getPreviewVoicings() const
{
    ChordEngine copy;
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        copy = engine;
    }
    copy.resetVoiceLeading();
    const auto g = getGlobalState();
    const int pads = getNumPads();

    std::array<std::vector<int>, 16> result;
    for (int i = 0; i < 16; ++i)
        if (copy.progression[(size_t) i].isActive)
            result[(size_t) i] = i < pads ? copy.voiceSlot (i, g) : copy.previewSlot (i, g, {});
    return result;
}

juce::String ChordCatProcessor::getRomanNumeral (int slot) const
{
    const auto prog = getProgression();
    const auto g = getGlobalState();
    const auto c = ChordEngine::applyScaleLock (prog[(size_t) juce::jlimit (0, 15, slot)].chord, g);
    return romanNumeral (c.rootNote, c.chordType, g.keyRoot, g.scaleIndex, g.customScaleMask);
}

void ChordCatProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (getProgression());
    const auto p = undoStack.back();
    undoStack.pop_back();
    setProgression (p, false);
}

void ChordCatProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (getProgression());
    const auto p = redoStack.back();
    redoStack.pop_back();
    setProgression (p, false);
}

void ChordCatProcessor::selectAB (int index)
{
    index = juce::jlimit (0, 1, index);
    if (index == currentAB) return;
    abSlots[(size_t) currentAB] = getProgression();
    abValid[(size_t) currentAB] = true;
    currentAB = index;
    if (abValid[(size_t) index])
        setProgression (abSlots[(size_t) index], true);
}

void ChordCatProcessor::generate (const GeneratorSettings& settings)
{
    const auto result = generator.generate (settings);
    setProgression (result.progression, true);

    if (auto* scale = apvts.getParameter ("scale"))
        scale->setValueNotifyingHost (scale->convertTo0to1 ((float) result.scaleIndex));
    if (auto* pads = apvts.getParameter ("numPads"))
        if (getNumPads() < settings.numChords)
            pads->setValueNotifyingHost (pads->convertTo0to1 ((float) settings.numChords));

    history.insert (history.begin(), result.progression);
    if (history.size() > 9) history.resize (9);
}

void ChordCatProcessor::recallHistory (int index)
{
    if (juce::isPositiveAndBelow (index, (int) history.size()))
        setProgression (history[(size_t) index], true);
}

void ChordCatProcessor::diceSlot (int slot, int what)
{
    auto& random = juce::Random::getSystemRandom();
    const auto g = getGlobalState();
    editSlot (slot, [&] (ProgressionSlot& s)
    {
        if (what == 0)      s.chord = ProgressionGenerator::diceRoot (s.chord, random, g);
        else if (what == 1) s.chord = ProgressionGenerator::diceType (s.chord, random);
        else                s.chord = ProgressionGenerator::diceVoicing (s.chord, random);
        s.isActive = true;
    });
}

void ChordCatProcessor::auditionSlot (int slot, bool on)
{
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0) uiEvents[(size_t) scope.startIndex1] = { slot, on };
    else if (scope.blockSize2 > 0) uiEvents[(size_t) scope.startIndex2] = { slot, on };
}

int ChordCatProcessor::getNumPads() const
{
    return juce::roundToInt (apvts.getRawParameterValue ("numPads")->load());
}

//==============================================================================
// Capture
void ChordCatProcessor::setCapturing (bool shouldCapture)
{
    if (shouldCapture)
    {
        const juce::SpinLock::ScopedLockType lock (captureLock);
        captureEvents.clear();
    }
    capturing = shouldCapture;
}

bool ChordCatProcessor::hasCapture() const
{
    const juce::SpinLock::ScopedLockType lock (captureLock);
    return ! captureEvents.empty();
}

juce::MidiMessageSequence ChordCatProcessor::getCaptureSequence() const
{
    std::vector<CaptureEvent> events;
    {
        const juce::SpinLock::ScopedLockType lock (captureLock);
        events = captureEvents;
    }

    juce::MidiMessageSequence seq;
    if (events.empty()) return seq;

    auto quantize = [] (double ppq) { return std::round (ppq * 4.0) / 4.0; }; // 1/16 grid
    double firstOn = 1.0e12;
    for (const auto& e : events) if (e.on) firstOn = std::min (firstOn, quantize (e.ppq));
    if (firstOn > 1.0e11) return seq;
    const double offset = std::floor (firstOn / 4.0) * 4.0; // start on a bar line

    std::map<std::pair<int, int>, double> openNotes;
    for (const auto& e : events)
    {
        const double t = quantize (e.ppq) - offset;
        const auto key = std::make_pair (e.channel, e.note);
        if (e.on)
        {
            if (openNotes.count (key))
            {
                seq.addEvent (juce::MidiMessage::noteOff (e.channel, e.note).withTimeStamp (juce::jmax (openNotes[key] + 0.25, t)));
                openNotes.erase (key);
            }
            seq.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) juce::jlimit (1, 127, e.velocity)).withTimeStamp (t));
            openNotes[key] = t;
        }
        else if (openNotes.count (key))
        {
            seq.addEvent (juce::MidiMessage::noteOff (e.channel, e.note).withTimeStamp (juce::jmax (openNotes[key] + 0.25, t)));
            openNotes.erase (key);
        }
    }
    for (const auto& [key, start] : openNotes)
        seq.addEvent (juce::MidiMessage::noteOff (key.first, key.second).withTimeStamp (start + 1.0));

    seq.sort();
    seq.updateMatchedPairs();
    return seq;
}

//==============================================================================
// Export / import
juce::MidiMessageSequence ChordCatProcessor::buildSlotSequence (int slot) const
{
    juce::MidiMessageSequence seq;
    const auto voicings = getPreviewVoicings();
    const auto prog = getProgression();
    const auto p = readParams();
    const auto& notes = voicings[(size_t) juce::jlimit (0, 15, slot)];

    auto marker = juce::MidiMessage::textMetaEvent (6, prog[(size_t) slot].chord.getName());
    seq.addEvent (marker.withTimeStamp (0.0));
    for (int n : notes)
    {
        seq.addEvent (juce::MidiMessage::noteOn (p.outChannel, n, (juce::uint8) 100).withTimeStamp (0.0));
        seq.addEvent (juce::MidiMessage::noteOff (p.outChannel, n).withTimeStamp (3.99));
    }
    seq.updateMatchedPairs();
    return seq;
}

juce::MidiMessageSequence ChordCatProcessor::buildProgressionSequence() const
{
    juce::MidiMessageSequence seq;
    const auto voicings = getPreviewVoicings();
    const auto prog = getProgression();
    const auto p = readParams();
    const auto groove = PatternPlayer::makeGroove ((Groove) p.groove, p.swing);
    const auto g = makeGlobal (p);

    double bar = 0.0;
    for (int i = 0; i < getNumPads(); ++i)
    {
        if (! prog[(size_t) i].isActive || voicings[(size_t) i].empty())
            continue;

        const auto& def = prog[(size_t) i].chord;
        const auto resolved = ChordEngine::applyScaleLock (def, g);
        auto name = resolved.getName (resolved.rootNote == 3 || resolved.rootNote == 8 || resolved.rootNote == 10);
        seq.addEvent (juce::MidiMessage::textMetaEvent (6, name).withTimeStamp (bar));

        std::vector<int> notes = voicings[(size_t) i];
        if (p.bassSplit)
        {
            int bass = notes.front() - 12;
            const int rootPc = ((resolved.rootNote + p.transpose) % 12 + 12) % 12;
            while (bass > 0 && bass % 12 != rootPc) --bass;
            seq.addEvent (juce::MidiMessage::noteOn (p.bassChannel, bass, (juce::uint8) 100).withTimeStamp (bar));
            seq.addEvent (juce::MidiMessage::noteOff (p.bassChannel, bass).withTimeStamp (bar + 3.99));
        }

        auto addChord = [&] (double start, double len, float vel)
        {
            for (int n : notes)
            {
                seq.addEvent (juce::MidiMessage::noteOn (p.outChannel, n, (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (vel))).withTimeStamp (start));
                seq.addEvent (juce::MidiMessage::noteOff (p.outChannel, n).withTimeStamp (start + len));
            }
        };

        if (def.rhythmPattern == 0)
            addChord (bar, 3.99, 100.0f);
        else
            PatternPlayer::forEachHit (def.rhythmPattern, bar, bar + 4.0, groove, [&] (double t, double len, float vm)
            {
                addChord (t, juce::jmin (len, bar + 4.0 - t), 100.0f * vm);
            });

        bar += 4.0;
    }

    seq.sort();
    seq.updateMatchedPairs();
    return seq;
}

bool ChordCatProcessor::importMidiFile (const juce::File& file)
{
    const auto seq = cat::midiexport::readMidiFileAsPpq (file);
    if (seq.getNumEvents() == 0)
        return false;

    // Group note-ons that start together (within a 1/16) into chords
    std::vector<std::pair<double, std::vector<int>>> groups;
    for (const auto* h : seq)
    {
        const auto& m = h->message;
        if (! m.isNoteOn()) continue;
        const double t = m.getTimeStamp();
        if (groups.empty() || t - groups.back().first > 0.24)
            groups.push_back ({ t, {} });
        groups.back().second.push_back (m.getNoteNumber());
    }

    Progression prog;
    int count = 0;
    juce::String last;
    for (const auto& [t, notes] : groups)
    {
        if (notes.size() < 2 || count >= 16) continue;
        const auto rec = recognizeChord (notes);
        if (! rec.valid) continue;
        const auto key = juce::String (rec.root) + rec.type;
        if (key == last) continue;
        last = key;

        auto& s = prog[(size_t) count++];
        s.isActive = true;
        s.chord.rootNote = rec.root;
        s.chord.chordType = rec.type;
        int lowest = 127;
        for (int n : notes) lowest = std::min (lowest, n);
        s.chord.octave = juce::jlimit (2, 6, lowest / 12 - 2);
    }

    if (count == 0)
        return false;

    setProgression (prog, true);
    if (auto* pads = apvts.getParameter ("numPads"))
        pads->setValueNotifyingHost (pads->convertTo0to1 ((float) juce::jlimit (4, 16, count)));
    return true;
}

//==============================================================================
// Presets
void ChordCatProcessor::loadFactoryPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    factoryIndex = index;
    const auto& fp = presets[(size_t) index];

    Progression prog;
    for (size_t i = 0; i < fp.chords.size() && i < 16; ++i)
    {
        prog[i].isActive = true;
        prog[i].chord.rootNote = fp.chords[i].first;
        prog[i].chord.chordType = fp.chords[i].second;
        prog[i].chord.voicingModel = fp.voicing;
    }
    setProgression (prog, true);

    auto setParam = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    setParam ("keyRoot", (float) fp.keyRoot);
    setParam ("scale", (float) juce::jmax (0, findScale (fp.scale)));
    setParam ("numPads", (float) juce::jlimit (4, 16, (int) fp.chords.size()));
}

void ChordCatProcessor::surprise()
{
    // A fresh progression in a random key / style, a matching voicing and a playing feel.
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };

    GeneratorSettings s;
    s.mood = r.nextInt (getMoodNames().size());
    s.genre = r.nextInt (getGenreNames().size());
    s.complexity = 1 + r.nextInt (4);                       // 7ths .. 13ths
    s.keyRoot = r.nextInt (12);
    s.scaleIndex = juce::roundToInt (apvts.getRawParameterValue ("scale")->load());
    s.customMask = (uint16_t) customScaleMask.load();
    s.moodSetsScale = true;
    s.numChords = r.nextBool() ? 4 : 8;

    set ("genMood", (float) s.mood);
    set ("genGenre", (float) s.genre);
    set ("genComplexity", (float) s.complexity);
    set ("keyRoot", (float) s.keyRoot);
    generate (s);

    static const int voicings[] = { VoicingClose, VoicingOpen, VoicingSpread, VoicingDrop2, VoicingWideNeoSoul, VoicingRootless };
    const int voicing = voicings[r.nextInt (6)];
    auto prog = getProgression();
    for (auto& slot : prog) slot.chord.voicingModel = voicing;
    setProgression (prog, false);

    // Feel: half the time a clean block chord, otherwise a strum
    const bool strummed = r.nextBool();
    set ("strumMs", strummed ? 15.0f + r.nextFloat() * 140.0f : 0.0f);
    set ("strumSync", 0.0f);
    set ("strumShape", strummed ? r.nextFloat() * 120.0f - 60.0f : 0.0f);
    set ("strumCurve", strummed ? r.nextFloat() * 80.0f - 20.0f : 0.0f);
    set ("strumDir", (float) r.nextInt (3));
    set ("humanTime", r.nextFloat() * 6.0f);
    set ("humanVel", r.nextFloat() * 8.0f);
    set ("swing", (float) (50 + r.nextInt (12)));
}

void ChordCatProcessor::applyStrumPreset (int index)
{
    const auto& list = getStrumPresets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    currentStrumPreset = index;
    // Every strum control back to its default, then the preset's own values
    for (auto* id : { "strumMs", "strumSync", "strumShape", "strumDir", "strumCurve", "strumShapeType", "strumStack", "strumTarget",
                      "strumWindow", "strumRepeats", "strumRoundTrip", "strumBurst", "strumMute", "strumRelease", "humanTime", "humanVel" })
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getDefaultValue());
            prm->endChangeGesture();
        }
    for (const auto& [id, v] : list[(size_t) index].values)
        cat::SurpriseControls::setParam (*this, id, v);
}

void ChordCatProcessor::randomStrum()
{
    // Musically bounded: mostly guitar-range strums, sometimes long harp rolls or bursts
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    const float kind = r.nextFloat();
    set ("strumSync", 0.0f);
    set ("strumMs", kind < 0.5f ? 10.0f + r.nextFloat() * 60.0f : (kind < 0.85f ? 80.0f + r.nextFloat() * 250.0f : 250.0f + r.nextFloat() * 400.0f));
    set ("strumDir", (float) r.nextInt (getStrumDirectionNames().size()));
    set ("strumShapeType", (float) r.nextInt (getStrumShapeNames().size()));
    set ("strumShape", r.nextFloat() < 0.6f ? 0.0f : r.nextFloat() * 120.0f - 60.0f);
    set ("strumCurve", r.nextFloat() * 60.0f - 20.0f);
    set ("strumStack", (float) (r.nextFloat() < 0.6f ? 0 : 1 + r.nextInt (2)));
    set ("strumTarget", r.nextFloat() < 0.7f ? 0.0f : (float) (20 + r.nextInt (108)));
    set ("strumWindow", r.nextFloat() < 0.8f ? 0.0f : (float) (2 + r.nextInt (3)));
    set ("strumRepeats", (float) (r.nextFloat() < 0.7f ? 1 : 2 + r.nextInt (3)));
    set ("strumRoundTrip", r.nextBool() ? 1.0f : 0.0f);
    set ("strumBurst", r.nextFloat() < 0.75f ? 0.0f : (float) (2 + r.nextInt (5)));
    set ("strumMute", r.nextFloat() < 0.7f ? 0.0f : r.nextFloat() * 50.0f);
    currentStrumPreset = -1;
}

juce::File ChordCatProcessor::getUserPresetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("CatSuite").getChildFile ("Chord-Cat").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

bool ChordCatProcessor::saveUserPreset (const juce::File& file)
{
    juce::MemoryBlock data;
    getStateInformation (data);
    if (auto xml = getXmlFromBinary (data.getData(), (int) data.getSize()))
        return xml->writeTo (file);
    return false;
}

bool ChordCatProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        pushUndo();
        juce::MemoryBlock data;
        copyXmlToBinary (*xml, data);
        setStateInformation (data.getData(), (int) data.getSize());
        return true;
    }
    return false;
}

//==============================================================================
void ChordCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("CHORDCAT");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("customScaleMask", customScaleMask.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        root.appendChild (engine.progressionToValueTree(), nullptr);
    }
    root.appendChild (midiLearn.toValueTree(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void ChordCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("CHORDCAT")) return;

    customScaleMask = (int) root.getProperty ("customScaleMask", 0x0AB5);

    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);

    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine.progressionFromValueTree (root.getChildWithName (ChordEngine::progressionId));
    }
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
    bumpVersion();
}

//==============================================================================
juce::AudioProcessorEditor* ChordCatProcessor::createEditor()
{
    // The editor is laid out at its design size and scaled as a whole (stays sharp at any size)
    return new cat::ScaledEditor (*this, std::make_unique<ChordCatEditor> (*this), "Chord-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordCatProcessor();
}
