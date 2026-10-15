#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace basscat;

juce::AudioProcessorValueTreeState::ParameterLayout BassCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                           AudioParameterFloatAttributes().withLabel ("%")));
    };
    auto boolean = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", getStyleNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Length", StringArray { "1 Bar", "2 Bars", "4 Bars" }, 1));
    pct ("density", "Density", 50.0f);
    pct ("fills", "Fills", 35.0f);
    pct ("ghosts", "Ghost Notes", 30.0f);
    pct ("slides", "Slides", 30.0f);

    pct ("swing", "Swing", 0.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "human", 1 }, "Human", NormalisableRange<float> (0.0f, 30.0f, 0.1f), 3.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    boolean ("followGroove", "Follow Drum Groove", false);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate", NormalisableRange<float> (10.0f, 150.0f, 1.0f), 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "velocity", 1 }, "Velocity", NormalisableRange<float> (10.0f, 150.0f, 1.0f), 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    pct ("velRandom", "Velocity Random", 5.0f);
    pct ("chance", "Chance", 100.0f);

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "lowNote", 1 }, "Low Note", 16, 52, 28));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "outChannel", 1 }, "Output Channel", 1, 16, 1));
    boolean ("latch", "Latch", false);
    boolean ("thru", "Thru", false);
    boolean ("keyTrigger", "Key Trigger", true);
    return layout;
}

BassCatProcessor::BassCatProcessor()
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
        caps.receiveChordsText = "Follow Chord-Cat's chords";
        caps.grooveParam = "followGroove";
        presetManager.enableLink (apvts, caps);
    }
    scheduler.monitor = &noteMonitor;
    held.reserve (128);
    seedBase = 0xba55;
    pattern = generatePattern (currentSettings());

    // Any change to the generator settings (UI, automation, presets, chains) writes a new line
    for (auto* id : { "style", "bars", "density", "fills", "ghosts", "slides" })
        apvts.addParameterListener (id, this);
    startTimerHz (10);

    cat::followDrumGroove (presetManager, apvts, [this] (const cat::CatLink::Groove& g)
    {
        cat::SurpriseControls::setParam (*this, "swing", g.swing);
        cat::SurpriseControls::setParam (*this, "human", g.human);
    });
}

BassCatProcessor::~BassCatProcessor()
{
    stopTimer();
    for (auto* id : { "style", "bars", "density", "fills", "ghosts", "slides" })
        apvts.removeParameterListener (id, this);
}

bool BassCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void BassCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSync.prepare (sampleRate);
    scheduler.prepare (8192);
    outBuffer.ensureSize ((size_t) juce::jmax (4096, samplesPerBlock * 4));
    sampleCounter = 0;
    held.clear();
    lastNote = -1;
}

float BassCatProcessor::hash (int64_t loop, int index, int salt) const
{
    uint64_t x = (uint64_t) seedBase.load() * 0x9E3779B97F4A7C15ull ^ (uint64_t) loop * 0xBF58476D1CE4E5B9ull
               ^ (uint64_t) (index + 1) * 0x94D049BB133111EBull ^ (uint64_t) (salt + 7) * 0xD6E8FEB86659FD93ull;
    x ^= x >> 31; x *= 0x9E3779B97F4A7C15ull; x ^= x >> 29;
    return (float) ((double) (x & 0xFFFFFF) / (double) 0xFFFFFF);
}

void BassCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    presetManager.linkClient->processInput (midi, buffer.getNumSamples());     // Cat Link: Chord-Cat's chord
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    hostSync.beginBlock (presetManager.linkClient->playHead (getPlayHead(), numSamples, getSampleRate()), numSamples);   // Cat Link: shared tempo in the apps
    lastBpm = hostSync.get().bpm;
    midiLearn.processMidi (midi);
    const bool keyTrigger = apvts.getRawParameterValue ("keyTrigger")->load() > 0.5f;
    const bool latch = apvts.getRawParameterValue ("latch")->load() > 0.5f;
    const bool thru = apvts.getRawParameterValue ("thru")->load() > 0.5f;

    int lastPos = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
        render (lastPos, pos);
        const auto m = meta.getMessage();

        if (m.isNoteOn())
        {
            // A new chord after all keys were up replaces the latched one
            if (held.empty() && hostSync.isRunning() == false && keyTrigger)
                hostSync.startFreeRunAt (pos);
            held.push_back (m.getNoteNumber());
            chord = analyseChord (held);
            if (thru) scheduler.schedule (sampleCounter + pos, m);
        }
        else if (m.isNoteOff())
        {
            held.erase (std::remove (held.begin(), held.end(), m.getNoteNumber()), held.end());
            if (! held.empty())
                chord = analyseChord (held);
            else if (! latch)
            {
                chord = {};
                if (hostSync.isFreeRunning()) hostSync.stopFreeRun();
            }
            if (thru) scheduler.schedule (sampleCounter + pos, m);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            chord = {};
            scheduler.allNotesOff (outBuffer, pos);
            scheduler.schedule (sampleCounter + pos, m);
        }
        else
        {
            scheduler.schedule (sampleCounter + pos, m);
        }
        lastPos = pos;
    }
    render (lastPos, numSamples);

    outBuffer.clear();
    scheduler.emit (outBuffer, sampleCounter, numSamples);
    midi.swapWith (outBuffer);

    uiChordRoot = chord.valid ? chord.rootPc : -1;
    uiChordQuality = chord.third;
    uiChordSeventh = chord.seventh;
    uiPpq = hostSync.ppqAt (0);
    uiTransportRunning = hostSync.isRunning();
    hostSync.endBlock();
    sampleCounter += numSamples;
}

void BassCatProcessor::render (int fromSample, int toSample)
{
    if (toSample <= fromSample || ! hostSync.isRunning() || ! chord.valid)
        return;

    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float swing = f ("swing") / 100.0f;
    const double humanPpq = f ("human") / 60000.0 * hostSync.get().bpm;
    const float gate = f ("gate") / 100.0f;
    const float velScale = f ("velocity") / 100.0f;
    const float velRandom = f ("velRandom") / 100.0f;
    const float chance = f ("chance") / 100.0f;
    const int lowNote = juce::roundToInt (f ("lowNote"));
    const int octave = juce::roundToInt (f ("octave"));
    const int channel = juce::roundToInt (f ("outChannel"));

    const double t0 = hostSync.ppqAt (fromSample), t1 = hostSync.ppqAt (toSample);
    const double spp = hostSync.samplesPerPpq();

    const juce::SpinLock::ScopedTryLockType lock (patternLock);
    if (! lock.isLocked() || pattern.notes.empty())
        return;

    const double L = pattern.lengthPpq();
    const double margin = 0.3 + humanPpq;

    for (int64_t k = (int64_t) std::floor ((t0 - margin) / L); k <= (int64_t) std::floor ((t1 + margin) / L); ++k)
    {
        for (int i = 0; i < (int) pattern.notes.size(); ++i)
        {
            const auto& n = pattern.notes[(size_t) i];
            double t = k * L + n.pos + n.micro;

            // Swing: off-beat 16ths move later (same as Drum-Cat / Patt-Cat)
            const double inGrid = n.pos / 0.25;
            if (std::abs (inGrid - std::round (inGrid)) < 1.0e-6 && ((int64_t) std::round (inGrid)) % 2 == 1)
                t += swing * 0.5 * 0.25;
            t += (hash (k, i, 1) * 2.0f - 1.0f) * humanPpq;
            if (k == 0 && t < 0.0 && n.pos >= 0.0) t = 0.0;
            if (t < t0 || t >= t1)
                continue;
            if (n.degree != Root && hash (k, i, 2) >= chance)   // roots always play: the line keeps its anchor
                continue;

            const int note = resolveDegree (n.degree, n.octave + octave, chord, lowNote, lastNote);
            const float v = (float) n.velocity * velScale * (1.0f + (hash (k, i, 4) * 2.0f - 1.0f) * velRandom);
            const int vel = juce::jlimit (1, 127, juce::roundToInt (v));
            const double len = n.slide ? n.len : n.len * gate;   // slides always reach the next note
            const int64_t on = sampleCounter + (int64_t) std::llround (hostSync.sampleForPpq (t));
            const int64_t off = on + juce::jmax ((int64_t) 1, (int64_t) (len * spp));

            // Monophonic: a new note ends the previous one, unless it slides into it
            if (lastNote >= 0 && lastNote != note)
                scheduler.releaseNote (channel, lastNote, n.slide ? on + (int64_t) (0.02 * spp) : on);
            scheduler.releaseNote (channel, note, on);
            scheduler.scheduleNote (on, off, channel, note, vel);
            if (n.degree != Ghost) lastNote = note;
        }
    }
}

