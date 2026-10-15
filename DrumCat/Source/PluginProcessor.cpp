#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"
#include <cmath>

using namespace drumcat;

namespace
{
    juce::String muteId (int lane) { return "mute" + getLaneNames()[lane].removeCharacters (" "); }
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                           AudioParameterFloatAttributes().withLabel ("%")));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", getStyleNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Length", StringArray { "1 Bar", "2 Bars", "4 Bars" }, 1));
    pct ("density", "Density", 50.0f);
    pct ("fills", "Fills", 35.0f);

    pct ("swing", "Swing", 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "grid", 1 }, "Grid", getGridNames(), 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "human", 1 }, "Human", NormalisableRange<float> (0.0f, 30.0f, 0.1f), 3.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "velocity", 1 }, "Velocity", NormalisableRange<float> (10.0f, 150.0f, 1.0f), 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    pct ("velRandom", "Velocity Random", 0.0f);
    pct ("chance", "Chance", 100.0f);
    pct ("open", "Open Hats", 0.0f);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "map", 1 }, "MIDI Map", getMapNames(), 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "outChannel", 1 }, "Output Channel", 1, 16, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "keyTrigger", 1 }, "Key Trigger", true));

    for (int l = 0; l < NumLanes; ++l)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { muteId (l), 1 }, "Mute " + getLaneNames()[l], false));

    return layout;
}

DrumCatProcessor::DrumCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                      ),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    {
        cat::LinkClient::Caps caps;
        caps.receiveLife = true;
        caps.receiveLifeText = "Answer Life-Cat with hits";
        presetManager.enableLink (apvts, caps);
    }
    scheduler.monitor = &noteMonitor;
    seedBase = 0x5eed;
    pattern = generatePattern (currentSettings());
    for (auto& h : laneHits) h.store (0);

    // Cat Link: publish the groove (swing, human timing, grid) whenever it changes
    presetManager.link.onTick = [this, last = std::make_shared<juce::String>()]
    {
        auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
        const int style = juce::roundToInt (f ("style"));
        const float swing = juce::jlimit (0.0f, 100.0f, f ("swing") + getStyleHints (style).swing * 100.0f);
        const float human = f ("human");
        const int grid = juce::roundToInt (f ("grid"));
        const auto key = juce::String (swing, 1) + "/" + juce::String (human, 1) + "/" + juce::String (grid);
        if (key != *last)
        {
            *last = key;
            presetManager.link.publishGroove (swing, human, grid);
        }
    };
}

DrumCatProcessor::~DrumCatProcessor() = default;

bool DrumCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DrumCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSync.prepare (sampleRate);
    scheduler.prepare (8192);
    outBuffer.ensureSize ((size_t) juce::jmax (4096, samplesPerBlock * 4));
    sampleCounter = 0;
    heldKeys = 0;
}

//==============================================================================
float DrumCatProcessor::hash (int64_t loop, int index, int salt) const
{
    // Deterministic per loop / hit: the same pass sounds the same if you scrub back to it
    uint64_t x = (uint64_t) seedBase.load() * 0x9E3779B97F4A7C15ull ^ (uint64_t) loop * 0xBF58476D1CE4E5B9ull
               ^ (uint64_t) (index + 1) * 0x94D049BB133111EBull ^ (uint64_t) (salt + 7) * 0xD6E8FEB86659FD93ull;
    x ^= x >> 31; x *= 0x9E3779B97F4A7C15ull; x ^= x >> 29;
    return (float) ((double) (x & 0xFFFFFF) / (double) 0xFFFFFF);
}

void DrumCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    hostSync.beginBlock (getPlayHead(), numSamples);
    lastBpm = hostSync.get().bpm;
    midiLearn.processMidi (midi);
    const bool keyTrigger = apvts.getRawParameterValue ("keyTrigger")->load() > 0.5f;

    int lastPos = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
        render (lastPos, pos);
        const auto m = meta.getMessage();

        if (m.isNoteOn())
        {
            // Holding a key plays the pattern from its start while the transport is stopped
            if (heldKeys++ == 0 && keyTrigger && ! hostSync.isRunning())
                hostSync.startFreeRunAt (pos);
        }
        else if (m.isNoteOff())
        {
            heldKeys = juce::jmax (0, heldKeys - 1);
            if (heldKeys == 0 && hostSync.isFreeRunning())
                hostSync.stopFreeRun();
        }
        else
        {
            scheduler.schedule (sampleCounter + pos, m);   // CC / pitch-bend etc. pass through
        }
        lastPos = pos;
    }
    render (lastPos, numSamples);

    // Cat Link: births and deaths in Life-Cat answered with hits on the next 16th
    presetManager.linkClient->pollLifeEvents ([this] (int kind, int role)
    {
        if (! hostSync.isRunning()) return;
        const double t = std::ceil (hostSync.ppqAt (0) * 4.0 + 1.0e-6) / 4.0;
        int lane = kind == 0 ? Perc : (kind == 2 ? Crash : Tom);
        if (role == 5 && kind != 0) lane = Crash;            // a giant falls
        auto& next = nextLifeHit[(size_t) lane];
        if (t < next) return;
        next = t + 1.0;
        const int map = juce::roundToInt (apvts.getRawParameterValue ("map")->load());
        const int channel = juce::roundToInt (apvts.getRawParameterValue ("outChannel")->load());
        if (apvts.getRawParameterValue (muteId (lane))->load() > 0.5f) return;
        const int64_t on = sampleCounter + (int64_t) std::llround (hostSync.sampleForPpq (t));
        scheduler.scheduleNote (on, on + juce::jmax ((int64_t) 1, (int64_t) (0.1 * hostSync.samplesPerPpq())), channel, noteFor (map, lane, 0), kind == 0 ? 72 : 96);
        laneHits[(size_t) lane].fetch_add (1, std::memory_order_relaxed);
    });

    outBuffer.clear();
    scheduler.emit (outBuffer, sampleCounter, numSamples);
    midi.swapWith (outBuffer);

    uiPpq = hostSync.ppqAt (0);
    uiTransportRunning = hostSync.isRunning();
    hostSync.endBlock();
    sampleCounter += numSamples;
}

