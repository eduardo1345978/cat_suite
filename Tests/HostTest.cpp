// Headless VST3 host test for the Cat Suite.
// Loads the built .vst3 bundles (exactly what Cubase loads), feeds MIDI with a
// 120 BPM 4/4 transport and checks timing, routing, MPE output and note balance.

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <set>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr double samplesPerBeat = sampleRate * 60.0 / 120.0; // 24000

int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  [ok]   " : "  [FAIL] ") << what << std::endl;
    if (! condition) ++failures;
}

struct TestPlayHead : public juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (120.0);
        info.setTimeSignature (TimeSignature { 4, 4 });
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        info.setTimeInSamples ((juce::int64) (ppq * samplesPerBeat));
        return info;
    }
};

std::unique_ptr<juce::AudioPluginInstance> loadPlugin (const juce::String& path)
{
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);
    if (types.isEmpty())
    {
        std::cout << "Could not scan " << path << std::endl;
        return nullptr;
    }

    juce::String error;
    auto instance = format.createInstanceFromDescription (*types[0], sampleRate, blockSize, error);
    if (instance == nullptr)
        std::cout << "Could not instantiate " << path << ": " << error << std::endl;
    return instance;
}

struct Event
{
    int64_t time;
    juce::MidiMessage msg;
};

/** Runs one block through a plugin; returns its output with absolute timestamps. */
std::vector<Event> runBlock (juce::AudioPluginInstance& plugin, const std::vector<Event>& input, int64_t blockStart)
{
    juce::AudioBuffer<float> audio (juce::jmax (1, plugin.getTotalNumOutputChannels()), blockSize);
    audio.clear();
    juce::MidiBuffer midi;
    for (const auto& e : input)
        if (e.time >= blockStart && e.time < blockStart + blockSize)
            midi.addEvent (e.msg, (int) (e.time - blockStart));

    plugin.processBlock (audio, midi);

    std::vector<Event> out;
    for (const auto meta : midi)
        out.push_back ({ blockStart + meta.samplePosition, meta.getMessage() });
    return out;
}

/** Checks every note-on has a matching note-off (per channel / note). */
bool balanced (const std::vector<Event>& events, juce::String& report)
{
    std::map<std::pair<int, int>, int> count;
    for (const auto& e : events)
    {
        if (e.msg.isNoteOn())  ++count[{ e.msg.getChannel(), e.msg.getNoteNumber() }];
        if (e.msg.isNoteOff()) --count[{ e.msg.getChannel(), e.msg.getNoteNumber() }];
    }
    bool ok = true;
    for (const auto& [key, c] : count)
        if (c != 0)
        {
            ok = false;
            report << "ch" << key.first << "/" << key.second << "=" << c << " ";
        }
    return ok;
}

int countIf (const std::vector<Event>& events, const std::function<bool (const juce::MidiMessage&)>& pred)
{
    int n = 0;
    for (const auto& e : events) if (pred (e.msg)) ++n;
    return n;
}

void prepare (juce::AudioPluginInstance& p, TestPlayHead& ph)
{
    p.setPlayHead (&ph);
    p.enableAllBuses();
    p.prepareToPlay (sampleRate, blockSize);
}

//==============================================================================
void testChordCatStandalone (const juce::String& path)
{
    std::cout << "\nChord-Cat: one-finger triggering (transport stopped)" << std::endl;
    auto chord = loadPlugin (path);
    if (chord == nullptr) { ++failures; return; }

    std::cout << "  name: " << chord->getName() << ", MIDI out: " << (chord->producesMidi() ? "yes" : "no")
              << ", params: " << chord->getParameters().size() << std::endl;

    TestPlayHead ph;
    prepare (*chord, ph);

    std::vector<Event> in {
        { 100, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },   // C3 -> pad 1 (Cmaj7)
        { 12000, juce::MidiMessage::noteOff (1, 60) },
        { 12100, juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90) },  // outside trigger range -> pass through
        { 13000, juce::MidiMessage::noteOff (1, 40) },
        { 14000, juce::MidiMessage::controllerEvent (1, 1, 64) },        // CC passes through
        { 20000, juce::MidiMessage::noteOn (1, 61, (juce::uint8) 100) }, // C#3 -> pad 2 (Am7), voice-led
        { 30000, juce::MidiMessage::noteOff (1, 61) },
    };

    std::vector<Event> out;
    for (int64_t t = 0; t < 40000; t += blockSize)
        for (auto& e : runBlock (*chord, in, t)) out.push_back (e);

    std::vector<int> firstChord, secondChord;
    for (const auto& e : out)
    {
        if (e.msg.isNoteOn() && e.time == 100) firstChord.push_back (e.msg.getNoteNumber());
        if (e.msg.isNoteOn() && e.time == 20000) secondChord.push_back (e.msg.getNoteNumber());
    }

    juce::String notes1, notes2;
    for (int n : firstChord) notes1 << juce::MidiMessage::getMidiNoteName (n, true, true, 3) << " ";
    for (int n : secondChord) notes2 << juce::MidiMessage::getMidiNoteName (n, true, true, 3) << " ";
    std::cout << "  pad 1 -> " << notes1 << "| pad 2 -> " << notes2 << std::endl;

    std::set<int> pcs1;
    for (int n : firstChord) pcs1.insert (n % 12);
    check (firstChord.size() == 4 && pcs1 == std::set<int> { 0, 4, 7, 11 }, "C3 triggers Cmaj7 (4 voices) on the same sample");

    std::set<int> pcs2;
    for (int n : secondChord) pcs2.insert (n % 12);
    check (pcs2 == std::set<int> { 9, 0, 4, 7 }, "C#3 triggers Am7");

    int moved = 0;
    for (size_t i = 0; i < std::min (firstChord.size(), secondChord.size()); ++i)
        moved += std::abs (firstChord[i] - secondChord[i]);
    check (moved <= 8, "voice leading keeps Cmaj7 -> Am7 movement small (" + juce::String (moved) + " semitones total)");

    check (countIf (out, [] (const auto& m) { return m.isNoteOn() && m.getNoteNumber() == 40; }) == 1,
           "notes outside the trigger range pass through (key-switches / expression maps)");
    check (countIf (out, [] (const auto& m) { return m.isController() && m.getControllerNumber() == 1; }) == 1,
           "CC messages pass through");

    juce::String report;
    check (balanced (out, report), "all note-ons have note-offs " + report);

    // State round trip
    juce::MemoryBlock state;
    chord->getStateInformation (state);
    auto other = loadPlugin (path);
    other->setStateInformation (state.getData(), (int) state.getSize());
    juce::MemoryBlock state2;
    other->getStateInformation (state2);
    check (state == state2 && state.getSize() > 100, "state save / restore round-trip (" + juce::String ((int) state.getSize()) + " bytes)");

    chord->releaseResources();
}

