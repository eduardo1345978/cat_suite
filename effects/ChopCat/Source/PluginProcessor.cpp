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
}

AudioProcessorValueTreeState::ParameterLayout ChopCatProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int k = 0; k < chop::numSteps; ++k)
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { stepId (k), 1 }, "Step " + String (k + 1), chop::fxNames(), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "seqOn", 1 }, "Sequencer", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaos", 1 }, "Chaos", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "fill", 1 }, "Fill", NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent()));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "lock", 1 }, "Lock", chop::lockNames(), 2));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Seed", 1, 999, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "quantize", 1 }, "Pad Quantize", chop::quantizeNames(), 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "smooth", 1 }, "Smooth", NormalisableRange<float> (0.0f, 1.0f), 0.3f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (0.5f + v * 3.0f, 1) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

const std::vector<ChopCatProcessor::Preset>& ChopCatProcessor::presets()
{
    // a 1/8  b 1/16  c 1/32  d 1/64  e ROLL  f REVERSE  g TAPE STOP  h PITCH UP  i PITCH DOWN  j SCATTER  k FREEZE  l MUTE
    // lock: 0 free, 1 = 1 bar, 2 = 2 bars, 3 = 4 bars; quantize: 0 off, 1 1/16, 2 1/8
    static const std::vector<Preset> list {
        { "Clean (pads only)",   "................", {} },
        { "Drill Rolls",         "..........c...ee", { { "chaos", 0.3f }, { "fill", 0.7f } } },
        { "Trap Hats",           "......c.......dd", { { "chaos", 0.2f } } },
        { "Breakcore Blender",   "..j.b.jf..j.ccee", { { "chaos", 0.7f }, { "fill", 0.8f }, { "lock", 0 } } },
        { "IDM Scatter",         "...j..j...j.j..j", { { "chaos", 0.4f }, { "lock", 3 } } },
        { "Lo-fi Tape",          "..............gg", { { "chaos", 0.15f }, { "smooth", 0.7f } } },
        { "Reverse Swells",      "...f.......f..ff", {} },
        { "Stutter Fill",        "............bbcc", {} },
        { "Half-Speed Drop",     "........iiii....", {} },
        { "Chipmunk Hats",       "..h...h...h...h.", {} },
        { "Freeze Frame",        "............kkkk", { { "smooth", 0.6f } } },
        { "Gated Mutes",         "..l...l...l..l.l", { { "smooth", 0.1f } } },
        { "Jungle Chop",         "j..j.j..j.j..jj.", { { "chaos", 0.3f }, { "lock", 2 } } },
        { "Glitch Hop",          "...b...f...c..ee", { { "chaos", 0.25f } } },
        { "Bit Rot",             "................", { { "chaos", 0.55f }, { "lock", 1 }, { "fill", 0.3f } } },
        { "Total Chaos",         "................", { { "chaos", 1.0f }, { "fill", 1.0f }, { "lock", 0 } } },
        { "Tape Stop Ending",    "............gggg", {} },
        { "Ghost Repeats",       "....a.......a...", { { "mix", 0.6f } } },
        { "Hyperpop",            "..h.c.h.i.dd.hee", { { "chaos", 0.3f } } },
        { "Vaporwave",           "iiii....kkkk....", { { "smooth", 0.8f } } },
        { "DnB Edits",           "..........jjf.ee", { { "chaos", 0.35f }, { "fill", 0.9f }, { "lock", 2 } } },
        { "Random Fills",        "................", { { "chaos", 0.5f }, { "fill", 1.0f }, { "lock", 3 } } },
        { "Machine Gun",         "dddd....dddd....", { { "smooth", 0.15f } } },
        { "Live Pads Only",      "................", { { "seqOn", 0.0f }, { "quantize", 1 } } },
    };
    return list;
}

void ChopCatProcessor::setStep (int k, int fx)
{
    if (auto* prm = apvts.getParameter (stepId (k))) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) fx));
}

void ChopCatProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    craftName = {};
    const auto& pr = presets()[(size_t) index];
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output" && rp->getParameterID() != "seed") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (int k = 0; k < chop::numSteps; ++k)
    {
        const char c = pr.steps[k];
        setStep (k, c >= 'a' && c <= 'l' ? 1 + (c - 'a') : 0);
    }
    for (auto& [id, v] : pr.values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String ChopCatProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

String ChopCatProcessor::craft (Random& r)
{
    undoState = apvts.copyState();
    undoName = craftName;
    auto chance = [&r] (float x) { return r.nextFloat() < x; };
    auto between = [&r] (float a, float b) { return a + (b - a) * r.nextFloat(); };

    // the steps: mostly empty (the beat must still breathe), glitches on the off-beats, and a fill at the end
    std::array<int, 16> st {};
    auto pickFx = [&] (bool fillZone)
    {
        static const int normal[] = { chop::Rpt16, chop::Rpt32, chop::Rpt8, chop::Reverse, chop::Scatter, chop::Scatter, chop::PitchUp, chop::PitchDown, chop::Mute, chop::Rpt64 };
        static const int fills[] = { chop::Roll, chop::Roll, chop::Rpt32, chop::Rpt64, chop::TapeStop, chop::Reverse, chop::Freeze, chop::Rpt16 };
        return fillZone ? fills[r.nextInt (8)] : normal[r.nextInt (10)];
    };
    const int spots[] = { 2, 3, 6, 7, 10, 11, 5, 9, 13 };
    const int count = r.nextInt (4);
    for (int i = 0; i < count; ++i)
    {
        const int k = spots[r.nextInt (9)];
        const int fx = pickFx (false);
        st[(size_t) k] = fx;
        if (chance (0.3f) && k + 1 < 12) st[(size_t) (k + 1)] = fx;
    }
    if (chance (0.75f))
    {
        const int len = 1 + r.nextInt (4);
        const int fx = pickFx (true);
        for (int k = 16 - len; k < 16; ++k) st[(size_t) k] = fx;
        if (len < 4 && chance (0.4f)) st[(size_t) (15 - len)] = pickFx (true);
    }
    for (int k = 0; k < 16; ++k) setStep (k, st[(size_t) k]);

    std::map<String, float> v;
    v["seqOn"] = 1.0f;
    v["chaos"] = chance (0.35f) ? 0.0f : chance (0.75f) ? between (0.1f, 0.4f) : between (0.45f, 0.8f);
    v["fill"] = between (0.3f, 1.0f);
    v["lock"] = chance (0.15f) ? 0.0f : (float) (1 + r.nextInt (3));
    v["seed"] = (float) (1 + r.nextInt (999));
    v["smooth"] = between (0.1f, 0.6f);
    v["mix"] = chance (0.8f) ? 1.0f : between (0.6f, 0.9f);
    for (auto& [id, x] : v)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (x));
    currentPreset = -1;

    static const char* a[] = { "Basement", "Midnight", "Broken", "Static", "Analog", "Rewind", "Neon", "Lost", "Warped", "Haunted", "Bootleg", "Overdub" };
    static const char* b[] = { "Rave", "Drill", "Break", "Session", "Signal", "Loop", "Cypher", "Freestyle", "Mixtape", "Riddim", "Edit", "Bounce" };
    craftName = "TAPE " + String (1 + r.nextInt (99)).paddedLeft ('0', 2) + "  " + a[r.nextInt (12)] + " " + b[r.nextInt (12)];
    return craftName;
}

bool ChopCatProcessor::undoCraft()
{
    if (! undoState.isValid()) return false;
    apvts.replaceState (undoState);
    craftName = undoName;
    undoState = {};
    return true;
}

ChopCatProcessor::ChopCatProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ChopCat", createLayout())
{
    applyPreset (1);
}

bool ChopCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void ChopCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
}

chop::Settings ChopCatProcessor::settings() const
{
    chop::Settings s;
    for (int k = 0; k < chop::numSteps; ++k) s.steps[(size_t) k] = getStep (k);
    s.seqOn = p ("seqOn") > 0.5f;
    s.chaos = p ("chaos"); s.fill = p ("fill"); s.lock = roundToInt (p ("lock")); s.seed = roundToInt (p ("seed"));
    s.quantize = roundToInt (p ("quantize")); s.smooth = p ("smooth"); s.mix = p ("mix"); s.outDb = p ("output");
    s.bpm = lastBpm;
    return s;
}

void ChopCatProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
    auto s = settings();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) s.ppq = *q;
            s.playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }
    s.bpm = lastBpm;
    engine.process (buffer, midi, s);
    midi.clear();
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), buffer.getNumSamples());
}

void ChopCatProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.setProperty ("craftName", craftName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ChopCatProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        craftName = state.getProperty ("craftName", "").toString();
        state.removeProperty ("preset", nullptr);
        state.removeProperty ("craftName", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* ChopCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<ChopCatEditor> (*this), "Chop-Cat");
}

#ifndef CHOP_CHECK
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<ChopCatProcessor>();
}
#endif