void DrumCatProcessor::render (int fromSample, int toSample)
{
    if (toSample <= fromSample || ! hostSync.isRunning())
        return;

    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float swing = f ("swing") / 100.0f;
    const double grid = gridToPpq (juce::roundToInt (f ("grid")));
    const double humanPpq = f ("human") / 60000.0 * hostSync.get().bpm;
    const float velScale = f ("velocity") / 100.0f;
    const float velRandom = f ("velRandom") / 100.0f;
    const float chance = f ("chance") / 100.0f;
    const float open = f ("open") / 100.0f;
    const int map = juce::roundToInt (f ("map"));
    const int channel = juce::roundToInt (f ("outChannel"));
    std::array<bool, NumLanes> muted {};
    for (int l = 0; l < NumLanes; ++l)
        muted[(size_t) l] = apvts.getRawParameterValue (muteId (l))->load() > 0.5f;

    const double t0 = hostSync.ppqAt (fromSample), t1 = hostSync.ppqAt (toSample);
    const double spp = hostSync.samplesPerPpq();

    const juce::SpinLock::ScopedTryLockType lock (patternLock);
    if (! lock.isLocked() || pattern.hits.empty())
        return;

    const double L = pattern.lengthPpq();
    const double margin = 0.3 + humanPpq;   // hits can be played early / late around their grid position

    for (int64_t k = (int64_t) std::floor ((t0 - margin) / L); k <= (int64_t) std::floor ((t1 + margin) / L); ++k)
    {
        for (int i = 0; i < (int) pattern.hits.size(); ++i)
        {
            const auto& h = pattern.hits[(size_t) i];
            if (muted[(size_t) h.lane])
                continue;

            double t = k * L + h.pos + h.micro;

            // Swing: hits on the off-beats of the swing grid move later
            const double inGrid = h.pos / grid;
            if (std::abs (inGrid - std::round (inGrid)) < 1.0e-6 && ((int64_t) std::round (inGrid)) % 2 == 1)
                t += swing * 0.5 * grid;

            t += (hash (k, i, 1) * 2.0f - 1.0f) * humanPpq;
            if (k == 0 && t < 0.0 && h.pos >= 0.0) t = 0.0;   // pattern start: nothing before the downbeat
            if (t < t0 || t >= t1)
                continue;

            if (hash (k, i, 2) >= chance)
                continue;

            int lane = h.lane;
            const int step = (int) std::round (h.pos * 4.0);
            if (lane == Hat && std::abs (h.pos * 4.0 - step) < 1.0e-6 && step % 4 == 2 && hash (k, i, 3) < open)
                lane = OpenHat;

            float v = (float) h.velocity * velScale * (1.0f + (hash (k, i, 4) * 2.0f - 1.0f) * velRandom);
            const int vel = juce::jlimit (1, 127, juce::roundToInt (v));

            const int64_t on = sampleCounter + (int64_t) std::llround (hostSync.sampleForPpq (t));
            const int64_t off = on + juce::jmax ((int64_t) 1, (int64_t) (0.1 * spp));
            scheduler.scheduleNote (on, off, channel, noteFor (map, lane, h.variant), vel);
            laneHits[(size_t) lane].fetch_add (1, std::memory_order_relaxed);
        }
    }
}

//==============================================================================
GeneratorSettings DrumCatProcessor::currentSettings() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    static const int bars[] = { 1, 2, 4 };
    GeneratorSettings s;
    s.style = juce::roundToInt (f ("style"));
    s.bars = bars[juce::jlimit (0, 2, juce::roundToInt (f ("bars")))];
    s.density = f ("density") / 100.0f;
    s.fills = f ("fills") / 100.0f;
    s.seed = seedBase.load() * 131 + variation.load() * 7919 + s.style * 104729;
    return s;
}

Pattern DrumCatProcessor::getPattern() const
{
    const juce::SpinLock::ScopedLockType lock (patternLock);
    return pattern;
}

void DrumCatProcessor::setPattern (const Pattern& p)
{
    auto copy = p;
    copy.sort();
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        pattern = std::move (copy);
    }
    patternVersion.fetch_add (1);
}

void DrumCatProcessor::generate (bool newSeed)
{
    if (newSeed)
    {
        seedBase = juce::Random::getSystemRandom().nextInt64() & 0x7fffffff;
        variation = 1;
    }
    setPattern (generatePattern (currentSettings()));
}

void DrumCatProcessor::selectStyle (int style)
{
    cat::SurpriseControls::setParam (*this, "style", (float) style);
    const auto hints = getStyleHints (style);
    cat::SurpriseControls::setParam (*this, "human", hints.human);
    generate (false);
}