//==============================================================================
void testChain (const juce::String& chordPath, const juce::String& pattPath, const juce::String& bendPath)
{
    std::cout << "\nChain: Chord-Cat -> Patt-Cat -> Bend-Cat (120 BPM, transport running)" << std::endl;
    auto chord = loadPlugin (chordPath);
    auto patt = loadPlugin (pattPath);
    auto bend = loadPlugin (bendPath);
    if (chord == nullptr || patt == nullptr || bend == nullptr) { ++failures; return; }

    TestPlayHead ph;
    ph.playing = true;
    prepare (*chord, ph);
    prepare (*patt, ph);
    prepare (*bend, ph);

    const int64_t bar = (int64_t) (4 * samplesPerBeat);
    std::vector<Event> in {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
        { 2 * bar - 2000, juce::MidiMessage::noteOff (1, 60) },
        { 2 * bar, juce::MidiMessage::noteOn (1, 63, (juce::uint8) 100) },
        { 4 * bar - 2000, juce::MidiMessage::noteOff (1, 63) },
    };

    std::vector<Event> chordOut, pattOut, bendOut;
    for (int64_t t = 0; t < 4 * bar + 24000; t += blockSize)
    {
        ph.ppq = (double) t / samplesPerBeat;
        auto a = runBlock (*chord, in, t);
        auto b = runBlock (*patt, a, t);
        auto c = runBlock (*bend, b, t);
        chordOut.insert (chordOut.end(), a.begin(), a.end());
        pattOut.insert (pattOut.end(), b.begin(), b.end());
        bendOut.insert (bendOut.end(), c.begin(), c.end());
    }

    // Patt-Cat default pattern: quarter-note chords
    std::set<int64_t> onTimes;
    bool onGrid = true;
    for (const auto& e : pattOut)
        if (e.msg.isNoteOn())
        {
            onTimes.insert (e.time);
            const double beats = (double) e.time / samplesPerBeat;
            if (std::abs (beats - std::round (beats)) * samplesPerBeat > 1.0) onGrid = false;
        }
    std::cout << "  Patt-Cat hits: " << onTimes.size() << ", note-ons: "
              << countIf (pattOut, [] (const auto& m) { return m.isNoteOn(); }) << std::endl;
    check (onTimes.size() == 16, "16 quarter-note hits over 4 bars (pattern 'Basic Quarters')");
    check (onGrid, "every hit lands exactly on the beat (sample-accurate, ppq-locked)");

    juce::String report;
    check (balanced (chordOut, report), "Chord-Cat output balanced " + report);
    report.clear();
    check (balanced (pattOut, report), "Patt-Cat output balanced " + report);

    // Bend-Cat: MPE
    std::set<int> noteChannels, bendChannels;
    bool bendBeforeNote = true;
    for (size_t i = 0; i < bendOut.size(); ++i)
    {
        const auto& m = bendOut[i].msg;
        if (m.isNoteOn())
        {
            noteChannels.insert (m.getChannel());
            bool found = false;
            for (size_t j = 0; j < i; ++j)
                if (bendOut[i].time - bendOut[j].time <= 1 && bendOut[j].msg.isPitchWheel() && bendOut[j].msg.getChannel() == m.getChannel())
                    found = true;
            bendBeforeNote = bendBeforeNote && found;
        }
        if (m.isPitchWheel()) bendChannels.insert (m.getChannel());
    }

    juce::String chans;
    for (int c : noteChannels) chans << c << " ";
    std::cout << "  Bend-Cat note channels: " << chans << "| pitch-bend messages: "
              << countIf (bendOut, [] (const auto& m) { return m.isPitchWheel(); }) << std::endl;

    check (! noteChannels.empty() && *noteChannels.begin() >= 2 && *noteChannels.rbegin() <= 16,
           "MPE: voices spread over member channels 2-16 (15 voices default)");
    check (noteChannels.size() >= 4, "chord voices get separate channels");
    check (bendBeforeNote, "each note-on is preceded by its channel's initial pitch-bend (same or previous sample)");

    bool hasMcm = false;
    for (size_t i = 0; i + 2 < bendOut.size(); ++i)
        if (bendOut[i].msg.isController() && bendOut[i].msg.getChannel() == 1 && bendOut[i].msg.getControllerNumber() == 100
            && bendOut[i].msg.getControllerValue() == 6 && bendOut[i + 1].msg.getControllerNumber() == 6)
            hasMcm = true;
    check (hasMcm, "MPE Configuration Message (RPN 6) sent on master channel 1");

    int minBend = 16383, maxBend = 0;
    for (const auto& e : bendOut)
        if (e.msg.isPitchWheel())
        {
            minBend = std::min (minBend, e.msg.getPitchWheelValue());
            maxBend = std::max (maxBend, e.msg.getPitchWheelValue());
        }
    std::cout << "  pitch-wheel range used: " << minBend << " .. " << maxBend << " (centre 8192)" << std::endl;
    check (maxBend > 8192 + 2000, "default 5-node envelope bends up (14-bit values above centre)");
    check (minBend == 8192, "no detune / drift by default: bends never go below centre");

    report.clear();
    check (balanced (bendOut, report), "Bend-Cat output balanced " + report);
}
//==============================================================================
// Regression tests for bugs found while testing in Cubase
//==============================================================================
void setParam (juce::AudioPluginInstance& p, const juce::String& name, const juce::String& valueText)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (100) == name)
        {
            prm->setValueNotifyingHost (prm->getValueForText (valueText));
            return;
        }
    std::cout << "  (parameter not found: " << name << ")" << std::endl;
    ++failures;
}

std::vector<Event> runAll (juce::AudioPluginInstance& p, TestPlayHead& ph, const std::vector<Event>& in, int64_t length)
{
    std::vector<Event> out;
    for (int64_t t = 0; t < length; t += blockSize)
    {
        ph.ppq = (double) t / samplesPerBeat;
        for (auto& e : runBlock (p, in, t)) out.push_back (e);
    }
    return out;
}

