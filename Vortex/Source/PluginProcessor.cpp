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
String secondsText (double s)
{
    return s >= 10.0 ? String (roundToInt (s)) + " s" : String (s, 1) + " s";
}
}

AudioProcessorValueTreeState::ParameterLayout VortexProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "spin", 1 }, "Spin", NormalisableRange<float> (-1.0f, 1.0f), 0.4f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const double r = vortex::spinRate (v);
            if (std::abs (r) < 1.0e-9) return String ("still");
            return String (r > 0.0 ? "up " : "down ") + secondsText (1.0 / std::abs (r)) + " / oct";
        })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "sync", 1 }, "Sync", vortex::syncNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", vortex::modeNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "focus", 1 }, "Focus", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (std::abs (v) < 0.02f) return String ("open");
            return (v < 0.0f ? "dark " : "bright ") + String (roundToInt (std::abs (v) * 100.0f));
        })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gen", 1 }, "Gen", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f), 0.0f, decibels()));
    return l;
}

const std::vector<VortexProcessor::Preset>& VortexProcessor::presets()
{
    // sync: 0 free, 1 = 1 bar, 2 = 2 bars, 3 = 4 bars, 4 = 8 bars, 5 = 16 bars; mode: 0 pitch, 1 filter, 2 both
    static const std::vector<Preset> list {
        { "Endless Rise",            { { "spin", 0.4f } } },
        { "Endless Fall",            { { "spin", -0.4f } } },
        { "Slow Ascension",          { { "spin", 0.2f }, { "focus", 0.35f }, { "width", 0.7f }, { "tone", -0.2f } } },
        { "Falling Forever",         { { "spin", -0.25f }, { "focus", 0.45f }, { "feedback", 0.3f } } },
        { "Tension Riser (8 bars)",  { { "spin", 0.5f }, { "sync", 4 }, { "mode", 2 } } },
        { "Drop Builder (4 bars)",   { { "spin", 0.5f }, { "sync", 3 }, { "mode", 2 }, { "focus", 0.6f }, { "feedback", 0.2f }, { "tone", 0.2f } } },
        { "Barber Pole",             { { "spin", 0.45f }, { "mode", 1 }, { "focus", 0.6f } } },
        { "Sinking Filter",          { { "spin", -0.4f }, { "mode", 1 }, { "focus", 0.7f } } },
        { "Wah Staircase (1 bar)",   { { "spin", 0.5f }, { "sync", 1 }, { "mode", 1 }, { "focus", 0.9f } } },
        { "Phaser Spiral",           { { "spin", 0.55f }, { "mode", 1 }, { "focus", 0.35f }, { "mix", 0.5f } } },
        { "Cascade",                 { { "spin", 0.35f }, { "feedback", 0.7f } } },
        { "Waterfall",               { { "spin", -0.5f }, { "feedback", 0.65f }, { "mix", 0.7f } } },
        { "Spiral Choir",            { { "spin", 0.25f }, { "focus", 0.25f }, { "width", 0.9f }, { "feedback", 0.3f }, { "tone", -0.3f } } },
        { "Shimmer Stairs",          { { "spin", 0.3f }, { "feedback", 0.5f }, { "tone", 0.4f }, { "focus", 0.6f }, { "mix", 0.6f } } },
        { "Wide Galaxy",             { { "spin", 0.3f }, { "width", 1.0f }, { "feedback", 0.4f }, { "tone", -0.4f } } },
        { "Elevator Music",          { { "spin", 0.15f }, { "focus", 0.8f }, { "mix", 0.5f } } },
        { "Gentle Drift",            { { "spin", 0.08f }, { "focus", 0.4f }, { "width", 0.8f }, { "mix", 0.6f } } },
        { "Chipmunk Escalator",      { { "spin", 0.8f }, { "focus", 0.9f } } },
        { "Black Hole",              { { "spin", -0.8f }, { "focus", 0.3f }, { "feedback", 0.6f }, { "tone", -0.6f } } },
        { "Hypnotic Loop (2 bars)",  { { "spin", 0.5f }, { "sync", 2 }, { "mode", 2 }, { "focus", 0.55f }, { "width", 0.8f } } },
        { "Panic Room",              { { "spin", 1.0f }, { "mode", 2 }, { "feedback", 0.5f } } },
        { "Shepard Tone",            { { "spin", 0.45f }, { "gen", 0.8f } } },
        { "Infinite Organ",          { { "spin", 0.25f }, { "gen", 0.6f }, { "feedback", 0.35f }, { "tone", -0.3f }, { "width", 0.9f } } },
        { "Down the Drain",          { { "spin", -0.6f }, { "gen", 0.5f }, { "mode", 2 }, { "feedback", 0.3f } } },
    };
    return list;
}

void VortexProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String VortexProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

String VortexProcessor::spinText() const
{
    const int sync = roundToInt (p ("sync"));
    const float spin = p ("spin");
    if (sync > 0)
    {
        const int bars = (int) vortex::syncBars (sync);
        return String (spin < 0.0f ? "falling" : "rising") + ", an octave every " + String (bars) + (bars == 1 ? " bar" : " bars");
    }
    const double r = vortex::spinRate (spin);
    if (std::abs (r) < 1.0e-9) return "standing still";
    return String (r > 0.0 ? "rising" : "falling") + ", an octave every " + secondsText (1.0 / std::abs (r));
}

VortexProcessor::VortexProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Vortex", createLayout())
{
    applyPreset (0);
}

bool VortexProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void VortexProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
}

void VortexProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());

    vortex::Settings s;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) s.ppq = *q;
            if (auto ts = pos->getTimeSignature(); ts.hasValue() && ts->numerator > 0 && ts->denominator > 0)
                s.barBeats = ts->numerator * 4.0 / ts->denominator;
            s.playing = pos->getIsPlaying();
        }
    s.bpm = lastBpm;
    s.spin = p ("spin");
    s.sync = roundToInt (p ("sync"));
    s.mode = (vortex::Mode) jlimit (0, 2, roundToInt (p ("mode")));
    s.focus = p ("focus"); s.feedback = p ("feedback"); s.width = p ("width"); s.tone = p ("tone");
    s.gen = p ("gen"); s.mix = p ("mix"); s.outDb = p ("output");
    engine.process (buffer, s);
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), buffer.getNumSamples());
}

void VortexProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VortexProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        state.removeProperty ("preset", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* VortexProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<VortexEditor> (*this), "Vortex");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VortexProcessor();
}
