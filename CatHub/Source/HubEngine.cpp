#include "HubEngine.h"
#include "SongRenderer.h"

namespace hub
{
//==============================================================================
const char* catName (int id)
{
    static const char* names[] = { "Chord-Cat", "Bass-Cat", "Drum-Cat", "Life-Cat", "Patt-Cat", "Bend-Cat", "Orbit-Cat", "Glide-Cat", "Sing-Cat", "Talk-Cat" };
    return names[juce::jlimit (0, (int) NumCats - 1, id)];
}

const char* sourceName (int s)
{
    static const char* names[] = { "Chords", "Bass line", "Drums", "Life creatures", "Lead / melody", "Counter-melody" };
    return names[juce::jlimit (0, (int) NumSources - 1, s)];
}

int catForSource (int s)
{
    switch (s)
    {
        case SrcBass:  return BassCat;
        case SrcDrums: return DrumCat;
        case SrcLife:  return LifeCat;
        default:       return ChordCat;
    }
}

const char* midiFxName (int f)
{
    static const char* names[] = { "No effect", "Patt-Cat  (patterns)", "Bend-Cat  (pitch bends)", "Orbit-Cat  (orbit arp)" };
    return names[juce::jlimit (0, (int) NumMidiFx - 1, f)];
}

int catForMidiFx (int f)
{
    switch (f)
    {
        case FxPatt:  return PattCat;
        case FxBend:  return BendCat;
        case FxOrbit: return OrbitCat;
        default:      return -1;
    }
}

const char* soundName (int s)
{
    static const char* names[] = { "Soft Pad", "E-Piano", "Pluck", "Strings", "Sub Bass", "Saw Bass", "Reese Bass", "Pluck Bass",
                                   "Boom Bap Kit", "808 Kit", "Lo-fi Kit", "Live Kit", "Glide-Cat", "Sing-Cat", "Talk-Cat", "Life-Cat's own",
                                   "Chip", "Glass Bell", "Piano", "Nylon Guitar" };
    return names[juce::jlimit (0, (int) NumSounds - 1, s)];
}

int catForSound (int s)
{
    switch (s)
    {
        case SndGlide: return GlideCat;
        case SndSing:  return SingCat;
        case SndTalk:  return TalkCat;
        default:       return -1;
    }
}

juce::StringArray soundGroups() { return { "Keys", "Bass", "Drum kits", "Sound Cats", "Life" }; }

int keysSound (int i)
{
    if (i <= 3) return SndPad + juce::jmax (0, i);
    static const int more[] = { SndChip, SndBell, SndPiano, SndGuitar };
    return more[juce::jlimit (0, 3, i - 4)];
}
int keysIndexOf (int s)
{
    if (s <= SndStrings) return s - SndPad;
    if (s == SndChip) return 4;
    if (s == SndBell) return 5;
    if (s == SndPiano) return 6;
    return 7;
}
bool isKeysSound (int s) { return s <= SndStrings || s == SndChip || s == SndBell || s == SndPiano || s == SndGuitar; }

int soundGroup (int s)
{
    if (isKeysSound (s)) return 0;
    if (s <= SndBassPluck) return 1;
    if (s <= SndLive) return 2;
    if (s <= SndTalk) return 3;
    return 4;
}

//==============================================================================
std::vector<int> Track::takeNotes()
{
    std::vector<int> out;
    for (int w = 0; w < 4; ++w)
    {
        const uint32_t bits = notes[(size_t) w].load() | hits[(size_t) w].exchange (0);
        for (int b = 0; b < 32; ++b) if (bits & (1u << b)) out.push_back (w * 32 + b);
    }
    return out;
}

void Track::copySettingsFrom (const Track& o)
{
    source = o.source.load(); midiFx = o.midiFx.load(); sound = o.sound.load();
    volume = o.volume.load(); pan = o.pan.load(); reverb = o.reverb.load();
    filter = o.filter.load(); drive = o.drive.load(); chorus = o.chorus.load(); delay = o.delay.load(); crush = o.crush.load();
    mute = o.mute.load(); solo = o.solo.load();
}

//==============================================================================
namespace
{
    juce::AudioProcessorParameter* findParam (juce::AudioProcessor& p, const juce::String& name)
    {
        for (auto* prm : p.getParameters())
            if (prm->getName (100) == name) return prm;
        return nullptr;
    }

    /** Sets a choice / switch parameter of a hosted Cat by the text of its value. */
    bool setChoice (juce::AudioProcessor* p, const juce::String& name, const juce::String& text)
    {
        if (p == nullptr) return false;
        auto* prm = findParam (*p, name);
        if (prm == nullptr) return false;
        const int steps = juce::jmax (2, prm->getNumSteps());
        for (int k = 0; k < steps; ++k)
        {
            const float v = (float) k / (float) (steps - 1);
            if (prm->getText (v, 100).trim().equalsIgnoreCase (text))
            {
                prm->setValueNotifyingHost (v);
                return true;
            }
        }
        return false;
    }

    int choiceIndex (juce::AudioProcessor* p, const juce::String& name)
    {
        if (p == nullptr) return -1;
        auto* prm = findParam (*p, name);
        if (prm == nullptr) return -1;
        const int steps = juce::jmax (2, prm->getNumSteps());
        return juce::roundToInt (prm->getValue() * (float) (steps - 1));
    }

    juce::StringArray choiceTexts (juce::AudioProcessor* p, const juce::String& name)
    {
        juce::StringArray out;
        if (p == nullptr) return out;
        if (auto* prm = findParam (*p, name))
        {
            const int steps = juce::jmax (2, prm->getNumSteps());
            if (steps > 64) return out;
            for (int k = 0; k < steps; ++k) out.add (prm->getText ((float) k / (float) (steps - 1), 100).trim());
        }
        return out;
    }

    template <typename Synth>
    void renderWithMidi (Synth& s, const juce::MidiBuffer& m, float* l, float* r, int n)
    {
        int pos = 0;
        for (const auto meta : m)
        {
            const int p = juce::jlimit (0, n, meta.samplePosition);
            if (p > pos) { s.render (l + pos, r + pos, p - pos); pos = p; }
            s.handle (meta.getMessage());
        }
        if (pos < n) s.render (l + pos, r + pos, n - pos);
    }

    // copies without giving up the destination's memory (no allocation on the audio thread)
    void copyMidi (juce::MidiBuffer& dst, const juce::MidiBuffer& src)
    {
        dst.clear();
        dst.addEvents (src, 0, -1, 0);
    }

    // song parts arrive on their own channels; the instruments want channel 1 (drums keep 10)
    void copyMidiTo (juce::MidiBuffer& dst, const juce::MidiBuffer& src, int channel)
    {
        dst.clear();
        for (const auto meta : src)
        {
            auto m = meta.getMessage();
            if (m.getChannel() > 0) m.setChannel (channel);
            dst.addEvent (m, meta.samplePosition);
        }
    }

    /** Changes one property in a hosted Cat's saved state (unwrapping the VST3 host's wrapper). */
    bool setCatStateProperty (juce::AudioPluginInstance* p, const juce::Identifier& name, const juce::String& value)
    {
        if (p == nullptr) return false;
        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        auto outer = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        if (outer == nullptr) return false;
        if (outer->hasTagName ("VST3PluginState"))
        {
            auto* comp = outer->getChildByName ("IComponent");
            if (comp == nullptr) return false;
            juce::MemoryBlock inner;
            if (! inner.fromBase64Encoding (comp->getAllSubText())) return false;
            auto xml = juce::AudioProcessor::getXmlFromBinary (inner.getData(), (int) inner.getSize());
            if (xml == nullptr) return false;
            xml->setAttribute (name, value);
            juce::MemoryBlock newInner;
            juce::AudioProcessor::copyXmlToBinary (*xml, newInner);
            comp->deleteAllTextElements();
            comp->addTextElement (newInner.toBase64Encoding());
        }
        else outer->setAttribute (name, value);
        juce::MemoryBlock out;
        juce::AudioProcessor::copyXmlToBinary (*outer, out);
        p->setStateInformation (out.getData(), (int) out.getSize());
        return true;
    }

    void addPanic (juce::MidiBuffer& m, int pos)
    {
        for (int ch = 1; ch <= 16; ++ch)
            m.addEvent (juce::MidiMessage::allNotesOff (ch), pos);
    }

    //==========================================================================
    struct TrackDef { int source, fx, sound; float vol, rev, filter = 0, drive = 0, chorus = 0, delay = 0, crush = 0, pan = 0.5f; };
    struct StyleDef
    {
        const char* name;
        double bpm;
        int bars;
        const char* progression;    // a Chord-Cat preset
        const char* drums;          // Drum-Cat style
        const char* bass;           // Bass-Cat style
        const char* life;           // a Life-Cat preset ("" = keep)
        std::vector<TrackDef> tracks;
    };

