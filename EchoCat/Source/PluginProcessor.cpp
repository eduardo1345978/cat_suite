#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace echocat;

//==============================================================================
const std::vector<EchoCatProcessor::Preset>& EchoCatProcessor::presets()
{
    // values: division index (0 1/32, 1 1/16, 2 1/16., 3 1/8T, 4 1/8, 5 1/8., 6 1/4T, 7 1/4, 8 1/4., 9 1/2, 10 bar)
    static const std::vector<Preset> list = {
        { "Dotted Eighth",      "Echo",   { { "division", 5 }, { "feedback", 0.38f }, { "mix", 0.32f }, { "pingpong", 0.35f }, { "tone", 0.7f }, { "lowcut", 0.2f } } },
        { "Slapback",           "Echo",   { { "sync", 0 }, { "timeMs", 95 }, { "feedback", 0.08f }, { "mix", 0.38f }, { "tone", 0.6f }, { "drive", 0.25f } } },
        { "Quarter Repeats",    "Echo",   { { "division", 7 }, { "feedback", 0.45f }, { "mix", 0.3f }, { "tone", 0.65f }, { "lowcut", 0.25f } } },
        { "Ping-Pong Party",    "Echo",   { { "division", 4 }, { "feedback", 0.5f }, { "mix", 0.4f }, { "pingpong", 1.0f }, { "tone", 0.8f }, { "lowcut", 0.3f } } },
        { "Vocal Polish",       "Vocals", { { "division", 4 }, { "feedback", 0.22f }, { "mix", 0.2f }, { "pingpong", 0.5f }, { "tone", 0.55f }, { "lowcut", 0.45f },
                                            { "space", 0.25f }, { "duck", 0.7f } } },
        { "Ducked Vocal Echo",  "Vocals", { { "division", 7 }, { "feedback", 0.4f }, { "mix", 0.45f }, { "pingpong", 0.6f }, { "tone", 0.55f }, { "lowcut", 0.4f },
                                            { "space", 0.15f }, { "duck", 1.0f } } },
        { "Big Ballad",         "Vocals", { { "division", 8 }, { "feedback", 0.45f }, { "mix", 0.3f }, { "pingpong", 0.7f }, { "tone", 0.5f }, { "lowcut", 0.35f },
                                            { "space", 0.6f }, { "duck", 0.6f } } },
        { "Tape Space",         "Tape",   { { "division", 5 }, { "feedback", 0.55f }, { "mix", 0.35f }, { "tone", 0.4f }, { "lowcut", 0.3f }, { "wobble", 0.35f },
                                            { "drive", 0.4f }, { "space", 0.3f } } },
        { "Lo-fi Cassette",     "Tape",   { { "division", 4 }, { "feedback", 0.45f }, { "mix", 0.4f }, { "tone", 0.2f }, { "lowcut", 0.5f }, { "wobble", 0.8f },
                                            { "drive", 0.6f } } },
        { "Dub Siren",          "Tape",   { { "division", 5 }, { "feedback", 0.85f }, { "mix", 0.5f }, { "pingpong", 0.3f }, { "tone", 0.3f }, { "lowcut", 0.35f },
                                            { "wobble", 0.2f }, { "drive", 0.55f }, { "space", 0.2f } } },
        { "Underwater",         "Tape",   { { "division", 6 }, { "feedback", 0.6f }, { "mix", 0.45f }, { "pingpong", 0.5f }, { "tone", 0.05f }, { "lowcut", 0.1f },
                                            { "wobble", 1.0f }, { "space", 0.4f } } },
        { "Shimmer Cathedral",  "Space",  { { "division", 8 }, { "feedback", 0.35f }, { "mix", 0.25f }, { "pingpong", 0.6f }, { "tone", 0.75f }, { "lowcut", 0.35f },
                                            { "space", 0.85f }, { "shimmer", 0.7f } } },
        { "Dreamy Haze",        "Space",  { { "division", 5 }, { "feedback", 0.5f }, { "mix", 0.3f }, { "pingpong", 0.8f }, { "tone", 0.5f }, { "lowcut", 0.3f },
                                            { "wobble", 0.3f }, { "space", 0.7f }, { "shimmer", 0.35f } } },
        { "Ambient Wash",       "Space",  { { "division", 9 }, { "feedback", 0.65f }, { "mix", 0.28f }, { "pingpong", 1.0f }, { "tone", 0.45f }, { "lowcut", 0.4f },
                                            { "wobble", 0.2f }, { "space", 0.85f }, { "shimmer", 0.5f }, { "dry", 0.8f } } },
        { "Starlight Plucks",   "Space",  { { "division", 3 }, { "feedback", 0.55f }, { "mix", 0.35f }, { "pingpong", 1.0f }, { "tone", 0.9f }, { "lowcut", 0.5f },
                                            { "space", 0.45f }, { "shimmer", 0.6f } } },
        { "Just the Room",      "Space",  { { "feedback", 0.0f }, { "mix", 0.0f }, { "space", 0.5f } } },
        // mode: 0 normal, 1 reverse, 2 steps; pitch in semitones per echo
        { "Backwards Dream",    "Reverse", { { "mode", 1 }, { "division", 7 }, { "feedback", 0.35f }, { "mix", 0.45f }, { "pingpong", 0.5f }, { "tone", 0.6f },
                                             { "lowcut", 0.3f }, { "space", 0.4f } } },
        { "Reverse Vocal Swell", "Reverse", { { "mode", 1 }, { "division", 9 }, { "feedback", 0.2f }, { "mix", 0.55f }, { "tone", 0.55f }, { "lowcut", 0.45f },
                                             { "space", 0.5f }, { "duck", 0.5f } } },
        { "Rising Stairs",      "Pitch",  { { "pitch", 12 }, { "division", 4 }, { "feedback", 0.5f }, { "mix", 0.35f }, { "pingpong", 0.6f }, { "tone", 0.75f },
                                            { "lowcut", 0.35f }, { "space", 0.3f } } },
        { "Falling Fifths",     "Pitch",  { { "pitch", -7 }, { "division", 5 }, { "feedback", 0.55f }, { "mix", 0.35f }, { "pingpong", 0.4f }, { "tone", 0.6f },
                                            { "lowcut", 0.15f } } },
        { "Octave Down Dub",    "Pitch",  { { "pitch", -12 }, { "division", 7 }, { "feedback", 0.5f }, { "mix", 0.4f }, { "tone", 0.4f }, { "drive", 0.4f },
                                            { "wobble", 0.2f } } },
        { "Gallop Steps",       "Steps",  { { "mode", 2 }, { "division", 1 }, { "feedback", 0.3f }, { "mix", 0.3f }, { "pingpong", 0.5f }, { "tone", 0.7f },
                                            { "step1", 1.0f }, { "step2", 0.0f }, { "step3", 0.8f }, { "step4", 0.8f }, { "step5", 0.0f }, { "step6", 0.6f },
                                            { "step7", 0.0f }, { "step8", 0.4f } } },
        { "Clave Echoes",       "Steps",  { { "mode", 2 }, { "division", 1 }, { "feedback", 0.25f }, { "mix", 0.4f }, { "pingpong", 0.8f }, { "tone", 0.75f },
                                            { "lowcut", 0.3f }, { "step1", 0.0f }, { "step2", 0.0f }, { "step3", 1.0f }, { "step4", 0.0f }, { "step5", 0.0f },
                                            { "step6", 0.8f }, { "step7", 0.0f }, { "step8", 0.6f } } },
        { "Bouncing Ball",      "Steps",  { { "mode", 2 }, { "division", 1 }, { "feedback", 0.0f }, { "mix", 0.45f }, { "tone", 0.8f },
                                            { "step1", 1.0f }, { "step2", 0.85f }, { "step3", 0.7f }, { "step4", 0.55f }, { "step5", 0.42f }, { "step6", 0.3f },
                                            { "step7", 0.2f }, { "step8", 0.1f } } },
        { "Singing Strings",    "Resonator", { { "mode", 3 }, { "resNote", 48 }, { "resChord", 3 }, { "feedback", 0.55f }, { "mix", 0.5f }, { "tone", 0.6f },
                                               { "pingpong", 0.6f }, { "space", 0.35f } } },
        { "Dark Drone Box",     "Resonator", { { "mode", 3 }, { "resNote", 36 }, { "resChord", 2 }, { "feedback", 0.85f }, { "mix", 0.45f }, { "tone", 0.25f },
                                               { "pingpong", 0.4f }, { "space", 0.5f } } },
        { "Metal Cluster",      "Resonator", { { "mode", 3 }, { "resNote", 60 }, { "resChord", 8 }, { "feedback", 0.7f }, { "mix", 0.4f }, { "tone", 0.9f },
                                               { "pingpong", 0.8f }, { "shimmer", 0.3f }, { "space", 0.3f } } },
        { "Follow the Chords",  "Resonator", { { "mode", 3 }, { "resNote", 48 }, { "resChord", 9 }, { "feedback", 0.6f }, { "mix", 0.45f }, { "tone", 0.55f },
                                               { "pingpong", 0.5f }, { "space", 0.4f } } },
    };
    return list;
}