//==============================================================================
GeneratorSettings BassCatProcessor::currentSettings() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    static const int bars[] = { 1, 2, 4 };
    GeneratorSettings s;
    s.style = juce::roundToInt (f ("style"));
    s.bars = bars[juce::jlimit (0, 2, juce::roundToInt (f ("bars")))];
    s.density = f ("density") / 100.0f;
    s.fills = f ("fills") / 100.0f;
    s.ghosts = f ("ghosts") / 100.0f;
    s.slides = f ("slides") / 100.0f;
    s.seed = seedBase.load() * 131 + variation.load() * 7919 + s.style * 104729;
    return s;
}

Pattern BassCatProcessor::getPattern() const
{
    const juce::SpinLock::ScopedLockType lock (patternLock);
    return pattern;
}

void BassCatProcessor::setPattern (const Pattern& p)
{
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        pattern = p;
    }
    patternVersion.fetch_add (1);
}

void BassCatProcessor::generate (bool newSeed)
{
    if (newSeed)
    {
        seedBase = juce::Random::getSystemRandom().nextInt64() & 0x7fffffff;
        variation = 1;
    }
    setPattern (generatePattern (currentSettings()));
}

void BassCatProcessor::setVariation (int index)
{
    variation = juce::jlimit (1, numVariations, index);
    generate (false);
}

juce::String BassCatProcessor::getChordText() const
{
    const int root = uiChordRoot.load();
    if (root < 0) return "-";
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String s (names[root]);
    const int third = uiChordQuality.load(), seventh = uiChordSeventh.load();
    if (third == 3) s << "m";
    else if (third == 5) s << "sus4";
    else if (third == 2) s << "sus2";
    if (seventh == 10) s << "7";
    else if (seventh == 11) s << "maj7";
    return s;
}

juce::MidiMessageSequence BassCatProcessor::buildPatternSequence() const
{
    // Export against the chord being held (C minor 7 when nothing is held)
    auto c = chord;
    if (! c.valid) c = analyseChord ({ 48, 51, 55, 58 });
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const int lowNote = juce::roundToInt (f ("lowNote")), octave = juce::roundToInt (f ("octave"));
    const int channel = juce::roundToInt (f ("outChannel"));
    const float gate = f ("gate") / 100.0f, swing = f ("swing") / 100.0f;

    juce::MidiMessageSequence seq;
    int prev = -1;
    for (const auto& n : getPattern().notes)
    {
        double t = n.pos + n.micro;
        const double inGrid = n.pos / 0.25;
        if (std::abs (inGrid - std::round (inGrid)) < 1.0e-6 && ((int64_t) std::round (inGrid)) % 2 == 1)
            t += swing * 0.5 * 0.25;
        t = juce::jmax (0.0, t);
        const int note = resolveDegree (n.degree, n.octave + octave, c, lowNote, prev);
        if (n.degree != Ghost) prev = note;
        const double len = n.slide ? n.len : n.len * gate;
        seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, n.velocity)), t);
        seq.addEvent (juce::MidiMessage::noteOff (channel, note), t + len);
    }
    seq.updateMatchedPairs();
    return seq;
}