    const std::vector<StyleDef>& styles()
    {
        static const std::vector<StyleDef> s = {
            { "Cat Jam (default)", 92, 1, "Neo Soul Glide", "Boom Bap", "Pop Roots", "Quiet Pond",
              { { SrcChords, FxNone, SndPad, 0.70f, 0.25f }, { SrcBass, FxNone, SndSub, 0.72f, 0.03f },
                { SrcDrums, FxNone, SndBoomBap, 0.75f, 0.08f }, { SrcLife, FxNone, SndLifeOwn, 0.62f, 0.0f } } },
            { "Lo-fi Study", 78, 1, "Lofi Rain", "Lo-Fi", "Dilla", "Rainy Garden",
              { { SrcChords, FxNone, SndEPiano, 0.72f, 0.20f, 0.35f, 0.0f, 0.35f, 0.15f, 0.25f },
                { SrcBass, FxNone, SndSub, 0.70f, 0.02f, 0.2f },
                { SrcDrums, FxNone, SndLofi, 0.74f, 0.06f, 0.15f },
                { SrcLife, FxNone, SndLifeOwn, 0.50f, 0.0f, 0.3f } } },
            { "Trap Night", 140, 2, "Dark Trap", "Trap", "808", "Night Forest",
              { { SrcChords, FxPatt, SndPluck, 0.66f, 0.25f, 0.1f, 0.0f, 0.0f, 0.35f },
                { SrcBass, FxNone, SndSub, 0.78f, 0.0f, 0.0f, 0.35f },
                { SrcDrums, FxNone, Snd808, 0.78f, 0.04f },
                { SrcChords, FxNone, SndStrings, 0.42f, 0.4f, 0.45f } } },
            { "House Party", 124, 2, "Club Lights", "House", "House", "Busy Garden",
              { { SrcChords, FxNone, SndStrings, 0.60f, 0.25f, 0.25f, 0.0f, 0.3f },
                { SrcBass, FxNone, SndSaw, 0.70f, 0.02f, 0.15f, 0.2f },
                { SrcDrums, FxNone, Snd808, 0.78f, 0.05f },
                { SrcLead, FxOrbit, SndPluck, 0.50f, 0.3f, 0.1f, 0.0f, 0.0f, 0.45f } } },
            { "Neo Soul", 88, 1, "Velvet Sunday", "Neo Soul", "Neo Soul", "Night Forest",
              { { SrcChords, FxNone, SndEPiano, 0.72f, 0.22f, 0.1f, 0.15f, 0.3f },
                { SrcBass, FxNone, SndReese, 0.62f, 0.02f, 0.35f },
                { SrcDrums, FxNone, SndLive, 0.74f, 0.1f },
                { SrcLead, FxBend, SndGlide, 0.48f, 0.3f, 0.0f, 0.0f, 0.0f, 0.25f } } },
            { "Ambient Garden", 70, 4, "Moon Garden", "Lo-Fi", "Drone", "Glass Garden",
              { { SrcChords, FxNone, SndStrings, 0.62f, 0.55f, 0.25f, 0.0f, 0.5f, 0.3f },
                { SrcBass, FxNone, SndSub, 0.50f, 0.1f },
                { SrcDrums, FxNone, SndLofi, 0.30f, 0.3f, 0.5f },
                { SrcLife, FxNone, SndLifeOwn, 0.72f, 0.1f },
                { SrcChords, FxNone, SndSing, 0.50f, 0.45f } } },
            { "Afro Sun", 108, 1, "Island Breeze", "Afrobeats", "Afrobeats", "Dolphin Bay",
              { { SrcChords, FxPatt, SndPluck, 0.62f, 0.2f, 0.0f, 0.0f, 0.2f, 0.2f },
                { SrcBass, FxNone, SndBassPluck, 0.70f, 0.02f },
                { SrcDrums, FxNone, SndLive, 0.78f, 0.06f },
                { SrcLife, FxNone, SndLifeOwn, 0.55f, 0.0f } } },
            { "Rock Anthem", 120, 1, "Four Chord Anthem", "Rock", "Rock", "",
              { { SrcChords, FxNone, SndSaw, 0.0f, 0.0f },   // placeholder replaced below
              } },
            { "Jazz Cafe", 112, 1, "Jazz 2-5-1", "Dilla", "Walking", "Coffee Steam",
              { { SrcChords, FxNone, SndEPiano, 0.68f, 0.2f, 0.15f },
                { SrcBass, FxNone, SndBassPluck, 0.70f, 0.05f, 0.3f },
                { SrcDrums, FxNone, SndLive, 0.62f, 0.12f, 0.2f },
                { SrcLead, FxOrbit, SndEPiano, 0.40f, 0.3f, 0.3f } } },
            { "Choir & Cats", 84, 2, "Cat Lullaby", "Neo Soul", "Pop Roots", "Whale Choir",
              { { SrcChords, FxNone, SndSing, 0.66f, 0.35f },
                { SrcBass, FxNone, SndSub, 0.62f, 0.05f },
                { SrcDrums, FxNone, SndBoomBap, 0.62f, 0.1f },
                { SrcLead, FxNone, SndTalk, 0.55f, 0.3f } } },
        };
        return s;
    }

    std::vector<TrackDef> rockTracks()
    {
        return { { SrcChords, FxNone, SndSaw, 0.55f, 0.15f, 0.2f, 0.55f, 0.2f },
                 { SrcBass, FxNone, SndSaw, 0.70f, 0.02f, 0.25f, 0.3f },
                 { SrcDrums, FxNone, SndLive, 0.80f, 0.12f },
                 { SrcLead, FxNone, SndStrings, 0.45f, 0.35f, 0.2f } };
    }
}

//==============================================================================
juce::Optional<juce::AudioPlayHead::PositionInfo> HubEngine::PlayHead::getPosition() const
{
    PositionInfo i;
    i.setBpm (bpm);
    i.setTimeSignature (TimeSignature { 4, 4 });
    i.setIsPlaying (playing);
    i.setIsRecording (false);
    i.setPpqPosition (ppq);
    i.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
    i.setTimeInSamples (samples);
    i.setTimeInSeconds ((double) samples / sampleRate);
    i.setIsLooping (false);
    return i;
}

//==============================================================================
HubEngine::HubEngine()
{
    formats.addFormat (new juce::VST3PluginFormat());
    for (auto& t : tracks) t = std::make_unique<Track>();
    writerThread.startThread();
}

HubEngine::~HubEngine()
{
    stopRecording();
    releaseCats();
    writerThread.stopThread (2000);
}

juce::Array<juce::File> HubEngine::defaultSearchFolders (const juce::File& extraFolder)
{
    juce::Array<juce::File> dirs;
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    if (extraFolder != juce::File()) dirs.add (extraFolder);
    dirs.add (exeDir.getChildFile ("Hub Cats"));        // the copies installed with the Hub (same version)
    dirs.add (exeDir);
   #if JUCE_WINDOWS
    dirs.add (juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("Programs").getChildFile ("Common").getChildFile ("VST3"));
    dirs.add (juce::File (juce::SystemStats::getEnvironmentVariable ("CommonProgramFiles", "C:\\Program Files\\Common Files")).getChildFile ("VST3"));
   #elif JUCE_MAC
    dirs.add (juce::File ("~/Library/Audio/Plug-Ins/VST3"));
    dirs.add (juce::File ("/Library/Audio/Plug-Ins/VST3"));
   #else
    dirs.add (juce::File ("~/.vst3"));
    dirs.add (juce::File ("/usr/lib/vst3"));
   #endif
    return dirs;
}

std::unique_ptr<juce::AudioPluginInstance> HubEngine::createCat (int id, juce::String& error)
{
    const auto file = catFiles[(size_t) id];
    if (file == juce::File()) { error = juce::String (catName (id)) + " not found"; return nullptr; }
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : formats.getFormats()) f->findAllTypesForFile (types, file.getFullPathName());
    if (types.isEmpty()) { error = juce::String (catName (id)) + " could not be read"; lastLoadError = error; return nullptr; }
    auto instance = formats.createPluginInstance (*types[0], sampleRate, maxBlockSize, error);
    if (instance == nullptr) { lastLoadError = juce::String (catName (id)) + " could not be loaded" + (error.isNotEmpty() ? ": " + error : juce::String()); return nullptr; }
    instance->enableAllBuses();
    instance->setPlayHead (&playHead);
    return instance;
}

void HubEngine::prepareCat (juce::AudioPluginInstance& p)
{
    p.setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
    p.prepareToPlay (sampleRate, maxBlockSize);
}

juce::String HubEngine::loadCats (const juce::Array<juce::File>& folders)
{
    juce::StringArray report;
    for (int id = 0; id < NumCats; ++id)
        if (catFiles[(size_t) id] == juce::File() || ! catFiles[(size_t) id].exists())
        {
            catFiles[(size_t) id] = juce::File();
            for (const auto& dir : folders)
            {
                const auto file = dir.getChildFile (juce::String (catName (id)) + ".vst3");
                if (file.exists()) { catFiles[(size_t) id] = file; break; }
            }
        }

    for (int id = 0; id < (int) core.size(); ++id)
    {
        if (core[(size_t) id] != nullptr) continue;
        juce::String error;
        auto c = createCat (id, error);
        if (c == nullptr) { report.add (error); continue; }
        if (prepared) prepareCat (*c);
        const juce::ScopedLock sl (renderLock);
        core[(size_t) id] = std::move (c);
    }

    if (auto* chord = getCoreCat (ChordCat))
        setChoice (chord, "Trigger Mode", "Chromatic");          // pads are played from C3 up
    keepLifeRunning();
    pollPluginSettings();
    if (numTracks.load() == 0) setDefaultTracks();
    return report.joinIntoString ("\n");
}