namespace
{
    const std::pair<const char*, float> defaults[] = {
        { "sync", 1 }, { "division", 5 }, { "timeMs", 350 }, { "feedback", 0.4f }, { "mix", 0.35f }, { "dry", 1.0f }, { "pingpong", 0.0f },
        { "tone", 0.7f }, { "lowcut", 0.15f }, { "wobble", 0.0f }, { "drive", 0.0f }, { "space", 0.0f }, { "shimmer", 0.0f }, { "duck", 0.0f },
        { "freeze", 0 }, { "output", 0.0f }, { "mode", 0 }, { "pitch", 0 }, { "resNote", 48 }, { "resChord", 3 },
        { "step1", 1.0f }, { "step2", 0.0f }, { "step3", 0.8f }, { "step4", 0.0f }, { "step5", 0.6f }, { "step6", 0.6f }, { "step7", 0.0f }, { "step8", 0.4f },
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout EchoCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto percent = [] { return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })
                                                             .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; }); };
    auto pct = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, percent()));
    };
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Sync", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Time (synced)", divisionNames(), 5));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "timeMs", 1 }, "Time (ms)", NormalisableRange<float> (10.0f, 2000.0f, 0.0f, 0.4f), 350.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })
                                                                                      .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); })));
    pct ("feedback", "Feedback", 0.4f);
    pct ("mix", "Echo", 0.35f);
    pct ("dry", "Dry", 1.0f);
    pct ("pingpong", "Ping-Pong", 0.0f);
    pct ("tone", "Tone", 0.7f);
    pct ("lowcut", "Low Cut", 0.15f);
    pct ("wobble", "Wobble", 0.0f);
    pct ("drive", "Drive", 0.0f);
    pct ("space", "Space", 0.0f);
    pct ("shimmer", "Shimmer", 0.0f);
    pct ("duck", "Duck", 0.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Echo Mode", modeNames(), 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "resNote", 1 }, "Resonator Note", 24, 84, 48,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 4); })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "resChord", 1 }, "Resonator Chord", chordNames(), 3));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "pitch", 1 }, "Pitch per Echo", -12, 12, 0,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("0 st") : (v > 0 ? "+" : "") + String (v) + " st"; })));
    {
        const float def[] = { 1.0f, 0.0f, 0.8f, 0.0f, 0.6f, 0.6f, 0.0f, 0.4f };
        for (int i = 0; i < numSteps; ++i)
        {
            const String id = "step" + String (i + 1);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, "Step " + String (i + 1), NormalisableRange<float> (0.0f, 1.0f), def[i], percent()));
        }
    }
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
                                                                                      .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); })));
    return layout;
}

