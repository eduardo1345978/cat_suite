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

AudioProcessorValueTreeState::ParameterLayout GateCatProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int k = 0; k < gate::maxSteps; ++k)
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { stepId (k), 1 }, "Step " + String (k + 1), NormalisableRange<float> (0.0f, 1.0f),
                                                      k % 2 == 0 ? 1.0f : 0.0f, percent()));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "steps", 1 }, "Steps", 2, gate::maxSteps, 16));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Rate", gate::rateNames(), 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, "Length", NormalisableRange<float> (0.05f, 1.0f), 0.6f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.0f, 1.0f), 0.1f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (0.5f + v * v * 60.0f, 1) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (0.0f, 1.0f), 0.2f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (2.0f + v * v * 250.0f)) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Depth", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "filter", 1 }, "Filter", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

const std::vector<GateCatProcessor::Preset>& GateCatProcessor::presets()
{
    // rate: 0 1/4, 1 1/8, 2 1/16, 3 1/32, 4 1/8 T, 5 1/16 T
    static const std::vector<Preset> list {
        { "Trance Pulse",       "9.9.9.9.9.9.9.9.", {} },
        { "Classic Trance Gate","99.9.99.9.99.9.9", { { "length", 0.8f } } },
        { "Offbeat Pump",       "..9...9...9...9.", { { "length", 0.9f }, { "attack", 0.3f }, { "release", 0.5f } } },
        { "Sidechain Feel",     "1369136913691369", { { "length", 1.0f }, { "attack", 0.6f }, { "release", 0.1f } } },
        { "Stutter 1/32",       "9999....99..9.9.", { { "rate", 3 }, { "length", 0.5f } } },
        { "Triplet Gallop",     "9.69.69.69.69.6.", { { "rate", 5 }, { "steps", 12 } } },
        { "Swing House",        "9.6.9.6.9.6.9.69", { { "swing", 0.6f } } },
        { "Breathing Pad",      "9......9......5.", { { "rate", 1 }, { "length", 1.0f }, { "attack", 0.9f }, { "release", 0.9f } } },
        { "Morse Code",         "9.9.999.9.9.999.", { { "length", 0.45f }, { "attack", 0.0f }, { "release", 0.0f } } },
        { "Filter Steps",       "9.5.7.3.9.5.7.3.", { { "filter", 0.8f }, { "depth", 0.5f } } },
        { "Dark Chop",          "9..9..9..9..9.9.", { { "filter", 1.0f }, { "length", 0.7f } } },
        { "Soft Tremolo",       "9696969696969696", { { "depth", 0.5f }, { "length", 1.0f }, { "attack", 0.7f }, { "release", 0.7f } } },
        { "Half Time",          "99999999........", { { "rate", 1 }, { "length", 1.0f }, { "release", 0.4f } } },
        { "Glitch Grid",        "99.9..99.9.9..99", { { "rate", 3 }, { "swing", 0.3f }, { "attack", 0.0f }, { "release", 0.05f } } },
        { "Rising Steps",       "1234567899876543", { { "length", 0.9f } } },
        { "Subtle Groove",      "9.7.9.7.9.7.9.77", { { "depth", 0.35f }, { "mix", 0.8f } } },
    };
    return list;
}

void GateCatProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output") rp->setValueNotifyingHost (rp->getDefaultValue());
    const auto& pr = presets()[(size_t) index];
    const String pat (pr.pattern);
    for (int k = 0; k < gate::maxSteps; ++k)
    {
        const auto c = k < pat.length() ? pat[k] : '.';
        const float v = (c >= '0' && c <= '9') ? (float) (c - '0') / 9.0f : 0.0f;
        if (auto* prm = apvts.getParameter (stepId (k))) prm->setValueNotifyingHost (v);
    }
    for (auto& [id, v] : pr.values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String GateCatProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

GateCatProcessor::GateCatProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GateCat", createLayout())
{
    applyPreset (0);
}

bool GateCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void GateCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
}

gate::Settings GateCatProcessor::currentSettings() const
{
    gate::Settings s;
    for (int k = 0; k < gate::maxSteps; ++k) s.level[(size_t) k] = p (stepId (k));
    s.steps = roundToInt (p ("steps"));
    s.rate = roundToInt (p ("rate"));
    s.swing = p ("swing"); s.length = p ("length"); s.attack = p ("attack"); s.release = p ("release");
    s.depth = p ("depth"); s.filter = p ("filter"); s.mix = p ("mix"); s.outDb = p ("output");
    s.bpm = lastBpm;
    return s;
}

void GateCatProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());

    auto s = currentSettings();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) s.ppq = *q;
            s.playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }
    s.bpm = lastBpm;
    engine.process (buffer, s);
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), buffer.getNumSamples());
}

void GateCatProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GateCatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        state.removeProperty ("preset", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* GateCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<GateCatEditor> (*this), "Gate-Cat");
}

#ifndef GATE_CHECK
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<GateCatProcessor>();
}
#endif