//==============================================================================
const std::vector<DrumCatProcessor::Preset>& DrumCatProcessor::presets()
{
    // style: 0 Boom Bap, 1 Dilla, 2 Neo Soul, 3 Lo-Fi, 4 Funky, 5 Trap, 6 Detroit   bars: 0 = 1, 1 = 2, 2 = 4   grid: 0 1/8, 1 1/16, 2 1/32
    static const std::vector<Preset> list = {
        { "Golden Era",      101,  { { "style", 0 }, { "density", 50 }, { "fills", 30 }, { "swing", 15 }, { "human", 4 }, { "velRandom", 6 }, { "open", 10 } } },
        { "Dusty Breakbeat", 202,  { { "style", 0 }, { "density", 65 }, { "fills", 45 }, { "swing", 25 }, { "human", 6 }, { "velRandom", 10 }, { "open", 20 } } },
        { "Head Nod",        303,  { { "style", 0 }, { "density", 40 }, { "fills", 20 }, { "swing", 10 }, { "human", 3 } } },
        { "4-Bar Journey",   2222, { { "style", 0 }, { "bars", 2 }, { "density", 55 }, { "fills", 60 }, { "swing", 20 }, { "human", 4 } } },
        { "Drunk Sticks",    404,  { { "style", 1 }, { "density", 55 }, { "fills", 35 }, { "human", 12 }, { "velRandom", 12 } } },
        { "Donut Shop",      505,  { { "style", 1 }, { "density", 70 }, { "fills", 50 }, { "human", 10 }, { "open", 15 } } },
        { "Lazy Sunday",     606,  { { "style", 1 }, { "density", 35 }, { "fills", 15 }, { "human", 9 }, { "velocity", 85 } } },
        { "Sparse Ghost",    2424, { { "style", 1 }, { "density", 25 }, { "fills", 10 }, { "human", 11 }, { "chance", 85 } } },
        { "Velvet Pocket",   707,  { { "style", 2 }, { "density", 50 }, { "fills", 30 }, { "swing", 20 }, { "human", 6 }, { "velRandom", 8 }, { "open", 15 } } },
        { "Ghost Notes",     808,  { { "style", 2 }, { "density", 75 }, { "fills", 35 }, { "swing", 15 }, { "human", 5 }, { "velRandom", 14 } } },
        { "Late Night Soul", 909,  { { "style", 2 }, { "density", 40 }, { "fills", 20 }, { "swing", 30 }, { "human", 7 }, { "velocity", 90 } } },
        { "Tape Hiss",       1010, { { "style", 3 }, { "density", 45 }, { "fills", 20 }, { "swing", 35 }, { "human", 8 }, { "velocity", 85 }, { "velRandom", 10 }, { "chance", 92 } } },
        { "Study Beats",     1111, { { "style", 3 }, { "density", 50 }, { "fills", 25 }, { "swing", 25 }, { "human", 6 } } },
        { "Rainy Window",    1212, { { "style", 3 }, { "density", 30 }, { "fills", 10 }, { "swing", 40 }, { "human", 9 }, { "velocity", 80 }, { "chance", 88 } } },
        { "1-Bar Loop",      2323, { { "style", 3 }, { "bars", 0 }, { "density", 45 }, { "fills", 0 }, { "swing", 30 }, { "human", 6 } } },
        { "Funky Drummer",   1313, { { "style", 4 }, { "density", 75 }, { "fills", 45 }, { "swing", 10 }, { "human", 4 }, { "velRandom", 10 }, { "open", 25 } } },
        { "Tight Pocket",    1414, { { "style", 4 }, { "density", 55 }, { "fills", 25 }, { "human", 2 }, { "velRandom", 5 } } },
        { "Open Hat Party",  1515, { { "style", 4 }, { "density", 65 }, { "fills", 40 }, { "human", 3 }, { "open", 45 } } },
        { "808 Rolls",       1616, { { "style", 5 }, { "density", 70 }, { "fills", 50 }, { "grid", 2 }, { "human", 1 }, { "velRandom", 8 } } },
        { "Dark Trap",       1717, { { "style", 5 }, { "density", 45 }, { "fills", 30 }, { "grid", 2 }, { "human", 1 } } },
        { "Bouncy Trap",     1818, { { "style", 5 }, { "density", 60 }, { "fills", 40 }, { "swing", 15 }, { "grid", 2 }, { "human", 1 } } },
        { "Motor City",      1919, { { "style", 6 }, { "density", 60 }, { "fills", 35 }, { "swing", 20 }, { "human", 5 }, { "open", 20 } } },
        { "Basement Jam",    2020, { { "style", 6 }, { "density", 70 }, { "fills", 50 }, { "human", 7 }, { "velRandom", 10 } } },
        { "Minimal Detroit", 2121, { { "style", 6 }, { "density", 35 }, { "fills", 15 }, { "human", 4 } } },
    };
    return list;
}