void HubEngine::releaseCats()
{
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> trash;
    {
        const juce::ScopedLock sl (renderLock);
        for (auto& c : core) if (c != nullptr) trash.push_back (std::move (c));
        for (auto& t : tracks)
        {
            if (t->fxCat != nullptr) trash.push_back (std::move (t->fxCat));
            if (t->soundCat != nullptr) trash.push_back (std::move (t->soundCat));
            t->fxCatId = t->soundCatId = -1;
        }
        prepared = false;
    }
    for (auto& c : trash) c->releaseResources();
    trash.clear();
}

void HubEngine::pollPluginSettings()
{
    if (auto* chord = getCoreCat (ChordCat))
    {
        if (auto* pads = findParam (*chord, "Pads"))  numPads = juce::jlimit (4, 16, 4 + juce::roundToInt (pads->getValue() * 12.0f));
        if (auto* low = findParam (*chord, "Trigger Low")) triggerLow = juce::jlimit (0, 112, juce::roundToInt (low->getValue() * 127.0f));
    }
}

//==============================================================================
void HubEngine::prepareTrack (Track& t)
{
    t.keys.prepare (sampleRate);
    t.bass.prepare (sampleRate);
    t.drums.prepare (sampleRate);
    t.fx.prepare (sampleRate);
    t.audio.setSize (2, maxBlockSize);
    t.midi.ensureSize (2048);
    t.leadHeld.clear();
    t.leadHeld.ensureStorageAllocated (128);
    t.leadNote = -1;
}

bool HubEngine::addTrack (int source, int midiFx, int sound)
{
    const int i = numTracks.load();
    if (i >= maxTracks) return false;
    auto& t = *tracks[(size_t) i];
    {
        const juce::ScopedLock sl (renderLock);
        Track fresh;
        t.copySettingsFrom (fresh);
        t.source = juce::jlimit (0, (int) NumSources - 1, source);
        t.midiFx = FxNone;
        t.sound = SndPad;
        if (prepared) prepareTrack (t);
        t.meter = 0.0f;
        for (auto& w : t.notes) w = 0;
    }
    setSound (i, sound);
    setMidiFx (i, midiFx);
    numTracks = i + 1;
    return true;
}

void HubEngine::removeTrack (int index)
{
    const int n = numTracks.load();
    if (! juce::isPositiveAndBelow (index, n)) return;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> trash;
    {
        const juce::ScopedLock sl (renderLock);
        auto removed = std::move (tracks[(size_t) index]);
        for (int k = index; k < n - 1; ++k) tracks[(size_t) k] = std::move (tracks[(size_t) k + 1]);
        if (removed->fxCat != nullptr) trash.push_back (std::move (removed->fxCat));
        if (removed->soundCat != nullptr) trash.push_back (std::move (removed->soundCat));
        removed->fxCatId = removed->soundCatId = -1;
        tracks[(size_t) n - 1] = std::move (removed);
        numTracks = n - 1;
    }
    for (auto& c : trash) c->releaseResources();
}

bool HubEngine::setMidiFx (int index, int fx)
{
    if (! juce::isPositiveAndBelow (index, maxTracks)) return false;
    auto& t = track (index);
    fx = juce::jlimit (0, (int) NumMidiFx - 1, fx);
    const int id = catForMidiFx (fx);
    std::unique_ptr<juce::AudioPluginInstance> fresh;
    if (id >= 0 && t.fxCatId != id)
    {
        juce::String error;
        fresh = createCat (id, error);
        if (fresh == nullptr) return false;
        if (prepared) prepareCat (*fresh);
    }
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock sl (renderLock);
        if (id < 0 || fresh != nullptr)
        {
            old = std::move (t.fxCat);
            t.fxCat = std::move (fresh);
            t.fxCatId = id;
        }
        t.midiFx = fx;
        t.keys.allOff(); t.bass.allOff();
    }
    if (old != nullptr) old->releaseResources();
    return true;
}

bool HubEngine::setSound (int index, int sound)
{
    if (! juce::isPositiveAndBelow (index, maxTracks)) return false;
    auto& t = track (index);
    sound = juce::jlimit (0, (int) NumSounds - 1, sound);
    const int id = catForSound (sound);
    std::unique_ptr<juce::AudioPluginInstance> fresh;
    if (id >= 0 && t.soundCatId != id)
    {
        juce::String error;
        fresh = createCat (id, error);
        if (fresh == nullptr) return false;
        if (prepared) prepareCat (*fresh);
    }
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock sl (renderLock);
        if (id < 0 || fresh != nullptr)
        {
            old = std::move (t.soundCat);
            t.soundCat = std::move (fresh);
            t.soundCatId = id;
        }
        t.sound = sound;
        t.keys.allOff(); t.bass.allOff(); t.drums.allOff();
    }
    if (old != nullptr) old->releaseResources();
    // Talk-Cat sings what it is played here (not its own demo song)
    if (t.soundCatId == TalkCat) setChoice (t.soundCat.get(), "Play Mode", "Live MIDI");
    if (songMode.load() && (t.soundCatId == TalkCat || t.soundCatId == SingCat)) lyricsSentFor = nullptr;   // give it the words
    return true;
}

void HubEngine::setDefaultTracks()
{
    currentStyle = styles()[0].name;
    while (numTracks.load() > 0) removeTrack (numTracks.load() - 1);
    const auto& d = styles()[0].tracks;
    for (const auto& td : d)
    {
        addTrack (td.source, td.fx, td.sound);
        auto& t = track (numTracks.load() - 1);
        t.volume = td.vol; t.reverb = td.rev;
    }
}

//==============================================================================
juce::StringArray HubEngine::getProgressionNames() const
{
    juce::StringArray names;
    if (auto* c = getCoreCat (ChordCat))
        for (int i = 0; i < c->getNumPrograms(); ++i) names.add (c->getProgramName (i));
    return names;
}

int HubEngine::getProgression() const
{
    auto* c = getCoreCat (ChordCat);
    return c != nullptr ? c->getCurrentProgram() : -1;
}

void HubEngine::setProgression (int index)
{
    if (auto* c = getCoreCat (ChordCat))
        if (juce::isPositiveAndBelow (index, c->getNumPrograms()))
        {
            const int key = getKey();
            c->setCurrentProgram (index);
            setChoice (c, "Trigger Mode", "Chromatic");
            if (key >= 0) setKey (key);          // keep the key the user picked
            pollPluginSettings();
        }
}

int HubEngine::getKey() const
{
    auto* c = getCoreCat (ChordCat);
    if (c == nullptr) return -1;
    const int root = juce::jmax (0, choiceIndex (c, "Key"));
    int transpose = 0;
    if (auto* prm = findParam (*c, "Transpose")) transpose = juce::roundToInt (prm->getValue() * 48.0f) - 24;
    return ((root + transpose) % 12 + 12) % 12;
}

void HubEngine::setKey (int pc)
{
    auto* c = getCoreCat (ChordCat);
    if (c == nullptr) return;
    const int root = juce::jmax (0, choiceIndex (c, "Key"));
    int delta = ((pc - root) % 12 + 12) % 12;
    if (delta > 6) delta -= 12;                      // the nearest way: -5 .. +6
    if (auto* prm = findParam (*c, "Transpose")) prm->setValueNotifyingHost ((float) (delta + 24) / 48.0f);
}

juce::StringArray HubEngine::styleNames()
{
    juce::StringArray n;
    for (const auto& s : styles()) n.add (s.name);
    return n;
}

void HubEngine::applyStyle (int index)
{
    const auto& all = styles();
    const auto& st = all[(size_t) juce::jlimit (0, (int) all.size() - 1, index)];
    leaveSongMode();             // a style is a jam with the Cats' own loops
    bpm = st.bpm;
    barsPerChord = st.bars;
    autoChords = true;
    currentStyle = st.name;

    // the Cats
    const auto progs = getProgressionNames();
    const int prog = progs.indexOf (st.progression);
    if (prog >= 0) setProgression (prog);
    setChoice (getCoreCat (DrumCat), "Style", st.drums);
    setChoice (getCoreCat (BassCat), "Style", st.bass);
    if (juce::String (st.life).isNotEmpty())
        if (auto* life = getCoreCat (LifeCat))
            for (int i = 0; i < life->getNumPrograms(); ++i)
                if (life->getProgramName (i) == st.life)
                {
                    life->setCurrentProgram (i);
                    keepLifeRunning();
                    break;
                }

    // the tracks
    const auto defs = juce::String (st.name) == "Rock Anthem" ? rockTracks() : st.tracks;
    while (numTracks.load() > 0) removeTrack (numTracks.load() - 1);
    for (const auto& td : defs)
    {
        if (! addTrack (td.source, FxNone, SndPad)) break;
        const int i = numTracks.load() - 1;
        if (! setSound (i, td.sound)) setSound (i, td.source == SrcDrums ? SndBoomBap : (td.source == SrcBass ? SndSub : SndPad));
        setMidiFx (i, td.fx);
        auto& t = track (i);
        t.volume = td.vol; t.reverb = td.rev; t.filter = td.filter; t.drive = td.drive;
        t.chorus = td.chorus; t.delay = td.delay; t.crush = td.crush; t.pan = td.pan;
    }
}

