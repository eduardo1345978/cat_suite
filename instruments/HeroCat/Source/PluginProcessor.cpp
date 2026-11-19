#include "ProEditor.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include <map>

using namespace juce;

namespace
{
AudioParameterFloatAttributes percent()
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
}
AudioParameterFloatAttributes seconds (float maxS)
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([maxS] (float v, int)
    {
        const float s = 0.001f + v * v * maxS;
        return s < 1.0f ? String (roundToInt (s * 1000.0f)) + " ms" : String (s, 2) + " s";
    });
}
}

AudioProcessorValueTreeState::ParameterLayout HeroCatProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&l] (const char* id, const char* name, StringArray items, int def) { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto amount = [&l] (const char* id, const char* name, float def) { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, percent())); };
    auto toggle = [&l] (const char* id, const char* name, bool def) { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    // the sound
    choice ("srcA", "Sound A", hero::sourceNames(), 0);
    auto bNames = hero::sourceNames(); bNames.insert (0, "Off");
    choice ("srcB", "Sound B", bNames, 0);
    amount ("mix", "A / B", 0.5f);
    choice ("octB", "B Octave", StringArray { "-1", "0", "+1", "+2" }, 1);
    amount ("detune", "Detune", 0.3f);
    amount ("cutoff", "Cutoff", 0.72f);
    amount ("reso", "Resonance", 0.2f);
    amount ("envAmt", "Filter Env", 0.3f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.0f, 1.0f), 0.05f, seconds (3.0f)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 }, "Decay", NormalisableRange<float> (0.0f, 1.0f), 0.35f, seconds (3.0f)));
    amount ("sustain", "Sustain", 0.75f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (0.0f, 1.0f), 0.3f, seconds (5.0f)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 }, "Glide", NormalisableRange<float> (0.0f, 1.0f), 0.0f, seconds (0.6f)));
    amount ("vibrato", "Vibrato", 0.3f);
    // chord lock
    choice ("chordMode", "Chord Lock", hero::chordModeNames(), 0);
    choice ("key", "Key", hero::keyNames(), 0);
    choice ("scale", "Scale", hero::scaleNames(), 0);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "strum", 1 }, "Strum", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * v * 120.0f)) + " ms"; })));
    toggle ("voiceLead", "Voice Leading", true);
    toggle ("midiOut", "Send MIDI", true);
    // the arp
    choice ("arpMode", "Arp", hero::arpModeNames(), 0);
    choice ("rate", "Arp Rate", hero::rateNames(), 2);
    choice ("octaves", "Arp Octaves", StringArray { "1", "2", "3", "4" }, 0);
    amount ("gate", "Gate", 0.6f);
    amount ("swing", "Swing", 0.0f);
    amount ("chaos", "Chaos", 0.0f);
    toggle ("latch", "Latch", false);
    // texture, modulation, effects
    choice ("texture", "Texture", hero::textureNames(), 0);
    amount ("texLevel", "Texture Level", 0.4f);
    choice ("lfoShape", "LFO Shape", hero::lfoShapeNames(), 0);
    choice ("lfoTarget", "LFO Target", hero::lfoTargetNames(), 0);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfoRate", 1 }, "LFO Rate", NormalisableRange<float> (0.05f, 12.0f, 0.0f, 0.4f), 2.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; })));
    amount ("lfoDepth", "LFO Depth", 0.0f);
    amount ("drive", "Drive", 0.0f);
    amount ("crush", "Crush", 0.0f);
    amount ("chorus", "Chorus", 0.2f);
    amount ("delay", "Delay", 0.2f);
    amount ("reverb", "Reverb", 0.3f);
    amount ("width", "Width", 0.2f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

const std::vector<HeroCatProcessor::Preset>& HeroCatProcessor::presets()
{
    // srcA: 0 Saw, 1 Square, 2 Supersaw, 3 Chip, 4 Guitar, 5 Strings, 6 Flute, 7 Choir, 8 Bell; srcB: the same + 1 (0 = Off)
    // chordMode: 0 Off, 1 triads, 2 7ths, 3 anime add9, 4 Major .. 9 Min7; arpMode: 0 Off, 1 Up, 2 Down, 3 Up-Down, 4 Random, 5 Waterfall, 6 Pattern
    // rate: 0 1/4 .. 2 1/16, 3 1/32, 4 1/8T, 5 1/16T; texture: 0 Off, 1 Rain, 2 Birds, 3 Room, 4 Chimes, 5 Wind
    static const std::vector<Preset> list {
        { "Hero Theme",            {} },
        { "Neon Supersaw",         { { "srcA", 2 }, { "detune", 0.55f }, { "cutoff", 0.8f }, { "chorus", 0.35f }, { "delay", 0.3f }, { "reverb", 0.4f }, { "width", 0.5f } } },
        { "Overworld Flute",       { { "srcA", 6 }, { "vibrato", 0.6f }, { "attack", 0.15f }, { "cutoff", 0.85f }, { "delay", 0.25f }, { "reverb", 0.45f }, { "texture", 2 }, { "texLevel", 0.5f } } },
        { "Cherry Waterfall",      { { "srcA", 8 }, { "srcB", 6 }, { "mix", 0.35f }, { "chordMode", 3 }, { "arpMode", 5 }, { "rate", 2 }, { "octaves", 1 }, { "sustain", 0.3f }, { "release", 0.6f }, { "reverb", 0.6f }, { "delay", 0.3f }, { "texture", 4 }, { "texLevel", 0.5f } } },
        { "Rainy Tokyo Guitar",    { { "srcA", 4 }, { "chordMode", 2 }, { "strum", 0.55f }, { "sustain", 0.9f }, { "release", 0.5f }, { "cutoff", 0.62f }, { "chorus", 0.4f }, { "reverb", 0.4f }, { "texture", 1 }, { "texLevel", 0.6f } } },
        { "Boss Battle",           { { "srcA", 0 }, { "srcB", 3 }, { "mix", 0.5f }, { "octB", 0 }, { "chordMode", 7 }, { "arpMode", 1 }, { "rate", 3 }, { "octaves", 1 }, { "gate", 0.5f }, { "drive", 0.45f }, { "cutoff", 0.6f }, { "envAmt", 0.6f }, { "reso", 0.35f }, { "delay", 0.15f } } },
        { "Save Point",            { { "srcA", 8 }, { "srcB", 8 }, { "mix", 0.4f }, { "chordMode", 1 }, { "arpMode", 3 }, { "rate", 1 }, { "octaves", 1 }, { "sustain", 0.2f }, { "release", 0.55f }, { "reverb", 0.65f }, { "texture", 3 }, { "texLevel", 0.6f } } },
        { "Chip Hero",             { { "srcA", 3 }, { "vibrato", 0.5f }, { "glide", 0.2f }, { "crush", 0.25f }, { "chorus", 0.0f }, { "reverb", 0.15f }, { "delay", 0.3f } } },
        { "Final Stage Strings",   { { "srcA", 5 }, { "srcB", 3 }, { "mix", 0.3f }, { "chordMode", 1 }, { "attack", 0.35f }, { "release", 0.55f }, { "cutoff", 0.7f }, { "reverb", 0.6f }, { "width", 0.6f } } },
        { "Pixel Choir",           { { "srcA", 7 }, { "chordMode", 3 }, { "attack", 0.4f }, { "release", 0.6f }, { "vibrato", 0.2f }, { "crush", 0.15f }, { "reverb", 0.55f }, { "texture", 5 }, { "texLevel", 0.3f } } },
        { "Crystal Cave",          { { "srcA", 8 }, { "srcB", 7 }, { "mix", 0.25f }, { "octB", 2 }, { "chordMode", 2 }, { "arpMode", 6 }, { "rate", 2 }, { "octaves", 1 }, { "sustain", 0.15f }, { "release", 0.5f }, { "delay", 0.45f }, { "reverb", 0.6f } }, "9050905090509078" },
        { "Victory Fanfare",       { { "srcA", 1 }, { "srcB", 1 }, { "mix", 0.4f }, { "octB", 2 }, { "chordMode", 4 }, { "strum", 0.25f }, { "attack", 0.03f }, { "vibrato", 0.45f }, { "reverb", 0.35f } } },
        { "Lo-fi Bedroom",         { { "srcA", 4 }, { "chordMode", 3 }, { "strum", 0.4f }, { "arpMode", 6 }, { "rate", 1 }, { "swing", 0.45f }, { "crush", 0.2f }, { "cutoff", 0.55f }, { "chorus", 0.5f }, { "texture", 1 }, { "texLevel", 0.45f } }, "9060906090609060" },
        { "Moonlight Bells",       { { "srcA", 8 }, { "chordMode", 1 }, { "scale", 6 }, { "arpMode", 5 }, { "rate", 1 }, { "octaves", 1 }, { "sustain", 0.1f }, { "release", 0.65f }, { "reverb", 0.7f }, { "delay", 0.35f }, { "texture", 4 }, { "texLevel", 0.4f } } },
        { "Dice Dreams",           { { "srcA", 3 }, { "srcB", 9 }, { "mix", 0.4f }, { "chordMode", 2 }, { "arpMode", 1 }, { "rate", 2 }, { "octaves", 2 }, { "chaos", 0.6f }, { "lfoTarget", 0 }, { "lfoShape", 4 }, { "lfoRate", 4.0f }, { "lfoDepth", 0.4f }, { "delay", 0.35f }, { "reverb", 0.4f } } },
        { "Sakura Wind",           { { "srcA", 6 }, { "srcB", 9 }, { "mix", 0.3f }, { "scale", 6 }, { "chordMode", 1 }, { "arpMode", 5 }, { "rate", 4 }, { "vibrato", 0.5f }, { "reverb", 0.6f }, { "texture", 5 }, { "texLevel", 0.5f } } },
    };
    return list;
}

void HeroCatProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    craftName = {};
    const auto& pr = presets()[(size_t) index];
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output" && rp->getParameterID() != "midiOut") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : pr.values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    if (pr.pattern != nullptr)
        for (int i = 0; i < 16 && pr.pattern[i] != 0; ++i) engine.pattern[(size_t) i] = (float) (pr.pattern[i] - '0') / 9.0f;
}

