#include "ProEditor.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

using namespace juce;

namespace
{
AudioParameterFloatAttributes percent()
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
}
AudioParameterFloatAttributes decibels()
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
}
}

String MeowCatProcessor::sizeName (float s)
{
    return s < 0.2f ? "kitten" : s < 0.55f ? "house cat" : s < 0.8f ? "big cat" : "lion";
}

String MeowCatProcessor::moodName (float m)
{
    return m < 0.2f ? "sweet" : m < 0.4f ? "chatty" : m < 0.6f ? "hungry" : m < 0.8f ? "grumpy" : "furious";
}

AudioProcessorValueTreeState::ParameterLayout MeowCatProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Meow", "Sing", "Chatter", "Purr" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "size", 1 }, "Size", NormalisableRange<float> (0.0f, 1.0f), 0.4f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return sizeName (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mood", 1 }, "Mood", NormalisableRange<float> (0.0f, 1.0f), 0.45f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return moodName (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, "Length", NormalisableRange<float> (0.0f, 1.0f), 0.35f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (meow::meowSeconds (v, 0.45f), 1) + " s"; })));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "choir", 1 }, "Choir", 1, meow::maxCats, 1,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 1 ? String ("1 cat") : String (v) + " cats"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaos", 1 }, "Chaos", NormalisableRange<float> (0.0f, 1.0f), 0.15f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "purr", 1 }, "Purr", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "hiss", 1 }, "Hiss", NormalisableRange<float> (0.0f, 1.0f), 0.3f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("never") : String (roundToInt (v * 100.0f)) + "%"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "alley", 1 }, "Alley", NormalisableRange<float> (0.0f, 1.0f), 0.25f, percent()));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v > 0 ? "+" + String (v) : String (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f), 0.0f, decibels()));
    return l;
}

const std::vector<MeowCatProcessor::Preset>& MeowCatProcessor::presets()
{
    // mode: 0 meow, 1 sing, 2 chatter, 3 purr
    static const std::vector<Preset> list {
        { "House Cat",               { } },
        { "Tiny Kitten",             { { "size", 0.05f }, { "mood", 0.1f }, { "length", 0.12f }, { "hiss", 0.0f } } },
        { "Hungry at 6 AM",          { { "mood", 0.62f }, { "length", 0.6f }, { "chaos", 0.3f } } },
        { "Cat Choir",               { { "mode", 1 }, { "choir", 5 }, { "mood", 0.4f }, { "alley", 0.45f }, { "hiss", 0.0f } } },
        { "Opera Cat",               { { "mode", 1 }, { "size", 0.5f }, { "mood", 0.58f }, { "alley", 0.65f }, { "hiss", 0.0f } } },
        { "Kitten Choir",            { { "mode", 1 }, { "size", 0.05f }, { "mood", 0.25f }, { "choir", 5 }, { "alley", 0.4f }, { "hiss", 0.0f } } },
        { "Midnight Serenade",       { { "mode", 1 }, { "mood", 0.72f }, { "length", 0.55f }, { "alley", 0.8f }, { "hiss", 0.0f } } },
        { "Alley Fight",             { { "mood", 1.0f }, { "choir", 3 }, { "chaos", 0.6f }, { "hiss", 0.8f }, { "alley", 0.6f }, { "length", 0.55f } } },
        { "Hiss!",                   { { "mood", 0.95f }, { "hiss", 1.0f } } },
        { "Lion King",               { { "size", 1.0f }, { "mood", 0.8f }, { "length", 0.5f }, { "octave", -1 }, { "alley", 0.5f } } },
        { "Big Cat Sings",           { { "mode", 1 }, { "size", 0.85f }, { "mood", 0.5f }, { "octave", -1 }, { "alley", 0.4f }, { "hiss", 0.0f } } },
        { "Siamese",                 { { "size", 0.35f }, { "mood", 0.78f }, { "length", 0.7f }, { "chaos", 0.25f } } },
        { "Purring on Your Lap",     { { "mode", 3 }, { "size", 0.45f }, { "alley", 0.1f } } },
        { "Sleepy Cat",              { { "mood", 0.15f }, { "length", 0.75f }, { "purr", 0.5f }, { "alley", 0.2f }, { "hiss", 0.0f } } },
        { "Birds at the Window",     { { "mode", 2 }, { "size", 0.35f }, { "mood", 0.3f }, { "chaos", 0.4f }, { "alley", 0.15f } } },
        { "Chatter Choir",           { { "mode", 2 }, { "choir", 4 }, { "chaos", 0.6f }, { "alley", 0.3f } } },
        { "Hundred Cats",            { { "choir", 6 }, { "chaos", 0.85f }, { "alley", 0.55f }, { "length", 0.45f } } },
        { "Broken Cat",              { { "chaos", 1.0f }, { "mood", 0.6f }, { "length", 0.5f } } },
        { "Cat Bass",                { { "mode", 1 }, { "size", 1.0f }, { "mood", 0.35f }, { "octave", -2 }, { "alley", 0.1f }, { "hiss", 0.0f } } },
        { "Squeaky Toy",             { { "size", 0.0f }, { "mood", 0.0f }, { "length", 0.0f }, { "octave", 1 }, { "hiss", 0.0f } } },
        { "Grumpy Old Tom",          { { "size", 0.75f }, { "mood", 0.85f }, { "length", 0.65f }, { "chaos", 0.2f }, { "purr", 0.25f } } },
        { "Space Cats",              { { "mode", 1 }, { "choir", 4 }, { "chaos", 0.5f }, { "alley", 1.0f }, { "mood", 0.3f }, { "hiss", 0.0f } } },
    };
    return list;
}

void MeowCatProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String MeowCatProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

MeowCatProcessor::MeowCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MeowCat", createLayout())
{
    applyPreset (0);
}

bool MeowCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void MeowCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
    keyboard.reset();
    monoSpare.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
}

void MeowCatProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboard.processNextMidiBuffer (midi, 0, n, true);

    meow::Settings s;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) s.ppq = *q;
            s.playing = pos->getIsPlaying();
        }
    s.bpm = lastBpm;
    s.mode = (meow::Mode) jlimit (0, 3, roundToInt (p ("mode")));
    s.size = p ("size"); s.mood = p ("mood"); s.length = p ("length");
    s.choir = jlimit (1, meow::maxCats, roundToInt (p ("choir")));
    s.chaos = p ("chaos"); s.purr = p ("purr"); s.hiss = p ("hiss"); s.alley = p ("alley");
    s.octave = jlimit (-2, 2, roundToInt (p ("octave")));
    s.outDb = p ("output");
    engine.setSettings (s);

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool mono = r == nullptr;
    if (mono)
    {
        if ((int) monoSpare.size() < n) monoSpare.resize ((size_t) n);
        std::fill (monoSpare.begin(), monoSpare.begin() + n, 0.0f);
        r = monoSpare.data();
    }

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int at = jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.render (l + pos, r + pos, at - pos); pos = at; }
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel()) engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc == 1) engine.modWheel ((float) m.getControllerValue() / 127.0f);
            else if (cc == 64) engine.sustain (m.getControllerValue() >= 64);
            else if (cc == 120 || cc == 123) engine.allNotesOff();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < n) engine.render (l + pos, r + pos, n - pos);
    engine.finish (l, r, n);
    limiter.process (l, r, n);
    if (mono)
        for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + r[i]);
}

void MeowCatProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MeowCatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        state.removeProperty ("preset", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* MeowCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<MeowCatEditor> (*this), "Meow-Cat");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<MeowCatProcessor>();
}
