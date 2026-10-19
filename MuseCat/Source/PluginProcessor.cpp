#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace cat::muse;

namespace
{
    const char* const volIds[NumParts]   = { "volChords", "volBass", "volDrums", "volMelody", "volCounter" };
    const char* const soundIds[NumParts] = { "sndChords", "sndBass", "sndDrums", "sndMelody", "sndCounter" };

    bool sameSong (const Settings& a, const Settings& b)
    {
        return a.genre == b.genre && a.mood == b.mood && a.seed == b.seed && a.endless == b.endless && a.partSeeds == b.partSeeds;
    }
}

//==============================================================================
const std::vector<MuseCatProcessor::Preset>& MuseCatProcessor::presets()
{
    static const std::vector<Preset> list = {
        { "Sunny Pop",              Pop,       Happy,  101, false },
        { "Rainy Lo-fi",            LoFi,      Sad,    202, false },
        { "Night Trap",             Trap,      Dark,   303, false },
        { "Dance All Night",        House,     Happy,  404, false },
        { "Dreamy Clouds",          Ambient,   Dreamy, 505, false },
        { "Cool Cat Jazz",          Jazz,      Chill,  606, false },
        { "Garage Rock",            Rock,      Epic,   707, false },
        { "Movie Hero",             Cinematic, Epic,   808, false },
        { "Sad Piano Pop",          Pop,       Sad,    909, false },
        { "Spooky Alley",           Cinematic, Dark,   1313, false },
        { "Study Beats (endless)",  LoFi,      Chill,  111, true },
        { "Endless Dream",          Ambient,   Dreamy, 222, true },
        { "Endless Club",           House,     Epic,   333, true },
    };
    return list;
}

juce::StringArray MuseCatProcessor::soundChoices (int part)
{
    switch (part)
    {
        case Bass:  return { "Auto (fits the genre)", "Sub", "Saw", "Reese", "Pluck" };
        case Drums: return { "Auto (fits the genre)", "Boom Bap", "808", "Lo-fi", "Live" };
        default:    return { "Auto (fits the genre)", "Soft Pad", "E-Piano", "Pluck", "Strings" };
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MuseCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "genre", 1 }, "Genre", genreNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mood", 1 }, "Mood", moodNames(), 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Song Number", 1, 999999, 101));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "endless", 1 }, "Endless", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sound", 1 }, "Own Sound", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "midiOut", 1 }, "MIDI Out", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Volume", NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    const auto parts = partNames();
    for (int p = 0; p < NumParts; ++p)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { volIds[p], 1 }, parts[p] + " Volume", NormalisableRange<float> (0.0f, 1.0f), 0.8f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { soundIds[p], 1 }, parts[p] + " Sound", soundChoices (p), 0));
    }
    return layout;
}

MuseCatProcessor::MuseCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            refs[r->getParameterID()] = apvts.getRawParameterValue (r->getParameterID());
    for (auto& m : parts) m.ensureSize (4096);
    outMidi.ensureSize (8192);
    applyPreset (0);
    startTimerHz (20);
}

MuseCatProcessor::~MuseCatProcessor()
{
    stopTimer();
}

float MuseCatProcessor::p (const char* id) const
{
    auto it = refs.find (id);
    return it != refs.end() && it->second != nullptr ? it->second->load() : 0.0f;
}

//==============================================================================
int MuseCatProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String MuseCatProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}

void MuseCatProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    const auto& pr = presets()[(size_t) index];
    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    set ("genre", (float) pr.genre);
    set ("mood", (float) pr.mood);
    set ("seed", (float) pr.seed);
    set ("endless", pr.endless ? 1.0f : 0.0f);
    partSeeds = {};
    currentPreset = index;
    updateSong (true);
}

Settings MuseCatProcessor::currentSettings() const
{
    Settings st;
    st.genre = juce::jlimit (0, (int) NumGenres - 1, juce::roundToInt (p ("genre")));
    st.mood = juce::jlimit (0, (int) NumMoods - 1, juce::roundToInt (p ("mood")));
    st.seed = (juce::uint32) juce::jlimit (1, 999999, juce::roundToInt (p ("seed")));
    st.endless = p ("endless") > 0.5f;
    st.partSeeds = partSeeds;
    return st;
}

void MuseCatProcessor::setSongNow (std::shared_ptr<const Song> s)
{
    player.setSong (std::move (s));
}

void MuseCatProcessor::updateSong (bool force)
{
    const auto st = currentSettings();
    if (force || ! composedOnce || ! sameSong (st, composedFor))
    {
        composedFor = st;
        composedOnce = true;
        setSongNow (std::make_shared<Song> (compose (st)));
    }
    // endless: write the next piece while this one plays
    if (player.needNext())
        if (auto now = player.getSong())
            player.queueNext (std::make_shared<Song> (composeNext (*now)));
}

void MuseCatProcessor::regenerate (int target)
{
    auto now = player.getSong();
    if (now == nullptr) return;
    auto s = std::make_shared<Song> (*now);
    cat::muse::regenerate (*s, target, (juce::uint32) (1 + juce::Random().nextInt (0x7ffffff)));
    partSeeds = s->settings.partSeeds;
    composedFor = currentSettings();
    setSongNow (s);
}

void MuseCatProcessor::setSeed (juce::uint32 seed)
{
    partSeeds = {};
    if (auto* prm = apvts.getParameter ("seed")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (1u, 999999u, seed)));
    updateSong();
}

