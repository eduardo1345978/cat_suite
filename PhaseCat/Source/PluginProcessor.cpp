#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace phasecat;

namespace
{
    juce::String lid (int lane, const char* what) { return "l" + juce::String (lane + 1) + what; }
    const int defaultNotes[numLanes] = { 36, 38, 42, 39 };
}

const std::vector<PhaseCatProcessor::Preset>& PhaseCatProcessor::presets()
{
    // notes (General MIDI drums): 36 kick, 37 rim, 38 snare, 39 clap, 42 closed hat, 46 open hat, 45 / 47 / 50 toms, 56 cowbell, 75 claves
    // rates: 0 1/4, 1 1/8, 2 1/8T, 3 1/16, 4 1/16T, 5 1/32, 6 1/4T, 7 1/2
    static const std::vector<Preset> list = {
        { "Clapping Music", "Phasing",
          { { "l1Note", 39 }, { "l1Rate", 1 }, { "l2Note", 37 }, { "l2Rate", 1 }, { "l2Drift", 0.6f }, { "l3On", 0 }, { "l4On", 0 } },
          { "xxx.xx.x.xx.", "xxx.xx.x.xx.", "", "" } },
        { "Drumming Hats", "Phasing",
          { { "l1Note", 42 }, { "l1Rate", 3 }, { "l2Note", 46 }, { "l2Rate", 3 }, { "l2Drift", 0.35f }, { "l3Note", 36 }, { "l3Rate", 0 }, { "l4On", 0 } },
          { "x.xo.xx.", "x.xo.xx.", "x...", "" } },
        { "Four Phases", "Phasing",
          { { "l1Note", 56 }, { "l2Note", 75 }, { "l2Drift", 0.25f }, { "l3Note", 37 }, { "l3Drift", 0.5f }, { "l4Note", 42 }, { "l4Drift", -0.35f },
            { "l1Rate", 1 }, { "l2Rate", 1 }, { "l3Rate", 1 }, { "l4Rate", 1 } },
          { "x.xx.x..", "x.xx.x..", "x.xx.x..", "x.xx.x.." } },
        { "Polymeter 4-3-5-7", "Polymeter",
          { { "l1Note", 36 }, { "l2Note", 37 }, { "l3Note", 42 }, { "l4Note", 45 } },
          { "x...", "x..", "x.o..", "..x.o.." } },
        { "Euclid Ladder", "Polymeter",
          { { "l1Note", 36 }, { "l2Note", 38 }, { "l3Note", 42 }, { "l4Note", 47 } },
          { "x.x..", "x.x.x..", "x.x.x.x..", "x.x.x.x.x.." } },
        { "Three Over Four", "Polymeter",
          { { "l1Note", 36 }, { "l1Rate", 0 }, { "l2Note", 56 }, { "l2Rate", 6 }, { "l3Note", 42 }, { "l3Rate", 3 }, { "l4On", 0 } },
          { "x...", "xoo", "xoxo", "" } },
        { "Risset Accelerando", "Risset",
          { { "l1Note", 36 }, { "l1Risset", 1 }, { "l2Note", 42 }, { "l2Risset", 1 }, { "l3Note", 38 }, { "l3Risset", 1 }, { "l4On", 0 }, { "rissetBars", 4 } },
          { "x...x...x...x...", "xoxoxoxoxoxoxoxo", "....x.......x...", "" } },
        { "Risset Falling", "Risset",
          { { "l1Note", 36 }, { "l1Risset", 1 }, { "l2Note", 42 }, { "l2Risset", 1 }, { "l3On", 0 }, { "l4On", 0 }, { "rissetBars", 6 }, { "rissetDown", 1 } },
          { "x.......x.......", "x.o.x.o.x.o.x.o.", "", "" } },
        { "Metric Shift Groove", "Modulation",
          { { "l1Note", 36 }, { "l2Note", 38 }, { "l3Note", 42 }, { "l4Note", 46 } },
          { "x.....x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "......x.......x." } },
        { "Empty", "Init",
          { { "l2On", 1 } },
          { "", "", "", "" } },
    };
    return list;
}

juce::AudioProcessorValueTreeState::ParameterLayout PhaseCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (int l = 0; l < numLanes; ++l)
    {
        const String n = "Lane " + String (l + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { lid (l, "On"), 1 }, n + "On", true));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { lid (l, "Len"), 1 }, n + "Length", 1, maxSteps, 16));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { lid (l, "Note"), 1 }, n + "Note", 0, 127, defaultNotes[l],
                                                         AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 4) + " (" + String (v) + ")"; })));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { lid (l, "Rate"), 1 }, n + "Rate", rateNames(), 3));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lid (l, "Drift"), 1 }, n + "Drift", NormalisableRange<float> (-3.0f, 3.0f, 0.01f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 2) + " %"; })
                                                                                          .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); })));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { lid (l, "Risset"), 1 }, n + "Risset", false));
    }
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "channel", 1 }, "MIDI Channel", 1, 16, 10));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate", NormalisableRange<float> (0.05f, 1.0f), 0.5f));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "rissetBars", 1 }, "Risset Turn (bars)", 1, 16, 4));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "rissetDown", 1 }, "Risset Down", false));
    return layout;
}