EchoCatProcessor::EchoCatProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            refs[r->getParameterID().toStdString()] = apvts.getRawParameterValue (r->getParameterID());
    for (int i = 0; i < numSteps; ++i) stepRefs[(size_t) i] = apvts.getRawParameterValue ("step" + juce::String (i + 1));
    applyPreset (0);

    // Cat Link: the resonator can ring on Chord-Cat's chord, in Micro-Cat's tuning
    cat::LinkClient::Caps caps;
    caps.receiveChords = true;
    caps.receiveChordsText = "Resonate on Chord-Cat's chords";
    caps.receiveTuning = true;
    presetManager.enableLink (apvts, caps);
    linkMidi.ensureSize (512);
}

float EchoCatProcessor::p (const char* id) const
{
    auto it = refs.find (id);
    return it != refs.end() && it->second != nullptr ? it->second->load() : 0.0f;
}

Settings EchoCatProcessor::readSettings (double bpm) const
{
    Settings s;
    s.sync = p ("sync") > 0.5f;
    s.division = juce::jlimit (0, numDivisions - 1, juce::roundToInt (p ("division")));
    s.timeMs = p ("timeMs");
    s.bpm = bpm;
    s.feedback = p ("feedback");
    s.mix = p ("mix");
    s.dry = p ("dry");
    s.pingpong = p ("pingpong");
    s.tone = p ("tone");
    s.lowcut = p ("lowcut");
    s.wobble = p ("wobble");
    s.drive = p ("drive");
    s.space = p ("space");
    s.shimmer = p ("shimmer");
    s.duck = p ("duck");
    s.freeze = p ("freeze") > 0.5f;
    s.outputDb = p ("output");
    s.mode = juce::jlimit (0, 3, juce::roundToInt (p ("mode")));
    // resonator strings: a chord shape on the note, or Chord-Cat's chord; Micro-Cat's tuning on top (Cat Link)
    {
        const int root = juce::roundToInt (p ("resNote"));
        const int chord = juce::roundToInt (p ("resChord"));
        std::array<int, maxStrings> notes {};
        int count = 0;
        if (chord == chordFollow)
            for (int k = 0; k < 128 && count < maxStrings; ++k)
                if (linkChord[(size_t) k].load (std::memory_order_relaxed)) notes[(size_t) count++] = k;
        if (count == 0)
        {
            count = chordShape (chord == chordFollow ? 3 : chord, notes);
            for (int i = 0; i < count; ++i) notes[(size_t) i] += root;
        }
        auto* link = presetManager.linkClient.get();
        for (int i = 0; i < count; ++i)
        {
            const float semis = (float) juce::jlimit (12, 108, notes[(size_t) i]);
            s.stringPitch[(size_t) i] = link != nullptr ? link->tuned (semis) : semis;
        }
        s.numStrings = count;
    }
    s.pitch = (float) juce::roundToInt (p ("pitch"));
    for (int i = 0; i < numSteps; ++i) s.steps[(size_t) i] = stepRefs[(size_t) i] != nullptr ? stepRefs[(size_t) i]->load() : 0.0f;
    return s;
}