void testRegressions (const juce::String& chordPath, const juce::String& pattPath, const juce::String& bendPath)
{
    std::cout << "\nRegressions" << std::endl;
    using M = juce::MidiMessage;

    // --- Chord-Cat: several trigger keys held, then released -------------------------------
    {
        auto chord = loadPlugin (chordPath);
        TestPlayHead ph;
        prepare (*chord, ph);
        const std::vector<Event> in {
            { 1000, M::noteOn (1, 60, (juce::uint8) 100) }, { 1096, M::noteOn (1, 64, (juce::uint8) 100) },
            { 1192, M::noteOn (1, 67, (juce::uint8) 100) },
            { 30000, M::noteOff (1, 67) }, { 32400, M::noteOff (1, 64) }, { 34800, M::noteOff (1, 60) },
        };
        const auto out = runAll (*chord, ph, in, 48000);
        std::set<int64_t> onTimes;
        int onsetsAfterRelease = 0;
        for (const auto& e : out)
            if (e.msg.isNoteOn())
            {
                onTimes.insert (e.time);
                if (e.time >= 30000) ++onsetsAfterRelease;
            }
        check (onsetsAfterRelease == 0, "Chord-Cat: releasing held trigger keys fires no extra chords ("
                                        + juce::String (onsetsAfterRelease) + " note-ons after release)");
        check (onTimes.size() <= 2, "Chord-Cat: keys pressed together give one chord (a second onset only when the presses straddle a block)");
        juce::String report;
        check (balanced (out, report), "Chord-Cat: balanced after multi-key press " + report);
    }

    // --- Chord-Cat: strum can be long and shaped ------------------------------------------
    {
        auto chord = loadPlugin (chordPath);
        TestPlayHead ph;
        prepare (*chord, ph);
        setParam (*chord, "Strum", "300");
        setParam (*chord, "Strum Velocity Curve", "100");
        const std::vector<Event> in { { 0, M::noteOn (1, 60, (juce::uint8) 80) }, { 40000, M::noteOff (1, 60) } };
        const auto out = runAll (*chord, ph, in, 48000);
        std::vector<Event> ons;
        for (const auto& e : out) if (e.msg.isNoteOn()) ons.push_back (e);
        const double spreadMs = ons.size() > 1 ? (double) (ons.back().time - ons.front().time) / sampleRate * 1000.0 : 0.0;
        check (ons.size() == 4 && std::abs (spreadMs - 300.0) < 2.0,
               "Chord-Cat: 300 ms strum spreads the chord over 300 ms (" + juce::String (spreadMs, 1) + " ms)");
        check (ons.size() == 4 && ons.back().msg.getVelocity() > 2 * ons.front().msg.getVelocity(),
               "Chord-Cat: strum velocity curve is strong (" + juce::String (ons.empty() ? 0 : ons.front().msg.getVelocity()) + " -> "
               + juce::String (ons.empty() ? 0 : ons.back().msg.getVelocity()) + ")");

        // Shape +100 %: slow start, so the second note comes late (well after a third of the strum)
        auto chord2 = loadPlugin (chordPath);
        prepare (*chord2, ph);
        setParam (*chord2, "Strum", "300");
        setParam (*chord2, "Strum Shape", "100");
        const auto out2 = runAll (*chord2, ph, in, 48000);
        std::vector<int64_t> t2;
        for (const auto& e : out2) if (e.msg.isNoteOn()) t2.push_back (e.time);
        check (t2.size() == 4 && (double) (t2[1] - t2[0]) / sampleRate * 1000.0 < 30.0,
               "Chord-Cat: strum shape bunches notes (shaped timing)");
    }

    // --- Patt-Cat: chord changes a few ms after the beat (Cubase / late playing) ------------
    {
        auto chord = loadPlugin (chordPath);
        auto patt = loadPlugin (pattPath);
        TestPlayHead ph;
        ph.playing = true;
        prepare (*chord, ph);
        prepare (*patt, ph);
        const int64_t bar = (int64_t) (4 * samplesPerBeat);
        const int64_t late = 300; // ~6 ms
        const std::vector<Event> in {
            { late, M::noteOn (1, 60, (juce::uint8) 100) },
            { bar + late, M::noteOn (1, 61, (juce::uint8) 100) }, { bar + late + 200, M::noteOff (1, 60) },
            { 2 * bar + late, M::noteOn (1, 62, (juce::uint8) 100) }, { 2 * bar + late + 200, M::noteOff (1, 61) },
            { 3 * bar + late, M::noteOn (1, 63, (juce::uint8) 100) }, { 3 * bar + late + 200, M::noteOff (1, 62) },
            { 4 * bar - 1000, M::noteOff (1, 63) },
        };
        std::vector<Event> pattOut;
        for (int64_t t = 0; t < 4 * bar + 4800; t += blockSize)
        {
            ph.ppq = (double) t / samplesPerBeat;
            auto a = runBlock (*chord, in, t);
            for (auto& e : runBlock (*patt, a, t)) pattOut.push_back (e);
        }

        // Every quarter must sound for (nearly) its full gate: no beat lost, none cut to a blip
        int goodBeats = 0;
        for (int beat = 0; beat < 16; ++beat)
        {
            const int64_t start = (int64_t) (beat * samplesPerBeat);
            int64_t longest = 0;
            for (size_t i = 0; i < pattOut.size(); ++i)
            {
                const auto& on = pattOut[i];
                if (! on.msg.isNoteOn() || on.time < start || on.time > start + 2 * late) continue;
                for (size_t j = i + 1; j < pattOut.size(); ++j)
                    if (pattOut[j].msg.isNoteOff() && pattOut[j].msg.getNoteNumber() == on.msg.getNoteNumber()
                        && pattOut[j].msg.getChannel() == on.msg.getChannel())
                    { longest = std::max (longest, pattOut[j].time - on.time); break; }
            }
            if (longest > samplesPerBeat / 4) ++goodBeats;
        }
        check (goodBeats == 16, "Patt-Cat: chord changes just after the beat keep every hit ("
                                + juce::String (goodBeats) + "/16 beats sound)");
        juce::String report;
        check (balanced (pattOut, report), "Patt-Cat: balanced across late chord changes " + report);
    }

    // --- Patt-Cat: legato keyboard playing must not merge two chords, one velocity per chord --
    {
        auto patt = loadPlugin (pattPath);
        TestPlayHead ph;
        ph.playing = true;
        prepare (*patt, ph);
        const int64_t beat = (int64_t) samplesPerBeat;
        const std::vector<Event> in {
            { 10, M::noteOn (1, 60, (juce::uint8) 100) }, { 20, M::noteOn (1, 64, (juce::uint8) 100) },
            { 30, M::noteOn (1, 67, (juce::uint8) 60) },                                  // last key softer
            { 2 * beat - 4800, M::noteOn (1, 62, (juce::uint8) 100) }, { 2 * beat - 4790, M::noteOn (1, 65, (juce::uint8) 100) },
            { 2 * beat - 4780, M::noteOn (1, 69, (juce::uint8) 100) },
            { 2 * beat + 4800, M::noteOff (1, 60) }, { 2 * beat + 4800, M::noteOff (1, 64) }, { 2 * beat + 4800, M::noteOff (1, 67) },
            { 4 * beat - 100, M::noteOff (1, 62) }, { 4 * beat - 100, M::noteOff (1, 65) }, { 4 * beat - 100, M::noteOff (1, 69) },
        };
        const auto out = runAll (*patt, ph, in, 4 * beat + 4800);
        std::set<int> notesAtBeat2, velocitiesBeat0;
        for (const auto& e : out)
            if (e.msg.isNoteOn())
            {
                if (e.time >= 2 * beat && e.time < 2 * beat + 2400) notesAtBeat2.insert (e.msg.getNoteNumber());
                if (e.time < 2400) velocitiesBeat0.insert (e.msg.getVelocity());
            }
        check (notesAtBeat2 == std::set<int> { 62, 65, 69 }, "Patt-Cat: overlapping (legato) chords are not merged into one cluster ("
                                                            + juce::String ((int) notesAtBeat2.size()) + " notes)");
        check (velocitiesBeat0.size() == 1 && *velocitiesBeat0.begin() >= 90,
               "Patt-Cat: one velocity for the chord (loudest key), not the last key's");
    }

    // --- Bend-Cat: bend amount does not depend on the bend range; config before first note ---
    {
        auto bend = loadPlugin (bendPath);
        TestPlayHead ph;
        prepare (*bend, ph);
        setParam (*bend, "Bend Range", "24");
        std::vector<Event> in;
        for (int k = 0; k < 3; ++k)
            for (int n : { 48, 55, 64, 71 })
            {
                in.push_back ({ 48000 + k * 24000, M::noteOn (1, n + k, (juce::uint8) 100) });
                in.push_back ({ 48000 + k * 24000 + 23000, M::noteOff (1, n + k) });
            }
        const auto out = runAll (*bend, ph, in, 48000 * 3);

        int maxBend = 0;
        for (const auto& e : out) if (e.msg.isPitchWheel()) maxBend = std::max (maxBend, e.msg.getPitchWheelValue());
        const double maxSemis = (maxBend - 8192) / 8192.0 * 24.0;
        check (maxSemis > 1.5 && maxSemis < 2.1, "Bend-Cat: range 24 st still bends by the 2 st amount (max "
                                                 + juce::String (maxSemis, 2) + " st), not by half an octave or more");

        bool rangeBeforeFirstNote = false;
        for (size_t i = 0; i + 2 < out.size() && ! out[i].msg.isNoteOn(); ++i)
            if (out[i].time >= 47000 && out[i].msg.isController() && out[i].msg.getControllerNumber() == 100
                && out[i].msg.getControllerValue() == 0 && out[i + 1].msg.getControllerNumber() == 6
                && out[i + 1].msg.getControllerValue() == 24)
                rangeBeforeFirstNote = true;
        check (rangeBeforeFirstNote, "Bend-Cat: bend range (RPN 0 = 24) is re-sent right before the first note");
        juce::String report;
        check (balanced (out, report), "Bend-Cat: balanced " + report);
    }
}
//==============================================================================
void testPattCatBounce (const juce::String& path)
{
    std::cout << "\nPatt-Cat Bounce (120 BPM)" << std::endl;
    using M = juce::MidiMessage;
    const int64_t beat = (int64_t) samplesPerBeat;

    auto ons = [] (const std::vector<Event>& ev, int note)
    {
        std::vector<Event> r;
        for (const auto& e : ev) if (e.msg.isNoteOn() && e.msg.getNoteNumber() == note) r.push_back (e);
        return r;
    };

    // Keys, straight bounces: note + 4 bounces a quarter apart, getting quieter
    {
        auto patt = loadPlugin (path);
        TestPlayHead ph;
        ph.playing = true;
        prepare (*patt, ph);
        setParam (*patt, "Bounce", "Keys");
        setParam (*patt, "Bounce Notes", "4");
        setParam (*patt, "Bounce Curve", "0");
        setParam (*patt, "Bounce Fade Out", "60");
        setParam (*patt, "Mute A", "On");          // only the bounce, no pattern
        const auto out = runAll (*patt, ph, { { 0, M::noteOn (1, 72, (juce::uint8) 100) }, { 6 * beat, M::noteOff (1, 72) } }, 6 * beat);
        const auto hits = ons (out, 72);
        bool spacing = hits.size() == 5;
        for (size_t i = 1; spacing && i < hits.size(); ++i)
            spacing = std::abs ((hits[i].time - hits[i - 1].time) - beat) <= 1;
        bool fading = hits.size() == 5;
        for (size_t i = 1; fading && i < hits.size(); ++i)
            fading = hits[i].msg.getVelocity() < hits[i - 1].msg.getVelocity();
        check (spacing, "Keys mode: a key plus 4 bounces, a quarter note apart (" + juce::String ((int) hits.size()) + " hits)");
        check (fading, "Fade Out: every bounce is quieter than the one before");
        juce::String report;
        check (balanced (out, report), "Bounce output balanced " + report);
    }

    // Curve +: gaps shrink like a real ball;  While Held: letting go stops the bounces
    {
        auto patt = loadPlugin (path);
        TestPlayHead ph;
        ph.playing = true;
        prepare (*patt, ph);
        setParam (*patt, "Bounce", "Keys");
        setParam (*patt, "Bounce Notes", "6");
        setParam (*patt, "Bounce Curve", "60");
        setParam (*patt, "Mute A", "On");
        const auto out = runAll (*patt, ph, { { 0, M::noteOn (1, 60, (juce::uint8) 100) }, { 6 * beat, M::noteOff (1, 60) } }, 6 * beat);
        const auto hits = ons (out, 60);
        bool shrinking = hits.size() == 7;
        for (size_t i = 2; shrinking && i < hits.size(); ++i)
            shrinking = (hits[i].time - hits[i - 1].time) < (hits[i - 1].time - hits[i - 2].time);
        check (shrinking, "Curve +60: each gap is shorter than the last (bouncing ball)");

        auto held = loadPlugin (path);
        prepare (*held, ph);
        setParam (*held, "Bounce", "Keys");
        setParam (*held, "Bounce Notes", "8");
        setParam (*held, "Bounce Curve", "0");
        setParam (*held, "Bounce Play Mode", "While Held");
        setParam (*held, "Mute A", "On");
        const auto out2 = runAll (*held, ph, { { 0, M::noteOn (1, 64, (juce::uint8) 100) }, { beat + beat / 2, M::noteOff (1, 64) } }, 6 * beat);
        check (ons (out2, 64).size() == 2, "While Held: releasing the key stops the remaining bounces ("
                                           + juce::String ((int) ons (out2, 64).size()) + " hits)");
        juce::String report;
        check (balanced (out2, report), "While Held output balanced " + report);
    }

    // Pattern Notes: every pattern hit gets its bounces, an octave up
    {
        auto patt = loadPlugin (path);
        TestPlayHead ph;
        ph.playing = true;
        prepare (*patt, ph);
        setParam (*patt, "Bounce", "Pattern Notes");
        setParam (*patt, "Bounce Notes", "2");
        setParam (*patt, "Bounce Rate", "1/8");
        setParam (*patt, "Bounce Octave", "1");
        const auto out = runAll (*patt, ph, { { 0, M::noteOn (1, 60, (juce::uint8) 100) }, { 4 * beat, M::noteOff (1, 60) } }, 4 * beat + beat);
        check (ons (out, 72).size() >= 6, "Pattern Notes: pattern hits bounce (an octave up) - "
                                          + juce::String ((int) ons (out, 72).size()) + " bounces");
        juce::String report;
        check (balanced (out, report), "Pattern bounce output balanced " + report);
    }
}