PhaseCatProcessor::PhaseCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)   // silent: Cubase loads MIDI Cats as instruments
#endif
                      ),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            refs[r->getParameterID()] = apvts.getRawParameterValue (r->getParameterID());
    applyPreset (0);
}

float PhaseCatProcessor::p (const juce::String& id) const
{
    auto it = refs.find (id);
    return it != refs.end() && it->second != nullptr ? it->second->load() : 0.0f;
}

void PhaseCatProcessor::setPattern (int lane, const juce::String& pattern)
{
    for (int i = 0; i < maxSteps; ++i)
    {
        const auto c = i < pattern.length() ? pattern[i] : '.';
        setStep (lane, i, c == 'x' ? 1.0f : c == 'o' ? 0.5f : 0.0f);
    }
}

int PhaseCatProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String PhaseCatProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}

void PhaseCatProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    const auto& pr = presets()[(size_t) index];
    for (int l = 0; l < numLanes; ++l)
    {
        set (lid (l, "On"), 1); set (lid (l, "Note"), (float) defaultNotes[l]); set (lid (l, "Rate"), 3);
        set (lid (l, "Drift"), 0); set (lid (l, "Risset"), 0);
        const juce::String pat (pr.patterns[(size_t) l]);
        setPattern (l, pat);
        set (lid (l, "Len"), (float) (pat.isEmpty() ? 16 : juce::jmin (maxSteps, pat.length())));
    }
    set ("gate", 0.5f); set ("rissetBars", 4); set ("rissetDown", 0);
    for (auto& [id, v] : pr.values) set (id, v);
    currentPreset = index;
    engine.requestRealign();
}

Settings PhaseCatProcessor::currentSettings() const
{
    Settings s;
    for (int l = 0; l < numLanes; ++l)
    {
        auto& lane = s.lanes[(size_t) l];
        lane.on = p (lid (l, "On")) > 0.5f;
        lane.length = juce::jlimit (1, maxSteps, juce::roundToInt (p (lid (l, "Len"))));
        lane.note = juce::jlimit (0, 127, juce::roundToInt (p (lid (l, "Note"))));
        lane.rate = juce::jlimit (0, numRates - 1, juce::roundToInt (p (lid (l, "Rate"))));
        lane.drift = p (lid (l, "Drift"));
        lane.risset = p (lid (l, "Risset")) > 0.5f;
        for (int i = 0; i < maxSteps; ++i) lane.steps[(size_t) i] = steps[(size_t) l][(size_t) i].load (std::memory_order_relaxed);
    }
    s.channel = juce::jlimit (1, 16, juce::roundToInt (p ("channel")));
    s.gate = p ("gate");
    s.rissetBars = (float) juce::jlimit (1, 16, juce::roundToInt (p ("rissetBars")));
    s.rissetDown = p ("rissetDown") > 0.5f;
    return s;
}

bool PhaseCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (! layouts.getMainInputChannelSet().isDisabled()) return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
#endif
}

void PhaseCatProcessor::prepareToPlay (double, int)
{
    engine.reset();
    outBuffer.ensureSize (2048);
    freePpq = 0.0;
    freeWas = false;
}

void PhaseCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;

    double ppq = 0.0, bpm = uiBpm.load(), barBeats = 4.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) bpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue()) ppq = *q;
            if (auto ts = pos->getTimeSignature(); ts.hasValue() && ts->denominator > 0) barBeats = ts->numerator * 4.0 / ts->denominator;
            playing = pos->getIsPlaying();
        }
    // without Cubase's transport (the Play button on the screen): its own clock
    if (! playing && freeRun.load())
    {
        if (! freeWas) freePpq = 0.0;
        ppq = freePpq;
        playing = true;
        freePpq += n * bpm / 60.0 / sr;
        freeWas = true;
    }
    else freeWas = false;
    uiBpm = bpm;
    uiPlaying = playing;

    outBuffer.clear();
    engine.process (currentSettings(), ppq, bpm, playing, barBeats, n, sr, outBuffer);
    midi.addEvents (outBuffer, 0, n, 0);       // whatever came in goes through too
}

void PhaseCatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    for (int l = 0; l < numLanes; ++l)
    {
        juce::StringArray v;
        for (int i = 0; i < maxSteps; ++i) v.add (juce::String (getStep (l, i), 3));
        state.setProperty ("steps" + juce::String (l + 1), v.joinIntoString (" "), nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PhaseCatProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentPreset = (int) state.getProperty ("preset", -1);
    for (int l = 0; l < numLanes; ++l)
    {
        const auto key = "steps" + juce::String (l + 1);
        const auto v = juce::StringArray::fromTokens (state.getProperty (key).toString(), " ", "");
        for (int i = 0; i < maxSteps; ++i) setStep (l, i, i < v.size() ? v[i].getFloatValue() : 0.0f);
        state.removeProperty (key, nullptr);
    }
    state.removeProperty ("preset", nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* PhaseCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<PhaseCatEditor> (*this), "Phase-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhaseCatProcessor();
}