//==============================================================================
int EchoCatProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String EchoCatProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}

void EchoCatProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    for (auto& [id, v] : defaults) set (id, v);
    for (auto& [id, v] : presets()[(size_t) index].values) set (id, v);
    currentPreset = index;
}

//==============================================================================
bool EchoCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void EchoCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    limiter.prepare (sampleRate);
    monoRight.assign ((size_t) juce::jmax (32, samplesPerBlock) * 2, 0.0f);
}

void EchoCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() == 0) return;
    const int ins = getTotalNumInputChannels();
    for (int ch = ins; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    // Cubase's tempo (else the last one known / 120)
    double bpm = uiBpm.load();
    bool fromHost = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) { bpm = *b; fromHost = true; }
    uiBpm = bpm;
    uiHostBpm = fromHost;

    // Chord-Cat's chord over Cat Link (as note-ons / note-offs): which notes are held
    if (auto* link = presetManager.linkClient.get())
    {
        linkMidi.clear();
        link->processInput (linkMidi, n);
        for (const auto meta : linkMidi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn()) linkChord[(size_t) m.getNoteNumber()] = true;
            else if (m.isNoteOff()) linkChord[(size_t) m.getNoteNumber()] = false;
        }
    }
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (r != nullptr && ins == 1) juce::FloatVectorOperations::copy (r, l, n);   // mono in -> stereo out
    if (r == nullptr)
    {
        if ((int) monoRight.size() < n) return;          // (sized in prepareToPlay)
        juce::FloatVectorOperations::copy (monoRight.data(), l, n);
        engine.process (l, monoRight.data(), n, readSettings (bpm));
        juce::FloatVectorOperations::add (l, monoRight.data(), n);
        juce::FloatVectorOperations::multiply (l, 0.5f, n);
        return;
    }
    engine.process (l, r, n, readSettings (bpm));
    limiter.process (l, r, n);
}

//==============================================================================
void EchoCatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void EchoCatProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentPreset = (int) state.getProperty ("preset", -1);
    state.removeProperty ("preset", nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* EchoCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<EchoCatEditor> (*this), "Echo-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EchoCatProcessor();
}