const String HeroCatProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

String HeroCatProcessor::chordName() const
{
    const int root = engine.chordRoot.load();
    if (root < 0) return {};
    return hero::keyNames()[root] + hero::chordKindName (engine.chordKind.load());
}

String HeroCatProcessor::craft (Random& r)
{
    // keep the sound from before, for UNDO
    undoState = apvts.copyState();
    for (int i = 0; i < 16; ++i) undoPattern[(size_t) i] = engine.pattern[(size_t) i].load();
    undoName = craftName;

    std::map<String, float> v;
    auto between = [&r] (float a, float b) { return a + (b - a) * r.nextFloat(); };
    auto chance = [&r] (float x) { return r.nextFloat() < x; };

    // 1. the sounds: what kind of sound decides the envelope (plucks fall, pads swell, leads sing)
    const int a = r.nextInt (hero::numSources);
    v["srcA"] = (float) a;
    const bool pluck = a == hero::Guitar || a == hero::Bell, pad = a == hero::Strings || a == hero::Choir;
    v["srcB"] = chance (0.5f) ? 0.0f : (float) (1 + r.nextInt (hero::numSources));
    v["mix"] = between (0.2f, 0.55f);
    v["octB"] = chance (0.6f) ? 1.0f : chance (0.7f) ? 2.0f : chance (0.5f) ? 3.0f : 0.0f;
    v["detune"] = between (0.15f, 0.6f);
    v["cutoff"] = between (0.55f, 0.9f);
    v["reso"] = between (0.0f, 0.4f);
    v["envAmt"] = between (0.1f, 0.6f);
    v["vibrato"] = pluck ? between (0.0f, 0.2f) : between (0.15f, 0.6f);
    v["glide"] = ! pluck && ! pad && chance (0.3f) ? between (0.15f, 0.35f) : 0.0f;
    v["attack"] = pad ? between (0.25f, 0.45f) : pluck ? between (0.0f, 0.04f) : between (0.02f, 0.12f);
    v["decay"] = between (0.25f, 0.55f);
    v["sustain"] = pluck ? between (0.1f, 0.5f) : between (0.55f, 0.9f);
    v["release"] = pad ? between (0.45f, 0.7f) : between (0.25f, 0.55f);

    // 2. chords: from a scale most of the time (it always fits the song)
    v["chordMode"] = chance (0.35f) ? 0.0f : chance (0.7f) ? (float) (1 + r.nextInt (3)) : (float) (4 + r.nextInt (6));
    v["key"] = (float) r.nextInt (12);
    v["scale"] = chance (0.6f) ? (float) r.nextInt (2) : (float) r.nextInt (7);
    v["strum"] = a == hero::Guitar ? between (0.3f, 0.6f) : chance (0.25f) ? between (0.1f, 0.35f) : 0.0f;

    // 3. the arp: off half of the time, the waterfall more often than the others
    if (chance (0.5f))
    {
        const int modes[] = { hero::ArpUp, hero::ArpUpDown, hero::ArpWaterfall, hero::ArpWaterfall, hero::ArpPattern, hero::ArpDown, hero::ArpRandom };
        const int m = modes[r.nextInt (7)];
        v["arpMode"] = (float) m;
        static const int rates[] = { 1, 2, 2, 4, 5 };    // 1/8, 1/16 (twice as likely), 1/8T, 1/16T
        v["rate"] = (float) (m == hero::ArpWaterfall ? 1 + r.nextInt (2) : rates[r.nextInt (5)]);
        v["octaves"] = (float) r.nextInt (2);
        v["gate"] = between (0.3f, 0.8f);
        v["swing"] = chance (0.3f) ? between (0.15f, 0.45f) : 0.0f;
        v["chaos"] = chance (0.6f) ? 0.0f : chance (0.75f) ? between (0.1f, 0.35f) : between (0.4f, 0.7f);
        if (m == hero::ArpPattern) engine.randomisePattern (r);
        if (v["chordMode"] < 0.5f) v["chordMode"] = 1.0f;          // an arp needs more than one note
    }
    else v["arpMode"] = 0.0f;

    // 4. texture, LFO, effects
    v["texture"] = chance (0.5f) ? (float) (1 + r.nextInt (5)) : 0.0f;
    v["texLevel"] = between (0.3f, 0.6f);
    if (chance (0.4f))
    {
        v["lfoShape"] = (float) r.nextInt (6);
        static const int targets[] = { 0, 0, 1, 2, 3, 4 };   // cutoff twice as likely, never volume (it would pump)
        v["lfoTarget"] = (float) targets[r.nextInt (6)];
        v["lfoRate"] = between (0.2f, 6.0f);
        v["lfoDepth"] = between (0.1f, 0.4f);
    }
    else v["lfoDepth"] = 0.0f;
    v["drive"] = chance (0.25f) ? between (0.3f, 0.6f) : between (0.0f, 0.15f);
    v["crush"] = chance (0.25f) ? between (0.1f, 0.35f) : 0.0f;
    v["chorus"] = between (0.0f, 0.5f);
    v["delay"] = between (0.1f, 0.45f);
    v["reverb"] = between (0.2f, 0.65f);
    v["width"] = between (0.2f, 0.6f);

    for (auto& [id, x] : v)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (x));
    currentPreset = -1;

    // 5. a name, like a track on a game's soundtrack
    static const char* first[] = { "Crystal", "Neon", "Midnight", "Pixel", "Sakura", "Final", "Hidden", "Lost", "Turbo", "Sky", "Moon", "Sunset", "Frozen", "Golden", "Secret", "Haunted" };
    static const char* second[] = { "Cave", "Castle", "Highway", "Garden", "Shrine", "Arcade", "Ocean", "Tower", "Forest", "Dungeon", "Station", "Village", "Temple", "Harbor" };
    static const char* third[] = { "Theme", "Battle", "Ending", "Stage 2", "Loop", "Overture", "Save Room", "Credits", "Boss", "Waltz", "Chase", "Lullaby" };
    craftName = String (first[r.nextInt (16)]) + " " + second[r.nextInt (14)] + " " + third[r.nextInt (12)];
    return craftName;
}