//==============================================================================
const std::vector<BassCatProcessor::Preset>& BassCatProcessor::presets()
{
    // style: 0 Pop Roots, 1 Walking, 2 Neo Soul, 3 Funk, 4 Boom Bap, 5 Dilla, 6 808, 7 Reggae, 8 House, 9 Disco Octaves, 10 Latin, 11 Drone, 12 Rock .. 17 Synth Arp
    // bars: 0 = 1, 1 = 2, 2 = 4
    static const std::vector<Preset> list = {
        { "Pop Root Eighths", 11,  { { "style", 0 }, { "density", 50 }, { "fills", 30 }, { "gate", 80 } } },
        { "Ballad Roots",     12,  { { "style", 0 }, { "density", 20 }, { "fills", 15 }, { "gate", 120 }, { "velocity", 85 } } },
        { "Stadium Pulse",    13,  { { "style", 9 }, { "density", 60 }, { "fills", 20 }, { "gate", 70 } } },
        { "Jazz Walking",     21,  { { "style", 1 }, { "bars", 2 }, { "fills", 0 }, { "swing", 35 }, { "human", 5 }, { "ghosts", 25 } } },
        { "Gospel Walk",      22,  { { "style", 1 }, { "bars", 2 }, { "swing", 20 }, { "human", 4 }, { "ghosts", 35 }, { "velocity", 110 } } },
        { "Motown Walk",      23,  { { "style", 1 }, { "density", 70 }, { "swing", 0 }, { "human", 3 } } },
        { "Neo Soul Walk",    31,  { { "style", 2 }, { "density", 55 }, { "ghosts", 45 }, { "slides", 45 }, { "swing", 15 }, { "human", 6 } } },
        { "Ghostly Neo",      32,  { { "style", 2 }, { "density", 40 }, { "ghosts", 70 }, { "slides", 30 }, { "human", 7 }, { "velocity", 90 } } },
        { "Funk Pocket",      41,  { { "style", 3 }, { "density", 55 }, { "ghosts", 50 }, { "human", 3 } } },
        { "Busy Funk",        42,  { { "style", 3 }, { "density", 85 }, { "ghosts", 70 }, { "fills", 50 }, { "human", 2 } } },
        { "Boom Bap Classic", 51,  { { "style", 4 }, { "density", 45 }, { "fills", 30 }, { "swing", 15 }, { "human", 4 } } },
        { "Lo-Fi Thump",      52,  { { "style", 4 }, { "density", 35 }, { "fills", 20 }, { "swing", 30 }, { "human", 7 }, { "velocity", 85 }, { "gate", 110 } } },
        { "Dilla Bounce",     61,  { { "style", 5 }, { "density", 55 }, { "ghosts", 40 }, { "human", 9 } } },
        { "808 Glide",        71,  { { "style", 6 }, { "density", 50 }, { "slides", 70 }, { "lowNote", 24 }, { "human", 0 } } },
        { "Trap Bounce",      72,  { { "style", 6 }, { "density", 75 }, { "slides", 50 }, { "lowNote", 24 }, { "human", 0 } } },
        { "Half-Time Sub",    73,  { { "style", 6 }, { "density", 20 }, { "slides", 30 }, { "lowNote", 24 }, { "fills", 10 } } },
        { "Reggae One Drop",  81,  { { "style", 7 }, { "density", 55 }, { "fills", 25 }, { "human", 4 } } },
        { "Deep House",       91,  { { "style", 8 }, { "density", 45 }, { "gate", 60 }, { "human", 1 } } },
        { "Detroit Groove",   92,  { { "style", 8 }, { "density", 70 }, { "swing", 20 }, { "ghosts", 30 }, { "human", 3 } } },
        { "Disco Octaves",    101, { { "style", 9 }, { "density", 50 }, { "gate", 55 } } },
        { "Synthwave Eighths", 102, { { "style", 0 }, { "density", 25 }, { "fills", 10 }, { "gate", 75 }, { "human", 0 } } },
        { "Latin Tumbao",     111, { { "style", 10 }, { "density", 50 }, { "fills", 25 } } },
        { "Cinematic Drone",  121, { { "style", 11 }, { "bars", 2 }, { "slides", 50 }, { "octave", -1 }, { "velocity", 90 } } },
        { "Sparse Sub",       122, { { "style", 11 }, { "density", 10 }, { "octave", -1 }, { "slides", 0 } } },
        // 12 Rock, 13 Dub, 14 Country, 15 Afrobeats, 16 Drill, 17 Synth Arp
        { "Rock Eighths",     131, { { "style", 12 }, { "density", 70 }, { "fills", 30 }, { "gate", 85 } } },
        { "Punk Drive",       132, { { "style", 12 }, { "density", 90 }, { "fills", 10 }, { "gate", 70 }, { "velocity", 115 }, { "human", 1 } } },
        { "Dub Pressure",     141, { { "style", 13 }, { "density", 45 }, { "slides", 30 }, { "octave", -1 }, { "human", 5 } } },
        { "Steppers",         142, { { "style", 13 }, { "density", 65 }, { "fills", 20 }, { "human", 3 } } },
        { "Country Two-Step", 151, { { "style", 14 }, { "density", 55 }, { "fills", 40 }, { "human", 3 } } },
        { "Afro Groove",      161, { { "style", 15 }, { "density", 60 }, { "ghosts", 30 }, { "swing", 15 }, { "human", 4 } } },
        { "Amapiano Log",     162, { { "style", 15 }, { "density", 40 }, { "slides", 40 }, { "lowNote", 28 }, { "human", 2 } } },
        { "Drill Slides",     171, { { "style", 16 }, { "density", 60 }, { "slides", 75 }, { "lowNote", 24 }, { "human", 0 } } },
        { "Dark Drill",       172, { { "style", 16 }, { "density", 40 }, { "slides", 60 }, { "lowNote", 24 }, { "fills", 15 } } },
        { "Synth Arp",        181, { { "style", 17 }, { "density", 100 }, { "gate", 60 }, { "human", 0 } } },
        { "Arcade Arp",       182, { { "style", 17 }, { "density", 100 }, { "gate", 40 }, { "octave", 1 }, { "human", 0 } } },
        { "Night Drive Arp",  183, { { "style", 17 }, { "density", 100 }, { "gate", 70 }, { "slides", 20 }, { "human", 0 } } },
    };
    return list;
}