void HubEngine::roll (juce::Random& r)
{
    // the dice writes a whole new song: genre, mood, seed, and sounds to go with it
    cat::muse::Settings st;
    st.seed = (juce::uint32) (1 + r.nextInt (999999));
    st.genre = r.nextInt ((int) cat::muse::NumGenres);
    st.mood = r.nextInt ((int) cat::muse::NumMoods);
    st.endless = songMode.load() ? songSettings.endless : true;   // never-ending, so it never repeats
    composeSong (st, true, &r);
}

//==============================================================================
void HubEngine::setPlaying (bool shouldPlay)
{
    if (shouldPlay) { stopRequested = false; startRequested = true; }
    else            { startRequested = false; stopRequested = true; }
}

void HubEngine::prepare (double newRate, int maxBlock)
{
    const juce::ScopedLock sl (renderLock);
    sampleRate = newRate > 0.0 ? newRate : 48000.0;
    maxBlockSize = juce::jmax (32, maxBlock);
    playHead.sampleRate = sampleRate;
    for (auto& c : core) if (c != nullptr) prepareCat (*c);
    for (auto& t : tracks)
    {
        if (t->fxCat != nullptr) prepareCat (*t->fxCat);
        if (t->soundCat != nullptr) prepareCat (*t->soundCat);
        prepareTrack (*t);
    }
    reverb.setSampleRate (sampleRate);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.62f; rp.damping = 0.45f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
    reverb.setParameters (rp);
    reverb.reset();
    limiter.prepare (sampleRate);
    masterFx.prepare (sampleRate);
    liveMidiIn.reset (sampleRate);
    liveMidi.ensureSize (1024);
    loopEvents.reserve (8192);
    catAudio.setSize (8, maxBlockSize);
    lifeAudio.setSize (2, maxBlockSize);
    sendAudio.setSize (2, maxBlockSize);
    mixAudio.setSize (2, maxBlockSize);
    for (auto* m : { &songMidi, &chordOut, &bassOut, &drumOut, &lifeOut, &leadOut, &melodyOut, &counterOut }) m->ensureSize (2048);
    for (auto& m : songParts) m.ensureSize (2048);
    heldTrigger = -1;
    prepared = true;
}

void HubEngine::runCat (juce::AudioPluginInstance* p, juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi, int n)
{
    if (p == nullptr) { midi.clear(); audio.clear(); return; }
    const int chans = juce::jlimit (2, 8, juce::jmax (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels()));
    catAudio.setSize (chans, n, false, false, true);
    catAudio.clear();
    {
        const juce::ScopedLock sl (p->getCallbackLock());
        if (p->isSuspended()) midi.clear();
        else p->processBlock (catAudio, midi);
    }
    audio.setSize (2, n, false, false, true);
    audio.clear();
    const int outs = p->getTotalNumOutputChannels();
    for (int ch = 0; ch < juce::jmin (2, outs); ++ch) audio.copyFrom (ch, 0, catAudio, ch, 0, n);
    if (outs == 1) audio.copyFrom (1, 0, catAudio, 0, 0, n);
}

void HubEngine::trackNotes (Track& s, const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            s.notes[(size_t) (n >> 5)].fetch_or (1u << (n & 31));
            s.hits[(size_t) (n >> 5)].fetch_or (1u << (n & 31));
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            s.notes[(size_t) (n >> 5)].fetch_and (~(1u << (n & 31)));
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& w : s.notes) w = 0;
    }
}

void HubEngine::makeLead (Track& t, const juce::MidiBuffer& chords, juce::MidiBuffer& out)
{
    // The top note of the chord, an octave up: a simple melody that follows the chords.
    out.clear();
    for (const auto meta : chords)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) t.leadHeld.add (m.getNoteNumber());
        else if (m.isNoteOff()) t.leadHeld.removeValue (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) t.leadHeld.clear();
        else continue;
        const int want = t.leadHeld.isEmpty() ? -1 : juce::jmin (108, t.leadHeld.getLast() + 12);
        if (want != t.leadNote)
        {
            if (t.leadNote >= 0) out.addEvent (juce::MidiMessage::noteOff (1, t.leadNote), meta.samplePosition);
            if (want >= 0) out.addEvent (juce::MidiMessage::noteOn (1, want, (juce::uint8) 96), meta.samplePosition);
            t.leadNote = want;
        }
        if (m.isAllNotesOff()) out.addEvent (m, meta.samplePosition);
    }
}

float HubEngine::bendRangeOf (juce::AudioPluginInstance* p) const
{
    if (p != nullptr)
        if (auto* prm = findParam (*p, "Bend Range"))
            return 1.0f + prm->getValue() * 47.0f;
    return 2.0f;
}

void HubEngine::render (float* outL, float* outR, int numSamples)
{
    const juce::ScopedTryLock sl (renderLock);
    if (! sl.isLocked() || ! prepared)
    {
        juce::FloatVectorOperations::clear (outL, numSamples);
        juce::FloatVectorOperations::clear (outR, numSamples);
        return;
    }
    for (int done = 0; done < numSamples;)
    {
        int n = juce::jmin (maxBlockSize, numSamples - done);
        if (songMode.load() && playing.load())
        {
            // never run across the end of a song piece: the Cats then see the new piece start at a block start
            double len = 0.0;
            const double local = songPlayer.localPosition (clockPpq, len);
            const double samplesPerBeat = sampleRate * 60.0 / juce::jlimit (30.0, 300.0, bpm.load());
            const int left = (int) std::floor ((len - local) * samplesPerBeat);
            if (len > 0.0 && left >= 1 && left < n) n = left;
        }
        renderBlock (outL + done, outR + done, n);
        done += n;
    }
}