void MuseCatProcessor::newSeed() { setSeed ((juce::uint32) (1 + juce::Random().nextInt (999999))); }

void MuseCatProcessor::timerCallback() { updateSong(); }

SoundSet MuseCatProcessor::soundSet() const
{
    auto ss = defaultSounds (juce::roundToInt (p ("genre")));
    int* slots[NumParts] = { &ss.chords, &ss.bass, &ss.drums, &ss.melody, &ss.counter };
    for (int part = 0; part < NumParts; ++part)
    {
        const int choice = juce::roundToInt (p (soundIds[part]));
        if (choice > 0) *slots[part] = choice - 1;
    }
    return ss;
}

bool MuseCatProcessor::exportWav (const juce::File& file, int onlyPart, std::function<bool (float)> progress) const
{
    auto song = player.getSong();
    return song != nullptr && SongRenderer::renderToWav (*song, soundSet(), file, 44100.0, onlyPart, std::move (progress));
}

bool MuseCatProcessor::exportMidi (const juce::File& file) const
{
    auto song = player.getSong();
    if (song == nullptr) return false;
    file.deleteFile();
    auto os = file.createOutputStream();
    return os != nullptr && toMidiFile (*song).writeTo (*os);
}

//==============================================================================
bool MuseCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
}

void MuseCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSync.prepare (sampleRate);
    renderer.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    shownSounds.fill (-1);
    internalPpq = 0.0;
    wasPreview = false;
}

void MuseCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    midi.clear();                      // Muse-Cat writes its own notes
    if (n <= 0) return;
    hostSync.beginBlock (getPlayHead(), n);
    const auto t = hostSync.get();
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;

    // the host's transport, or Muse-Cat's own Play (standalone, or Cubase stopped)
    bool playing = false;
    double ppq = 0.0, pps = 120.0 / 60.0 / sr;
    double bpmNow = t.bpm;
    if (t.hostPlaying)
    {
        playing = true;
        ppq = t.ppqStart;
        pps = t.ppqPerSample;
        wasPreview = false;
        if (previewPlaying.load()) previewPlaying = false;
    }
    else if (previewPlaying.load())
    {
        if (! wasPreview) internalPpq = 0.0;
        wasPreview = true;
        bpmNow = player.songBpm();
        pps = bpmNow / 60.0 / sr;
        playing = true;
        ppq = internalPpq;
        internalPpq += pps * n;
    }
    else wasPreview = false;
    uiRunning = playing;
    uiHostPlaying = t.hostPlaying;
    uiBpm = bpmNow;

    std::array<juce::MidiBuffer*, NumParts> ptrs;
    for (int part = 0; part < NumParts; ++part) { parts[(size_t) part].clear(); ptrs[(size_t) part] = &parts[(size_t) part]; }
    player.process (ppq, pps, n, playing, ptrs);

    // its own sound
    const auto ss = soundSet();
    const std::array<int, 5> want { ss.chords, ss.bass, ss.drums, ss.melody, ss.counter };
    if (want != shownSounds) { renderer.setSounds (ss); shownSounds = want; }
    static const float baseLevels[NumParts] = { 0.45f, 0.75f, 0.4f, 1.4f, 0.7f };
    for (int part = 0; part < NumParts; ++part) renderer.levels[(size_t) part] = baseLevels[part] * 1.25f * p (volIds[part]);
    if (p ("sound") > 0.5f && buffer.getNumChannels() > 0)
    {
        for (int part = 0; part < NumParts; ++part) renderer.handle (part, parts[(size_t) part]);
        float* l = buffer.getWritePointer (0);
        if (buffer.getNumChannels() > 1)
            renderer.render (l, buffer.getWritePointer (1), n);
        else
        {
            juce::HeapBlock<float> r ((size_t) n);
            renderer.render (l, r.get(), n);
            juce::FloatVectorOperations::add (l, r.get(), n);
            juce::FloatVectorOperations::multiply (l, 0.5f, n);
        }
        const float master = p ("master");
        buffer.applyGain (master * master * 1.5625f);   // 0.8 = as written
    }
    else renderer.allOff();

    // MIDI out: every part on its own channel
    if (p ("midiOut") > 0.5f)
        for (auto& m : parts) midi.addEvents (m, 0, n, 0);
}

//==============================================================================
void MuseCatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::StringArray seeds;
    for (auto v : partSeeds) seeds.add (juce::String ((juce::int64) v));
    state.setProperty ("partSeeds", seeds.joinIntoString (","), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MuseCatProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    const auto seeds = juce::StringArray::fromTokens (state.getProperty ("partSeeds", "").toString(), ",", "");
    partSeeds = {};
    for (int i = 0; i < juce::jmin (seeds.size(), (int) partSeeds.size()); ++i) partSeeds[(size_t) i] = (juce::uint32) seeds[i].getLargeIntValue();
    currentPreset = (int) state.getProperty ("preset", -1);
    state.removeProperty ("partSeeds", nullptr);
    state.removeProperty ("preset", nullptr);
    apvts.replaceState (state);
    if (juce::MessageManager::existsAndIsCurrentThread()) updateSong (true);
    else composedOnce = false;       // the timer writes it
}

juce::AudioProcessorEditor* MuseCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<MuseCatEditor> (*this), "Muse-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MuseCatProcessor();
}