void BassCatProcessor::applyPreset (int index)
{
    const auto& list = presets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    currentPreset = index;
    const auto& preset = list[(size_t) index];

    for (auto* id : { "style", "bars", "density", "fills", "ghosts", "slides", "swing", "human", "gate", "velocity", "velRandom", "chance", "lowNote", "octave" })
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getDefaultValue());
            prm->endChangeGesture();
        }
    for (const auto& [id, v] : preset.values)
        cat::SurpriseControls::setParam (*this, id, v);

    seedBase = preset.seed;
    variation = 1;
    generate (false);
    regenerate = false;
}

void BassCatProcessor::surprise()
{
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    const int style = r.nextInt (NumStyles);
    set ("style", (float) style);
    set ("bars", (float) (r.nextFloat() < 0.6f ? 1 : (r.nextBool() ? 0 : 2)));
    set ("density", 25.0f + r.nextFloat() * 60.0f);
    set ("fills", r.nextFloat() * 60.0f);
    set ("ghosts", r.nextFloat() * 70.0f);
    set ("slides", r.nextFloat() * 70.0f);
    set ("swing", getStyleHints (style).swing + (r.nextFloat() < 0.7f ? 0.0f : r.nextFloat() * 25.0f));
    set ("human", getStyleHints (style).human + r.nextFloat() * 3.0f);
    set ("gate", 60.0f + r.nextFloat() * 60.0f);
    generate (true);
}

//==============================================================================
void BassCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("BASSCAT");
    root.setProperty ("seed", juce::String (seedBase.load()), nullptr);
    root.setProperty ("variation", variation.load(), nullptr);
    root.setProperty ("preset", currentPreset.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (getPattern().toValueTree(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void BassCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("BASSCAT")) return;

    seedBase = root.getProperty ("seed", "1").toString().getLargeIntValue();
    variation = juce::jlimit (1, numVariations, (int) root.getProperty ("variation", 1));
    currentPreset = (int) root.getProperty ("preset", -1);
    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);
    if (auto p = root.getChildWithName (Pattern::treeId); p.isValid())
        setPattern (Pattern::fromValueTree (p));
    regenerate = false;    // keep the saved (possibly hand-edited) line
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
}

juce::AudioProcessorEditor* BassCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<BassCatEditor> (*this), "Bass-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassCatProcessor();
}
