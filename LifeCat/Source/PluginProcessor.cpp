#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace lifecat;

namespace
{
    juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    juce::StringArray delayNames() { return { "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2" }; }
    double delayBeatsFor (int i) { static const double b[] = { 0.5, 0.75, 1.0, 1.5, 2.0 }; return b[juce::jlimit (0, 4, i)]; }

    juce::String spId (int role, const char* what) { return "sp" + juce::String (role) + what; }

    /** Parameters a world preset resets before applying its own values. */
    const juce::StringArray& presetParamIds()
    {
        static const juce::StringArray ids = []
        {
            juce::StringArray a { "lifeSpeed", "foodGrowth", "mutation", "metabolism", "breeding", "lifespan", "maxPop", "immigration",
                                  "pitchMode", "key", "scale", "lowNote", "highNote", "quantize", "grid", "swing", "density", "maxNotes",
                                  "noteLength", "theme", "brightness", "attack", "release", "reverbMix", "reverbSize", "delayMix",
                                  "delayTime", "delayFb", "width",
                                  "gravity", "wind", "windDir", "dayLength", "seasonLength", "season", "weather", "ambience", "dayNightSound" };
            for (int r = 0; r < numRoles; ++r)
                for (auto* w : { "Sing", "Vol", "Oct", "Sound", "Speed", "Max" }) a.add (spId (r, w));
            return a;
        }();
        return ids;
    }

    const float dayLengths[] = { 0.0f, 60.0f, 120.0f, 240.0f, 480.0f };
    const float seasonLengths[] = { 0.0f, 60.0f, 120.0f, 300.0f, 600.0f };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LifeCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto fl = [&] (const String& id, const String& name, float lo, float hi, float def, const char* unit = "", float skew = 1.0f)
    {
        const String u (unit);
        auto toText = [u] (float v, int) -> String
        {
            if (u == "%")  return String (roundToInt (v * 100.0f)) + "%";
            if (u == "pm") return (v > 0.005f ? "+" : "") + String (roundToInt (v * 100.0f));
            if (u == "x")  return String (v, 2) + "x";
            if (u == "dB") return String (v, 1) + " dB";
            if (u == "sw") return String (roundToInt (v)) + "%";
            if (u == "deg") return String (roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
            return String (v, 2);
        };
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, skew), def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
    };
    auto pct = [&] (const String& id, const String& name, float def) { fl (id, name, 0.0f, 1.0f, def, "%"); };
    auto bip = [&] (const String& id, const String& name, float def) { fl (id, name, -1.0f, 1.0f, def, "pm"); };
    auto ch = [&] (const String& id, const String& name, const StringArray& items, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto in = [&] (const String& id, const String& name, int lo, int hi, int def, std::function<String (int)> fmt = {})
    {
        auto attrs = AudioParameterIntAttributes();
        if (fmt) attrs = attrs.withStringFromValueFunction ([fmt] (int v, int) { return fmt (v); });
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def, attrs));
    };
    auto bo = [&] (const String& id, const String& name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto noteName = [] (int v) { return MidiMessage::getMidiNoteName (v, true, true, 3); };

    // Life
    bo ("running", "Life Running", true);
    fl ("lifeSpeed", "Life Speed", 0.1f, 4.0f, 1.0f, "x", 0.5f);
    pct ("foodGrowth", "Food Growth", 0.5f);
    pct ("mutation", "Mutation", 0.25f);
    pct ("metabolism", "Hunger", 0.5f);
    pct ("breeding", "Breeding", 0.5f);
    fl ("lifespan", "Lifespan", 0.3f, 3.0f, 1.0f, "x", 0.6f);
    in ("maxPop", "Max Population", 4, maxCreatures, 48);
    bo ("immigration", "Newcomers", true);

    // Music
    ch ("pitchMode", "Notes From", pitchModeNames(), 0);
    ch ("key", "Key", keyNames(), 0);
    ch ("scale", "Scale", scaleNames(), 7);
    in ("lowNote", "Lowest Note", 24, 96, 45, noteName);
    in ("highNote", "Highest Note", 36, 108, 86, noteName);
    bo ("quantize", "Quantize", true);
    ch ("grid", "Grid", gridNames(), 1);
    fl ("swing", "Swing", 0.0f, 100.0f, 0.0f, "sw");
    pct ("density", "Density", 0.55f);
    in ("maxNotes", "Max Notes per Step", 1, 12, 3);
    fl ("noteLength", "Note Length", 0.25f, 2.5f, 1.0f, "x", 0.6f);
    bo ("followGroove", "Follow Drum", false);
    bo ("playStopped", "Play When Stopped", true);

    // Sound
    bo ("sound", "Own Sound", true);
    ch ("theme", "World", themeNames(), 0);
    bip ("brightness", "Brightness", 0.0f);
    bip ("attack", "Attack", 0.0f);
    bip ("release", "Release", 0.0f);
    pct ("reverbMix", "Reverb", 0.35f);
    pct ("reverbSize", "Reverb Size", 0.75f);
    pct ("delayMix", "Delay", 0.15f);
    ch ("delayTime", "Delay Time", delayNames(), 1);
    fl ("delayFb", "Delay Feedback", 0.0f, 0.9f, 0.4f, "%");
    pct ("width", "Width", 0.8f);
    fl ("volume", "Volume", -36.0f, 6.0f, 0.0f, "dB");

    // World (sandbox physics)
    pct ("gravity", "Gravity", 0.0f);
    pct ("wind", "Wind", 0.0f);
    fl ("windDir", "Wind Direction", -180.0f, 180.0f, 0.0f, "deg");
    ch ("dayLength", "Day / Night", { "Off (always day)", "1 min day", "2 min day", "4 min day", "8 min day" }, 0);
    ch ("seasonLength", "Seasons", { "Off (fixed)", "1 min each", "2 min each", "5 min each", "10 min each" }, 0);
    ch ("season", "Season", { "Spring", "Summer", "Autumn", "Winter" }, 1);
    ch ("weather", "Weather", { "Auto", "Clear", "Rain", "Snow", "Fog", "Storm" }, 1);
    pct ("ambience", "Weather Sounds", 0.35f);
    pct ("dayNightSound", "Day/Night Colour", 0.5f);

    // Species (one set per kind of creature)
    const auto roles = roleParamNames();
    for (int r = 0; r < numRoles; ++r)
    {
        const auto n = roles[r];
        bo (spId (r, "Sing"), n + " Sing", true);
        fl (spId (r, "Vol"), n + " Volume", -30.0f, 6.0f, 0.0f, "dB");
        in (spId (r, "Oct"), n + " Octave", -2, 2, 0, [] (int v) { return v > 0 ? "+" + String (v) : String (v); });
        ch (spId (r, "Sound"), n + " Sound", soundTypeNames(), 0);
        fl (spId (r, "Speed"), n + " Speed", 0.25f, 2.0f, 1.0f, "x");
        in (spId (r, "Max"), n + " Max", 0, maxCreatures, roleInfo ((Role) r).defaultMax);
    }

    // MIDI
    bo ("midiOut", "MIDI Out", false);
    ch ("midiChannels", "MIDI Channels", { "All on channel 1", "One channel per kind (1-7)" }, 0);
    return layout;
}

