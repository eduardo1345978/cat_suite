#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

using namespace juce;

namespace
{
AudioParameterFloatAttributes percent()
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
}
AudioParameterFloatAttributes seconds()
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; });
}
}

AudioProcessorValueTreeState::ParameterLayout GlitchCatProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto unit = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, percent()));
    };
    unit ("madness", "Madness", 0.2f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "doDetune", 1 }, "Madness detunes", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "doRolls", 1 }, "Madness rolls", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "doStutter", 1 }, "Madness stutters", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "doOdd", 1 }, "Madness odd rhythm", true));
    unit ("swarm", "Bugs", 0.5f);   // id kept for saved projects
    unit ("flight", "Flight", 0.5f);
    unit ("tape", "Tape", 0.3f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.001f, 3.0f, 0.0f, 0.3f), 0.02f, seconds()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (0.01f, 5.0f, 0.0f, 0.35f), 0.6f, seconds()));
    unit ("synthLevel", "Synth", 0.8f);
    unit ("tone", "Tone", 0.6f);
    unit ("shape", "Shape", 0.2f);
    unit ("voiceLevel", "Voice", 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "voiceType", 1 }, "Voice type", glitch::voiceTypeNames(), 1));
    unit ("drumLevel", "Drums", 0.7f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "kit", 1 }, "Drum kit", hub::DrumKit::soundNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "beatMode", 1 }, "Beat", glitch::beatModeNames(), 3));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "beatStyle", 1 }, "Beat style", glitch::beatStyleNames(), 0));
    unit ("space", "Space", 0.35f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (-30.0f, 6.0f), -6.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo (no DAW)", NormalisableRange<float> (60.0f, 200.0f, 1.0f), 120.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " BPM"; })));
    return layout;
}

const std::vector<GlitchCatProcessor::Preset>& GlitchCatProcessor::presets()
{
    static const std::vector<Preset> list {
        { "Firefly Dream",     { { "madness", 0.1f }, { "swarm", 0.6f }, { "tape", 0.45f }, { "attack", 0.3f }, { "release", 1.5f }, { "tone", 0.45f },
                                 { "shape", 0.1f }, { "drumLevel", 0.45f }, { "beatStyle", 2 }, { "kit", 2 }, { "space", 0.55f } } },
        { "Drill Bugs",        { { "madness", 0.55f }, { "flight", 0.6f }, { "tape", 0.25f }, { "attack", 0.005f }, { "release", 0.3f }, { "tone", 0.7f },
                                 { "shape", 0.35f }, { "drumLevel", 0.85f }, { "beatStyle", 0 }, { "space", 0.25f } } },
        { "Broken Lullaby",    { { "madness", 0.35f }, { "swarm", 0.4f }, { "tape", 0.6f }, { "shape", 0.85f }, { "attack", 0.01f }, { "release", 1.2f },
                                 { "drumLevel", 0.5f }, { "beatStyle", 1 }, { "kit", 2 }, { "space", 0.5f } } },
        { "Whisper Cloud",     { { "madness", 0.25f }, { "swarm", 0.9f }, { "flight", 0.9f }, { "synthLevel", 0.15f }, { "voiceLevel", 0.8f }, { "voiceType", 0 },
                                 { "attack", 0.4f }, { "release", 2.0f }, { "drumLevel", 0.0f }, { "beatMode", 2 }, { "space", 0.7f } } },
        { "Ant Choir",         { { "madness", 0.3f }, { "swarm", 0.8f }, { "synthLevel", 0.2f }, { "voiceLevel", 0.8f }, { "voiceType", 2 }, { "tape", 0.35f },
                                 { "attack", 0.2f }, { "release", 1.5f }, { "drumLevel", 0.4f }, { "beatStyle", 2 }, { "space", 0.6f } } },
        { "Chattering Mouths", { { "madness", 0.6f }, { "swarm", 0.7f }, { "synthLevel", 0.1f }, { "voiceLevel", 0.9f }, { "voiceType", 3 },
                                 { "drumLevel", 0.6f }, { "beatStyle", 1 }, { "space", 0.3f } } },
        { "Machine Gun Hats",  { { "madness", 0.85f }, { "tone", 0.8f }, { "shape", 0.4f }, { "drumLevel", 1.0f }, { "beatStyle", 0 }, { "kit", 1 },
                                 { "attack", 0.003f }, { "release", 0.25f }, { "space", 0.2f } } },
        { "Melting Tape",      { { "madness", 0.45f }, { "tape", 1.0f }, { "doRolls", 0 }, { "tone", 0.5f }, { "attack", 0.1f }, { "release", 1.0f },
                                 { "drumLevel", 0.5f }, { "beatStyle", 2 }, { "kit", 2 }, { "space", 0.45f } } },
        { "Insect Rave",       { { "madness", 0.5f }, { "swarm", 0.6f }, { "shape", 0.3f }, { "tone", 0.8f }, { "drumLevel", 0.9f }, { "beatStyle", 3 },
                                 { "kit", 1 }, { "attack", 0.005f }, { "release", 0.4f }, { "space", 0.3f } } },
        { "Hive Mind",         { { "madness", 0.7f }, { "swarm", 1.0f }, { "flight", 1.0f }, { "voiceLevel", 0.4f }, { "voiceType", 1 }, { "drumLevel", 0.7f },
                                 { "beatStyle", 0 }, { "space", 0.4f } } },
        { "Total Collapse",    { { "madness", 1.0f }, { "swarm", 0.7f }, { "flight", 0.8f }, { "drumLevel", 0.9f }, { "beatStyle", 0 }, { "tape", 0.5f },
                                 { "space", 0.3f } } },
        { "My Voice Swarm",    { { "madness", 0.35f }, { "synthLevel", 0.2f }, { "voiceLevel", 0.9f }, { "voiceType", 4 }, { "swarm", 0.6f },
                                 { "drumLevel", 0.5f }, { "beatStyle", 1 }, { "space", 0.4f } } },
    };
    return list;
}

void GlitchCatProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* wp = dynamic_cast<RangedAudioParameter*> (prm))
            if (wp->getParameterID() != "tempo" && wp->getParameterID() != "volume")
                wp->setValueNotifyingHost (wp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String GlitchCatProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

void GlitchCatProcessor::mutate()
{
    Random rng;
    auto nudge = [&] (const char* id, float amount)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (jlimit (0.0f, 1.0f, prm->getValue() + (rng.nextFloat() * 2.0f - 1.0f) * amount));
    };
    for (auto* id : { "swarm", "flight", "tape", "tone", "shape", "space" }) nudge (id, 0.25f);
    if (rng.nextFloat() < 0.3f) nudge ("beatStyle", 1.0f);
    if (rng.nextFloat() < 0.3f) nudge ("kit", 1.0f);
    if (p ("voiceLevel") > 0.05f && rng.nextFloat() < 0.3f && roundToInt (p ("voiceType")) != glitch::MyVoice)
        if (auto* prm = apvts.getParameter ("voiceType")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) rng.nextInt (4)));
}

//==============================================================================
GlitchCatProcessor::GlitchCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)
                                       .withInput ("Voice in", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "GlitchCat", createLayout())
{
    startTimer (500);
}

GlitchCatProcessor::~GlitchCatProcessor()
{
    stopTimer();
    engine.setSample (nullptr);
}

bool GlitchCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void GlitchCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    engine.prepare (sr, samplesPerBlock);
    limiter.prepare (sr);
    if (! recording.load()) recBuffer.assign ((size_t) (maxVoiceSeconds * sr), 0.0f);
}

void GlitchCatProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    mergeComputerKeyboard (midi, buffer.getNumSamples());
    const int n = buffer.getNumSamples();

    // the voice coming in (side-chain): meter + recording
    const int inCh = jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    if (inCh > 0)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < inCh; ++ch) peak = jmax (peak, buffer.getMagnitude (ch, 0, n));
        inputLevel = jmax (peak, inputLevel.load() * 0.9f);
        if (peak > 1.0e-5f) inputSeen = true;
        if (recording.load())
        {
            int pos = recPos.load();
            for (int i = 0; i < n && pos < (int) recBuffer.size(); ++i, ++pos)
            {
                float v = 0.0f;
                for (int ch = 0; ch < inCh; ++ch) v += buffer.getSample (ch, i);
                recBuffer[(size_t) pos] = v / (float) inCh;
            }
            recPos = pos;
            if (pos >= (int) recBuffer.size()) recording = false;    // full: the timer makes it the voice
        }
    }

    glitch::Settings s;
    s.madness = p ("madness");
    s.detune = p ("doDetune") > 0.5f; s.rolls = p ("doRolls") > 0.5f; s.stutter = p ("doStutter") > 0.5f; s.odd = p ("doOdd") > 0.5f;
    s.swarm = p ("swarm"); s.flight = p ("flight"); s.tape = p ("tape");
    s.attack = p ("attack"); s.release = p ("release");
    s.synthLevel = p ("synthLevel"); s.tone = p ("tone"); s.shape = p ("shape");
    s.voiceLevel = p ("voiceLevel"); s.voiceType = roundToInt (p ("voiceType"));
    s.drumLevel = p ("drumLevel"); s.kit = roundToInt (p ("kit"));
    s.beatMode = roundToInt (p ("beatMode")); s.beatStyle = roundToInt (p ("beatStyle"));
    s.space = p ("space"); s.volumeDb = p ("volume"); s.fallbackBpm = p ("tempo");

    glitch::Engine::Transport tp;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue()) tp.bpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue() && pos->getIsPlaying()) { tp.playing = true; tp.ppq = *q; }
        }

    engine.process (buffer, midi, s, tp);
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), n);
    midi.clear();
}

//==============================================================================
void GlitchCatProcessor::setVoice (std::unique_ptr<glitch::Sample> s)
{
    const ScopedLock l (voiceLock);
    engine.setSample (s.get());
    if (voice != nullptr) retired.push_back (std::move (voice));
    retiredAt = Time::getMillisecondCounter();
    voice = std::move (s);
}

void GlitchCatProcessor::timerCallback()
{
    {
        const ScopedLock l (voiceLock);
        if (! retired.empty() && Time::getMillisecondCounter() - retiredAt > 2000) retired.clear();
    }
    // a recording that filled up while nobody pressed stop
    if (! recording.load() && recPos.load() > 0) stopRecording();
}

static std::unique_ptr<glitch::Sample> tidy (std::vector<float> mono, double rate, const String& name)
{
    // trim the silence at both ends, then bring it up to a good level
    float peak = 0.0f;
    for (float v : mono) peak = jmax (peak, std::abs (v));
    if (peak < 1.0e-4f) return nullptr;
    const float gate = peak * 0.05f;
    size_t a = 0, b = mono.size();
    while (a < b && std::abs (mono[a]) < gate) ++a;
    while (b > a && std::abs (mono[b - 1]) < gate) --b;
    a = (size_t) jmax (0, (int) a - (int) (rate * 0.01));
    b = jmin (mono.size(), b + (size_t) (rate * 0.05));
    if (b - a < (size_t) (rate * 0.05)) return nullptr;
    auto s = std::make_unique<glitch::Sample>();
    s->data.assign (mono.begin() + (long) a, mono.begin() + (long) b);
    for (auto& v : s->data) v *= 0.9f / peak;
    // a few ms fades so the loop points don't click
    const int f = jmin ((int) s->data.size() / 4, (int) (rate * 0.005));
    for (int i = 0; i < f; ++i)
    {
        const float g = (float) i / (float) f;
        s->data[(size_t) i] *= g;
        s->data[s->data.size() - 1 - (size_t) i] *= g;
    }
    s->rate = rate;
    s->name = name;
    return s;
}