//==============================================================================
void testDrumCat (const juce::String& path)
{
    std::cout << "\nDrum-Cat: pattern playback (120 BPM, transport running)" << std::endl;
    auto drum = loadPlugin (path);
    if (drum == nullptr) { ++failures; return; }
    TestPlayHead ph;
    ph.playing = true;
    prepare (*drum, ph);
    setParam (*drum, "Human", "0");

    const int64_t bar = (int64_t) (4 * samplesPerBeat);
    auto out = runAll (*drum, ph, {}, 4 * bar);
    {
        // Stop the transport and let the last note-offs come out
        ph.playing = false;
        for (int64_t t = 4 * bar; t < 4 * bar + 24000; t += blockSize)
            for (auto& e : runBlock (*drum, {}, t)) out.push_back (e);
        ph.playing = true;
    }

    int kicks = 0, snares = 0, ons = 0;
    std::set<int> notes;
    bool kickOnDownbeats = true;
    for (const auto& e : out)
        if (e.msg.isNoteOn())
        {
            ++ons;
            notes.insert (e.msg.getNoteNumber());
            if (e.msg.getNoteNumber() == 36) ++kicks;
            if (e.msg.getNoteNumber() == 38 || e.msg.getNoteNumber() == 37) ++snares;
        }
    for (int b = 0; b < 4; ++b)
    {
        bool found = false;
        for (const auto& e : out)
            if (e.msg.isNoteOn() && e.msg.getNoteNumber() == 36 && std::abs (e.time - b * bar) < (int64_t) (0.03 * samplesPerBeat))
                found = true;
        kickOnDownbeats = kickOnDownbeats && found;
    }
    std::cout << "  " << ons << " hits over 4 bars, " << kicks << " kicks, " << snares << " backbeats" << std::endl;
    check (ons > 30 && kicks >= 8 && snares >= 8, "Drum-Cat plays a full groove (kicks, backbeats, hats)");
    check (kickOnDownbeats, "kick lands on every downbeat (host-synced)");
    check (notes.count (42) > 0, "General MIDI map: closed hat on note 42");
    juce::String report;
    check (balanced (out, report), "Drum-Cat output balanced " + report);

    // Every style generates something sensible, and Sequential mapping moves the notes
    setParam (*drum, "MIDI Map", "Sequential C1");
    const auto seqOut = runAll (*drum, ph, {}, bar);
    bool inRange = true;
    for (const auto& e : seqOut) if (e.msg.isNoteOn()) inRange = inRange && e.msg.getNoteNumber() >= 36 && e.msg.getNoteNumber() <= 47;
    check (inRange, "Sequential C1 map keeps all lanes on pads C1-B1");

    // Key trigger with the transport stopped
    auto drum2 = loadPlugin (path);
    TestPlayHead stopped;
    prepare (*drum2, stopped);
    const std::vector<Event> keys { { 1000, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) }, { 1000 + bar, juce::MidiMessage::noteOff (1, 60) } };
    const auto keyOut = runAll (*drum2, stopped, keys, 2 * bar);
    int during = 0, after = 0;
    for (const auto& e : keyOut)
        if (e.msg.isNoteOn()) { if (e.time < 1000 + bar) ++during; else ++after; }
    check (during > 5 && after == 0, "holding a key plays the pattern while the transport is stopped, releasing stops it");
}

void testOrbitCat (const juce::String& path)
{
    std::cout << "\nOrbit-Cat: orbital arpeggiator (120 BPM)" << std::endl;
    auto orbit = loadPlugin (path);
    if (orbit == nullptr) { ++failures; return; }
    TestPlayHead ph;
    ph.playing = true;
    prepare (*orbit, ph);

    const int64_t bar = (int64_t) (4 * samplesPerBeat);
    std::vector<Event> in;
    for (int n : { 48, 60, 64, 67 })
    {
        in.push_back ({ 100, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
        in.push_back ({ 4 * bar, juce::MidiMessage::noteOff (1, n) });
    }
    const auto out = runAll (*orbit, ph, in, 4 * bar + 48000);

    std::map<int, int> perNote;
    bool onGrid = true;
    int64_t lastOn = 0;
    for (const auto& e : out)
        if (e.msg.isNoteOn())
        {
            ++perNote[e.msg.getNoteNumber()];
            const double sixteenths = (double) e.time / (samplesPerBeat / 4.0);
            if (std::abs (sixteenths - std::round (sixteenths)) * (samplesPerBeat / 4.0) > 1.0) onGrid = false;
            lastOn = e.time;
        }
    juce::String counts;
    for (auto& [n, c] : perNote) counts << juce::MidiMessage::getMidiNoteName (n, true, true, 3) << "=" << c << " ";
    std::cout << "  triggers per note: " << counts << std::endl;
    check (perNote.size() == 4, "every held note orbits and plays");
    check (perNote[48] > 0 && perNote[67] > 0 && perNote.begin()->second != perNote.rbegin()->second,
           "inner and outer orbits play at different rates (Kepler)");
    check (onGrid, "Quantize 1/16 puts every trigger on the 16th grid");
    check (lastOn <= 4 * bar + (int64_t) (samplesPerBeat / 4.0), "releasing the keys stops the orbits");
    juce::String report;
    check (balanced (out, report), "Orbit-Cat output balanced " + report);
}
//==============================================================================
// Sing-Cat: makes audio, so these tests look at the sound itself
//==============================================================================
struct Rendered { std::vector<float> mono; float peak = 0.0f; bool finite = true; double cpu = 0.0; int midiOut = 0; };

Rendered renderAudio (juce::AudioPluginInstance& p, const std::vector<Event>& in, int64_t length)
{
    Rendered r;
    juce::AudioBuffer<float> audio (juce::jmax (1, p.getTotalNumOutputChannels()), blockSize);
    const auto start = juce::Time::getMillisecondCounterHiRes();
    for (int64_t t = 0; t < length; t += blockSize)
    {
        audio.clear();
        juce::MidiBuffer midi;
        for (const auto& e : in)
            if (e.time >= t && e.time < t + blockSize)
                midi.addEvent (e.msg, (int) (e.time - t));
        p.processBlock (audio, midi);
        r.midiOut += midi.getNumEvents();
        for (int i = 0; i < blockSize; ++i)
        {
            float m = 0.0f;
            for (int c = 0; c < audio.getNumChannels(); ++c)
            {
                const float v = audio.getSample (c, i);
                r.finite = r.finite && std::isfinite (v);
                r.peak = juce::jmax (r.peak, std::abs (v));
                m += v;
            }
            r.mono.push_back (m / (float) audio.getNumChannels());
        }
    }
    r.cpu = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0 / ((double) length / sampleRate);
    return r;
}

float rms (const std::vector<float>& x, int64_t from, int64_t to)
{
    double sum = 0.0;
    from = juce::jlimit<int64_t> (0, (int64_t) x.size(), from);
    to = juce::jlimit<int64_t> (from, (int64_t) x.size(), to);
    for (auto i = from; i < to; ++i) sum += (double) x[(size_t) i] * x[(size_t) i];
    return to > from ? (float) std::sqrt (sum / (double) (to - from)) : 0.0f;
}

/** Fundamental by normalised autocorrelation (60..1000 Hz) with parabolic refinement. */
double pitchOf (const std::vector<float>& x, int64_t from, int length)
{
    const int minLag = (int) (sampleRate / 1000.0), maxLag = (int) (sampleRate / 60.0);
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
    {
        double s = 0.0, e1 = 0.0, e2 = 0.0;
        for (int i = 0; i < length; ++i)
        {
            const double a = x[(size_t) (from + i)], b = x[(size_t) (from + i + lag)];
            s += a * b; e1 += a * a; e2 += b * b;
        }
        ac[(size_t) lag] = s / std::sqrt (e1 * e2 + 1.0e-20);
    }
    int best = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] > ac[(size_t) best] * 1.0 + 0.0 && ac[(size_t) lag] > 0.0) best = lag;
    // prefer the first strong peak (avoid octave errors)
    for (int lag = minLag; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] > 0.85 * ac[(size_t) best] && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1])
        { best = lag; break; }
    const double y0 = ac[(size_t) best - 1], y1 = ac[(size_t) best], y2 = ac[(size_t) best + 1];
    const double shift = 0.5 * (y0 - y2) / (y0 - 2.0 * y1 + y2 + 1.0e-12);
    return sampleRate / ((double) best + shift);
}

/** Energy of the signal in a frequency band (Goertzel over the harmonics' range). */
double bandEnergy (const std::vector<float>& x, int64_t from, int length, double lo, double hi)
{
    double total = 0.0;
    for (double f = lo; f <= hi; f += 10.0)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sampleRate, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < length; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / length);
            const double s0 = x[(size_t) (from + i)] * win + c * s1 - s2;
            s2 = s1; s1 = s0;
        }
        total += s1 * s1 + s2 * s2 - c * s1 * s2;
    }
    return total;
}