void HubEngine::renderBlock (float* outL, float* outR, int n)
{
    const double bpmNow = juce::jlimit (30.0, 300.0, bpm.load());
    const double samplesPerBeat = sampleRate * 60.0 / bpmNow;
    songMidi.clear();
    bool panic = false;

    // ---- transport
    if (startRequested.exchange (false))
    {
        playing = true;
        playHead.ppq = 0.0;
        playHead.samples = 0;
        clockPpq = 0.0;
    }
    if (stopRequested.exchange (false) && playing)
    {
        playing = false;
        panic = true;
    }
    playHead.bpm = bpmNow;
    playHead.playing = playing.load();

    // ---- which Chord-Cat pad is held (song or your fingers)
    auto switchPad = [this] (int pad, int pos)
    {
        const int note = pad >= 0 ? juce::jlimit (0, 127, triggerLow.load() + pad) : -1;
        if (note == heldTrigger) return;
        if (heldTrigger >= 0) songMidi.addEvent (juce::MidiMessage::noteOff (1, heldTrigger), pos);
        if (note >= 0) songMidi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), pos);
        heldTrigger = note;
        currentPad = pad;
    };
    const int live = livePad.load();
    const int pads = juce::jmax (1, numPads.load());
    if (panic) { switchPad (-1, 0); addPanic (songMidi, 0); }
    const bool song = songMode.load();
    if (song) { if (! panic) switchPad (-1, 0); }
    else if (live >= 0) switchPad (live % 16, 0);
    else if (playing && autoChords)
    {
        const double chordLen = 4.0 * juce::jlimit (1, 8, barsPerChord.load());
        const double ppq = playHead.ppq;
        const int slot = (int) std::floor (ppq / chordLen + 1.0e-9);
        switchPad (slot % pads, 0);
        const int at = (int) std::ceil (((slot + 1) * chordLen - ppq) * samplesPerBeat);
        if (at >= 0 && at < n) switchPad ((slot + 1) % pads, at);
    }
    else switchPad (-1, 0);

    if (song)
    {
        // ---- a composed song: the parts come from the song player
        std::array<juce::MidiBuffer*, cat::muse::NumParts> ptrs;
        for (int p = 0; p < cat::muse::NumParts; ++p) { songParts[(size_t) p].clear(); ptrs[(size_t) p] = &songParts[(size_t) p]; }
        double len = 0.0;
        double local = songPlayer.localPosition (clockPpq, len);
        if (len > 0.0 && len - local < 1.5 / samplesPerBeat) local = 0.0;     // a new piece starts right here
        if (playing) playHead.ppq = local;        // the Cats see the song's own position: a new piece / loop = a jump
        songPlayer.process (clockPpq, 1.0 / samplesPerBeat, n, playing, ptrs);
        copyMidiTo (chordOut, songParts[cat::muse::Chords], 1);
        copyMidiTo (bassOut, songParts[cat::muse::Bass], 1);
        copyMidiTo (drumOut, songParts[cat::muse::Drums], 10);
        copyMidiTo (melodyOut, songParts[cat::muse::Melody], 1);
        copyMidiTo (counterOut, songParts[cat::muse::Counter], 1);
        if (panic) for (auto* m : { &chordOut, &bassOut, &drumOut, &melodyOut, &counterOut }) addPanic (*m, 0);
    }
    else
    {
        // ---- the four core Cats
        copyMidi (chordOut, songMidi);
        if (hasCat (ChordCat)) runCat (core[ChordCat].get(), lifeAudio, chordOut, n);
        if (panic) addPanic (chordOut, 0);

        copyMidi (bassOut, chordOut);
        if (hasCat (BassCat)) runCat (core[BassCat].get(), lifeAudio, bassOut, n); else bassOut.clear();
        if (panic) addPanic (bassOut, 0);

        drumOut.clear();
        if (panic) addPanic (drumOut, 0);
        if (hasCat (DrumCat)) runCat (core[DrumCat].get(), lifeAudio, drumOut, n); else drumOut.clear();
        melodyOut.clear();
        counterOut.clear();
    }

    copyMidi (lifeOut, chordOut);
    if (hasCat (LifeCat)) runCat (core[LifeCat].get(), lifeAudio, lifeOut, n);
    else { lifeAudio.setSize (2, n, false, false, true); lifeAudio.clear(); lifeOut.clear(); }
    if (panic) addPanic (lifeOut, 0);

    mixAudio.setSize (2, n, false, false, true);
    sendAudio.setSize (2, n, false, false, true);
    mixAudio.clear();
    sendAudio.clear();

    const int count = numTracks.load();
    liveMidi.clear();
    liveMidiIn.removeNextBlockOfMessages (liveMidi, n);
    const int kbTrack = pickKeyboardTrack();
    keyboardTrackNow = kbTrack;
    bool anySolo = false;
    for (int i = 0; i < count; ++i) anySolo = anySolo || tracks[(size_t) i]->solo.load();

    for (int i = 0; i < count; ++i)
    {
        auto& t = *tracks[(size_t) i];
        const int src = t.source.load();

        // notes in
        switch (src)
        {
            case SrcBass:  copyMidi (t.midi, bassOut); break;
            case SrcDrums: copyMidi (t.midi, drumOut); break;
            case SrcLife:  copyMidi (t.midi, lifeOut); break;
            case SrcLead:  if (song) copyMidi (t.midi, melodyOut); else makeLead (t, chordOut, t.midi); break;
            case SrcCounter: if (song) copyMidi (t.midi, counterOut); else makeLead (t, chordOut, t.midi); break;
            default:       copyMidi (t.midi, chordOut); break;
        }
        if (i == kbTrack) runLoop (n, samplesPerBeat, t.midi, playing.load());
        if (i == kbTrack && ! liveMidi.isEmpty())
            for (const auto m : liveMidi)
            {
                auto msg = m.getMessage();
                if (msg.getChannel() > 0 && src != SrcDrums) msg.setChannel (1);
                t.midi.addEvent (msg, juce::jlimit (0, n - 1, m.samplePosition));
            }
        if (panic) addPanic (t.midi, 0);

        // MIDI effect Cat
        if (t.fxCat != nullptr && t.midiFx.load() != FxNone)
        {
            runCat (t.fxCat.get(), t.audio, t.midi, n);
            if (panic) addPanic (t.midi, 0);
        }
        trackNotes (t, t.midi);

        // sound
        t.audio.setSize (2, n, false, false, true);
        t.audio.clear();
        const int snd = t.sound.load();
        float* l = t.audio.getWritePointer (0);
        float* r = t.audio.getWritePointer (1);
        const float bend = t.midiFx.load() == FxBend ? bendRangeOf (t.fxCat.get()) : 2.0f;
        if (catForSound (snd) >= 0)
        {
            if (t.soundCat != nullptr) runCat (t.soundCat.get(), t.audio, t.midi, n);
        }
        else if (snd == SndLifeOwn)
        {
            if (src == SrcLife)
                for (int ch = 0; ch < 2; ++ch) t.audio.copyFrom (ch, 0, lifeAudio, ch, 0, n);
        }
        else if (isKeysSound (snd))
        {
            t.keys.setSound (keysIndexOf (snd));
            t.keys.setBendRange (bend);
            renderWithMidi (t.keys, t.midi, l, r, n);
        }
        else if (snd <= SndBassPluck)
        {
            t.bass.setSound (snd - SndSub);
            t.bass.setBendRange (bend);
            renderWithMidi (t.bass, t.midi, l, r, n);
        }
        else
        {
            t.drums.setSound (snd - SndBoomBap);
            renderWithMidi (t.drums, t.midi, l, r, n);
        }

        // effects
        TrackFxSettings fs;
        fs.filter = t.filter.load(); fs.drive = t.drive.load(); fs.chorus = t.chorus.load();
        fs.delay = t.delay.load(); fs.crush = t.crush.load(); fs.bpm = bpmNow;
        t.fx.process (l, r, n, fs);

        // energy for the self test (before the fader)
        double e = 0.0;
        for (int k = 0; k < n; ++k) e += (double) l[k] * l[k] + (double) r[k] * r[k];
        t.energy = t.energy.load() + e;

        // pan, volume, mix, room
        const float v = t.volume.load();
        const float gain = (t.mute || (anySolo && ! t.solo)) ? 0.0f : v * v;
        const float pan = juce::jlimit (0.0f, 1.0f, t.pan.load());
        const float gl = gain * std::cos (pan * juce::MathConstants<float>::halfPi) * 1.41421356f;
        const float gr = gain * std::sin (pan * juce::MathConstants<float>::halfPi) * 1.41421356f;
        mixAudio.addFrom (0, 0, t.audio, 0, 0, n, gl);
        mixAudio.addFrom (1, 0, t.audio, 1, 0, n, gr);
        const float rv = t.reverb.load();
        sendAudio.addFrom (0, 0, t.audio, 0, 0, n, gl * rv);
        sendAudio.addFrom (1, 0, t.audio, 1, 0, n, gr * rv);
        const float peak = juce::jmax (t.audio.getMagnitude (0, 0, n) * gl, t.audio.getMagnitude (1, 0, n) * gr);
        t.meter = juce::jmax (peak, t.meter.load() * 0.85f);
    }

    // ---- reverb send, master, safety limiter
    reverb.processStereo (sendAudio.getWritePointer (0), sendAudio.getWritePointer (1), n);
    const float room = juce::jlimit (0.0f, 1.0f, masterRoom.load());
    mixAudio.addFrom (0, 0, sendAudio, 0, 0, n, room);
    mixAudio.addFrom (1, 0, sendAudio, 1, 0, n, room);
    auto* l = mixAudio.getWritePointer (0);
    auto* r = mixAudio.getWritePointer (1);
    masterFx.process (l, r, n, juce::jlimit (-1.0f, 1.0f, masterTone.load()), juce::jlimit (0.0f, 1.0f, masterGlue.load()),
                      juce::jlimit (0.0f, 1.0f, masterWarmth.load()), juce::jlimit (0.0f, 2.0f, masterWidth.load()));
    masterReduction = masterFx.gainReduction();
    const float master = masterVolume.load();
    mixAudio.applyGain (master * master);
    limiter.process (l, r, n);
    juce::FloatVectorOperations::copy (outL, l, n);
    juce::FloatVectorOperations::copy (outR, r, n);
    masterMeter = juce::jmax (mixAudio.getMagnitude (0, n), masterMeter.load() * 0.85f);

    if (recording)
    {
        const juce::ScopedTryLock wl (writerLock);
        if (wl.isLocked() && writer != nullptr)
        {
            const float* chans[2] = { l, r };
            writer->write (chans, n);
        }
    }

    if (playing)
    {
        playHead.ppq += n / samplesPerBeat;
        playHead.samples += n;
        clockPpq += n / samplesPerBeat;
    }
    ppqShown = playHead.ppq;
    if (song) songBeatShown = songPlayer.getPlayPosition();
}

//==============================================================================
void MasterFx::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    lpL = lpR = env = 0.0f;
    gain = 1.0f;
    lastReduction = 0.0f;
}