bool GlitchCatProcessor::loadVoice (const File& file, String& error)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Glitch-Cat can't read " + file.getFileName(); return false; }
    // a broken or hostile file can claim anything in its header: only believe sane numbers
    if (reader->numChannels < 1 || reader->numChannels > 32 || ! (reader->sampleRate >= 4000.0 && reader->sampleRate <= 768000.0))
    { error = file.getFileName() + " looks broken"; return false; }
    const int len = (int) jmin (reader->lengthInSamples, (int64) (reader->sampleRate * maxVoiceSeconds));
    if (len <= 0) { error = "That file is empty"; return false; }
    AudioBuffer<float> buf ((int) jmax (1u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, true);
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < len; ++i) mono[(size_t) i] += buf.getSample (ch, i) / (float) buf.getNumChannels();
    auto s = tidy (std::move (mono), reader->sampleRate, file.getFileNameWithoutExtension());
    if (s == nullptr) { error = "That recording is silent"; return false; }
    setVoice (std::move (s));
    return true;
}

void GlitchCatProcessor::clearVoice() { setVoice (nullptr); }

String GlitchCatProcessor::voiceName() const
{
    const ScopedLock l (voiceLock);
    return voice != nullptr ? voice->name : String();
}

float GlitchCatProcessor::voiceSeconds() const
{
    const ScopedLock l (voiceLock);
    return voice != nullptr ? (float) (voice->data.size() / voice->rate) : 0.0f;
}

void GlitchCatProcessor::startRecording()
{
    if (recording.load() || recBuffer.empty()) return;
    recPos = 0;
    recording = true;
}

void GlitchCatProcessor::stopRecording()
{
    recording = false;
    const int n = recPos.exchange (0);
    if (n <= 0) return;
    std::vector<float> data (recBuffer.begin(), recBuffer.begin() + n);
    if (auto s = tidy (std::move (data), sr, "My recording")) setVoice (std::move (s));
}

//==============================================================================
void GlitchCatProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.removeChild (state.getChildWithName ("VOICE"), nullptr);
    {
        const ScopedLock l (voiceLock);
        if (voice != nullptr)
        {
            // 16-bit PCM, base64
            MemoryBlock pcm (voice->data.size() * 2);
            auto* d = static_cast<int16*> (pcm.getData());
            for (size_t i = 0; i < voice->data.size(); ++i) d[i] = (int16) jlimit (-32767, 32767, roundToInt (voice->data[i] * 32767.0f));
            ValueTree v ("VOICE");
            v.setProperty ("rate", voice->rate, nullptr);
            v.setProperty ("name", voice->name, nullptr);
            v.setProperty ("pcm", pcm.toBase64Encoding(), nullptr);
            state.appendChild (v, nullptr);
        }
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GlitchCatProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = ValueTree::fromXml (*xml);
    currentPreset = (int) state.getProperty ("preset", -1);
    state.removeProperty ("preset", nullptr);
    if (auto v = state.getChildWithName ("VOICE"); v.isValid())
    {
        MemoryBlock pcm;
        if (pcm.fromBase64Encoding (v.getProperty ("pcm").toString()) && pcm.getSize() >= 4)
        {
            auto s = std::make_unique<glitch::Sample>();
            const auto* d = static_cast<const int16*> (pcm.getData());
            s->data.resize (pcm.getSize() / 2);
            for (size_t i = 0; i < s->data.size(); ++i) s->data[i] = (float) d[i] / 32767.0f;
            s->rate = jlimit (8000.0, 192000.0, (double) v.getProperty ("rate", 48000.0));
            s->name = v.getProperty ("name").toString();
            setVoice (std::move (s));
        }
        state.removeChild (v, nullptr);
    }
    else setVoice (nullptr);
    apvts.replaceState (state);
}

AudioProcessorEditor* GlitchCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<GlitchCatEditor> (*this), "Glitch-Cat");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchCatProcessor();
}
