#include "ProEditor.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

using namespace juce;
using namespace board;

namespace
{
String pct (float v) { return String (roundToInt (v * 100.0f)) + "%"; }

/** What each knob shows (pedal, knob a / b). */
String knobText (int pedal, bool second, float v)
{
    switch (pedal)
    {
        case Drive:  return second ? String (1.5f * std::pow (14.0f / 1.5f, v), 1) + " kHz" : "+" + String (roundToInt (36.0f * v)) + " dB";
        case Comp:   return second ? "+" + String (roundToInt (18.0f * v)) + " dB" : String (roundToInt (-6.0f - 34.0f * v)) + " dB";
        case Filter: return second ? filterSpeedName (v) : pct (v);
        case Phaser: return second ? pct (v) : String (0.05 * std::pow (80.0, (double) v), 2) + " Hz";
        case Chorus: return second ? pct (v) : String (0.1 + 2.4 * (double) v, 2) + " Hz";
        case Delay:  return second ? pct (v) : delayName (v);
        case board::Reverb: return pct (v);
        default:     return pct (v);
    }
}
const char* knobNames[numPedals][2] = { { "Gain", "Tone" }, { "Squash", "Level" }, { "Sweep", "Speed" }, { "Rate", "Depth" },
                                        { "Rate", "Mix" }, { "Time", "Repeats" }, { "Size", "Mix" } };
const float knobDefaults[numPedals][2] = { { 0.4f, 0.55f }, { 0.4f, 0.3f }, { 0.6f, 0.5f }, { 0.35f, 0.6f }, { 0.35f, 0.45f }, { 0.5f, 0.35f }, { 0.55f, 0.3f } };
}

String knobCaption (int pedal, int knob) { return knobNames[jlimit (0, 6, pedal)][jlimit (0, 1, knob)]; }

AudioProcessorValueTreeState::ParameterLayout PolyBoardProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int pd = 0; pd < numPedals; ++pd)
    {
        const String name = pedalName (pd);
        l.add (std::make_unique<AudioParameterBool> (ParameterID { paramId (pd, "on"), 1 }, name + " On", pd != Phaser && pd != Filter && pd != Drive));
        for (int k = 0; k < 2; ++k)
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { paramId (pd, k == 0 ? "a" : "b"), 1 }, name + " " + knobNames[pd][k],
                NormalisableRange<float> (0.0f, 1.0f), knobDefaults[pd][k],
                AudioParameterFloatAttributes().withStringFromValueFunction ([pd, k] (float v, int) { return knobText (pd, k == 1, v); })));
    }
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f, db));
    return l;
}

