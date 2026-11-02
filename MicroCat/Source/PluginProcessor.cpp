#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace micro;

namespace
{
    const int bendRanges[] = { 2, 12, 24, 48, 96 };
}

juce::AudioProcessorValueTreeState::ParameterLayout MicroCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "rootKey", 1 }, "Root Key", 0, 127, 60,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                     { return MidiMessage::getMidiNoteName (v, true, true, 4); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "rootHz", 1 }, "Root Frequency", NormalisableRange<float> (20.0f, 2000.0f, 0.0f, 0.3f),
                                                       261.6256f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                       { return String (v, 2) + " Hz"; }).withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mapping", 1 }, "Keys",
                                                        StringArray { "As the tuning says", "One key per step", "12-key layout" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bendRange", 1 }, "Bend Range",
                                                        StringArray { "2 semitones", "12 semitones", "24 semitones", "48 semitones (MPE)", "96 semitones" }, 3));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "voices", 1 }, "Voices", 1, 15, 15));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sendConfig", 1 }, "Send MPE Set-up", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass (normal tuning)", false));
    return layout;
}

MicroCatProcessor::MicroCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)   // silent: Cubase loads MIDI Cats as instruments
#endif
                      ),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            refs[r->getParameterID().toStdString()] = apvts.getRawParameterValue (r->getParameterID());
    outBuffer.ensureSize (4096);
    previewBuffer.ensureSize (512);
    preview.reset (48000.0);
    applyPreset (0);
}

float MicroCatProcessor::p (const char* id) const
{
    auto it = refs.find (id);
    return it != refs.end() && it->second != nullptr ? it->second->load() : 0.0f;
}

//==============================================================================
int MicroCatProcessor::getNumPrograms() { return (int) library().size(); }

const juce::String MicroCatProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) library().size()) ? library()[(size_t) i].name : juce::String();
}

void MicroCatProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) library().size())) return;
    setTuning (library()[(size_t) index]);
    currentPreset = index;
}

Tuning MicroCatProcessor::getTuning() const
{
    const juce::SpinLock::ScopedLockType sl (tuningLock);
    return tuning;
}

void MicroCatProcessor::setTuning (const Tuning& t, bool edited)
{
    if (! t.isValid()) return;
    {
        const juce::SpinLock::ScopedLockType sl (tuningLock);
        tuning = t;
        pendingData = TuningData::from (t);
    }
    if (edited) currentPreset = -1;
    ++tuningVersion;
}

void MicroCatProcessor::setRoot (int key, double hz)
{
    if (auto* prm = apvts.getParameter ("rootKey")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (0, 127, key)));
    if (auto* prm = apvts.getParameter ("rootHz")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (20.0, 2000.0, hz)));
}

int MicroCatProcessor::effectiveMapping() const
{
    const int forced = juce::roundToInt (p ("mapping"));
    int count = 12, own = MapTwelve;
    {
        const juce::SpinLock::ScopedLockType sl (tuningLock);
        count = pendingData.count;
        own = pendingData.mapping;
    }
    const int m = forced == 1 ? MapPerStep : forced == 2 ? MapTwelve : own;
    return m == MapTwelve && count != 12 ? MapPerStep : m;   // the 12-key layout needs 12 notes
}

Settings MicroCatProcessor::currentSettings() const
{
    Settings s;
    s.rootKey = juce::jlimit (0, 127, juce::roundToInt (p ("rootKey")));
    s.rootHz = juce::jlimit (20.0, 2000.0, (double) p ("rootHz"));
    s.bendRange = bendRanges[juce::jlimit (0, 4, juce::roundToInt (p ("bendRange")))];
    s.voices = juce::jlimit (1, 15, juce::roundToInt (p ("voices")));
    s.sendConfig = p ("sendConfig") > 0.5f;
    s.bypass = p ("bypass") > 0.5f;
    return s;
}

void MicroCatProcessor::previewNote (int key, bool on)
{
    auto m = on ? juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, key), (juce::uint8) 96) : juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, key));
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    preview.addMessageToQueue (m);
}

//==============================================================================
bool MicroCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void MicroCatProcessor::prepareToPlay (double sampleRate, int)
{
    preview.reset (sampleRate > 0.0 ? sampleRate : 48000.0);
    engine.reset();
    engineVersion = 0;
}

void MicroCatProcessor::syncEngineTuning()
{
    const int v = tuningVersion.load();
    if (v == engineVersion) return;
    const juce::SpinLock::ScopedTryLockType sl (tuningLock);   // never wait on the audio thread
    if (! sl.isLocked()) return;
    auto d = pendingData;
    const int forced = juce::roundToInt (p ("mapping"));
    if (forced == 1) d.mapping = MapPerStep;
    if (forced == 2 && d.count == 12) d.mapping = MapTwelve;
    engine.setTuning (d);
    engineVersion = v;
}

void MicroCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();

    // the mapping parameter changes how the engine's copy is made: re-copy when it moves
    const int forced = juce::roundToInt (p ("mapping"));
    if (forced != lastForced) { lastForced = forced; engineVersion = 0; }
    syncEngineTuning();

    // send the MPE set-up again whenever the transport starts (the copy sent at load time is easily lost)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            const bool playing = pos->getIsPlaying();
            if (playing && ! wasPlaying) engine.requestConfig();
            wasPlaying = playing;
        }

    // notes played on the screen's keyboard
    previewBuffer.clear();
    if (n > 0) preview.removeNextBlockOfMessages (previewBuffer, n);
    if (! previewBuffer.isEmpty()) midi.addEvents (previewBuffer, 0, n, 0);

    outBuffer.clear();
    engine.process (midi, outBuffer, currentSettings());
    midi.swapWith (outBuffer);
}

//==============================================================================
void MicroCatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    const auto t = getTuning();
    juce::ValueTree tv ("TUNING");
    tv.setProperty ("name", t.name, nullptr);
    tv.setProperty ("category", t.category, nullptr);
    tv.setProperty ("categoryId", t.categoryId, nullptr);
    tv.setProperty ("description", t.description, nullptr);
    tv.setProperty ("mapping", t.mapping, nullptr);
    juce::StringArray cents;
    for (auto c : t.cents) cents.add (juce::String (c, 6));
    tv.setProperty ("cents", cents.joinIntoString (" "), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.removeChild (state.getChildWithName ("TUNING"), nullptr);
    state.appendChild (tv, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MicroCatProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    const auto tv = state.getChildWithName ("TUNING");
    const int preset = (int) state.getProperty ("preset", -1);
    state.removeChild (tv, nullptr);
    state.removeProperty ("preset", nullptr);
    apvts.replaceState (state);
    if (tv.isValid())
    {
        Tuning t;
        t.name = tv.getProperty ("name").toString();
        t.category = tv.getProperty ("category").toString();
        t.categoryId = (int) tv.getProperty ("categoryId", (int) CatMine);
        t.description = tv.getProperty ("description").toString();
        t.mapping = (int) tv.getProperty ("mapping", (int) MapPerStep);
        for (auto& c : juce::StringArray::fromTokens (tv.getProperty ("cents").toString(), " ", "")) if (c.isNotEmpty()) t.cents.push_back (c.getDoubleValue());
        if (t.isValid()) setTuning (t);
    }
    currentPreset = preset;
}

juce::AudioProcessorEditor* MicroCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<MicroCatEditor> (*this), "Micro-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MicroCatProcessor();
}