void DrumCatProcessor::applyPreset (int index)
{
    const auto& list = presets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    currentPreset = index;
    const auto& preset = list[(size_t) index];

    // Groove parameters to their defaults (map, channel, key trigger and mutes stay), then the preset
    for (auto* id : { "style", "bars", "density", "fills", "swing", "grid", "human", "velocity", "velRandom", "chance", "open" })
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
}

void DrumCatProcessor::setVariation (int index)
{
    variation = juce::jlimit (1, numVariations, index);
    generate (false);
}

juce::MidiMessageSequence DrumCatProcessor::buildPatternSequence() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const auto p = getPattern();
    const float swing = f ("swing") / 100.0f;
    const double grid = gridToPpq (juce::roundToInt (f ("grid")));
    const float velScale = f ("velocity") / 100.0f;
    const int map = juce::roundToInt (f ("map"));
    const int channel = juce::roundToInt (f ("outChannel"));

    juce::MidiMessageSequence seq;
    for (const auto& h : p.hits)
    {
        if (apvts.getRawParameterValue (muteId (h.lane))->load() > 0.5f)
            continue;
        double t = h.pos + h.micro;
        const double inGrid = h.pos / grid;
        if (std::abs (inGrid - std::round (inGrid)) < 1.0e-6 && ((int64_t) std::round (inGrid)) % 2 == 1)
            t += swing * 0.5 * grid;
        t = juce::jlimit (0.0, p.lengthPpq() - 0.01, t);

        const int note = noteFor (map, h.lane, h.variant);
        const int vel = juce::jlimit (1, 127, juce::roundToInt ((float) h.velocity * velScale));
        seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) vel).withTimeStamp (t));
        seq.addEvent (juce::MidiMessage::noteOff (channel, note).withTimeStamp (juce::jmin (p.lengthPpq(), t + 0.1)));
    }
    seq.sort();
    seq.updateMatchedPairs();
    return seq;
}

void DrumCatProcessor::surprise()
{
    // New style / length / feel and a fresh pattern
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    const int style = r.nextInt (getStyleNames().size());
    set ("style", (float) style);
    set ("bars", (float) (r.nextFloat() < 0.6f ? 1 : (r.nextBool() ? 0 : 2)));
    set ("density", 25.0f + r.nextFloat() * 60.0f);
    set ("fills", r.nextFloat() * 70.0f);
    set ("swing", r.nextFloat() < 0.6f ? 0.0f : r.nextFloat() * 40.0f);
    set ("human", getStyleHints (style).human + r.nextFloat() * 4.0f);
    set ("velRandom", r.nextFloat() * 12.0f);
    set ("chance", r.nextFloat() < 0.7f ? 100.0f : 85.0f + r.nextFloat() * 15.0f);
    set ("open", r.nextFloat() < 0.6f ? 0.0f : r.nextFloat() * 35.0f);
    generate (true);
}

//==============================================================================
void DrumCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("DRUMCAT");
    root.setProperty ("preset", currentPreset.load(), nullptr);
    root.setProperty ("seed", juce::String (seedBase.load()), nullptr);
    root.setProperty ("variation", variation.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (getPattern().toValueTree(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrumCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("DRUMCAT")) return;
    currentPreset = (int) root.getProperty ("preset", -1);

    seedBase = root.getProperty ("seed", "1").toString().getLargeIntValue();
    variation = juce::jlimit (1, numVariations, (int) root.getProperty ("variation", 1));
    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);
    if (auto p = root.getChildWithName (Pattern::treeId); p.isValid())
        setPattern (Pattern::fromValueTree (p));
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
    patternVersion.fetch_add (1);
}

juce::AudioProcessorEditor* DrumCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<DrumCatEditor> (*this), "Drum-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumCatProcessor();
}