const std::vector<PolyBoardProcessor::Preset>& PolyBoardProcessor::presets()
{
    // pedals: 0 drive, 1 comp, 2 filter, 3 phaser, 4 chorus, 5 delay, 6 reverb
    const std::array<int, 7> std { 0, 1, 2, 3, 4, 5, 6 };
    static const std::vector<Preset> list {
        { "Clean Shine",         { { "comp_on", 1 }, { "chorus_on", 1 }, { "reverb_on", 1 } }, std },
        { "Seventies Lead",      { { "drive_on", 1 }, { "drive_a", 0.45f }, { "drive_b", 0.45f }, { "comp_on", 1 }, { "delay_on", 1 }, { "delay_a", 0.5f }, { "delay_b", 0.3f }, { "reverb_on", 1 }, { "chorus_on", 0 } }, std },
        { "Funky Wah Bass",      { { "comp_on", 1 }, { "comp_a", 0.6f }, { "filter_on", 1 }, { "filter_a", 0.7f }, { "filter_b", 0.83f }, { "drive_on", 1 }, { "drive_a", 0.25f }, { "chorus_on", 0 }, { "delay_on", 0 }, { "reverb_on", 0 } }, std },
        { "Disco Phaser",        { { "phaser_on", 1 }, { "phaser_a", 0.4f }, { "phaser_b", 0.75f }, { "comp_on", 1 }, { "chorus_on", 0 }, { "reverb_a", 0.4f } }, std },
        { "Wide Strings",        { { "chorus_on", 1 }, { "chorus_a", 0.2f }, { "chorus_b", 0.8f }, { "reverb_on", 1 }, { "reverb_a", 0.8f }, { "reverb_b", 0.45f }, { "delay_on", 1 }, { "delay_a", 0.67f }, { "delay_b", 0.25f } }, std },
        { "Dub Echo",            { { "filter_on", 1 }, { "filter_a", 0.4f }, { "filter_b", 0.17f }, { "delay_on", 1 }, { "delay_a", 0.5f }, { "delay_b", 0.75f }, { "reverb_on", 1 }, { "chorus_on", 0 } }, std },
        { "Space Pad",           { { "chorus_on", 1 }, { "phaser_on", 1 }, { "phaser_a", 0.1f }, { "delay_on", 1 }, { "delay_a", 0.83f }, { "delay_b", 0.6f }, { "reverb_on", 1 }, { "reverb_a", 1.0f }, { "reverb_b", 0.6f } }, std },
        { "Dirty Bass",          { { "drive_on", 1 }, { "drive_a", 0.75f }, { "drive_b", 0.3f }, { "comp_on", 1 }, { "comp_a", 0.7f }, { "chorus_on", 0 }, { "reverb_on", 0 } }, { 1, 0, 2, 3, 4, 5, 6 } },
        { "Acid Squelch",        { { "drive_on", 1 }, { "drive_a", 0.55f }, { "filter_on", 1 }, { "filter_a", 0.95f }, { "filter_b", 1.0f }, { "delay_on", 1 }, { "delay_a", 0.33f }, { "delay_b", 0.4f }, { "chorus_on", 0 }, { "reverb_on", 0 } }, std },
        { "Shoegaze Wall",       { { "drive_on", 1 }, { "drive_a", 0.9f }, { "drive_b", 0.3f }, { "chorus_on", 1 }, { "chorus_b", 0.7f }, { "reverb_on", 1 }, { "reverb_a", 1.0f }, { "reverb_b", 0.75f }, { "delay_on", 1 }, { "delay_b", 0.55f } }, { 0, 4, 6, 5, 1, 2, 3 } },
        { "Tight & Punchy",      { { "comp_on", 1 }, { "comp_a", 0.75f }, { "comp_b", 0.2f }, { "chorus_on", 0 }, { "reverb_on", 0 } }, std },
        { "Everything On",       { { "drive_on", 1 }, { "filter_on", 1 }, { "phaser_on", 1 } }, std },
    };
    return list;
}

void PolyBoardProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output" && rp->getParameterID() != "input") rp->setValueNotifyingHost (rp->getDefaultValue());
    const auto& pr = presets()[(size_t) index];
    for (auto& [id, v] : pr.values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    setOrder (pr.order);
}

const String PolyBoardProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

PolyBoardProcessor::PolyBoardProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PolyBoard", createLayout())
{
    for (int i = 0; i < numPedals; ++i) order[(size_t) i] = i;
    tunerBuf.assign (8192, 0.0f);
    applyPreset (0);
}

std::array<int, numPedals> PolyBoardProcessor::getOrder() const
{
    std::array<int, numPedals> o {};
    for (int i = 0; i < numPedals; ++i) o[(size_t) i] = order[(size_t) i].load();
    return o;
}

void PolyBoardProcessor::setOrder (const std::array<int, numPedals>& o)
{
    // only a real order (each pedal once) is taken
    std::array<bool, numPedals> seen {};
    for (int v : o) { if (v < 0 || v >= numPedals || seen[(size_t) v]) return; seen[(size_t) v] = true; }
    for (int i = 0; i < numPedals; ++i) order[(size_t) i] = o[(size_t) i];
}

void PolyBoardProcessor::movePedal (int from, int to)
{
    auto o = getOrder();
    if (! isPositiveAndBelow (from, numPedals) || ! isPositiveAndBelow (to, numPedals) || from == to) return;
    const int pedal = o[(size_t) from];
    std::vector<int> v (o.begin(), o.end());
    v.erase (v.begin() + from);
    v.insert (v.begin() + to, pedal);
    std::copy (v.begin(), v.end(), o.begin());
    setOrder (o);
}

bool PolyBoardProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void PolyBoardProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
    monoSpare.assign ((size_t) jmax (1, samplesPerBlock), 0.0f);
}

void PolyBoardProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels();
    // a mono input (the Poly D is mono) goes to both sides
    if (ins == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int ch = jmax (ins, ins == 1 ? 2 : 0); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    // a MIDI footswitch: CC 80..86 switch the pedals (in their order on the board), a program change picks a preset
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isController() && m.getControllerNumber() >= 80 && m.getControllerNumber() < 80 + numPedals && m.getControllerValue() >= 64)
        {
            const int pedal = order[(size_t) (m.getControllerNumber() - 80)].load();
            if (auto* prm = apvts.getParameter (paramId (pedal, "on"))) prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            ++footswitchHits;
        }
        else if (m.isProgramChange()) MessageManager::callAsync ([this, pc = m.getProgramChangeNumber()] { setCurrentProgram (pc % getNumPrograms()); });
    }
    midi.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) bpm = *b;

    Settings s;
    for (int pd = 0; pd < numPedals; ++pd)
    {
        s.on[(size_t) pd] = p (paramId (pd, "on")) > 0.5f;
        s.a[(size_t) pd] = p (paramId (pd, "a"));
        s.b[(size_t) pd] = p (paramId (pd, "b"));
    }
    s.order = getOrder();
    s.input = p ("input");
    s.output = p ("output");
    s.bpm = bpm.load();

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool mono = r == nullptr;
    if (mono)
    {
        if ((int) monoSpare.size() < n) monoSpare.resize ((size_t) n);
        std::copy (l, l + n, monoSpare.begin());
        r = monoSpare.data();
    }

    // the tuner listens to the input, before the pedals
    int tp = tunerPos.load();
    for (int i = 0; i < n; ++i) { tunerBuf[(size_t) tp] = l[i]; tp = (tp + 1) % (int) tunerBuf.size(); }
    tunerPos = tp;

    engine.process (l, r, n, s);
    limiter.process (l, r, n);
    if (mono) for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + r[i]);
}

float PolyBoardProcessor::detectPitch()
{
    // autocorrelation over the last ~85 ms: where the wave looks like itself again = one period
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const int len = (int) tunerBuf.size();
    const int n = jmin (len, (int) (sr * 0.085));
    std::vector<float> x ((size_t) n);
    const int start = tunerPos.load();
    for (int i = 0; i < n; ++i) x[(size_t) i] = tunerBuf[(size_t) ((start - n + i + len * 2) % len)];
    double e = 0.0;
    for (float v : x) e += (double) v * v;
    if (e / n < 1.0e-6) return 0.0f;                       // too quiet
    const int minLag = (int) (sr / 1500.0), maxLag = jmin (n / 2, (int) (sr / 40.0));
    std::vector<double> c ((size_t) maxLag + 2, 0.0);
    double best = 0.0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double a = 0.0, b = 0.0, d = 0.0;
        for (int i = 0; i + lag < n; ++i) { a += (double) x[(size_t) i] * x[(size_t) (i + lag)]; b += (double) x[(size_t) i] * x[(size_t) i]; d += (double) x[(size_t) (i + lag)] * x[(size_t) (i + lag)]; }
        c[(size_t) lag] = a / std::sqrt (b * d + 1.0e-20);
        best = jmax (best, c[(size_t) lag]);
    }
    if (best < 0.6) return 0.0f;
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (c[(size_t) lag] > 0.9 * best && c[(size_t) lag] >= c[(size_t) lag - 1] && c[(size_t) lag] >= c[(size_t) lag + 1])
        {
            const double y0 = c[(size_t) lag - 1], y1 = c[(size_t) lag], y2 = c[(size_t) lag + 1];
            const double d = 0.5 * (y0 - y2) / (y0 - 2.0 * y1 + y2 + 1.0e-12);
            return (float) (sr / (lag + d));
        }
    return 0.0f;
}

void PolyBoardProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    String o;
    for (int v : getOrder()) o << v << ",";
    state.setProperty ("order", o, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PolyBoardProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        StringArray parts;
        parts.addTokens (state.getProperty ("order").toString(), ",", "");
        parts.removeEmptyStrings();
        if (parts.size() == numPedals)
        {
            std::array<int, numPedals> o {};
            for (int i = 0; i < numPedals; ++i) o[(size_t) i] = parts[i].getIntValue();
            setOrder (o);
        }
        state.removeProperty ("preset", nullptr);
        state.removeProperty ("order", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* PolyBoardProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<PolyBoardEditor> (*this), "Poly Board");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<PolyBoardProcessor>();
}