void MasterFx::process (float* l, float* r, int n, float tone, float glue, float warmth, float width)
{
    // tone: a tilt around ~700 Hz (negative = warmer / darker, positive = brighter)
    if (std::abs (tone) > 0.001f)
    {
        const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 700.0f / (float) sr);
        const float lo = 1.0f - 0.35f * tone, hi = 1.0f + 0.55f * tone;
        for (int i = 0; i < n; ++i)
        {
            lpL += a * (l[i] - lpL); lpR += a * (r[i] - lpR);
            l[i] = lpL * lo + (l[i] - lpL) * hi;
            r[i] = lpR * lo + (r[i] - lpR) * hi;
        }
        if (! std::isfinite (lpL) || ! std::isfinite (lpR)) lpL = lpR = 0.0f;
    }
    // glue: a slow, gentle compressor that holds the mix together
    float maxRed = 0.0f;
    if (glue > 0.001f)
    {
        const float thrDb = -6.0f - 18.0f * glue, ratio = 1.0f + 4.0f * glue;
        const float makeupDb = -thrDb * (1.0f - 1.0f / ratio) * 0.45f;
        const float att = std::exp (-1.0f / (0.010f * (float) sr)), rel = std::exp (-1.0f / (0.180f * (float) sr));
        const float gs = std::exp (-1.0f / (0.004f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            const float x = juce::jmax (std::abs (l[i]), std::abs (r[i]));
            env = x > env ? att * env + (1.0f - att) * x : rel * env + (1.0f - rel) * x;
            const float lev = juce::Decibels::gainToDecibels (env, -100.0f);
            const float red = lev > thrDb ? (lev - thrDb) * (1.0f - 1.0f / ratio) : 0.0f;
            maxRed = juce::jmax (maxRed, red);
            const float target = juce::Decibels::decibelsToGain (makeupDb - red);
            gain = gs * gain + (1.0f - gs) * target;
            l[i] *= gain; r[i] *= gain;
        }
        if (! std::isfinite (env) || ! std::isfinite (gain)) { env = 0.0f; gain = 1.0f; }
    }
    else gain = 1.0f;
    lastReduction = maxRed;
    // warmth: soft tape-like saturation (the same loudness, rounder peaks)
    if (warmth > 0.001f)
    {
        const float d = 1.0f + 2.5f * warmth, norm = 1.0f / std::tanh (d);
        for (int i = 0; i < n; ++i)
        {
            l[i] = l[i] * (1.0f - warmth) + warmth * std::tanh (l[i] * d) * norm;
            r[i] = r[i] * (1.0f - warmth) + warmth * std::tanh (r[i] * d) * norm;
        }
    }
    // width: 0 = mono, 1 = as it is, 2 = extra wide
    if (std::abs (width - 1.0f) > 0.001f)
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]) * width;
            l[i] = m + s; r[i] = m - s;
        }
}

//==============================================================================
void HubEngine::runLoop (int n, double samplesPerBeat, juce::MidiBuffer& out, bool playingNow)
{
    // requests from the message thread
    const double loopLen = 4.0 * juce::jlimit (1, 16, loopBars.load());
    const double pos0 = std::fmod (juce::jmax (0.0, clockPpq), loopLen);
    if (loopClearRequest.exchange (false))
    {
        loopEvents.clear();
        for (int k = 0; k < 128; ++k) if (loopHeld[(size_t) k]) out.addEvent (juce::MidiMessage::noteOff (1, k), 0);
        loopHeld.fill (false);
    }
    if (loopUndoRequest.exchange (false) && ! loopEvents.empty())
    {
        int last = 0;
        for (auto& e : loopEvents) last = juce::jmax (last, e.take);
        loopEvents.erase (std::remove_if (loopEvents.begin(), loopEvents.end(), [last] (const LoopEvent& e) { return e.take == last; }), loopEvents.end());
        for (int k = 0; k < 128; ++k) if (loopHeld[(size_t) k]) out.addEvent (juce::MidiMessage::noteOff (1, k), 0);
        loopHeld.fill (false);
    }
    const int req = loopRecordRequest.exchange (-1);
    if (req == 1 && ! loopRecording) { loopRecording = true; ++loopTake; }
    if (req == 0 && loopRecording)
    {
        loopRecording = false;
        // close notes still held, so nothing hangs in the loop
        std::array<bool, 128> open {};
        for (auto& e : loopEvents)
            if (e.take == loopTake)
            {
                if (e.msg.isNoteOn()) open[(size_t) e.msg.getNoteNumber()] = true;
                else if (e.msg.isNoteOff()) open[(size_t) e.msg.getNoteNumber()] = false;
            }
        for (int k = 0; k < 128; ++k)
            if (open[(size_t) k] && loopEvents.size() < loopEvents.capacity())
                loopEvents.push_back ({ pos0, juce::MidiMessage::noteOff (1, k), loopTake });
    }
    if (! playingNow) { loopPosShown = 0.0; return; }

    // play what is in the loop
    const double blockBeats = n / samplesPerBeat;
    for (auto& e : loopEvents)
    {
        double d = e.beat - pos0;
        if (d < 0.0) d += loopLen;
        if (d < blockBeats)
        {
            if (loopRecording && e.take == loopTake) continue;   // don't double what is being played right now
            out.addEvent (e.msg, juce::jlimit (0, n - 1, (int) (d * samplesPerBeat)));
            if (e.msg.isNoteOn()) loopHeld[(size_t) e.msg.getNoteNumber()] = true;
            else if (e.msg.isNoteOff()) loopHeld[(size_t) e.msg.getNoteNumber()] = false;
        }
    }
    // record what the keyboard plays
    if (loopRecording)
        for (const auto m : liveMidi)
        {
            const auto msg = m.getMessage();
            if (! msg.isNoteOnOrOff() || loopEvents.size() >= loopEvents.capacity()) continue;
            auto copy = msg;
            copy.setChannel (1);
            loopEvents.push_back ({ std::fmod (pos0 + m.samplePosition / samplesPerBeat, loopLen), copy, loopTake });
        }
    int notes = 0;
    for (auto& e : loopEvents) if (e.msg.isNoteOn()) ++notes;
    loopNotesShown = notes;
    loopPosShown = pos0 / loopLen;
}

int HubEngine::pickKeyboardTrack() const
{
    const int count = numTracks.load();
    if (count <= 0) return -1;
    const int chosen = keyboardTrack.load();
    if (chosen >= 0) return chosen < count ? chosen : -1;
    for (int i = 0; i < count; ++i) if (tracks[(size_t) i]->source.load() == SrcLead) return i;
    for (int i = 0; i < count; ++i) if (isKeysSound (tracks[(size_t) i]->sound.load())) return i;
    return 0;
}

void HubEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! prepared) return;
    if (m.isNoteOnOrOff() || m.isPitchWheel() || m.isController() || m.isAftertouch() || m.isChannelPressure())
        liveMidiIn.addMessageToQueue (m);
}

//==============================================================================
void HubEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void HubEngine::audioDeviceStopped() {}

void HubEngine::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples,
                                                  const juce::AudioIODeviceCallbackContext&)
{
    if (numOut <= 0) return;
    float* l = out[0];
    float* r = numOut > 1 ? out[1] : nullptr;
    if (r == nullptr)
    {
        juce::HeapBlock<float> tmp ((size_t) numSamples);
        render (l, tmp.get(), numSamples);
        juce::FloatVectorOperations::add (l, tmp.get(), numSamples);
        juce::FloatVectorOperations::multiply (l, 0.5f, numSamples);
    }
    else
        render (l, r, numSamples);
    for (int ch = 2; ch < numOut; ++ch)
        if (out[ch] != nullptr) juce::FloatVectorOperations::clear (out[ch], numSamples);
}

//==============================================================================
bool HubEngine::startRecording (const juce::File& file)
{
    stopRecording();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) return false;
    {
        const juce::ScopedLock sl (writerLock);
        writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w.release(), writerThread, 1 << 16);
    }
    recordFile = file;
    recording = true;
    return true;
}

void HubEngine::stopRecording()
{
    recording = false;
    const juce::ScopedLock sl (writerLock);
    writer.reset();
}