juce::StringArray LifeCatProcessor::roleParamNames()
{
    return { "Grazers", "Hunters", "Drifters", "Pollinators", "Scavengers", "Giants", "Swarms" };
}

//==============================================================================
const std::vector<LifeCatProcessor::WorldPreset>& LifeCatProcessor::presets()
{
    // scale: 0 Major 1 Minor 2 Dorian 3 Mixo 4 Lydian 5 Phrygian 6 HarmMinor 7 MajPenta 8 MinPenta 9 Japanese 10 WholeTone
    // grid: 0 1/4 1 1/8 2 1/16 3 1/8T 4 1/16T 5 1/32
    // counts: grazers, hunters, drifters, pollinators, scavengers, giants, swarms
    // "sp<kind>Max" 0 keeps a kind out of a world; dayLength 1..4 = 1/2/4/8 min days; weather 0 auto 1 clear 2 rain 3 snow 4 fog 5 storm
    static const std::vector<WorldPreset> list = {
        { "Quiet Pond", 0, 11, { 8, 2, 4, 2, 1, 0, 0 }, 40, { { "scale", 7 }, { "density", 0.45f }, { "reverbMix", 0.45f }, { "sp5Max", 0 }, { "sp6Max", 0 } } },
        { "Busy Garden", 0, 23, { 16, 4, 4, 4, 2, 0, 8 }, 80, { { "grid", 2 }, { "density", 0.7f }, { "maxNotes", 5 }, { "foodGrowth", 0.7f } } },
        { "Kalimba Meadow", 0, 5, { 10, 2, 3, 6, 0, 0, 0 }, 60, { { "scale", 0 }, { "key", 2 }, { "release", -0.3f }, { "brightness", 0.2f }, { "grid", 2 }, { "sp4Max", 0 }, { "sp5Max", 0 }, { "sp6Max", 0 } }, 8 },
        { "Two Ponds", 0, 31, { 12, 3, 4, 2, 1, 0, 0 }, 50, { { "scale", 1 }, { "key", 9 }, { "sp5Max", 0 } }, 1 },
        { "Predator Waves", 0, 17, { 20, 6, 3, 0, 3, 0, 10 }, 70, { { "scale", 8 }, { "key", 4 }, { "grid", 2 }, { "swing", 30.0f }, { "sp3Max", 0 } } },
        { "Evolution Lab", 0, 99, { 14, 3, 5, 2, 2, 0, 0 }, 60, { { "mutation", 0.9f }, { "lifeSpeed", 2.0f }, { "lifespan", 0.6f }, { "scale", 2 }, { "sp5Max", 0 }, { "sp6Max", 0 } } },
        { "Follow My Chords", 0, 12, { 12, 3, 5, 2, 0, 0, 6 }, 50, { { "pitchMode", 2 }, { "grid", 2 } } },
        { "Deep Ocean", 1, 7, { 10, 3, 8, 0, 2, 1, 0 }, 50, { { "scale", 2 }, { "key", 2 }, { "density", 0.45f }, { "reverbMix", 0.55f }, { "sp3Max", 0 }, { "sp6Max", 0 } } },
        { "Jellyfish Lullaby", 1, 42, { 4, 1, 12, 0, 0, 0, 0 }, 30, { { "scale", 7 }, { "quantize", 0 }, { "density", 0.4f }, { "attack", 0.5f }, { "release", 0.6f },
                                                                    { "sp3Max", 0 }, { "sp4Max", 0 }, { "sp5Max", 0 }, { "sp6Max", 0 } } },
        { "River Current", 1, 21, { 12, 3, 6, 0, 1, 0, 10 }, 60, { { "scale", 3 }, { "key", 7 }, { "sp3Max", 0 } }, 3 },
        { "Coral Reef", 1, 77, { 16, 4, 6, 3, 2, 0, 12 }, 90, { { "grid", 2 }, { "swing", 30.0f }, { "scale", 0 }, { "key", 5 } } },
        { "Nebula", 2, 3, { 6, 2, 12, 0, 0, 1, 0 }, 30, { { "scale", 4 }, { "quantize", 0 }, { "reverbMix", 0.7f }, { "reverbSize", 0.9f }, { "sp3Max", 0 }, { "sp4Max", 0 }, { "sp6Max", 0 } } },
        { "Comet Rain", 2, 19, { 14, 5, 4, 0, 2, 0, 10 }, 60, { { "grid", 2 }, { "delayMix", 0.3f }, { "scale", 8 }, { "key", 9 }, { "sp3Max", 0 } } },
        { "Warm Star", 2, 55, { 10, 2, 8, 2, 0, 1, 0 }, 40, { { "scale", 4 }, { "key", 7 }, { "density", 0.5f }, { "sp6Max", 0 } }, 2 },
        { "Whole Tone Drift", 2, 61, { 8, 2, 10, 0, 0, 0, 0 }, 40, { { "scale", 10 }, { "quantize", 0 }, { "density", 0.45f }, { "sp3Max", 0 }, { "sp4Max", 0 }, { "sp5Max", 0 }, { "sp6Max", 0 } } },
        { "Petri Dish", 3, 8, { 14, 3, 4, 0, 2, 0, 8 }, 70, { { "scale", 9 }, { "grid", 2 }, { "sp3Max", 0 } } },
        { "Cell Clicks", 3, 64, { 18, 4, 2, 0, 4, 0, 12 }, 100, { { "grid", 5 }, { "maxNotes", 6 }, { "release", -0.6f }, { "scale", 8 }, { "sp3Max", 0 }, { "sp5Max", 0 } } },
        { "Four Rooms", 3, 4, { 14, 3, 4, 0, 2, 0, 0 }, 60, { { "scale", 6 }, { "key", 2 }, { "grid", 2 }, { "sp3Max", 0 }, { "sp5Max", 0 }, { "sp6Max", 0 } }, 4 },
        // ---- sandbox worlds
        { "Bee Meadow", 0, 101, { 8, 1, 2, 8, 1, 0, 0 }, 90, { { "scale", 0 }, { "key", 7 }, { "grid", 2 }, { "density", 0.5f }, { "sp5Max", 0 }, { "sp6Max", 0 } }, 8 },
        { "Night Forest", 0, 102, { 10, 3, 8, 3, 2, 1, 0 }, 60, { { "dayLength", 2 }, { "scale", 1 }, { "key", 9 }, { "reverbMix", 0.5f }, { "dayNightSound", 0.8f }, { "sp6Max", 0 } } },
        { "Four Seasons", 0, 103, { 14, 3, 5, 4, 2, 1, 8 }, 80, { { "seasonLength", 2 }, { "dayLength", 2 }, { "weather", 0 }, { "scale", 7 }, { "key", 5 } } },
        { "Rainy Garden", 0, 104, { 12, 2, 4, 3, 2, 0, 6 }, 70, { { "weather", 2 }, { "scale", 2 }, { "key", 2 }, { "ambience", 0.5f }, { "reverbMix", 0.45f } } },
        { "Whale Song", 1, 105, { 6, 1, 6, 0, 2, 3, 14 }, 80, { { "scale", 7 }, { "key", 0 }, { "density", 0.5f }, { "reverbMix", 0.6f }, { "sp3Max", 0 } }, 5 },
        { "Starling Swarm", 2, 106, { 4, 2, 4, 0, 0, 0, 22 }, 70, { { "grid", 2 }, { "scale", 8 }, { "key", 4 }, { "delayMix", 0.25f }, { "sp3Max", 0 } } },
        { "Aquarium", 1, 107, { 8, 2, 4, 0, 3, 1, 12 }, 70, { { "gravity", 0.6f }, { "scale", 3 }, { "key", 2 }, { "sp3Max", 0 } }, 7 },
        { "Windy Island", 0, 108, { 10, 2, 4, 4, 2, 1, 8 }, 70, { { "wind", 0.45f }, { "windDir", 20.0f }, { "weather", 0 }, { "scale", 4 }, { "key", 9 } }, 5 },
        { "Bone Yard", 3, 109, { 14, 6, 2, 0, 6, 0, 6 }, 70, { { "grid", 2 }, { "swing", 20.0f }, { "scale", 5 }, { "key", 4 }, { "sp3Max", 0 } }, 9 },
        { "Snow Globe", 2, 110, { 8, 2, 6, 0, 2, 1, 10 }, 50, { { "season", 3 }, { "weather", 3 }, { "scale", 9 }, { "key", 2 }, { "ambience", 0.5f }, { "sp3Max", 0 } } },
    };
    return list;
}