void testSingCat (const juce::String& path)
{
    std::cout << "\nSing-Cat: physically modelled cat & human voices" << std::endl;
    auto talk = loadPlugin (path);
    if (talk == nullptr) { ++failures; return; }
    TestPlayHead ph;
    prepare (*talk, ph);
    check (talk->getTotalNumOutputChannels() == 2, "stereo instrument output");

    // A steady soloist: tenor, no vibrato / drift / room, so the pitch is easy to measure
    setParam (*talk, "Mode", "Solo");
    setParam (*talk, "Cast", "Men");
    setParam (*talk, "Style", "Chant");
    setParam (*talk, "Drift", "0");
    setParam (*talk, "Vibrato", "0");
    setParam (*talk, "Breath", "0");
    setParam (*talk, "Room Mix", "0");
    setParam (*talk, "Release", "300");
    const int64_t sec = (int64_t) sampleRate;
    const std::vector<Event> note { { 0, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100) }, { 2 * sec, juce::MidiMessage::noteOff (1, 57) } };
    const auto r = renderAudio (*talk, note, 4 * sec);
    const double f0 = pitchOf (r.mono, sec / 2, 9600);
    std::cout << "  A3: " << juce::String (f0, 2) << " Hz (expected 220), level " << juce::String (rms (r.mono, sec / 2, sec), 3)
              << ", tail " << juce::String (rms (r.mono, (int64_t) (3.5 * (double) sec), 4 * sec), 6) << std::endl;
    check (r.finite, "no NaN / Inf in the output");
    check (rms (r.mono, sec / 2, sec) > 0.02f, "the singer is clearly audible");
    check (std::abs (1200.0 * std::log2 (f0 / 220.0)) < 15.0, "sings A3 in tune (within 15 cents incl. the singer's own detune)");
    check (rms (r.mono, (int64_t) (3.5 * (double) sec), 4 * sec) < 1.0e-3f, "falls silent after the release");
    check (r.midiOut == 0, "no MIDI output (it is an instrument)");

    // Vowels: /i/ has its first formant low, /a/ high
    auto vowel = [&] (const char* x, const char* y)
    {
        setParam (*talk, "Vowel Front-Back", x);
        setParam (*talk, "Vowel Close-Open", y);
        const auto v = renderAudio (*talk, { { 0, juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100) }, { sec, juce::MidiMessage::noteOff (1, 45) } }, 2 * sec);
        return bandEnergy (v.mono, sec / 2, 8192, 200, 400) / bandEnergy (v.mono, sec / 2, 8192, 600, 900);
    };
    const double ratioI = vowel ("0.06", "0.06"), ratioA = vowel ("0.55", "0.95");
    std::cout << "  low/mid formant energy: /i/ " << juce::String (ratioI, 2) << ", /a/ " << juce::String (ratioA, 3) << std::endl;
    check (ratioI > 4.0 * ratioA, "vowel chart moves the formants (/i/ low F1, /a/ high F1)");

    // MPE: per-note bend with a 48 semitone range
    setParam (*talk, "Mode", "Choir");
    setParam (*talk, "MPE", "On");
    setParam (*talk, "Bend Range", "48");
    const int up12 = 8192 + (int) std::lround (8191.0 * 12.0 / 48.0);
    const auto mpe = renderAudio (*talk, { { 0, juce::MidiMessage::noteOn (2, 57, (juce::uint8) 100) },
                                           { 10, juce::MidiMessage::pitchWheel (2, up12) },
                                           { sec, juce::MidiMessage::noteOff (2, 57) } }, 2 * sec);
    const double bent = pitchOf (mpe.mono, sec / 2, 9600);
    std::cout << "  MPE bend +12: " << juce::String (bent, 1) << " Hz" << std::endl;
    check (std::abs (1200.0 * std::log2 (bent / 440.0)) < 25.0, "MPE pitch bend (range 48) moves the note an octave up");

    // Regression (found in Cubase): letting go of a chord must only fade - no jump in level -
    // and no singer takes an audible breath afterwards, even with Breath at 100 %
    {
        auto choir = loadPlugin (path);
        prepare (*choir, ph);
        setParam (*choir, "Mode", "Tutti");
        setParam (*choir, "Singers", "16");
        setParam (*choir, "Breath", "100");
        setParam (*choir, "Room Mix", "0");
        setParam (*choir, "Release", "400");
        std::vector<Event> held;
        for (int n : { 48, 55, 60, 64 })
        {
            held.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
            held.push_back ({ sec, juce::MidiMessage::noteOff (1, n) });
        }
        const auto rel = renderAudio (*choir, held, 3 * sec);
        const float before = rms (rel.mono, sec - 4800, sec);
        float loudestAfter = 0.0f;
        for (int64_t t = sec; t < 2 * sec; t += 960)
            loudestAfter = juce::jmax (loudestAfter, rms (rel.mono, t, t + 960));
        const float tail = rms (rel.mono, (int64_t) (2.2 * (double) sec), 3 * sec);
        std::cout << "  chord release: before " << juce::String (before, 4) << ", loudest 20 ms after " << juce::String (loudestAfter, 4)
                  << ", tail " << juce::String (tail, 6) << std::endl;
        check (loudestAfter < before * 1.15f, "releasing a chord only fades (no level jump)");
        check (tail < 1.0e-3f, "no breathing noise after the phrase, even with Breath 100 %");
    }

    // Every preset sings, stays finite and below clipping
    bool allOk = true;
    juce::String quiet;
    for (int i = 0; i < talk->getNumPrograms(); ++i)
    {
        auto fresh = loadPlugin (path);
        prepare (*fresh, ph);
        fresh->setCurrentProgram (i);
        std::vector<Event> chord;
        for (int n : { 48, 55, 60, 64 })
        {
            chord.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
            chord.push_back ({ (int64_t) (1.5 * (double) sec), juce::MidiMessage::noteOff (1, n) });
        }
        const auto pr = renderAudio (*fresh, chord, 2 * sec);
        const float level = rms (pr.mono, sec / 2, (int64_t) (1.4 * (double) sec));
        if (! pr.finite || pr.peak > 1.0f || level < 0.005f)
        {
            allOk = false;
            quiet << fresh->getProgramName (i) << " (rms " << juce::String (level, 4) << ", peak " << juce::String (pr.peak, 2) << ") ";
        }
    }
    check (allOk && talk->getNumPrograms() >= 16, juce::String (talk->getNumPrograms()) + " presets all sing, finite and unclipped " + quiet);

    // Regression: releasing a chord (fingers lifting a few ms apart) must never get louder
    {
        auto choir = loadPlugin (path);
        prepare (*choir, ph);
        setParam (*choir, "Mode", "Tutti");
        setParam (*choir, "Singers", "16");
        setParam (*choir, "Breath", "60");
        setParam (*choir, "Release", "900");
        std::vector<Event> ev;
        int k = 0;
        for (int n : { 48, 55, 60, 64 })
        {
            ev.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
            ev.push_back ({ 3 * sec + 1200 * k++, juce::MidiMessage::noteOff (1, n) });
        }
        const auto rr = renderAudio (*choir, ev, 4 * sec);
        auto shortRms = [&rr] (int64_t from, int64_t to)
        {
            float mx = 0.0f;
            for (auto t = from; t + 480 < to; t += 120) mx = juce::jmax (mx, rms (rr.mono, t, t + 480));
            return mx;
        };
        const float held = shortRms (2 * sec, 3 * sec), released = shortRms (3 * sec, 3 * sec + sec / 2);
        std::cout << "  chord release: held max " << juce::String (held, 3) << ", after release max " << juce::String (released, 3) << std::endl;
        check (released <= held * 1.05f, "no level stab when a Tutti chord is released note by note");
    }

    // Words: each note sings the next syllable; "sh" of "shoo" is a real fricative (hiss)
    {
        auto words = loadPlugin (path);
        prepare (*words, ph);
        int doowop = -1;
        for (int i = 0; i < words->getNumPrograms(); ++i)
            if (words->getProgramName (i) == "Doo-Wop Words") doowop = i;
        words->setCurrentProgram (juce::jmax (0, doowop));
        setParam (*words, "Room Mix", "0");
        std::vector<Event> ev;
        for (int k = 0; k < 6; ++k)
        {
            ev.push_back ({ (int64_t) k * sec / 2, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100) });
            ev.push_back ({ (int64_t) k * sec / 2 + sec * 2 / 5, juce::MidiMessage::noteOff (1, 52) });
        }
        const auto wr = renderAudio (*words, ev, 3 * sec);
        // syllables: doo bee doo wah shoo bop -> note 4 = "wah", note 5 = "shoo"
        const double wah = bandEnergy (wr.mono, 3 * sec / 2 + 600, 2400, 3000, 7000) / (bandEnergy (wr.mono, 3 * sec / 2 + 600, 2400, 150, 1500) + 1.0e-12);
        const double shoo = bandEnergy (wr.mono, 2 * sec + 600, 2400, 3000, 7000) / (bandEnergy (wr.mono, 2 * sec + 600, 2400, 150, 1500) + 1.0e-12);
        std::cout << "  words: high/low energy at the start of 'wah' " << juce::String (wah, 4) << ", of 'shoo' " << juce::String (shoo, 4) << std::endl;
        check (doowop >= 0 && wr.finite && shoo > 3.0 * wah, "lyrics: each note sings the next syllable and 'sh' is a hissing fricative");
    }

    // 64 singers in Tutti: CPU
    auto big = loadPlugin (path);
    prepare (*big, ph);
    setParam (*big, "Mode", "Tutti");
    setParam (*big, "Singers", "64");
    std::vector<Event> chord;
    for (int n : { 48, 55, 60, 64 })
        chord.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
    const auto tutti = renderAudio (*big, chord, 2 * sec);
    std::cout << "  64-singer Tutti: " << juce::String (tutti.cpu * 100.0, 1) << "% of one core, peak " << juce::String (tutti.peak, 2) << std::endl;
    check (tutti.finite && tutti.cpu < 0.8, "64 singers run in real time");

    juce::MemoryBlock state, state2;
    talk->getStateInformation (state);
    auto other = loadPlugin (path);
    other->setStateInformation (state.getData(), (int) state.getSize());
    other->getStateInformation (state2);
    check (state == state2 && state.getSize() > 100, "state save / restore round-trip incl. stage + path (" + juce::String ((int) state.getSize()) + " bytes)");
}
void testPresetPrograms()
{
    std::cout << "\nFactory presets exposed to the host" << std::endl;
    struct P { const char* path; const char* name; int minCount; };
    for (const auto& pl : { P { BENDCAT_VST3, "Bend-Cat", 25 }, P { DRUMCAT_VST3, "Drum-Cat", 24 }, P { ORBITCAT_VST3, "Orbit-Cat", 32 }, P { SINGCAT_VST3, "Sing-Cat", 48 }, P { BASSCAT_VST3, "Bass-Cat", 24 }, P { GLIDECAT_VST3, "Glide-Cat", 40 }, P { TALKCAT_VST3, "Talk-Cat", 30 } })
    {
        auto plug = loadPlugin (pl.path);
        if (plug == nullptr) { ++failures; continue; }
        TestPlayHead ph;
        prepare (*plug, ph);
        bool allNamed = true, statesDiffer = false;
        juce::MemoryBlock first;
        for (int i = 0; i < plug->getNumPrograms(); ++i)
        {
            plug->setCurrentProgram (i);
            allNamed = allNamed && plug->getProgramName (i).isNotEmpty();
            juce::MemoryBlock st;
            plug->getStateInformation (st);
            if (i == 0) first = st; else statesDiffer = statesDiffer || ! (st == first);
        }
        check (plug->getNumPrograms() >= pl.minCount && allNamed && statesDiffer,
               juce::String (pl.name) + ": " + juce::String (plug->getNumPrograms()) + " named presets that change the sound");
    }
}
juce::String paramText (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (100) == name)
            return prm->getCurrentValueAsText();
    return "(missing)";
}

