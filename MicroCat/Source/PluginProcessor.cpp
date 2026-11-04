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
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "mtsEsp", 1 }, "MTS-ESP Master", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "morph", 1 }, "Morph to Tuning B", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })
                                                                                      .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "liveJust", 1 }, "Live Just Intonation",
                                                        StringArray { "Off", "5-limit (pure 3rds + 5ths)", "7-limit (bluesy 7ths)" }, 0));
    // its own sound: like an instrument in Ableton, it plays the tuning itself (no other synth needed)
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "ownSound", 1 }, "Own Sound", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "sound", 1 }, "Sound", hub::KeysSynth::soundNames(), 6));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (-40.0f, 6.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return layout;
}

MicroCatProcessor::MicroCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)   // Micro-Cat's own sound (Cubase loads MIDI Cats as instruments)
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
    setTuningB (library()[0]);                    // morph goes to normal tuning until another B is picked

    cat::LinkClient::Caps caps;
    caps.sendTuning = true;
    presetManager.enableLink (apvts, caps);
    startTimerHz (25);
}

int MicroCatProcessor::mappingFor (const TuningData& d) const
{
    const int forced = juce::roundToInt (p ("mapping"));
    const int m = forced == 1 ? MapPerStep : forced == 2 ? MapTwelve : d.mapping;
    return m == MapTwelve && d.count != 12 ? MapPerStep : m;   // the 12-key layout needs 12 notes
}

PitchMap MicroCatProcessor::currentMap() const
{
    PitchMap m;
    {
        const juce::SpinLock::ScopedLockType sl (tuningLock);
        m.a = pendingData;
        m.b = pendingDataB;
    }
    m.a.mapping = mappingFor (m.a);
    m.b.mapping = mappingFor (m.b);
    const auto s = currentSettings();
    m.rootKey = s.rootKey;
    m.rootHz = s.rootHz;
    m.morph = s.morph;
    m.just = s.just;
    m.justRoot = s.just != JustOff ? engine.justRoot.load() : -1;
    return m;
}

std::array<float, 128> MicroCatProcessor::keyPitches() const
{
    const auto m = currentMap();
    std::array<float, 128> out {};
    for (int k = 0; k < 128; ++k)
    {
        bool valid = true;
        const double s = keySemis (m, k, valid);
        out[(size_t) k] = valid && std::isfinite (s) ? (float) s : (float) k;
    }
    return out;
}