void LifeCatProcessor::applyLayout (World& w, int layout)
{
    w.clearWallsAndZones();
    w.clearTerrain();
    auto box = [&w] (float x0, float y0, float x1, float y1, Terrain t)
    {
        const int n = terrainSize;
        for (int cy = juce::jlimit (0, n, (int) (y0 * n)); cy < juce::jlimit (0, n, (int) std::ceil (y1 * n)); ++cy)
            for (int cx = juce::jlimit (0, n, (int) (x0 * n)); cx < juce::jlimit (0, n, (int) std::ceil (x1 * n)); ++cx)
                w.terrain[(size_t) (cy * n + cx)] = (uint8_t) t;
    };
    switch (layout)
    {
        case 1: w.addWall (0.5f, 0.0f, 0.5f, 0.42f); w.addWall (0.5f, 0.58f, 0.5f, 1.0f); break;            // two ponds with a gap
        case 2: w.addZone (ZoneType::Warm, 0.5f, 0.5f, 0.22f, 0, 0); w.addZone (ZoneType::Cold, 0.1f, 0.1f, 0.15f, 0, 0);
                w.addZone (ZoneType::Cold, 0.9f, 0.9f, 0.15f, 0, 0); break;
        case 3: w.addZone (ZoneType::Current, 0.3f, 0.5f, 0.25f, 1.0f, -0.2f); w.addZone (ZoneType::Current, 0.7f, 0.5f, 0.25f, 0.8f, 0.3f);
                w.addZone (ZoneType::Warm, 0.85f, 0.2f, 0.12f, 0, 0); break;
        case 4: w.addWall (0.5f, 0.0f, 0.5f, 0.4f); w.addWall (0.5f, 0.6f, 0.5f, 1.0f);
                w.addWall (0.0f, 0.5f, 0.4f, 0.5f); w.addWall (0.6f, 0.5f, 1.0f, 0.5f); break;
        case 5:   // island: water all around a meadow
            box (0.0f, 0.0f, 1.0f, 1.0f, Terrain::Water);
            w.paintTerrain (0.5f, 0.5f, 0.33f, Terrain::Ground);
            w.paintTerrain (0.5f, 0.5f, 0.18f, Terrain::Meadow);
            w.paintTerrain (0.62f, 0.4f, 0.06f, Terrain::Rock);
            break;
        case 6:   // river with stepping stones
            for (float t = 0.0f; t <= 1.0f; t += 0.02f) w.paintTerrain (t, 0.35f + 0.3f * t, 0.07f, Terrain::Water);
            w.paintTerrain (0.3f, 0.45f, 0.03f, Terrain::Rock);
            w.paintTerrain (0.7f, 0.57f, 0.03f, Terrain::Rock);
            break;
        case 7:   // aquarium: sand floor, rocks, weed
            box (0.0f, 0.0f, 1.0f, 1.0f, Terrain::Water);
            box (0.0f, 0.9f, 1.0f, 1.0f, Terrain::Ground);
            w.paintTerrain (0.25f, 0.88f, 0.07f, Terrain::Rock);
            w.paintTerrain (0.72f, 0.9f, 0.09f, Terrain::Rock);
            w.paintTerrain (0.5f, 0.86f, 0.08f, Terrain::Meadow);
            break;
        case 8:   // sunny glade: meadow with patches of sunlight
            w.paintTerrain (0.5f, 0.5f, 0.38f, Terrain::Meadow);
            w.paintTerrain (0.35f, 0.4f, 0.09f, Terrain::Light);
            w.paintTerrain (0.66f, 0.62f, 0.08f, Terrain::Light);
            break;
        case 9:   // canyon: rocks with paths between
            box (0.0f, 0.2f, 0.35f, 0.32f, Terrain::Rock);
            box (0.65f, 0.2f, 1.0f, 0.32f, Terrain::Rock);
            box (0.2f, 0.62f, 0.8f, 0.72f, Terrain::Rock);
            w.paintTerrain (0.5f, 0.45f, 0.08f, Terrain::Meadow);
            break;
        default: break;
    }
}

