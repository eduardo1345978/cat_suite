#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace glidecat;
#include "GlideData.h"

namespace
{
    juce::String lid (int layer, const char* id) { return "l" + juce::String (layer) + "_" + id; }
}

//==============================================================================
// The parameters and the factory presets live in GlideData (shared with the Cat Box's Glide engine)
juce::StringArray GlideCatProcessor::glideParamIds() { return glidecat::glideParamIds(); }
juce::StringArray GlideCatProcessor::soundParamIds() { return glidecat::soundParamIds(); }
const std::vector<GlideCatProcessor::Preset>& GlideCatProcessor::soundPresets() { return glidecat::soundPresets(); }
const std::vector<GlideCatProcessor::Preset>& GlideCatProcessor::glidePresets() { return glidecat::glidePresets(); }

juce::AudioProcessorValueTreeState::ParameterLayout GlideCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& info : glidecat::params())
    {
        switch (info.type)
        {
            case glidecat::ParamInfo::Float:
            {
                auto toText = [info] (float v, int) { return info.format (v); };
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { info.id, 1 }, info.name,
                                                                   NormalisableRange<float> (info.lo, info.hi, 0.0f, info.skew), info.def,
                                                                   AudioParameterFloatAttributes().withLabel (info.unit == "%" ? String() : info.unit)
                                                                       .withStringFromValueFunction (toText)));
                break;
            }
            case glidecat::ParamInfo::Choice:
                layout.add (std::make_unique<AudioParameterChoice> (ParameterID { info.id, 1 }, info.name, info.choices, roundToInt (info.def)));
                break;
            case glidecat::ParamInfo::Int:
                layout.add (std::make_unique<AudioParameterInt> (ParameterID { info.id, 1 }, info.name, roundToInt (info.lo), roundToInt (info.hi), roundToInt (info.def)));
                break;
            case glidecat::ParamInfo::Bool:
                layout.add (std::make_unique<AudioParameterBool> (ParameterID { info.id, 1 }, info.name, info.def > 0.5f));
                break;
        }
    }
    return layout;
}

int GlideCatProcessor::getNumPrograms() { return (int) soundPresets().size(); }
const juce::String GlideCatProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (soundPresets()[(size_t) i].name) : juce::String();
}

//==============================================================================
GlideCatProcessor::GlideCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    {
        cat::LinkClient::Caps caps;
        caps.receiveChords = true;
        caps.receiveChordsText = "Play Chord-Cat's chords";
        presetManager.enableLink (apvts, caps);
    }
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            refs[r->getParameterID().toStdString()] = apvts.getRawParameterValue (r->getParameterID());
}

GlideCatProcessor::~GlideCatProcessor() = default;

bool GlideCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.getMainInputChannelSet().isDisabled()) return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void GlideCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    limiter.prepare (sampleRate);
    hostSync.prepare (sampleRate);
    synth.prepare (sampleRate, samplesPerBlock);
    effects.prepare (sampleRate, samplesPerBlock);
    work.setSize (2, juce::jmax (512, samplesPerBlock), false, false, true);
}

float GlideCatProcessor::p (const char* id) const
{
    const auto it = refs.find (id);
    jassert (it != refs.end());
    return it != refs.end() ? it->second->load() : 0.0f;
}

void GlideCatProcessor::readSettings()
{
    glidecat::readSettings ([this] (const char* id) { return p (id); }, settings, fxSettings);
}

void GlideCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    presetManager.linkClient->processInput (midi, buffer.getNumSamples());     // Cat Link: Chord-Cat's chord
    const int numSamples = buffer.getNumSamples();
    mergeComputerKeyboard (midi, numSamples);      // standalone app: notes from the computer keyboard
    buffer.clear();
    hostSync.beginBlock (presetManager.linkClient->playHead (getPlayHead(), numSamples, getSampleRate()), numSamples);   // Cat Link: shared tempo in the apps
    midiLearn.processMidi (midi);
    readSettings();
    synth.setSettings (settings);
    synth.setHostPosition (hostSync.ppqAt (0), hostSync.get().bpm, hostSync.get().hostPlaying);

    if (work.getNumSamples() < numSamples)
        work.setSize (2, numSamples, false, false, true);
    work.clear();
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);

    // Note-ons that start together (within 25 ms of this block) are one chord: voice-led as a group
    std::vector<std::pair<int, int>> group;
    std::vector<float> groupVel;
    int groupPos = -1, pos = 0;
    const int window = (int) (0.025 * getSampleRate());
    auto flushGroup = [&]
    {
        if (group.empty()) return;
        if (groupPos > pos) { synth.render (l + pos, r + pos, groupPos - pos); pos = groupPos; }
        synth.notesOn (group, groupVel);
        group.clear();
        groupVel.clear();
    };

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        if (m.isNoteOn())
        {
            if (! group.empty() && at - groupPos > window) flushGroup();
            if (group.empty()) groupPos = at;
            group.push_back ({ m.getChannel(), m.getNoteNumber() });
            groupVel.push_back (m.getFloatVelocity());
            noteMonitor.noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity());
            continue;
        }
        flushGroup();
        if (at > pos) { synth.render (l + pos, r + pos, at - pos); pos = at; }
        const int ch = m.getChannel();
        if (m.isNoteOff()) { synth.noteOff (ch, m.getNoteNumber()); noteMonitor.noteOff (m.getNoteNumber()); }
        else if (m.isPitchWheel()) synth.pitchBend (ch, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isChannelPressure()) synth.pressure (ch, (float) m.getChannelPressureValue() / 127.0f);
        else if (m.isAftertouch()) synth.pressure (ch, (float) m.getAfterTouchValue() / 127.0f);
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const float v = (float) m.getControllerValue() / 127.0f;
            if (cc == 1) synth.modWheel (v);
            else if (cc == 74) synth.timbre (ch, v);
            else if (cc == 64) synth.sustain (m.getControllerValue() >= 64);
            else if (cc == 120 || cc == 123) { synth.allNotesOff(); noteMonitor.clear(); }
        }
    }
    flushGroup();
    if (pos < numSamples) synth.render (l + pos, r + pos, numSamples - pos);
    midi.clear();

    fxSettings.bpm = hostSync.get().bpm;
    effects.process (l, r, numSamples, fxSettings);
    const float vol = settings.volume;
    int overloaded = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        float a = l[i] * vol, b = r[i] * vol;
        limiter.processSample (a, b);
        l[i] = a; r[i] = b;
        overloaded += limiter.isOverloaded() ? 1 : 0;
    }
    // Last line of defence: if the effects ever blow up (the limiter has had to hold the
    // level down by more than 20 dB for a quarter of a second), clear their memory.
    overloadSamples = overloaded == numSamples ? overloadSamples + numSamples : 0;
    if (overloadSamples > (int) (getSampleRate() * 0.25))
    {
        effects.reset();
        limiter.reset();
        overloadSamples = 0;
    }
    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.copyFrom (1, 0, work, 1, 0, numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.addFrom (0, 0, work, 1, 0, numSamples);
        buffer.applyGain (0, 0, numSamples, 0.5f);
    }

    uiPpq = hostSync.ppqAt (0);
    uiTransportRunning = hostSync.isRunning();
    hostSync.endBlock();
}

//==============================================================================
void GlideCatProcessor::resetToDefaults (const juce::StringArray& ids)
{
    for (const auto& id : ids)
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getDefaultValue());
            prm->endChangeGesture();
        }
}

void GlideCatProcessor::applySoundPreset (int index)
{
    const auto& list = soundPresets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    currentSound = index;
    resetToDefaults (soundParamIds());
    for (const auto& [id, v] : list[(size_t) index].values)
        cat::SurpriseControls::setParam (*this, id, v);
}

void GlideCatProcessor::applyGlidePreset (int index)
{
    const auto& list = glidePresets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    currentGlide = index;
    resetToDefaults (glideParamIds());
    for (const auto& [id, v] : list[(size_t) index].values)
        cat::SurpriseControls::setParam (*this, id, v);
}

