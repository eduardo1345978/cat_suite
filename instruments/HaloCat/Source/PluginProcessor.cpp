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
}

AudioProcessorValueTreeState::ParameterLayout HaloCatProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "progression", 1 }, "Progression", halo::progressionNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "change", 1 }, "Change every", StringArray { "1 bar", "2 bars", "4 bars" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "orbits", 1 }, "Orbits", halo::orbitNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "spin", 1 }, "Spin", NormalisableRange<float> (0.25f, 4.0f, 0.0f, 0.5f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "x"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "vowel", 1 }, "Vowel", NormalisableRange<float> (0.0f, 3.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const auto n = halo::vowelNames();
            const int a = jlimit (0, 3, roundToInt (v));
            return std::abs (v - (float) a) < 0.15f ? n[a] : n[jlimit (0, 3, (int) v)] + "-" + n[jlimit (0, 3, (int) v + 1)];
        })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "choir", 1 }, "Choir", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "bells", 1 }, "Bells", NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "dust", 1 }, "Star Dust", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 }, "Glide", NormalisableRange<float> (0.0f, 1.0f), 0.4f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt ((0.02f + v * v * 1.5f) * 1000.0f)) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "shimmer", 1 }, "Shimmer", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "brightness", 1 }, "Brightness", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

const std::vector<HaloCatProcessor::Preset>& HaloCatProcessor::presets()
{
    // progression: 0 Pop, 1 Dreamy, 2 Epic, 3 Minor Night, 4 Andalusian, 5 Floating, 6 Jazz, 7 Hold
    // change: 0 1 bar, 1 2 bars, 2 4 bars; orbits: 0 Waltzing, 1 Calm, 2 Busy, 3 Polyrhythm, 4 Sparse; vowel 0 Ah .. 3 Ee
    static const std::vector<Preset> list {
        { "Halo",                 {} },
        { "Cathedral Night",      { { "progression", 3 }, { "change", 2 }, { "orbits", 1 }, { "vowel", 1.0f }, { "shimmer", 0.8f }, { "dust", 0.2f }, { "glide", 0.6f } } },
        { "Dreamy Planets",       { { "progression", 1 }, { "orbits", 3 }, { "vowel", 2.0f }, { "bells", 0.8f }, { "dust", 0.5f }, { "brightness", 0.65f } } },
        { "Epic Sky",             { { "progression", 2 }, { "change", 1 }, { "choir", 0.9f }, { "vowel", 0.0f }, { "shimmer", 0.7f }, { "bells", 0.4f } } },
        { "Music Box Galaxy",     { { "progression", 0 }, { "orbits", 2 }, { "choir", 0.25f }, { "bells", 0.9f }, { "dust", 0.6f }, { "brightness", 0.8f } } },
        { "Floating Monks",       { { "progression", 5 }, { "change", 2 }, { "orbits", 4 }, { "vowel", 2.0f }, { "choir", 0.85f }, { "bells", 0.3f }, { "glide", 0.85f }, { "shimmer", 0.75f } } },
        { "Andalusian Moon",      { { "progression", 4 }, { "change", 0 }, { "orbits", 0 }, { "vowel", 1.0f }, { "glide", 0.3f } } },
        { "Late Night Jazz",      { { "progression", 6 }, { "change", 0 }, { "orbits", 3 }, { "vowel", 1.5f }, { "brightness", 0.35f }, { "dust", 0.15f } } },
        { "Busy Little Stars",    { { "progression", 0 }, { "orbits", 2 }, { "spin", 2.0f }, { "dust", 0.8f }, { "choir", 0.5f } } },
        { "Ee Choir",             { { "progression", 7 }, { "vowel", 3.0f }, { "choir", 0.9f }, { "bells", 0.2f }, { "dust", 0.1f }, { "shimmer", 0.6f } } },
        { "Slow Orbit",           { { "progression", 1 }, { "change", 2 }, { "orbits", 4 }, { "spin", 0.5f }, { "glide", 1.0f }, { "shimmer", 0.9f } } },
        { "Only Bells",           { { "progression", 0 }, { "choir", 0.0f }, { "bells", 1.0f }, { "orbits", 3 }, { "dust", 0.4f } } },
    };
    return list;
}

void HaloCatProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String HaloCatProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

String HaloCatProcessor::chordName() const
{
    const int root = engine.chordRoot.load();
    if (root < 0) return {};
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const char* kinds[] = { "", "m", "maj7", "m7", "sus2", "7" };
    const int k = engine.chordKind.load();
    return String (names[root]) + (k >= 0 && k < 6 ? kinds[k] : "");
}

HaloCatProcessor::HaloCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HaloCat", createLayout())
{
    applyPreset (0);
}

bool HaloCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void HaloCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
}

void HaloCatProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    keyboard.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    halo::Settings s;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) s.ppq = *q;
            s.playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }
    s.bpm = lastBpm;
    s.progression = roundToInt (p ("progression"));
    const float changes[] = { 1.0f, 2.0f, 4.0f };
    s.change = changes[jlimit (0, 2, roundToInt (p ("change")))];
    s.orbits = roundToInt (p ("orbits"));
    s.spin = p ("spin"); s.vowel = p ("vowel");
    s.choir = p ("choir"); s.bells = p ("bells"); s.dust = p ("dust"); s.glide = p ("glide");
    s.shimmer = p ("shimmer"); s.brightness = p ("brightness"); s.outDb = p ("output");

    // the notes: all at the start of the block (a chord changes by itself only on the bar)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allOff();
    }
    engine.process (buffer, s);
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), buffer.getNumSamples());
}

void HaloCatProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void HaloCatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        state.removeProperty ("preset", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* HaloCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<HaloCatEditor> (*this), "Halo-Cat");
}

#ifndef HALO_CHECK
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<HaloCatProcessor>();
}
#endif