//==============================================================================
juce::ValueTree HubEngine::saveState() const
{
    juce::ValueTree t ("CATHUB");
    t.setProperty ("version", 2, nullptr);
    t.setProperty ("bpm", bpm.load(), nullptr);
    t.setProperty ("barsPerChord", barsPerChord.load(), nullptr);
    t.setProperty ("autoChords", autoChords.load(), nullptr);
    t.setProperty ("master", masterVolume.load(), nullptr);
    t.setProperty ("style", currentStyle, nullptr);
    t.setProperty ("room", masterRoom.load(), nullptr);
    t.setProperty ("tone", masterTone.load(), nullptr);
    t.setProperty ("glue", masterGlue.load(), nullptr);
    t.setProperty ("warmth", masterWarmth.load(), nullptr);
    t.setProperty ("width", masterWidth.load(), nullptr);
    t.setProperty ("keyboardTrack", keyboardTrack.load(), nullptr);
    auto stateOf = [] (juce::AudioPluginInstance* p)
    {
        juce::MemoryBlock mb;
        if (p != nullptr) p->getStateInformation (mb);
        return mb.toBase64Encoding();
    };
    for (int i = 0; i < numTracks.load(); ++i)
    {
        const auto& s = *tracks[(size_t) i];
        juce::ValueTree p ("TRACK");
        p.setProperty ("source", s.source.load(), nullptr);
        p.setProperty ("midiFx", s.midiFx.load(), nullptr);
        p.setProperty ("sound", s.sound.load(), nullptr);
        p.setProperty ("volume", s.volume.load(), nullptr);
        p.setProperty ("pan", s.pan.load(), nullptr);
        p.setProperty ("reverb", s.reverb.load(), nullptr);
        p.setProperty ("filter", s.filter.load(), nullptr);
        p.setProperty ("drive", s.drive.load(), nullptr);
        p.setProperty ("chorus", s.chorus.load(), nullptr);
        p.setProperty ("delay", s.delay.load(), nullptr);
        p.setProperty ("crush", s.crush.load(), nullptr);
        p.setProperty ("mute", s.mute.load(), nullptr);
        p.setProperty ("solo", s.solo.load(), nullptr);
        if (s.fxCat != nullptr) p.setProperty ("fxState", stateOf (s.fxCat.get()), nullptr);
        if (s.soundCat != nullptr) p.setProperty ("soundState", stateOf (s.soundCat.get()), nullptr);
        t.appendChild (p, nullptr);
    }
    if (songMode.load())
    {
        juce::ValueTree sg ("SONG");
        sg.setProperty ("seed", (juce::int64) songSettings.seed, nullptr);
        sg.setProperty ("genre", songSettings.genre, nullptr);
        sg.setProperty ("mood", songSettings.mood, nullptr);
        sg.setProperty ("endless", songSettings.endless, nullptr);
        sg.setProperty ("key", songSettings.key, nullptr);
        juce::StringArray seeds;
        for (auto v : songSettings.partSeeds) seeds.add (juce::String ((juce::int64) v));
        sg.setProperty ("partSeeds", seeds.joinIntoString (","), nullptr);
        if (! arrangedForm.empty())
        {
            juce::StringArray form;
            for (const auto& sec : arrangedForm)
            {
                int on = 0;
                for (int p = 0; p < cat::muse::NumParts; ++p) if (sec.on[(size_t) p]) on |= 1 << p;
                form.add (juce::String (sec.type) + ":" + juce::String (sec.bars) + ":" + juce::String (sec.progression) + ":"
                          + juce::String (sec.energy, 3) + ":" + juce::String (on));
            }
            sg.setProperty ("form", form.joinIntoString (";"), nullptr);
        }
        t.appendChild (sg, nullptr);
    }
    for (int id = 0; id < (int) core.size(); ++id)
        if (auto* c = getCoreCat (id))
        {
            juce::ValueTree s ("CAT");
            s.setProperty ("name", catName (id), nullptr);
            s.setProperty ("state", stateOf (c), nullptr);
            t.appendChild (s, nullptr);
        }
    return t;
}

void HubEngine::restoreState (const juce::ValueTree& t)
{
    if (! t.hasType ("CATHUB")) return;
    bpm = juce::jlimit (30.0, 300.0, (double) t.getProperty ("bpm", 92.0));
    barsPerChord = juce::jlimit (1, 8, (int) t.getProperty ("barsPerChord", 1));
    autoChords = (bool) t.getProperty ("autoChords", true);
    masterVolume = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("master", 0.8f));
    currentStyle = t.getProperty ("style", "").toString();
    masterRoom = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("room", 0.5f));
    masterTone = juce::jlimit (-1.0f, 1.0f, (float) t.getProperty ("tone", 0.0f));
    masterGlue = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("glue", 0.0f));
    masterWarmth = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("warmth", 0.0f));
    masterWidth = juce::jlimit (0.0f, 2.0f, (float) t.getProperty ("width", 1.0f));
    keyboardTrack = juce::jlimit (-1, maxTracks - 1, (int) t.getProperty ("keyboardTrack", -1));

    auto load = [] (juce::AudioPluginInstance* p, const juce::var& v)
    {
        juce::MemoryBlock mb;
        if (p != nullptr && mb.fromBase64Encoding (v.toString()) && mb.getSize() > 0)
            p->setStateInformation (mb.getData(), (int) mb.getSize());
    };

    for (const auto& c : t)
        if (c.hasType ("CAT"))
            for (int id = 0; id < (int) core.size(); ++id)
                if (c.getProperty ("name").toString() == catName (id))
                    load (getCoreCat (id), c.getProperty ("state"));

    if ((int) t.getProperty ("version", 1) >= 2)
    {
        while (numTracks.load() > 0) removeTrack (numTracks.load() - 1);
        for (const auto& p : t)
        {
            if (! p.hasType ("TRACK")) continue;
            if (! addTrack ((int) p.getProperty ("source", 0), FxNone, SndPad)) break;
            const int i = numTracks.load() - 1;
            auto& s = track (i);
            if (! setSound (i, (int) p.getProperty ("sound", 0))) setSound (i, SndPad);
            setMidiFx (i, (int) p.getProperty ("midiFx", 0));
            s.volume = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("volume", 0.75f));
            s.pan = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("pan", 0.5f));
            s.reverb = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("reverb", 0.15f));
            s.filter = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("filter", 0.0f));
            s.drive = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("drive", 0.0f));
            s.chorus = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("chorus", 0.0f));
            s.delay = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("delay", 0.0f));
            s.crush = juce::jlimit (0.0f, 1.0f, (float) p.getProperty ("crush", 0.0f));
            s.mute = (bool) p.getProperty ("mute", false);
            s.solo = (bool) p.getProperty ("solo", false);
            load (s.fxCat.get(), p.getProperty ("fxState"));
            load (s.soundCat.get(), p.getProperty ("soundState"));
        }
        if (numTracks.load() == 0) setDefaultTracks();
    }
    const auto sg = t.getChildWithName ("SONG");
    if (sg.isValid())
    {
        cat::muse::Settings st;
        st.seed = (juce::uint32) (juce::int64) sg.getProperty ("seed", 1);
        st.genre = juce::jlimit (0, (int) cat::muse::NumGenres - 1, (int) sg.getProperty ("genre", 0));
        st.mood = juce::jlimit (0, (int) cat::muse::NumMoods - 1, (int) sg.getProperty ("mood", 0));
        st.endless = (bool) sg.getProperty ("endless", false);
        st.key = juce::jlimit (-1, 11, (int) sg.getProperty ("key", -1));
        const auto seeds = juce::StringArray::fromTokens (sg.getProperty ("partSeeds", "").toString(), ",", "");
        for (int i = 0; i < juce::jmin (seeds.size(), (int) st.partSeeds.size()); ++i) st.partSeeds[(size_t) i] = (juce::uint32) seeds[i].getLargeIntValue();
        const double savedBpm = bpm.load();
        composeSong (st, false);
        if (! st.endless)
        {
            std::vector<cat::muse::Section> form;
            for (const auto& item : juce::StringArray::fromTokens (sg.getProperty ("form", "").toString(), ";", ""))
            {
                const auto f = juce::StringArray::fromTokens (item, ":", "");
                if (f.size() < 5) continue;
                cat::muse::Section sec;
                sec.type = f[0].getIntValue(); sec.bars = f[1].getIntValue(); sec.progression = f[2].getIntValue();
                sec.energy = f[3].getFloatValue();
                for (int p = 0; p < cat::muse::NumParts; ++p) sec.on[(size_t) p] = (f[4].getIntValue() >> p) & 1;
                form.push_back (sec);
            }
            if (! form.empty()) arrangeSong (form);
        }
        bpm = savedBpm;
    }
    else leaveSongMode();
    keepLifeRunning();
    pollPluginSettings();
}

void HubEngine::keepLifeRunning()
{
    // Life-Cat's world only moves while "Life Running" is on (its Life button - Space can toggle it
    // when its window has the keyboard focus), and tracks need its MIDI out to play its creatures.
    if (auto* life = getCoreCat (LifeCat))
    {
        if (auto* prm = findParam (*life, "Life Running")) if (prm->getValue() < 0.5f) prm->setValueNotifyingHost (1.0f);
        if (auto* prm = findParam (*life, "MIDI Out")) if (prm->getValue() < 0.5f) prm->setValueNotifyingHost (1.0f);
    }
}

//==============================================================================
void HubEngine::setSong (std::shared_ptr<const cat::muse::Song> song)
{
    if (song == nullptr) return;
    songSettings = song->settings;
    bpm = song->bpm;
    songPlayer.setSong (song);
    songMode = true;
    lyricsSentFor = nullptr;
    currentStyle = song->title;
    tickSong();
}

void HubEngine::composeSong (const cat::muse::Settings& settings, bool newTracks, juce::Random* dice)
{
    auto song = std::make_shared<cat::muse::Song> (cat::muse::compose (settings));
    arrangedForm.clear();
    if (newTracks) setUpSongTracks (settings.genre, dice);
    setSong (song);
}

void HubEngine::regenerateSong (int target, juce::uint32 seed)
{
    auto now = songPlayer.getSong();
    if (now == nullptr) return;
    auto s = std::make_shared<cat::muse::Song> (*now);
    cat::muse::regenerate (*s, target, seed);
    if (target == cat::muse::NewArrangement) arrangedForm.clear();
    else if (! arrangedForm.empty()) arrangedForm = s->sections;
    if (s->chunk == 0) songSettings = s->settings;
    setSong (s);
}

bool HubEngine::arrangeSong (std::vector<cat::muse::Section> sections)
{
    auto now = songPlayer.getSong();
    if (now == nullptr || now->settings.endless || sections.empty()) return false;
    auto s = std::make_shared<cat::muse::Song> (*now);
    cat::muse::arrange (*s, sections);
    arrangedForm = s->sections;
    const double keepBpm = bpm.load();
    setSong (s);
    bpm = keepBpm;
    return true;
}