//==============================================================================
LifeCatProcessor::LifeCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            refs[r->getParameterID()] = apvts.getRawParameterValue (r->getParameterID());
    for (int r = 0; r < numRoles; ++r)
    {
        auto& sr = speciesRefs[(size_t) r];
        sr.sing = apvts.getRawParameterValue (spId (r, "Sing"));
        sr.vol = apvts.getRawParameterValue (spId (r, "Vol"));
        sr.oct = apvts.getRawParameterValue (spId (r, "Oct"));
        sr.sound = apvts.getRawParameterValue (spId (r, "Sound"));
        sr.speed = apvts.getRawParameterValue (spId (r, "Speed"));
        sr.max = apvts.getRawParameterValue (spId (r, "Max"));
    }

    world = std::make_unique<World>();
    snapshot = std::make_unique<World>();
    const auto& p0 = presets()[0];
    world->reset (p0.seed, p0.counts, p0.plants);
    *snapshot = *world;
    hasSnapshot = true;
    conductor.seed (p0.seed);
    commands.reserve (256);
    commandsAudio.reserve (256);
    sung.reserve (512);
    recorded.reserve (4096);
    applyPreset (0);

    cat::followDrumGroove (presetManager, apvts, [this] (const cat::CatLink::Groove& g)
    {
        cat::SurpriseControls::setParam (*this, "swing", g.swing);
    });
}

LifeCatProcessor::~LifeCatProcessor() = default;

float LifeCatProcessor::p (const char* id) const
{
    auto it = refs.find (id);
    return it != refs.end() ? it->second->load() : 0.0f;
}

bool LifeCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::disabled();
}

void LifeCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate, samplesPerBlock);
    hostSync.prepare (sampleRate);
    work.setSize (2, juce::jmax (512, samplesPerBlock), false, false, true);
    numOffs = 0;
    numHeld = 0;
}

LifeSettings LifeCatProcessor::lifeSettings() const
{
    LifeSettings s;
    s.speed = p ("lifeSpeed");
    s.foodGrowth = p ("foodGrowth");
    s.mutation = p ("mutation");
    s.metabolism = p ("metabolism");
    s.breeding = p ("breeding");
    s.lifespan = p ("lifespan");
    s.maxPopulation = juce::roundToInt (p ("maxPop"));
    s.immigration = p ("immigration") > 0.5f;
    for (int r = 0; r < numRoles; ++r)
    {
        s.species[(size_t) r].speed = speciesRefs[(size_t) r].speed->load();
        s.species[(size_t) r].maxCount = juce::roundToInt (speciesRefs[(size_t) r].max->load());
    }
    s.gravity = p ("gravity");
    s.wind = p ("wind");
    s.windDirection = juce::degreesToRadians (p ("windDir"));
    s.dayLength = dayLengths[juce::jlimit (0, 4, juce::roundToInt (p ("dayLength")))];
    s.seasonLength = seasonLengths[juce::jlimit (0, 4, juce::roundToInt (p ("seasonLength")))];
    s.fixedSeason = juce::roundToInt (p ("season"));
    s.weatherMode = juce::roundToInt (p ("weather"));
    return s;
}

MusicSettings LifeCatProcessor::musicSettings() const
{
    MusicSettings m;
    m.pitchMode = juce::roundToInt (p ("pitchMode"));
    m.key = juce::roundToInt (p ("key"));
    m.scale = juce::roundToInt (p ("scale"));
    m.lowNote = juce::roundToInt (p ("lowNote"));
    m.highNote = juce::jmax (m.lowNote + 12, juce::roundToInt (p ("highNote")));
    m.quantize = p ("quantize") > 0.5f;
    m.grid = juce::roundToInt (p ("grid"));
    m.swing = p ("swing");
    m.density = p ("density");
    m.maxNotes = juce::roundToInt (p ("maxNotes"));
    m.lengthScale = p ("noteLength");
    for (int r = 0; r < numRoles; ++r)
    {
        m.speciesSing[(size_t) r] = speciesRefs[(size_t) r].sing->load() > 0.5f;
        m.speciesOctave[(size_t) r] = juce::roundToInt (speciesRefs[(size_t) r].oct->load());
    }
    return m;
}