void GlideCatProcessor::glideDice()
{
    // Musically bounded glide behaviour
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    set ("glideMode", r.nextFloat() < 0.75f ? 0.0f : 1.0f);
    set ("glideMs", 80.0f + std::pow (r.nextFloat(), 1.5f) * 1500.0f);
    set ("rateMs", 20.0f + r.nextFloat() * 120.0f);
    set ("curve", (float) r.nextInt (5));
    set ("glideSpread", r.nextFloat() * 0.5f);
    set ("leading", r.nextFloat() < 0.8f ? 0.0f : 1.0f);
    set ("newVoices", r.nextBool() ? 0.0f : 1.0f);
    set ("staggerMs", r.nextFloat() < 0.5f ? 0.0f : r.nextFloat() * 160.0f);
    set ("staggerOrd", (float) r.nextInt (5));
    set ("path", r.nextFloat() < 0.5f ? 0.0f : (float) (1 + r.nextInt (4)));
    set ("pathDepth", 1.0f + r.nextFloat() * 5.0f);
    set ("startFrom", (float) r.nextInt (6));
    set ("startDepth", r.nextFloat() < 0.5f ? 0.0f : 2.0f + r.nextFloat() * 10.0f);
    set ("releaseBend", r.nextFloat() < 0.7f ? 0.0f : (r.nextBool() ? -1.0f : 1.0f) * (float) (2 + r.nextInt (11)));
    set ("pilot", r.nextFloat() < 0.5f ? 0.0f : (float) (1 + r.nextInt (10)));
    set ("pilotRate", 0.2f + r.nextFloat() * 2.0f);
    set ("pilotSync", r.nextFloat() < 0.5f ? 0.0f : (float) (2 + r.nextInt (4)));
    set ("pilotDensity", 0.3f + r.nextFloat() * 0.7f);
    set ("pilotSmooth", 0.2f + r.nextFloat() * 0.7f);
    set ("pilotRange", (float) (5 + r.nextInt (20)));
    set ("pilotScale", (float) r.nextInt (10));
    currentGlide = -1;
}

void GlideCatProcessor::surprise()
{
    // Sound dice: a random factory sound as the base, then bounded changes
    auto& r = juce::Random::getSystemRandom();
    applySoundPreset (r.nextInt ((int) soundPresets().size()));
    auto set = [this] (const juce::String& id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    auto get = [this] (const char* id) { return p (id); };
    for (int l = 1; l <= 2; ++l)
    {
        set (lid (l, "lpf"), juce::jlimit (60.0f, 12000.0f, get (lid (l, "lpf").toRawUTF8()) * std::pow (2.0f, r.nextFloat() * 2.0f - 1.0f)));
        set (lid (l, "detune"), r.nextFloat() * 25.0f);
        set (lid (l, "pwm"), r.nextFloat() * 0.6f);
        set (lid (l, "egDepth"), get (lid (l, "egDepth").toRawUTF8()) + r.nextFloat() * 2.0f - 1.0f);
    }
    set ("ring", r.nextFloat() < 0.8f ? 0.0f : r.nextFloat() * 0.5f);
    set ("vibDepth", r.nextFloat() * 0.3f);
    set ("drift", r.nextFloat() * 0.5f);
    set ("ensMix", r.nextFloat() * 0.7f);
    set ("prismMix", r.nextFloat() < 0.7f ? 0.0f : r.nextFloat() * 0.5f);
    set ("roomType", (float) r.nextInt (6));
    set ("roomMix", 0.1f + r.nextFloat() * 0.4f);
    currentSound = -1;
}

//==============================================================================
void GlideCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("GLIDECAT");
    root.setProperty ("sound", currentSound.load(), nullptr);
    root.setProperty ("glide", currentGlide.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void GlideCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("GLIDECAT")) return;
    currentSound = (int) root.getProperty ("sound", 0);
    currentGlide = (int) root.getProperty ("glide", 0);
    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
}

juce::AudioProcessorEditor* GlideCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<GlideCatEditor> (*this), "Glide-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlideCatProcessor();
}