void MicroCatProcessor::timerCallback()
{
    const bool mtsOn = p ("mtsEsp") > 0.5f;
    if (mtsOn != mts.isEnabled()) { mts.setEnabled (mtsOn); publishedKey = {}; }
    if (mtsOn && mts.state() != micro::MtsMaster::State::Connected && ++mtsPollCount % 25 == 0)
    {
        mts.poll();                                            // the other master may have gone
        if (mts.state() == micro::MtsMaster::State::Connected) publishedKey = {};
    }

    mtsConnected = mts.state() == micro::MtsMaster::State::Connected;
    const auto s = currentSettings();
    const juce::String key = juce::String (tuningVersion.load()) + "/" + juce::String (s.rootKey) + "/" + juce::String (s.rootHz, 4)
                           + "/" + juce::String (effectiveMapping()) + "/" + juce::String ((int) s.bypass) + "/" + juce::String ((int) mtsOn)
                           + "/" + juce::String (s.morph, 3) + "/" + juce::String (s.just) + "/" + juce::String (s.just != JustOff ? engine.justRoot.load() : -1);
    if (key == publishedKey) return;
    publishedKey = key;
    const auto pitches = keyPitches();
    if (auto* link = presetManager.linkClient.get())
    {
        link->publishTuning (pitches, ! s.bypass);
        ++linkPublishes;
    }
    if (mtsOn)
    {
        std::array<double, 128> hz {};
        std::array<bool, 128> valid {};
        const auto m = currentMap();
        for (int k = 0; k < 128; ++k)
        {
            bool v = true;
            keySemis (m, k, v);
            valid[(size_t) k] = s.bypass || v;
            hz[(size_t) k] = 440.0 * std::pow (2.0, ((s.bypass ? (double) k : (double) pitches[(size_t) k]) - 69.0) / 12.0);
        }
        mts.send (hz, valid, s.bypass ? juce::String ("12-EDO") : getTuning().name);
    }
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

micro::Tuning MicroCatProcessor::getTuningB() const
{
    const juce::SpinLock::ScopedLockType sl (tuningLock);
    return tuningB;
}

void MicroCatProcessor::setTuningB (const Tuning& t)
{
    if (! t.isValid()) return;
    {
        const juce::SpinLock::ScopedLockType sl (tuningLock);
        tuningB = t;
        pendingDataB = TuningData::from (t);
    }
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
    s.morph = juce::jlimit (0.0f, 1.0f, p ("morph"));
    s.just = juce::jlimit (0, 2, juce::roundToInt (p ("liveJust")));
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
    synth.prepare (sampleRate > 0.0 ? sampleRate : 48000.0);
    synthSound = -1;
    engine.reset();
    engineVersion = 0;
}

void MicroCatProcessor::syncEngineTuning()
{
    const int v = tuningVersion.load();
    if (v == engineVersion) return;
    const juce::SpinLock::ScopedTryLockType sl (tuningLock);   // never wait on the audio thread
    if (! sl.isLocked()) return;
    auto a = pendingData, b = pendingDataB;
    a.mapping = mappingFor (a);
    b.mapping = mappingFor (b);
    engine.setTuning (a);
    engine.setTuningB (b);
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
    auto settings = currentSettings();
    if (mtsConnected.load (std::memory_order_relaxed)) settings.bypass = true;   // MTS-ESP tunes the synths: the notes go out as they are
    settings.blockSize = n;
    settings.blockSeconds = n / juce::jmax (1000.0, getSampleRate());
    engine.process (midi, outBuffer, settings);

    // its own sound: the tuned MPE notes it just made, played by the built-in keys
    const bool own = p ("ownSound") > 0.5f && buffer.getNumChannels() > 0;
    if (own)
    {
        const int snd = juce::roundToInt (p ("sound"));
        if (snd != synthSound) { synthSound = snd; synth.setSound (snd); }
        synth.setBendRange ((float) settings.bendRange);
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (juce::jmin (1, buffer.getNumChannels() - 1));
        int pos = 0;
        for (const auto meta : outBuffer)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos) { synth.render (l + pos, r + pos, at - pos); pos = at; }
            synth.handle (meta.getMessage());
        }
        if (n > pos) synth.render (l + pos, r + pos, n - pos);

        const float target = juce::Decibels::decibelsToGain (p ("volume")) * 1.4f;   // same level as Cat Box's keys
        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
            buffer.applyGainRamp (ch, 0, n, synthGain, target);
        synthGain = target;
    }
    else if (synthWasOn)
        synth.allOff();
    synthWasOn = own;

    midi.swapWith (outBuffer);
}

//==============================================================================
namespace
{
    juce::ValueTree tuningTree (const Tuning& t, const char* type)
    {
        juce::ValueTree tv (type);
        tv.setProperty ("name", t.name, nullptr);
        tv.setProperty ("category", t.category, nullptr);
        tv.setProperty ("categoryId", t.categoryId, nullptr);
        tv.setProperty ("description", t.description, nullptr);
        tv.setProperty ("mapping", t.mapping, nullptr);
        juce::StringArray cents;
        for (auto c : t.cents) cents.add (juce::String (c, 6));
        tv.setProperty ("cents", cents.joinIntoString (" "), nullptr);
        return tv;
    }

    Tuning tuningFrom (const juce::ValueTree& tv)
    {
        Tuning t;
        t.name = tv.getProperty ("name").toString();
        t.category = tv.getProperty ("category").toString();
        t.categoryId = (int) tv.getProperty ("categoryId", (int) CatMine);
        t.description = tv.getProperty ("description").toString();
        t.mapping = (int) tv.getProperty ("mapping", (int) MapPerStep);
        for (auto& c : juce::StringArray::fromTokens (tv.getProperty ("cents").toString(), " ", "")) if (c.isNotEmpty()) t.cents.push_back (c.getDoubleValue());
        return t;
    }
}

void MicroCatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("TUNINGB"), nullptr);
    state.appendChild (tuningTree (getTuningB(), "TUNINGB"), nullptr);
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
    const auto tvb = state.getChildWithName ("TUNINGB");
    const int preset = (int) state.getProperty ("preset", -1);
    state.removeChild (tv, nullptr);
    state.removeChild (tvb, nullptr);
    if (tvb.isValid()) { const auto b = tuningFrom (tvb); if (b.isValid()) setTuningB (b); }
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