void testCatLink()
{
    std::cout << "\nCat Link: shared groove and chain presets between the Cats" << std::endl;
    auto drum = loadPlugin (DRUMCAT_VST3);
    auto patt = loadPlugin (PATTCAT_VST3);
    auto orbit = loadPlugin (ORBITCAT_VST3);
    auto chord = loadPlugin (CHORDCAT_VST3);
    auto bend = loadPlugin (BENDCAT_VST3);
    auto bassc = loadPlugin (BASSCAT_VST3);
    if (! drum || ! patt || ! orbit || ! chord || ! bend || ! bassc) { ++failures; return; }
    setParam (*bassc, "Follow Drum Groove", "1");

    setParam (*patt, "Follow Drum Groove", "1");
    setParam (*orbit, "Follow Drum Groove", "1");
    setParam (*chord, "Follow Drum Groove", "1");
    setParam (*drum, "Human", "6");
    setParam (*drum, "Swing", "30");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);

    std::cout << "  Drum swing 30 % / human 6 ms -> Patt " << paramText (*patt, "Global Swing") << ", Orbit " << paramText (*orbit, "Swing")
              << ", Chord swing " << paramText (*chord, "Swing") << " / " << paramText (*chord, "Humanize Timing") << std::endl;
    check (paramText (*patt, "Global Swing").getIntValue() == 30, "Patt-Cat follows Drum-Cat's swing");
    check (std::abs (paramText (*bassc, "Swing").getFloatValue() - 30.0f) < 0.6f, "Bass-Cat follows Drum-Cat's swing");
    check (std::abs (paramText (*orbit, "Swing").getFloatValue() - 30.0f) < 0.6f, "Orbit-Cat follows Drum-Cat's swing");
    check (paramText (*chord, "Swing").getIntValue() == 58 && std::abs (paramText (*chord, "Humanize Timing").getFloatValue() - 6.0f) < 0.2f,
           "Chord-Cat follows Drum-Cat's swing (as 58 % MPC swing) and timing");

    // A chain picked in "another Cat": written straight into the link file
    {
        juce::XmlElement x ("CATLINK");
        if (auto old = juce::XmlDocument::parse (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CatSuiteLink.xml")))
            x = *old;
        auto* c = x.getChildByName ("CHAIN");
        if (c == nullptr) c = x.createNewChildElement ("CHAIN");
        c->setAttribute ("name", "Trap Night");
        c->setAttribute ("stamp", juce::String (juce::Time::currentTimeMillis() + 10));
        c->setAttribute ("source", "host-test");
        x.writeTo (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CatSuiteLink.xml"));
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (1200);
    auto now = [] (juce::AudioPluginInstance& p) { return p.getProgramName (p.getCurrentProgram()); };
    std::cout << "  chain 'Trap Night' -> " << now (*chord) << " / " << now (*patt) << " / " << now (*bend) << " / " << now (*drum) << std::endl;
    std::cout << "  Drum style " << paramText (*drum, "Style") << ", Chord scale " << paramText (*chord, "Scale") << ", Bend amount "
              << paramText (*bend, "Bend Amount") << std::endl;
    check (paramText (*drum, "Style") == "Trap" && paramText (*chord, "Scale").containsIgnoreCase ("Minor") && paramText (*bend, "Bend Amount").getIntValue() == 5
               && paramText (*bassc, "Style") == "808",
           "a chain preset picked in one Cat loads the matching preset in every Cat");
}
void testBassCat()
{
    std::cout << "\nBass-Cat: bass lines that follow the chords (120 BPM)" << std::endl;
    auto bass = loadPlugin (BASSCAT_VST3);
    if (bass == nullptr) { ++failures; return; }
    TestPlayHead ph;
    ph.playing = true;
    prepare (*bass, ph);

    const int64_t bar = (int64_t) (4 * samplesPerBeat);
    std::vector<Event> in;
    for (int n : { 48, 51, 55, 58 })   // C minor 7 for two bars ...
    {
        in.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
        in.push_back ({ 2 * bar - 200, juce::MidiMessage::noteOff (1, n) });
    }
    for (int n : { 53, 57, 60 })       // ... then F major for two bars
    {
        in.push_back ({ 2 * bar - 100, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
        in.push_back ({ 4 * bar - 200, juce::MidiMessage::noteOff (1, n) });
    }
    const auto out = runAll (*bass, ph, in, 4 * bar + 24000);

    int ons = 0, onDownbeat = -1, onBar3 = -1, low = 128, high = 0, maxOverlap = 0, sounding = 0;
    std::set<int> pcsFirst, pcsSecond;
    for (const auto& e : out)
    {
        if (e.msg.isNoteOn())
        {
            ++ons;
            const int n = e.msg.getNoteNumber();
            low = juce::jmin (low, n); high = juce::jmax (high, n);
            if (e.time < 200 && onDownbeat < 0) onDownbeat = n;
            if (std::abs (e.time - 2 * bar) < 400 && onBar3 < 0) onBar3 = n;
            (e.time < 2 * bar - 400 ? pcsFirst : pcsSecond).insert (n % 12);
            maxOverlap = juce::jmax (maxOverlap, ++sounding);
        }
        else if (e.msg.isNoteOff()) sounding = juce::jmax (0, sounding - 1);
    }
    std::cout << "  " << ons << " notes, range " << juce::MidiMessage::getMidiNoteName (low, true, true, 3) << " - "
              << juce::MidiMessage::getMidiNoteName (high, true, true, 3) << ", downbeat " << onDownbeat << ", bar 3 " << onBar3 << std::endl;
    check (ons >= 8, "plays a bass line while a chord is held");
    check (onDownbeat >= 0 && onDownbeat % 12 == 0 && onDownbeat < 48, "C minor 7 -> the line starts on a low C on the downbeat");
    check (onBar3 >= 0 && onBar3 % 12 == 5, "after the change to F major the next downbeat plays F");
    check (low >= 16 && high <= 64, "stays in the bass register");
    check (maxOverlap <= 2, "monophonic (only slides overlap)");
    juce::String report;
    check (balanced (out, report), "Bass-Cat output balanced " + report);

    // Inversion: E G C (C major, 1st inversion) still has C as the root
    auto inv = loadPlugin (BASSCAT_VST3);
    prepare (*inv, ph);
    std::vector<Event> in2;
    for (int n : { 52, 55, 60 })
    {
        in2.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
        in2.push_back ({ bar, juce::MidiMessage::noteOff (1, n) });
    }
    const auto out2 = runAll (*inv, ph, in2, bar + 24000);
    int first = -1;
    for (const auto& e : out2) if (e.msg.isNoteOn()) { first = e.msg.getNoteNumber(); break; }
    check (first >= 0 && first % 12 == 0, "chord analysis finds the root of an inversion (E-G-C -> C)");

    // Nothing held, no latch: silence
    auto quiet = loadPlugin (BASSCAT_VST3);
    prepare (*quiet, ph);
    const auto out3 = runAll (*quiet, ph, {}, bar);
    check (countIf (out3, [] (const juce::MidiMessage& m) { return m.isNoteOn(); }) == 0, "silent when no chord is held");
}
void testChordCatStrum()
{
    std::cout << "\nChord-Cat Strum engine" << std::endl;
    TestPlayHead stopped;
    auto strumRun = [&] (std::function<void (juce::AudioPluginInstance&)> setup, int64_t length, bool playing = false)
    {
        auto c = loadPlugin (CHORDCAT_VST3);
        TestPlayHead ph;
        ph.playing = playing;
        prepare (*c, ph);
        setup (*c);
        std::vector<Event> in { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) }, { length - 4800, juce::MidiMessage::noteOff (1, 60) } };
        return runAll (*c, ph, in, length);
    };
    auto onsOf = [] (const std::vector<Event>& out) { std::vector<Event> v; for (auto& e : out) if (e.msg.isNoteOn()) v.push_back (e); return v; };

    // Down strum, 100 ms: highest note first, last note ~100 ms later
    auto down = onsOf (strumRun ([] (auto& c) { setParam (c, "Strum", "100"); setParam (c, "Strum Direction", "Down"); }, 48000));
    const double spanMs = down.size() > 1 ? (double) (down.back().time - down.front().time) / sampleRate * 1000.0 : 0.0;
    bool descending = down.size() > 2;
    for (size_t i = 1; i < down.size(); ++i) descending = descending && down[i].msg.getNoteNumber() < down[i - 1].msg.getNoteNumber();
    std::cout << "  down strum: " << down.size() << " notes over " << juce::String (spanMs, 1) << " ms" << std::endl;
    check (descending && std::abs (spanMs - 100.0) < 3.0, "Down strum: high to low, spread over the strum time");

    // Octave stack: the chord again an octave higher
    auto stacked = onsOf (strumRun ([] (auto& c) { setParam (c, "Strum", "60"); setParam (c, "Strum Octave Stack", "1"); }, 48000));
    check (stacked.size() == 2 * down.size(), "octave stack doubles the chord one octave up (" + juce::String ((int) stacked.size()) + " notes)");

    // Round trip with 2 strokes: up then back down, the turning note not struck twice
    auto trip = onsOf (strumRun ([] (auto& c) { setParam (c, "Strum", "80"); setParam (c, "Strum Repeats", "2"); setParam (c, "Strum Round Trip", "1"); }, 48000));
    check (trip.size() == 2 * down.size() - 1 && trip[down.size()].msg.getNoteNumber() < trip[down.size() - 1].msg.getNoteNumber(),
           "Round Trip: up, then back down (" + juce::String ((int) trip.size()) + " notes)");

    // Burst 1/8 with the transport running: one stroke per eighth note while held (1 bar = 8 strokes)
    auto burst = strumRun ([] (auto& c) { setParam (c, "Strum", "20"); setParam (c, "Strum Burst", "1/8"); setParam (c, "Strum Direction", "Guitar"); },
                           (int64_t) (4 * samplesPerBeat) + 4800, true);
    std::set<int64_t> strokes;
    for (auto& e : onsOf (burst)) strokes.insert ((int64_t) std::llround ((double) e.time / (samplesPerBeat / 2.0)));
    juce::String report;
    check (strokes.size() >= 8 && balanced (burst, report), "Burst 1/8: re-strums the held chord on every eighth, balanced " + report);
}
} // namespace

void testGlideCat()
{
    std::cout << "\nGlide-Cat: voice-leading poly glide synth" << std::endl;
    auto glide = loadPlugin (GLIDECAT_VST3);
    if (glide == nullptr) { ++failures; return; }
    TestPlayHead ph;
    prepare (*glide, ph);
    check (glide->getTotalNumOutputChannels() == 2, "stereo instrument output");

    // A plain sine voice with no expression / effects, so the pitch is easy to measure
    for (auto [n, v] : { std::pair { "Layer II On", "0" }, { "Layer I Saw", "0" }, { "Layer I Pulse", "0" }, { "Layer I Sine", "1" },
                         { "Layer I Noise", "0" }, { "Layer I Unison", "1" }, { "Layer I Detune", "0" }, { "Layer I Fine", "0" },
                         { "Layer I LP Cutoff", "18000" }, { "Layer I Filter EG Depth", "0" }, { "Layer I Attack", "0.005" },
                         { "Layer I Release", "0.2" }, { "Swell", "0" }, { "Brighten", "0" }, { "Drift", "0" }, { "Timbre Wander", "0" },
                         { "Vibrato Depth", "0" }, { "Ensemble Mix", "0" }, { "Prism Mix", "0" }, { "Room Mix", "0" },
                         { "Glide Mode", "Time" }, { "Glide Time", "300" }, { "Glide Curve", "Linear" }, { "Glide Spread", "0" },
                         { "Glide Path", "Direct" }, { "Stagger", "0" }, { "Release Bend", "0" }, { "Autopilot", "Off" } })
        setParam (*glide, n, v);

    const int64_t sec = (int64_t) sampleRate;
    // A3 for 1 s, then A4: the voice glides up an octave over 300 ms instead of restarting
    const auto r = renderAudio (*glide, { { 0, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100) },
                                          { sec, juce::MidiMessage::noteOff (1, 57) },
                                          { sec, juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100) },
                                          { 2 * sec, juce::MidiMessage::noteOff (1, 69) } }, 3 * sec);
    const double fA = pitchOf (r.mono, sec / 2, 4800), fMid = pitchOf (r.mono, sec + sec * 13 / 100, 1600), fB = pitchOf (r.mono, sec * 17 / 10, 4800);
    std::cout << "  A3 " << juce::String (fA, 1) << " Hz -> mid-glide " << juce::String (fMid, 1) << " Hz -> A4 " << juce::String (fB, 1)
              << " Hz, level " << juce::String (rms (r.mono, sec / 2, sec), 3) << std::endl;
    check (r.finite, "no NaN / Inf in the output");
    check (rms (r.mono, sec / 2, sec) > 0.02f, "the voice is clearly audible");
    check (std::abs (1200.0 * std::log2 (fA / 220.0)) < 10.0, "plays A3 in tune");
    check (std::abs (1200.0 * std::log2 (fB / 440.0)) < 10.0, "arrives at A4 in tune");
    check (fMid > 250.0 && fMid < 400.0, "is in between half-way through the glide (it glides, it does not jump)");
    check (rms (r.mono, sec + sec / 10, sec + sec / 5) > 0.5f * rms (r.mono, sec / 2, sec), "no gap while gliding to the next note");
    check (rms (r.mono, (int64_t) (2.8 * (double) sec), 3 * sec) < 1.0e-3f, "falls silent after the release");
    check (r.midiOut == 0, "no MIDI output (it is an instrument)");

    // Chord to chord: C-E-G-B -> F-A-C-E, all voices glide, no clicks, stays below clipping
    setParam (*glide, "Layer II On", "1");
    setParam (*glide, "Layer I Saw", "0.8");
    setParam (*glide, "Layer I Unison", "3");
    setParam (*glide, "Room Mix", "0.3");
    std::vector<Event> chords;
    for (int n : { 48, 60, 64, 67, 71 }) chords.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 110) });
    for (int n : { 48, 60, 64, 67, 71 }) chords.push_back ({ sec, juce::MidiMessage::noteOff (1, n) });
    for (int n : { 53, 65, 69, 72, 76 }) chords.push_back ({ sec + 64, juce::MidiMessage::noteOn (1, n, (juce::uint8) 110) });
    for (int n : { 53, 65, 69, 72, 76 }) chords.push_back ({ 2 * sec, juce::MidiMessage::noteOff (1, n) });
    const auto c = renderAudio (*glide, chords, 3 * sec);
    auto maxStepIn = [&] (int64_t from, int64_t to)
    {
        float m = 0.0f;
        for (auto i = (size_t) from; i < (size_t) to; ++i) m = juce::jmax (m, std::abs (c.mono[i + 1] - c.mono[i]));
        return m;
    };
    const float steady = maxStepIn (sec / 2, sec - 2000), maxStep = maxStepIn (sec - 2000, sec + sec / 2);
    std::cout << "  chord change: peak " << juce::String (c.peak, 2) << ", largest sample step: held chord " << juce::String (steady, 3) << ", around the change " << juce::String (maxStep, 3)
              << ", level " << juce::String (rms (c.mono, sec / 2, sec), 3) << " -> " << juce::String (rms (c.mono, sec + sec / 4, 2 * sec), 3) << std::endl;
    check (c.finite && c.peak < 1.0f, "5-note chord stays below clipping");
    check (maxStep < 1.5f * steady + 0.02f, "no click when the chord changes (no bigger jumps than the held chord)");
    check (rms (c.mono, sec + sec / 4, 2 * sec) > 0.5f * rms (c.mono, sec / 2, sec), "the chord keeps sounding through the change (voices glide, not retrigger)");

    // Autopilot moves a held note by itself
    setParam (*glide, "Layer II On", "0");
    setParam (*glide, "Layer I Saw", "0");
    setParam (*glide, "Layer I Unison", "1");
    setParam (*glide, "Room Mix", "0");
    setParam (*glide, "Autopilot", "Octave Leaps");
    setParam (*glide, "Pilot Sync", "Free (Hz)");
    setParam (*glide, "Pilot Rate", "4");
    setParam (*glide, "Pilot Density", "1");
    const auto ap = renderAudio (*glide, { { 0, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100) }, { 3 * sec, juce::MidiMessage::noteOff (1, 57) } }, 3 * sec);
    int moved = 0;
    for (int k = 2; k < 28; ++k)
        if (std::abs (1200.0 * std::log2 (pitchOf (ap.mono, sec * k / 10, 2400) / 220.0)) > 200.0) ++moved;
    std::cout << "  autopilot (Octave Leaps, 4 Hz): pitch away from A3 in " << moved << " of 26 windows" << std::endl;
    check (moved >= 3 && moved <= 23, "Autopilot moves a held note away and back");
    setParam (*glide, "Autopilot", "Off");

    // CPU: 16 voices, both layers, all effects
    setParam (*glide, "Voices", "16");
    setParam (*glide, "Layer II On", "1");
    setParam (*glide, "Layer I Saw", "0.8");
    setParam (*glide, "Layer I Unison", "3");
    setParam (*glide, "Layer II Unison", "3");
    setParam (*glide, "Ensemble Mix", "0.5");
    setParam (*glide, "Prism Mix", "0.3");
    setParam (*glide, "Room Mix", "0.3");
    std::vector<Event> big;
    for (int n = 0; n < 16; ++n) big.push_back ({ 0, juce::MidiMessage::noteOn (1, 40 + n * 3, (juce::uint8) 100) });
    const auto b = renderAudio (*glide, big, 4 * sec);
    std::cout << "  16 voices x 2 layers x 3 saws + FX: " << juce::String (b.cpu * 100.0, 1) << " % of one core, peak " << juce::String (b.peak, 2) << std::endl;
    check (b.finite && b.peak <= 1.0f, "16-voice chord stays finite and below clipping");
    check (b.cpu < 0.6, "16 voices run comfortably in real time");

    // Glide preset changes only the glide, sound preset only the sound
    juce::MemoryBlock state, state2;
    glide->getStateInformation (state);
    auto other = loadPlugin (GLIDECAT_VST3);
    other->setStateInformation (state.getData(), (int) state.getSize());
    other->getStateInformation (state2);
    check (state == state2 && state.getSize() > 100, "state save / restore round-trip (" + juce::String ((int) state.getSize()) + " bytes)");
}