SoundSettings LifeCatProcessor::soundSettings (double bpm, const World& w) const
{
    SoundSettings s;
    s.theme = (Theme) juce::roundToInt (p ("theme"));
    s.brightness = p ("brightness");
    s.attack = p ("attack");
    s.release = p ("release");
    s.reverbMix = p ("reverbMix");
    s.reverbSize = p ("reverbSize");
    s.delayMix = p ("delayMix");
    s.delayBeats = delayBeatsFor (juce::roundToInt (p ("delayTime")));
    s.delayFeedback = p ("delayFb");
    s.width = p ("width");
    s.volume = juce::Decibels::decibelsToGain (p ("volume"));
    s.bpm = bpm;

    // the world colours the sound: night is darker and wetter, fog is wetter, winter is darker
    const float dn = p ("dayNightSound");
    const float night = 1.0f - w.light;
    const float fog = w.weather == Weather::Fog ? w.weatherAmount : 0.0f;
    s.brightness = juce::jlimit (-1.0f, 1.0f, s.brightness - dn * (0.45f * night + 0.2f * std::max (0.0f, -w.temperature)));
    s.reverbMix = juce::jlimit (0.0f, 1.0f, s.reverbMix + dn * 0.2f * night + 0.25f * fog);
    s.ambience = p ("ambience");
    s.rain = (w.weather == Weather::Rain ? 1.0f : (w.weather == Weather::Storm ? 1.3f : 0.0f)) * w.weatherAmount;
    s.wind = juce::jlimit (0.0f, 1.0f, p ("wind") * 0.8f + (w.weather == Weather::Storm ? w.weatherAmount : 0.0f));
    s.snow = w.weather == Weather::Snow ? w.weatherAmount : 0.0f;
    return s;
}

void LifeCatProcessor::send (const Command& c)
{
    const juce::SpinLock::ScopedLockType sl (commandLock);
    if (commands.size() < 256) commands.push_back (c);
}

void LifeCatProcessor::apply (const Command& c)
{
    auto& w = *world;
    switch (c.type)
    {
        case Command::Food:      w.addFood (c.x, c.y, juce::jmax (1, (int) c.r), 0.035f); break;
        case Command::Add:       w.addCreature ((Role) juce::jlimit (0, 2, c.role), c.x, c.y); break;
        case Command::Remove:    w.removeNear (c.x, c.y, c.r); break;
        case Command::Wall:      w.addWall (c.x, c.y, c.x2, c.y2); break;
        case Command::Zone:      w.addZone ((ZoneType) juce::jlimit (0, 2, c.zone), c.x, c.y, c.r, c.x2, c.y2); break;
        case Command::EraseZone: w.removeZoneOrWallNear (c.x, c.y, c.r); break;
        case Command::ClearZones: w.clearWallsAndZones(); break;
        case Command::Breed:     w.breed (c.id, c.id2); break;
        case Command::Clone:     w.clone (c.id); break;
        case Command::Favourite: w.setFavourite (c.id, c.flag); break;
        case Command::RemoveId:  w.removeId (c.id); break;
        case Command::NewWorld:
            w.reset (c.seed, c.counts, c.plants);
            applyLayout (w, c.layout);
            conductor.seed (c.seed);
            break;
        case Command::Paint:        w.paintTerrain (c.x, c.y, c.r, (Terrain) juce::jlimit (0, numTerrains - 1, c.terrain)); break;
        case Command::ClearTerrain: w.clearTerrain(); break;
        case Command::SetGenes:     w.setGenes (c.id, c.genes); break;
        case Command::AddGenes:     w.addCreature ((Role) juce::jlimit (0, numRoles - 1, c.role), c.x, c.y, &c.genes); break;
        case Command::ListenGenes:
        {
            Creature tmp;
            tmp.role = (Role) juce::jlimit (0, numRoles - 1, c.role);
            tmp.genes = c.genes;
            tmp.y = 0.5f;
            const auto ms = musicSettings();
            NoteRequest r;
            r.midiNote = conductor.pitchFor (tmp, ms, held.data(), numHeld);
            r.velocity = 0.8f;
            r.lengthSec = 0.8f;
            r.genes = c.genes;
            r.role = tmp.role;
            const int snd = juce::roundToInt (speciesRefs[(size_t) tmp.role].sound->load());
            r.sound = snd == 0 ? naturalSound (tmp.role) : (SoundType) (snd - 1);
            synth.noteOn (r);
            break;
        }
        case Command::Listen:
            if (auto* cr = w.find (c.id))
            {
                const auto ms = musicSettings();
                NoteRequest r;
                r.midiNote = conductor.pitchFor (*cr, ms, held.data(), numHeld);
                r.velocity = 0.8f;
                r.lengthSec = 0.8f;
                r.pan = cr->x * 2.0f - 1.0f;
                r.genes = cr->genes;
                r.role = cr->role;
                const int snd = juce::roundToInt (speciesRefs[(size_t) cr->role].sound->load());
                r.sound = snd == 0 ? naturalSound (cr->role) : (SoundType) (snd - 1);
                r.gain = juce::Decibels::decibelsToGain (speciesRefs[(size_t) cr->role].vol->load());
                synth.noteOn (r);
                cr->singFlash = 1.0f;
            }
            break;
    }
}

