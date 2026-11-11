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
float rateHz (float v) { return 2000.0f * std::pow (48000.0f / 2000.0f, jlimit (0.0f, 1.0f, v)); }
}

AudioProcessorValueTreeState::ParameterLayout CryptFxProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&l] (const char* id, const char* name, float lo, float hi, float def, AudioParameterFloatAttributes a)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def, a)); };
    f ("bits", "Bits", 2.0f, 16.0f, 8.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v >= 15.9f ? String ("clean") : String (roundToInt (v)) + " bit"; }));
    f ("rate", "Rate", 0.0f, 1.0f, 0.55f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v >= 0.995f ? String ("clean") : String (rateHz (v) / 1000.0f, 1) + " kHz"; }));
    f ("drive", "Drive", 0.0f, 1.0f, 0.15f, percent());
    f ("gloom", "Gloom", 0.0f, 1.0f, 0.3f, percent());
    f ("crypt", "Crypt", 0.0f, 1.0f, 0.35f, percent());
    f ("echo", "Echo", 0.0f, 1.0f, 0.15f, percent());
    f ("mix", "Mix", 0.0f, 1.0f, 1.0f, percent());
    f ("output", "Output", -12.0f, 12.0f, 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }));
    return l;
}

const std::vector<CryptFxProcessor::Preset>& CryptFxProcessor::presets()
{
    static const std::vector<Preset> list {
        { "8-bit Crypt",         { } },
        { "NES Dungeon",         { { "bits", 7.0f }, { "rate", 0.6f }, { "crypt", 0.25f }, { "echo", 0.1f } } },
        { "Game Boy Speaker",    { { "bits", 4.0f }, { "rate", 0.4f }, { "gloom", 0.5f }, { "drive", 0.3f }, { "crypt", 0.0f }, { "echo", 0.0f } } },
        { "Deep Catacomb",       { { "bits", 10.0f }, { "rate", 0.75f }, { "gloom", 0.55f }, { "crypt", 0.9f }, { "echo", 0.3f } } },
        { "Tomb Radio",          { { "bits", 5.0f }, { "rate", 0.35f }, { "gloom", 0.65f }, { "drive", 0.5f }, { "crypt", 0.3f } } },
        { "Broken Cartridge",    { { "bits", 3.0f }, { "rate", 0.2f }, { "drive", 0.6f }, { "crypt", 0.2f }, { "echo", 0.0f } } },
        { "Whispering Walls",    { { "bits", 12.0f }, { "rate", 0.9f }, { "gloom", 0.4f }, { "crypt", 0.75f }, { "echo", 0.6f }, { "mix", 0.6f } } },
        { "Haunted Echo",        { { "bits", 9.0f }, { "rate", 0.7f }, { "gloom", 0.45f }, { "crypt", 0.5f }, { "echo", 0.8f } } },
        { "Boss Room",           { { "bits", 6.0f }, { "rate", 0.5f }, { "drive", 0.7f }, { "gloom", 0.2f }, { "crypt", 0.55f } } },
        { "Just a Bit Dirty",    { { "bits", 10.0f }, { "rate", 0.85f }, { "drive", 0.1f }, { "gloom", 0.15f }, { "crypt", 0.1f }, { "echo", 0.0f }, { "mix", 0.5f } } },
        { "Underwater Tomb",     { { "bits", 8.0f }, { "rate", 0.45f }, { "gloom", 0.85f }, { "crypt", 0.6f }, { "echo", 0.2f } } },
        { "Clean Crypt (no 8-bit)", { { "bits", 16.0f }, { "rate", 1.0f }, { "drive", 0.0f }, { "gloom", 0.3f }, { "crypt", 0.7f }, { "echo", 0.25f } } },
    };
    return list;
}

void CryptFxProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String CryptFxProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

CryptFxProcessor::CryptFxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CryptFX", createLayout())
{
    applyPreset (0);
}

bool CryptFxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void CryptFxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    crusher.prepare (sampleRate);
    space.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
    dry.setSize (2, jmax (1, samplesPerBlock));
    mixS.reset (sampleRate, 0.05);
    mixS.setCurrentAndTargetValue (p ("mix"));
}

void CryptFxProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    const bool mono = buffer.getNumChannels() < 2;
    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    dry.copyFrom (0, 0, l, n);
    dry.copyFrom (1, 0, r, n);
    if (mono)
    {
        // process a copy as the right side, keep the left
        dry.copyFrom (1, 0, l, n);
    }
    float* rr = mono ? dry.getWritePointer (1) : r;
    if (mono) { /* rr holds a copy of l; it is processed alongside and dropped */ }

    crusher.process (l, rr, n, p ("bits"), p ("rate") >= 0.995f ? (float) getSampleRate() : rateHz (p ("rate")));
    space.process (l, rr, n, p ("gloom"), p ("drive"), p ("crypt"), p ("echo"), lastBpm);

    mixS.setTargetValue (p ("mix"));
    const float gain = Decibels::decibelsToGain (p ("output"));
    const float* dl = dry.getReadPointer (0);
    const float* dr = mono ? dry.getReadPointer (0) : dry.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float m = mixS.getNextValue();
        l[i] = (l[i] * m + dl[i] * (1.0f - m)) * gain;
        if (! mono) r[i] = (r[i] * m + dr[i] * (1.0f - m)) * gain;
    }
    limiter.process (l, mono ? l : r, n);

    float pk = 0.0f;
    int sp = scopePos.load();
    for (int i = 0; i < n; ++i)
    {
        pk = jmax (pk, std::abs (l[i]));
        if ((i & 1) == 0) { scope[(size_t) sp].store (l[i], std::memory_order_relaxed); sp = (sp + 1) % (int) scope.size(); }
    }
    scopePos = sp;
    level = level.load() * 0.7f + pk * 0.3f;
}

void CryptFxProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void CryptFxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        state.removeProperty ("preset", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* CryptFxProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<CryptFxEditor> (*this), "Crypt FX");
}

#ifndef CRYPT_CHECK   // the test links both plug-ins into one program
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<CryptFxProcessor>();
}
#endif