void HubEngine::setSongEndless (bool endless)
{
    auto st = songSettings;
    st.endless = endless;
    composeSong (st, false);
}

void HubEngine::leaveSongMode()
{
    songMode = false;
    songPlayer.setSong (nullptr);
    lyricsSentFor = nullptr;
}

void HubEngine::setUpSongTracks (int genre, juce::Random* dice)
{
    // the parts a song has, with sounds that suit the genre
    const auto ss = cat::muse::defaultSounds (genre);
    struct Def { int src, sound; float vol, rev, pan; };
    const bool wet = genre == cat::muse::Ambient || genre == cat::muse::Cinematic || genre == cat::muse::Aero || genre == cat::muse::Orchestral;
    std::vector<Def> defs = {
        { SrcChords,  keysSound (ss.chords),  0.62f, wet ? 0.4f : 0.2f,  0.5f },
        { SrcBass,    SndSub + ss.bass,       0.72f, 0.05f,              0.5f },
        { SrcDrums,   SndBoomBap + ss.drums,  0.70f, 0.08f,              0.5f },
        { SrcLead,    keysSound (ss.melody),  0.78f, wet ? 0.35f : 0.18f, 0.5f },
        { SrcCounter, keysSound (ss.counter), 0.52f, wet ? 0.4f : 0.22f, 0.32f },
    };
    if (dice != nullptr)
    {
        // the dice also picks the sounds: sometimes Sing-, Talk- or Glide-Cat sings the melody
        const int r = dice->nextInt (100);
        if (r < 20 && catAvailable (SingCat)) defs[3].sound = SndSing;
        else if (r < 35 && catAvailable (TalkCat)) defs[3].sound = SndTalk;
        else if (r < 45 && catAvailable (GlideCat)) defs[3].sound = SndGlide;
        else if (r < 70) defs[3].sound = SndPad + dice->nextInt (4);
        if (dice->nextInt (3) == 0) defs[0].sound = SndPad + dice->nextInt (4);
        if (dice->nextInt (3) == 0) defs[4].sound = SndPad + dice->nextInt (4);
        if (dice->nextInt (3) == 0) defs[1].sound = SndSub + dice->nextInt (4);
        if (dice->nextInt (3) == 0) defs[2].sound = SndBoomBap + dice->nextInt (4);
    }
    while (numTracks.load() > 0) removeTrack (numTracks.load() - 1);
    for (const auto& d : defs)
    {
        if (! addTrack (d.src, FxNone, SndPad)) break;
        const int i = numTracks.load() - 1;
        if (! setSound (i, d.sound)) setSound (i, d.src == SrcDrums ? SndBoomBap : (d.src == SrcBass ? SndSub : SndPluck));
        auto& t = track (i);
        t.volume = d.vol; t.reverb = d.rev; t.pan = d.pan;
        if (d.sound == SndSing || d.sound == SndTalk) t.volume = 0.85f;
        if (dice != nullptr && d.src != SrcDrums && d.src != SrcBass)
        {
            t.chorus = dice->nextInt (3) == 0 ? dice->nextFloat() * 0.5f : 0.0f;
            t.delay = dice->nextInt (4) == 0 ? dice->nextFloat() * 0.35f : 0.0f;
            t.filter = dice->nextInt (4) == 0 ? dice->nextFloat() * 0.3f : 0.0f;
        }
        if (dice != nullptr && d.src == SrcDrums) t.crush = dice->nextInt (6) == 0 ? dice->nextFloat() * 0.35f : 0.0f;
    }
}

void HubEngine::sendLyrics (const cat::muse::Song& song)
{
    // Talk- and Sing-Cat on the melody sing one syllable per note: give them the song's words
    for (int i = 0; i < numTracks.load(); ++i)
    {
        auto& t = track (i);
        if (t.soundCat == nullptr) continue;
        const bool melody = t.source.load() == SrcLead && songMode.load();
        if (t.soundCatId == TalkCat)
        {
            setChoice (t.soundCat.get(), "Play Mode", "Live MIDI");
            if (melody) setCatStateProperty (t.soundCat.get(), "liveLyrics", song.lyricsText);
        }
        else if (t.soundCatId == SingCat && melody)
        {
            setCatStateProperty (t.soundCat.get(), "lyrics", song.lyricsText.substring (0, 2000));
            if (auto* w = findParam (*t.soundCat, "Words")) if (w->getValue() < 0.5f) w->setValueNotifyingHost (1.0f);
        }
    }
    sungLyrics = song.lyricsText;
}

void HubEngine::tickSong()
{
    if (! songMode.load()) return;
    auto now = songPlayer.getSong();
    if (now == nullptr) return;
    // endless: write the next piece while this one plays
    if (songPlayer.needNext())
        songPlayer.queueNext (std::make_shared<cat::muse::Song> (cat::muse::composeNext (*now)));

    // the words: the piece playing now, or the next one once this one's last sung note has gone by
    auto target = now;
    if (auto next = songPlayer.getQueued(); next != nullptr && next != now && ! now->parts[cat::muse::Melody].empty())
        if (songPlayer.getPlayPosition() > now->parts[cat::muse::Melody].back().start + 0.05) target = next;
    if (target.get() != lyricsSentFor)
    {
        lyricsSentFor = target.get();
        sendLyrics (*target);
    }
}

bool HubEngine::exportSongMidi (const juce::File& file) const
{
    auto song = songPlayer.getSong();
    if (song == nullptr) return false;
    file.deleteFile();
    auto os = file.createOutputStream();
    return os != nullptr && cat::muse::toMidiFile (*song).writeTo (*os);
}

bool HubEngine::exportSong (const juce::File& file, int onlyTrack, std::function<bool (float)> progress)
{
    auto song = songPlayer.getSong();
    if (song == nullptr || ! songMode.load()) return false;
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    const double rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) return false;

    // solo the one part for a stem
    std::vector<std::pair<bool, bool>> saved;
    for (int i = 0; i < numTracks.load(); ++i)
    {
        auto& t = track (i);
        saved.push_back ({ t.mute.load(), t.solo.load() });
        if (onlyTrack >= 0) { t.mute = i != onlyTrack; t.solo = false; }
    }
    if (! prepared) prepare (rate, 512);
    const bool wasPlaying = playing.load();
    if (wasPlaying) { setPlaying (false); juce::AudioBuffer<float> b (2, 512); render (b.getWritePointer (0), b.getWritePointer (1), 512); }
    // start clean: no echo or reverb left from before
    {
        const juce::ScopedLock sl (renderLock);
        reverb.reset();
        for (auto& t : tracks) t->fx.reset();
    }
    lyricsSentFor = nullptr;
    sendLyrics (*song);
    lyricsSentFor = song.get();

    setPlaying (true);
    const double samplesPerBeat = rate * 60.0 / juce::jlimit (30.0, 300.0, bpm.load());
    const juce::int64 songSamples = (juce::int64) std::floor (song->lengthBeats * samplesPerBeat);
    const juce::int64 total = songSamples + (juce::int64) (rate * 3.0);
    constexpr int block = 512;
    juce::AudioBuffer<float> buf (2, block);
    bool ok = true;
    for (juce::int64 pos = 0; pos < total && ok;)
    {
        int n = (int) std::min<juce::int64> (block, total - pos);
        if (pos < songSamples) n = (int) std::min<juce::int64> (n, songSamples - pos);   // stop exactly at the end
        if (pos == songSamples) setPlaying (false);
        render (buf.getWritePointer (0), buf.getWritePointer (1), n);
        ok = w->writeFromAudioSampleBuffer (buf, 0, n);
        pos += n;
        if (progress && ! progress ((float) ((double) pos / (double) total))) ok = false;
    }
    setPlaying (false);
    { juce::AudioBuffer<float> b (2, 512); render (b.getWritePointer (0), b.getWritePointer (1), 512); }
    w.reset();
    for (int i = 0; i < (int) saved.size(); ++i) { track (i).mute = saved[(size_t) i].first; track (i).solo = saved[(size_t) i].second; }
    if (wasPlaying) setPlaying (true);
    return ok;
}

double HubEngine::getLifeTime() const
{
    // how long Life-Cat's world has been living (from its saved state) - for the self test
    if (auto* life = getCoreCat (LifeCat))
    {
        juce::MemoryBlock mb;
        life->getStateInformation (mb);
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        if (xml != nullptr && xml->hasTagName ("VST3PluginState"))         // a hosted VST3 wraps the Cat's own state
            if (auto* comp = xml->getChildByName ("IComponent"))
            {
                juce::MemoryBlock inner;
                inner.fromBase64Encoding (comp->getAllSubText());
                xml = juce::AudioProcessor::getXmlFromBinary (inner.getData(), (int) inner.getSize());
            }
        if (xml != nullptr)
            if (auto* w = xml->getChildByName ("WORLD")) return w->getDoubleAttribute ("time", -1.0);
    }
    return -1.0;
}
} // namespace hub