//==============================================================================
void LifeCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    mergeComputerKeyboard (midi, numSamples);      // standalone app: notes from the computer keyboard
    buffer.clear();
    hostSync.beginBlock (getPlayHead(), numSamples);
    midiLearn.processMidi (midi);
    const auto t = hostSync.get();
    lastBpm = t.bpm;
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;

    // a whole new world from the message thread (preset / state)
    if (worldSwapLock.tryEnter())
    {
        if (hasPendingWorld && pendingWorld != nullptr)
        {
            std::swap (world, pendingWorld);      // the old world is freed later on the message thread
            hasPendingWorld = false;
            conductor.seed (world->seedValue);
        }
        worldSwapLock.exit();
    }
    // gardener commands
    if (commandLock.tryEnter())
    {
        commandsAudio.clear();
        for (const auto& c : commands) commandsAudio.push_back (c);
        commands.clear();
        commandLock.exit();
        for (const auto& c : commandsAudio) apply (c);
    }

    // held notes (for "Held chord")
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            noteMonitor.noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity());
            if (numHeld < (int) held.size()) held[(size_t) numHeld++] = m.getNoteNumber();
            std::sort (held.begin(), held.begin() + numHeld);
        }
        else if (m.isNoteOff())
        {
            noteMonitor.noteOff (m.getNoteNumber());
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == m.getNoteNumber()) { held[(size_t) i] = held[(size_t) --numHeld]; break; }
            std::sort (held.begin(), held.begin() + numHeld);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { numHeld = 0; noteMonitor.clear(); }
    }
    midi.clear();

    // life goes on (fixed 10 ms steps)
    if (p ("running") > 0.5f)
    {
        const auto ls = lifeSettings();
        stepAccumulator += (float) numSamples / (float) sr;
        int guard = 0;
        while (stepAccumulator >= 0.01f && guard++ < 50)
        {
            world->step (0.01f, ls);
            stepAccumulator -= 0.01f;
        }
    }

    // song position: the host transport, or our own clock while stopped
    double ppq0 = 0.0, ppqPerSample = t.bpm / 60.0 / sr;
    bool singing = false;
    if (t.hostPlaying)
    {
        ppq0 = t.ppqStart;
        if (t.ppqPerSample > 0.0) ppqPerSample = t.ppqPerSample;
        singing = true;
        freePpq = ppq0 + ppqPerSample * numSamples;
    }
    else if (p ("playStopped") > 0.5f && p ("running") > 0.5f)
    {
        ppq0 = freePpq;
        freePpq += ppqPerSample * numSamples;
        singing = true;
    }

    sung.clear();
    if (singing)
        conductor.process (*world, musicSettings(), ppq0, ppqPerSample, numSamples, held.data(), numHeld, sung);

    synth.setSettings (soundSettings (t.bpm, *world));
    const bool ownSound = p ("sound") > 0.5f;
    const bool midiOut = p ("midiOut") > 0.5f;
    const bool byRole = juce::roundToInt (p ("midiChannels")) == 1;

    if (work.getNumSamples() < numSamples) work.setSize (2, numSamples, false, false, true);
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    int pos = 0;
    for (const auto& s : sung)
    {
        if (s.sample > pos) { synth.render (l + pos, r + pos, s.sample - pos); pos = s.sample; }
        const auto& c = world->creatures[(size_t) s.creature];
        const float lengthSec = (float) (s.lengthBeats * 60.0 / juce::jmax (30.0, t.bpm));
        if (ownSound)
        {
            NoteRequest req;
            req.midiNote = s.midiNote;
            req.velocity = s.velocity;
            req.lengthSec = lengthSec;
            req.pan = s.pan;
            req.genes = c.genes;
            req.role = c.role;
            const auto& sr = speciesRefs[(size_t) c.role];
            const int snd = juce::roundToInt (sr.sound->load());
            req.sound = snd == 0 ? naturalSound (c.role) : (SoundType) (snd - 1);
            req.gain = juce::Decibels::decibelsToGain (sr.vol->load(), -30.0f);
            synth.noteOn (req);
        }
        const int channel = byRole ? 1 + (int) c.role : 1;          // 1..7
        const int note = juce::jlimit (0, 127, juce::roundToInt (s.midiNote));
        if (midiOut)
            midi.addEvent (juce::MidiMessage::noteOn (channel, note, juce::jlimit (0.05f, 1.0f, s.velocity)), s.sample);
        noteMonitor.noteOn (1 + (int) c.role, note, juce::roundToInt (s.velocity * 127.0f));   // keyboard + cat show who sings
        if (numOffs < (int) offs.size()) offs[(size_t) numOffs++] = { note, channel, (double) s.sample + lengthSec * sr, midiOut };
        // remember for Drag MIDI
        int s1, n1, s2, n2;
        recFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0)
        {
            recBuffer[(size_t) s1] = { ppq0 + s.sample * ppqPerSample, juce::roundToInt (s.midiNote), s.velocity, s.lengthBeats, channel };
            recFifo.finishedWrite (1);
        }
    }
    if (pos < numSamples) synth.render (l + pos, r + pos, numSamples - pos);

    // MIDI note-offs due in this block
    for (int i = 0; i < numOffs;)
    {
        auto& o = offs[(size_t) i];
        if (o.samplesLeft < numSamples)
        {
            if (o.midi) midi.addEvent (juce::MidiMessage::noteOff (o.channel, o.note), juce::jlimit (0, numSamples - 1, (int) o.samplesLeft));
            noteMonitor.noteOff (o.note);
            offs[(size_t) i] = offs[(size_t) --numOffs];
            continue;
        }
        o.samplesLeft -= numSamples;
        ++i;
    }

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.copyFrom (1, 0, work, 1, 0, numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.addFrom (0, 0, work, 1, 0, numSamples);
        buffer.applyGain (0, 0, numSamples, 0.5f);
    }

    // a copy of the world for the screen (~30 per second)
    snapCounter += numSamples;
    if (snapCounter >= (int) (sr / 30.0))
    {
        if (snapLock.tryEnter())
        {
            *snapshot = *world;
            snapPpq = ppq0;
            snapSinging = singing;
            hasSnapshot = true;
            snapCounter = 0;
            snapLock.exit();
        }
    }

    uiPpq = ppq0;
    uiTransportRunning = singing;
    hostSync.endBlock();
}

//==============================================================================
bool LifeCatProcessor::getSnapshot (World& into, double& ppq, bool& singing) const
{
    const juce::SpinLock::ScopedLockType sl (snapLock);
    if (! hasSnapshot) return false;
    into = *snapshot;
    ppq = snapPpq;
    singing = snapSinging;
    return true;
}

juce::MidiMessageSequence LifeCatProcessor::takeRecording()
{
    int s1, n1, s2, n2;
    recFifo.prepareToRead (recFifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) recorded.push_back (recBuffer[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) recorded.push_back (recBuffer[(size_t) (s2 + i)]);
    recFifo.finishedRead (n1 + n2);
    if (recorded.size() > 4000) recorded.erase (recorded.begin(), recorded.begin() + (long) (recorded.size() - 4000));

    juce::MidiMessageSequence seq;
    if (recorded.empty()) return seq;
    // the last 8 bars that were played, starting on a bar line
    const double last = recorded.back().ppq;
    const double from = std::floor ((last - 32.0) / 4.0) * 4.0;
    for (const auto& n : recorded)
    {
        if (n.ppq < from) continue;
        const double at = n.ppq - from;
        seq.addEvent (juce::MidiMessage::noteOn (n.channel, n.note, juce::jlimit (0.05f, 1.0f, n.vel)), at);
        seq.addEvent (juce::MidiMessage::noteOff (n.channel, n.note), at + juce::jmax (0.05, (double) n.lenBeats));
    }
    seq.updateMatchedPairs();
    return seq;
}

//==============================================================================
void LifeCatProcessor::applyPreset (int index)
{
    const auto& list = presets();
    if (index < 0 || index >= (int) list.size()) return;
    const auto& pr = list[(size_t) index];
    for (const auto& id : presetParamIds())
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->getDefaultValue());
    cat::SurpriseControls::setParam (*this, "theme", (float) pr.theme);
    for (const auto& [id, v] : pr.values) cat::SurpriseControls::setParam (*this, id, v);

    Command c;
    c.type = Command::NewWorld;
    c.seed = pr.seed;
    c.counts = pr.counts;
    c.plants = pr.plants;
    c.layout = pr.layout;
    // apply directly too, so the screen shows the new world at once (even before audio runs)
    auto fresh = std::make_unique<World>();
    fresh->reset (c.seed, c.counts, c.plants);
    applyLayout (*fresh, c.layout);
    {
        const juce::SpinLock::ScopedLockType sl (snapLock);
        *snapshot = *fresh;
        hasSnapshot = true;
    }
    {
        const juce::SpinLock::ScopedLockType sl (worldSwapLock);
        pendingWorld = std::move (fresh);
        hasPendingWorld = true;
    }
    currentPreset = index;
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
}

