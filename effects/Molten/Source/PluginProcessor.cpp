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

AudioProcessorValueTreeState::ParameterLayout MoltenProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "heat", 1 }, "Heat", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 1200.0f)) + " C"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "drip", 1 }, "Drip", NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "wobble", 1 }, "Wobble", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "body", 1 }, "Body", NormalisableRange<float> (-6.0f, 9.0f), 2.0f, decibels()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "dripTime", 1 }, "Drip time", molten::dripTimeNames(), 2));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "monoBass", 1 }, "Mono bass", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f), 0.0f, decibels()));
    return l;
}

const std::vector<MoltenProcessor::Preset>& MoltenProcessor::presets()
{
    static const std::vector<Preset> list {
        { "Warm Glow",          { { "heat", 0.3f }, { "drip", 0.1f }, { "wobble", 0.15f }, { "body", 2.0f }, { "dripTime", 2 } } },
        { "808 in the Sun",     { { "heat", 0.62f }, { "drip", 0.12f }, { "wobble", 0.1f }, { "body", 5.0f }, { "dripTime", 1 } } },
        { "Reese Lava Lamp",    { { "heat", 0.72f }, { "drip", 0.45f }, { "wobble", 0.35f }, { "body", 3.0f }, { "dripTime", 2 } } },
        { "Phonk Furnace",      { { "heat", 0.9f }, { "drip", 0.3f }, { "wobble", 0.2f }, { "body", 6.0f }, { "dripTime", 1 } } },
        { "Supersaw Meltdown",  { { "heat", 0.55f }, { "drip", 0.55f }, { "wobble", 0.4f }, { "body", 1.0f }, { "dripTime", 4 } } },
        { "Trance Candle",      { { "heat", 0.4f }, { "drip", 0.65f }, { "wobble", 0.2f }, { "body", 0.0f }, { "dripTime", 2 } } },
        { "Magma Sub",          { { "heat", 0.5f }, { "drip", 0.0f }, { "wobble", 0.05f }, { "body", 8.0f }, { "dripTime", 2 } } },
        { "Dripping Pluck",     { { "heat", 0.45f }, { "drip", 0.8f }, { "wobble", 0.25f }, { "body", 1.0f }, { "dripTime", 0 } } },
        { "Wax Tape Bus",       { { "heat", 0.25f }, { "drip", 0.0f }, { "wobble", 0.35f }, { "body", 1.5f }, { "mix", 0.7f } } },
        { "Lo-Fi Puddle",       { { "heat", 0.6f }, { "drip", 0.5f }, { "wobble", 0.7f }, { "body", 2.0f }, { "dripTime", 3 } } },
        { "Hardstyle Forge",    { { "heat", 1.0f }, { "drip", 0.15f }, { "wobble", 0.0f }, { "body", 4.0f }, { "dripTime", 0 } } },
        { "Slow Ooze",          { { "heat", 0.35f }, { "drip", 0.7f }, { "wobble", 0.6f }, { "body", 2.0f }, { "dripTime", 5 } } },
        { "Total Meltdown",     { { "heat", 1.0f }, { "drip", 0.85f }, { "wobble", 0.8f }, { "body", 5.0f }, { "dripTime", 2 } } },
    };
    return list;
}

void MoltenProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String MoltenProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

MoltenProcessor::MoltenProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Molten", createLayout())
{
}

bool MoltenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void MoltenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latencySamples());
    limiter.prepare (sampleRate);
}

void MoltenProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;

    molten::Settings s;
    s.heat = p ("heat"); s.drip = p ("drip"); s.wobble = p ("wobble"); s.bodyDb = p ("body"); s.mix = p ("mix");
    s.dripBeats = molten::dripTimeBeats (roundToInt (p ("dripTime")));
    s.monoBass = p ("monoBass") > 0.5f;
    s.outDb = p ("output");
    s.bpm = lastBpm;
    engine.process (buffer, s);
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), buffer.getNumSamples());
}

void MoltenProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MoltenProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        state.removeProperty ("preset", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* MoltenProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<MoltenEditor> (*this), "Molten");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<MoltenProcessor>();
}