void testTalkCatSinger()
{
    std::cout << "\nTalk-Cat: singer with lyrics (piano roll + live)" << std::endl;
    auto talk = loadPlugin (TALKCAT_VST3);
    if (talk == nullptr) { ++failures; return; }
    TestPlayHead ph;
    prepare (*talk, ph);
    check (talk->getTotalNumOutputChannels() == 2, "stereo instrument output");
    for (auto [n, v] : { std::pair { "Vibrato Depth", "0" }, { "Pitch Drift", "0" }, { "Reverb Mix", "0" }, { "Scoop", "0" }, { "Overshoot", "0" },
                         { "Delay Mix", "0" } })
        setParam (*talk, n, v);

    // Score: the built-in demo song (Twinkle, C C G G A A G ...) plays with the Cubase transport (120 BPM = 0.5 s per beat)
    const int64_t sec = (int64_t) sampleRate;
    juce::AudioBuffer<float> audio (2, blockSize);
    std::vector<float> mono;
    double cpuStart = juce::Time::getMillisecondCounterHiRes();
    const int64_t length = 6 * sec;
    for (int64_t t = 0; t < length; t += blockSize)
    {
        ph.playing = t < 4 * sec;
        ph.ppq = (double) t / samplesPerBeat;
        audio.clear();
        juce::MidiBuffer midi;
        talk->processBlock (audio, midi);
        for (int i = 0; i < blockSize; ++i) mono.push_back (0.5f * (audio.getSample (0, i) + audio.getSample (1, i)));
    }
    const double cpu = (juce::Time::getMillisecondCounterHiRes() - cpuStart) / 1000.0 / 6.0;
    bool finite = true;
    for (auto v : mono) finite = finite && std::isfinite (v);
    auto at = [&] (double beat) { return (int64_t) (beat * samplesPerBeat); };
    const double fC = pitchOf (mono, at (0.45), 4800), fG = pitchOf (mono, at (2.45), 4800), fA = pitchOf (mono, at (4.45), 4800);
    std::cout << "  score: beat 1 " << juce::String (fC, 1) << " Hz (C4 261.6), beat 3 " << juce::String (fG, 1) << " Hz (G4 392), beat 5 "
              << juce::String (fA, 1) << " Hz (A4 440), level " << juce::String (rms (mono, at (2.2), at (2.8)), 3) << ", " << juce::String (cpu * 100.0, 1) << " % CPU" << std::endl;
    check (finite, "no NaN / Inf in the output");
    check (rms (mono, at (2.2), at (2.8)) > 0.02f, "the score is sung when Cubase plays");
    check (std::abs (1200.0 * std::log2 (fG / 392.0)) < 20.0 && std::abs (1200.0 * std::log2 (fA / 440.0)) < 20.0,
           "notes are sung at the right pitch, in time with the host");
    check (rms (mono, (int64_t) (5.3 * (double) sec), 6 * sec) < 1.0e-3f, "falls silent when the transport stops");

    // Live: MIDI notes sing the live lyrics
    setParam (*talk, "Play Mode", "Live MIDI");
    ph.playing = false;
    const auto live = renderAudio (*talk, { { 0, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100) }, { sec, juce::MidiMessage::noteOff (1, 57) } }, 2 * sec);
    const double fLive = pitchOf (live.mono, sec / 2, 4800);
    std::cout << "  live A3: " << juce::String (fLive, 1) << " Hz, level " << juce::String (rms (live.mono, sec / 4, sec), 3) << std::endl;
    check (rms (live.mono, sec / 4, sec) > 0.02f && std::abs (1200.0 * std::log2 (fLive / 220.0)) < 20.0, "live MIDI note sings in tune");
    check (rms (live.mono, (int64_t) (1.8 * (double) sec), 2 * sec) < 1.0e-3f, "silent after the live note ends");
    check (live.midiOut == 0, "no MIDI output (it is an instrument)");

    // Every language + every voice preset stays finite and below clipping
    bool allOk = true;
    float worst = 0.0f;
    for (auto lang : { "English", "Slovak", "Japanese", "Cat" })
    {
        setParam (*talk, "Language", lang);
        for (int pr = 0; pr < talk->getNumPrograms(); pr += 3)
        {
            talk->setCurrentProgram (pr);
            setParam (*talk, "Play Mode", "Live MIDI");
            const auto r = renderAudio (*talk, { { 0, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 110) }, { sec / 2, juce::MidiMessage::noteOff (1, 64) } }, sec);
            allOk = allOk && r.finite && r.peak <= 1.0f;
            worst = juce::jmax (worst, r.peak);
        }
    }
    check (allOk, "all languages and voice presets are finite and below clipping (peak " + juce::String (worst, 2) + ")");

    // CPU: double + 3 harmonies on the score
    setParam (*talk, "Play Mode", "Score + Live");
    for (auto [n, v] : { std::pair { "Harmony 1", "3rd above" }, { "Harmony 2", "5th below" }, { "Harmony 3", "Octave below" }, { "Double", "0.8" } })
        setParam (*talk, n, v);
    double start = juce::Time::getMillisecondCounterHiRes();
    for (int64_t t = 0; t < 4 * sec; t += blockSize)
    {
        ph.playing = true;
        ph.ppq = (double) t / samplesPerBeat;
        audio.clear();
        juce::MidiBuffer midi;
        talk->processBlock (audio, midi);
    }
    const double cpu5 = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0 / 4.0;
    std::cout << "  5 voices (lead + double + 3 harmonies): " << juce::String (cpu5 * 100.0, 1) << " % of one core" << std::endl;
    check (cpu5 < 0.5, "5 singing voices run comfortably in real time");

    juce::MemoryBlock state, state2;
    talk->getStateInformation (state);
    auto other = loadPlugin (TALKCAT_VST3);
    other->setStateInformation (state.getData(), (int) state.getSize());
    other->getStateInformation (state2);
    check (state == state2 && state.getSize() > 500, "state (incl. the score) save / restore round-trip (" + juce::String ((int) state.getSize()) + " bytes)");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    testChordCatStandalone (CHORDCAT_VST3);
    testChain (CHORDCAT_VST3, PATTCAT_VST3, BENDCAT_VST3);
    testRegressions (CHORDCAT_VST3, PATTCAT_VST3, BENDCAT_VST3);
    testPattCatBounce (PATTCAT_VST3);
    testDrumCat (DRUMCAT_VST3);
    testOrbitCat (ORBITCAT_VST3);
    testSingCat (SINGCAT_VST3);
    testPresetPrograms();
    testCatLink();
    testBassCat();
    testChordCatStrum();
    testGlideCat();
    testTalkCatSinger();

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " TEST(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