bool HeroCatProcessor::undoCraft()
{
    if (! undoState.isValid()) return false;
    apvts.replaceState (undoState);
    for (int i = 0; i < 16; ++i) engine.pattern[(size_t) i] = undoPattern[(size_t) i];
    craftName = undoName;
    undoState = {};
    return true;
}

HeroCatProcessor::HeroCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HeroCat", createLayout())
{
    applyPreset (0);
}

bool HeroCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void HeroCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
}

hero::Settings HeroCatProcessor::settings() const
{
    hero::Settings s;
    s.srcA = c ("srcA"); s.srcB = c ("srcB") - 1; s.mix = p ("mix"); s.octB = c ("octB") - 1;
    s.detune = p ("detune"); s.cutoff = p ("cutoff"); s.reso = p ("reso"); s.envAmt = p ("envAmt");
    s.attack = p ("attack"); s.decay = p ("decay"); s.sustain = p ("sustain"); s.release = p ("release");
    s.glide = p ("glide"); s.vibrato = p ("vibrato");
    s.chordMode = c ("chordMode"); s.key = c ("key"); s.scale = c ("scale"); s.strum = p ("strum"); s.voiceLead = p ("voiceLead") > 0.5f;
    s.arpMode = c ("arpMode"); s.rate = c ("rate"); s.octaves = c ("octaves") + 1;
    s.gate = p ("gate"); s.swing = p ("swing"); s.chaos = p ("chaos"); s.latch = p ("latch") > 0.5f;
    s.texture = c ("texture"); s.texLevel = p ("texLevel");
    s.lfoShape = c ("lfoShape"); s.lfoTarget = c ("lfoTarget"); s.lfoRate = p ("lfoRate"); s.lfoDepth = p ("lfoDepth");
    s.drive = p ("drive"); s.crush = p ("crush"); s.chorus = p ("chorus"); s.delay = p ("delay"); s.reverb = p ("reverb"); s.width = p ("width");
    s.outDb = p ("output");
    s.bpm = lastBpm;
    return s;
}

void HeroCatProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    keyboard.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    auto s = settings();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) s.ppq = *q;
            s.playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }
    s.bpm = lastBpm;

    MidiBuffer out;
    engine.process (buffer, midi, p ("midiOut") > 0.5f ? &out : nullptr, s);
    midi.swapWith (out);      // the DAW gets the notes Hero-Cat really played (or nothing)
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), buffer.getNumSamples());
}

void HeroCatProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    String pat;
    for (auto& x : engine.pattern) pat << String (x.load(), 2) << " ";
    state.setProperty ("pattern", pat.trim(), nullptr);
    state.setProperty ("craftName", craftName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void HeroCatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        auto pat = StringArray::fromTokens (state.getProperty ("pattern", "").toString(), " ", "");
        for (int i = 0; i < 16 && i < pat.size(); ++i) engine.pattern[(size_t) i] = jlimit (0.0f, 1.0f, pat[i].getFloatValue());
        state.removeProperty ("preset", nullptr);
        state.removeProperty ("pattern", nullptr);
        craftName = state.getProperty ("craftName", "").toString();
        state.removeProperty ("craftName", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* HeroCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<HeroCatEditor> (*this), "Hero-Cat");
}

#ifndef HERO_CHECK
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<HeroCatProcessor>();
}
#endif