void LifeCatProcessor::surprise()
{
    getStateInformation (surpriseUndo);
    juce::Random r;
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    set ("theme", (float) r.nextInt (4));
    set ("scale", (float) r.nextInt (11));
    set ("key", (float) r.nextInt (12));
    set ("quantize", r.nextFloat() < 0.75f ? 1.0f : 0.0f);
    set ("grid", (float) (1 + r.nextInt (2)));
    set ("swing", r.nextFloat() < 0.5f ? 0.0f : r.nextFloat() * 50.0f);
    set ("density", 0.35f + r.nextFloat() * 0.45f);
    set ("brightness", r.nextFloat() * 1.2f - 0.6f);
    set ("attack", r.nextFloat() * 1.4f - 0.7f);
    set ("release", r.nextFloat() * 1.4f - 0.5f);
    set ("reverbMix", 0.2f + r.nextFloat() * 0.5f);
    set ("mutation", r.nextFloat() * 0.7f);
    Command c;
    c.type = Command::NewWorld;
    c.seed = (uint32_t) r.nextInt (1 << 30) + 1u;
    c.counts = { 6 + r.nextInt (14), 1 + r.nextInt (4), 2 + r.nextInt (8), r.nextInt (5), r.nextInt (3), r.nextInt (2), r.nextInt (12) };
    c.plants = 30 + r.nextInt (60);
    c.layout = r.nextInt (10);
    set ("weather", r.nextFloat() < 0.6f ? 1.0f : (float) r.nextInt (6));
    set ("dayLength", r.nextFloat() < 0.6f ? 0.0f : (float) (1 + r.nextInt (3)));
    send (c);
    currentPreset = -1;
}

int LifeCatProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String LifeCatProcessor::getProgramName (int i)
{
    return i >= 0 && i < (int) presets().size() ? juce::String (presets()[(size_t) i].name) : juce::String();
}

//==============================================================================
juce::ValueTree LifeCatProcessor::worldToTree (const World& w)
{
    juce::ValueTree t ("WORLD");
    t.setProperty ("seed", (int) w.seedValue, nullptr);
    t.setProperty ("nextId", (int) w.nextId, nullptr);
    t.setProperty ("rng", (int) w.rng.s, nullptr);
    t.setProperty ("time", w.time, nullptr);
    t.setProperty ("gen", w.maxGeneration, nullptr);
    t.setProperty ("births", w.births, nullptr);
    t.setProperty ("deaths", w.deaths, nullptr);
    for (const auto& c : w.creatures)
    {
        if (! c.alive) continue;
        juce::ValueTree n ("C");
        n.setProperty ("id", (int) c.id, nullptr);
        n.setProperty ("r", (int) c.role, nullptr);
        n.setProperty ("x", c.x, nullptr); n.setProperty ("y", c.y, nullptr);
        n.setProperty ("e", c.energy, nullptr); n.setProperty ("a", c.age, nullptr);
        n.setProperty ("g", c.generation, nullptr);
        n.setProperty ("fav", c.favourite, nullptr);
        n.setProperty ("pa", (int) c.parentA, nullptr); n.setProperty ("pb", (int) c.parentB, nullptr);
        juce::String genes;
        for (int i = 0; i < numGenes; ++i) genes << juce::String (c.genes[i], 4) << (i + 1 < numGenes ? " " : "");
        n.setProperty ("genes", genes, nullptr);
        t.appendChild (n, nullptr);
    }
    juce::String plants;
    for (const auto& pl : w.plants) if (pl.alive) plants << juce::String (pl.x, 3) << "," << juce::String (pl.y, 3) << "," << juce::String (pl.size, 2) << " ";
    t.setProperty ("plants", plants.trim(), nullptr);
    juce::String walls;
    for (int i = 0; i < w.numWalls; ++i)
    {
        const auto& wl = w.walls[(size_t) i];
        walls << juce::String (wl.x1, 3) << "," << juce::String (wl.y1, 3) << "," << juce::String (wl.x2, 3) << "," << juce::String (wl.y2, 3) << " ";
    }
    t.setProperty ("walls", walls.trim(), nullptr);
    juce::String zones;
    for (int i = 0; i < w.numZones; ++i)
    {
        const auto& z = w.zones[(size_t) i];
        zones << (int) z.type << "," << juce::String (z.x, 3) << "," << juce::String (z.y, 3) << "," << juce::String (z.r, 3) << ","
              << juce::String (z.dx, 3) << "," << juce::String (z.dy, 3) << " ";
    }
    t.setProperty ("zones", zones.trim(), nullptr);

    // v2: terrain, what has died, the sky
    juce::String terrain;
    for (auto v : w.terrain) terrain << (char) ('0' + v);
    t.setProperty ("terrain", terrain, nullptr);
    juce::String corpses;
    for (const auto& k : w.corpses)
        if (k.alive) corpses << juce::String (k.x, 3) << "," << juce::String (k.y, 3) << "," << juce::String (k.food, 3) << "," << juce::String (k.age, 1) << "," << (int) k.role << " ";
    t.setProperty ("corpses", corpses.trim(), nullptr);
    t.setProperty ("day", w.dayPhase, nullptr);
    t.setProperty ("season", w.season, nullptr);
    t.setProperty ("weather", (int) w.weather, nullptr);
    t.setProperty ("weatherTarget", (int) w.weatherTarget, nullptr);
    t.setProperty ("weatherAmount", w.weatherAmount, nullptr);
    t.setProperty ("weatherTimer", w.weatherTimer, nullptr);
    return t;
}

void LifeCatProcessor::worldFromTree (const juce::ValueTree& t, World& w)
{
    w.reset ((uint32_t) (int) t.getProperty ("seed", 1), {}, 0);
    w.nextId = (uint32_t) (int) t.getProperty ("nextId", 1);
    w.rng.s = (uint32_t) (int) t.getProperty ("rng", 12345);
    w.time = (double) t.getProperty ("time", 0.0);
    w.maxGeneration = (int) t.getProperty ("gen", 0);
    w.births = (int) t.getProperty ("births", 0);
    w.deaths = (int) t.getProperty ("deaths", 0);
    int ci = 0;
    for (const auto& n : t)
    {
        if (! n.hasType ("C") || ci >= maxCreatures) continue;
        auto& c = w.creatures[(size_t) ci++];
        c = Creature();
        c.alive = true;
        c.id = (uint32_t) (int) n.getProperty ("id", 1);
        c.role = (Role) juce::jlimit (0, numRoles - 1, (int) n.getProperty ("r", 0));
        c.x = (float) n.getProperty ("x", 0.5f); c.y = (float) n.getProperty ("y", 0.5f);
        c.energy = (float) n.getProperty ("e", 0.6f); c.age = (float) n.getProperty ("a", 0.0f);
        c.generation = (int) n.getProperty ("g", 0);
        c.favourite = (bool) n.getProperty ("fav", false);
        c.parentA = (uint32_t) (int) n.getProperty ("pa", 0); c.parentB = (uint32_t) (int) n.getProperty ("pb", 0);
        const auto genes = juce::StringArray::fromTokens (n.getProperty ("genes", "").toString(), " ", "");
        // worlds saved before the v2 genes get gentle defaults for them
        static const float v2Defaults[numGenes - numGenesV1] = { 0.5f, 0.3f, 0.5f, 0.5f, 0.5f, 0.3f, 0.15f, 0.1f, 0.4f, 0.2f };
        for (int i = 0; i < numGenes; ++i)
            c.genes[i] = i < genes.size() ? juce::jlimit (0.0f, 1.0f, genes[i].getFloatValue()) : (i >= numGenesV1 ? v2Defaults[i - numGenesV1] : 0.5f);
        c.group = c.id;
    }
    int pi = 0;
    for (const auto& tok : juce::StringArray::fromTokens (t.getProperty ("plants", "").toString(), " ", ""))
    {
        const auto v = juce::StringArray::fromTokens (tok, ",", "");
        if (v.size() < 3 || pi >= maxPlants) continue;
        w.plants[(size_t) pi++] = { true, v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue() };
    }
    for (const auto& tok : juce::StringArray::fromTokens (t.getProperty ("walls", "").toString(), " ", ""))
    {
        const auto v = juce::StringArray::fromTokens (tok, ",", "");
        if (v.size() >= 4) w.addWall (v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue(), v[3].getFloatValue());
    }
    for (const auto& tok : juce::StringArray::fromTokens (t.getProperty ("zones", "").toString(), " ", ""))
    {
        const auto v = juce::StringArray::fromTokens (tok, ",", "");
        if (v.size() >= 6) w.addZone ((ZoneType) juce::jlimit (0, 2, v[0].getIntValue()), v[1].getFloatValue(), v[2].getFloatValue(),
                                      v[3].getFloatValue(), v[4].getFloatValue(), v[5].getFloatValue());
    }
    const auto terrain = t.getProperty ("terrain", "").toString();
    for (int i = 0; i < (int) w.terrain.size() && i < terrain.length(); ++i)
        w.terrain[(size_t) i] = (uint8_t) juce::jlimit (0, numTerrains - 1, (int) (terrain[i] - '0'));
    int ki = 0;
    for (const auto& tok : juce::StringArray::fromTokens (t.getProperty ("corpses", "").toString(), " ", ""))
    {
        const auto v = juce::StringArray::fromTokens (tok, ",", "");
        if (v.size() < 5 || ki >= maxCorpses) continue;
        w.corpses[(size_t) ki++] = { true, v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue(), v[3].getFloatValue(),
                                     (Role) juce::jlimit (0, numRoles - 1, v[4].getIntValue()) };
    }
    w.dayPhase = (float) t.getProperty ("day", 0.4f);
    w.season = (float) t.getProperty ("season", 1.0f);
    w.weather = (Weather) juce::jlimit (0, numWeathers - 1, (int) t.getProperty ("weather", 0));
    w.weatherTarget = (Weather) juce::jlimit (0, numWeathers - 1, (int) t.getProperty ("weatherTarget", (int) w.weather));
    w.weatherAmount = (float) t.getProperty ("weatherAmount", 0.0f);
    w.weatherTimer = (float) t.getProperty ("weatherTimer", 20.0f);
}

void LifeCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("LIFECAT");
    root.appendChild (apvts.copyState(), nullptr);
    {
        World copy;
        double ppq = 0.0;
        bool singing = false;
        if (getSnapshot (copy, ppq, singing)) root.appendChild (worldToTree (copy), nullptr);
    }
    root.setProperty ("preset", currentPreset.load(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destData);
}

void LifeCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("LIFECAT")) return;
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    currentPreset = (int) root.getProperty ("preset", 0);
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
    auto wt = root.getChildWithName ("WORLD");
    if (wt.isValid())
    {
        auto fresh = std::make_unique<World>();
        worldFromTree (wt, *fresh);
        {
            const juce::SpinLock::ScopedLockType sl (snapLock);
            *snapshot = *fresh;
            hasSnapshot = true;
        }
        const juce::SpinLock::ScopedLockType sl (worldSwapLock);
        pendingWorld = std::move (fresh);
        hasPendingWorld = true;
    }
}

juce::AudioProcessorEditor* LifeCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<LifeCatEditor> (*this), "Life-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LifeCatProcessor();
}
