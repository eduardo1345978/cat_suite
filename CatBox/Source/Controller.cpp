#include "Controller.h"
#include "Music.h"
#include "Samples.h"
#include "Templates.h"
#include <cmath>

namespace catbox
{
const char* buttonName (int b)
{
    static const char* n[NumButtons] = { "Track 1", "Track 2", "Track 3", "Track 4", "Track 5", "Track 6", "Track 7", "Track 8",
                                         "Sound", "Seq", "Note FX", "Mix", "Project", "Prev", "Next", "Shift", "Scene", "Rec", "Play", "Chaos" };
    return b >= 0 && b < NumButtons ? n[b] : "";
}

const char* modeName (int m)
{
    static const char* n[NumModes] = { "SOUND", "BROWSE", "SEQ", "NOTE FX", "MIX", "PROJECT", "MENU" };
    return n[juce::jlimit (0, NumModes - 1, m)];
}

namespace
{
// ---- the MIX and PROJECT knobs
enum MasterId { MVolume = 0, MGlue, MTape, MWidth, MRevSize, MRevDamp, MDelayTime, MDelayFb, MRevLowCut, MRevWidth, MDelayTone, MPingPong,
                MTempo, MSwing, MKey, MScale, MMetronome, MDj, MPads, MOctave, MReset,
                MRevType, MRevPre, MRevDuck, MRevLevel, MDelayType, MDelayWow, MDelayDuck, MDelayToRev, MDelayLevel, MFxType, MFxLevel, MFxDuck, NumMasterIds };

const std::vector<ParamDef>& masterDefs()
{
    static const std::vector<ParamDef> d = []
    {
        using namespace defs;
        std::vector<ParamDef> v {
            pct ("volume", "Master", 0.8f), pct ("glue", "Glue", 0.25f), pct ("tape", "Tape", 0.0f), pct ("width", "Width", 0.5f),
            pct ("revsize", "Reverb size", 0.6f), pct ("revdamp", "Reverb damp", 0.5f), choice ("dtime", "Delay time", delayNames(), 4),
            pct ("dfb", "Delay feedback", 0.4f), pct ("revlow", "Reverb low cut", 0.3f), pct ("revwidth", "Reverb width", 1.0f),
            pct ("dtone", "Delay tone", 0.5f), pct ("pingpong", "Ping-pong", 1.0f),
            num ("tempo", "Tempo", 40, 240, 110), pct ("swing", "Swing", 0.0f), choice ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0),
            choice ("scale", "Scale", {}, 1), toggle ("metro", "Click", false), bi ("dj", "DJ filter", 0.0f),
            choice ("pads", "Pads", { "Piano", "Scale" }, 0), num ("oct", "Pad octave", 0, 7, 3),
            num ("reset", "Factory reset", 0, 100, 0),
            choice ("revtype", "Reverb type", revTypeNames(), 0), pct ("revpre", "Pre-delay", 0.1f), pct ("revduck", "Reverb duck", 0.0f),
            pct ("revlevel", "Reverb level", 0.8f), choice ("dtype", "Delay type", delayTypeNames(), 0), pct ("dwow", "Delay wow", 0.3f),
            pct ("dduck", "Delay duck", 0.0f), pct ("d2rev", "Delay > reverb", 0.0f), pct ("dlevel", "Delay level", 0.8f),
            choice ("fxtype", "FX send", insertNames(), InsChorus), pct ("fxlevel", "FX level", 0.8f), pct ("fxduck", "FX duck", 0.0f),
        };
        v[MRevPre].formatFn = [] (float x) { return juce::String (juce::roundToInt (x * 250.0f)) + " ms"; };
        for (int s = 0; s < music::numScales; ++s) v[MScale].choices.add (music::scaleName (s));
        v[MScale].hi = (float) (music::numScales - 1);
        v[MSwing].hi = 0.75f;
        v[MDelayFb].hi = 0.95f;
        return v;
    }();
    return d;
}

const std::vector<ParamDef>& mixDefs()
{
    static const std::vector<ParamDef> d = []
    {
        using namespace defs;
        std::vector<ParamDef> v;
        for (int t = 0; t < numTracks; ++t)
        {
            const juce::String n (t + 1);
            auto a = pct ("vol", "", 0.75f); a.name = "Vol " + n;
            auto b = bi ("pan", "", 0.0f); b.name = "Pan " + n;
            auto c = pct ("sa", "", 0.12f); c.name = "Reverb " + n;
            auto e = pct ("sb", "", 0.0f); e.name = "Delay " + n;
            v.push_back (a); v.push_back (b); v.push_back (c); v.push_back (e);
        }
        return v;
    }();
    return d;
}

const std::vector<ParamDef>& lookDefs()
{
    static const std::vector<ParamDef> d = []
    {
        using namespace defs;
        return std::vector<ParamDef> {
            choice ("theme", "Theme", themeNames(), 0), choice ("accent", "Colour", accentNames(), 0),
            choice ("text", "Text", { "Normal", "Big" }, 0), choice ("knobs", "Knobs", { "Bars", "Rings" }, 0),
            toggle ("trackColours", "Track colours", false), toggle ("hint", "Help line", true) };
    }();
    return d;
}

const ParamDef& insTypeDef() { static const ParamDef d = defs::choice ("type", "Effect", insertNames(), 0); return d; }
const ParamDef& insOnDef() { static const ParamDef d = defs::toggle ("on", "On", true); return d; }
const ParamDef& fxSelDef() { static const ParamDef d = defs::num ("slot", "FX slot", 1, numInserts, 1); return d; }
const ParamDef& modSelDef() { static const ParamDef d = defs::num ("mslot", "Mod slot", 1, numModSlots, 1); return d; }
const ParamDef& modSourceDef() { static const ParamDef d = defs::choice ("src", "Source", modSourceNames(), 0); return d; }
} // namespace

juce::StringArray themeNames() { return { "Graphite", "Paper", "Midnight", "Forest", "Terminal", "High contrast", "Plum", "Sand" }; }
juce::StringArray accentNames() { return { "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink", "Red", "White" }; }

//==============================================================================
juce::StringArray starterSampleNames();
void makeStarterSamples (SampleBank&);

Controller::Controller (const juce::File& dir) : dataDir (dir)
{
    dataDir.createDirectory();
    SampleBank::get().loadFolder (dataDir.getChildFile ("samples"), starterSampleNames(), [] (SampleBank& b) { makeStarterSamples (b); });
    SampleBank::get().addLibrary (SampleBank::findLibrary (dataDir));   // VSCO 2 CE, Dirt-Samples ... (read when used)
    resetToDefaults();
    heldSteps.fill (-1);
    loadState();
    loadScenes();
    loadMidiMap();
    loadLook();
    prepareSamples(); audio.push (project);
    startTimerHz (30);
}

Controller::~Controller()
{
    stopTimer();
}

void Controller::resetToDefaults()
{
    for (int t = 0; t < numTracks; ++t) octave[(size_t) t] = 3;
    octave[1] = 2;
    project = Project::makeDefault();
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& d = engineDef (project.tracks[(size_t) t].engine);
        presetIdx[(size_t) t] = 0;
        for (size_t i = 0; i < d.presets.size(); ++i)
        {
            float v[maxParams];
            d.applyPreset ((int) i, v);
            if (std::memcmp (v, project.tracks[(size_t) t].params.data(), sizeof (float) * d.params.size()) == 0) { presetIdx[(size_t) t] = (int) i; break; }
        }
        edited[(size_t) t] = false;
        userName[(size_t) t] = {};
        soundPage[(size_t) t] = 0;
        bar[(size_t) t] = 0;
        lane[(size_t) t] = 0;
    }
    mixPage = projectPage = 0;
    fxSlot.fill (0); modSlot.fill (0); insPreset.fill (0);
    assignPending = -1;
    scalePads = false;
    numHeldSteps = 0;
    loadedScene = -1;
}

void Controller::factoryReset()
{
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "reset!"); sendRemote (o); return; }
    pushUndo ("Factory reset");
    audio.post ({ Event::Stop, 0, 0, 0.0f });
    audio.post ({ Event::AllOff, 0, 0, 0.0f });
    audio.setRecording (false);
    resetToDefaults();
    // the scenes are kept as a backup file (scenes-backup.json), not deleted
    const auto sf = dataDir.getChildFile ("scenes.json");
    if (sf.existsAsFile()) sf.copyFileTo (dataDir.getChildFile ("scenes-backup.json"));
    for (size_t i = 0; i < scenes.size(); ++i) scenesBeforeReset[i] = std::move (scenes[i]);
    saveScenes();
    track = 0;
    audio.viewTrack = 0;
    mode = ModeSound;
    changed();
    saveState();
    setFlash ("Factory reset: everything is new (Shift + Chaos = undo)");
}

void Controller::runOnMessageThread (std::function<void()> f)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread()) { f(); return; }
    juce::WeakReference<Controller> weak (this);
    juce::MessageManager::callAsync ([weak, f = std::move (f)] { if (weak.get() != nullptr) f(); });
}

void Controller::changed (bool atBar)
{
    if (! isRemote()) { prepareSamples(); audio.push (project, atBar); }
    dirty = true;
    if (onChange) onChange();
}

void Controller::setFlash (const juce::String& s)
{
    flash = s;
    flashUntil = juce::Time::getMillisecondCounter() + 1800;
    if (onChange) onChange();
}

juce::String Controller::flashText() const
{
    return juce::Time::getMillisecondCounter() < flashUntil ? flash : juce::String();
}

void Controller::timerCallback()
{
    pump();
    if (resetProgress > 0.0f && juce::Time::getMillisecondCounter() - resetTouched > 2500) { resetProgress = 0.0f; if (onChange) onChange(); }   // not turned all the way: back to 0
    // Chaos is a hold (0.6 s): a quick tap does nothing
    if (btnDown[BtnChaos] && ! chaosDone && ! shift.load() && juce::Time::getMillisecondCounter() - chaosDownAt > 600)
    {
        chaosDone = true;
        chaos();
    }
    if (dirty && ++saveCountdown > 30 * 5) { saveState(); }   // a few seconds after a change
    if (onChange) onChange();
}

void Controller::pump()
{
    RecNote r;
    bool any = false;
    while (audio.popRec (r)) { if (! any) pushUndo ("Recording", true); applyRecNote (r); any = true; }
    if (any) { dirty = true; if (onChange) onChange(); }
}

void Controller::applyRecNote (const RecNote& r)
{
    if (r.track < 0 || r.track >= numTracks || r.step < 0 || r.step >= maxSteps) return;
    auto& pat = project.tracks[(size_t) r.track].pat;
    switch (r.kind)
    {
        case RecNote::DrumHit: pat.drums[(size_t) juce::jlimit (0, numLanes - 1, (int) r.note)][(size_t) r.step] = (uint8_t) juce::jlimit (1, 127, (int) r.value); break;
        case RecNote::NoteAdd:
        {
            auto& st = pat.steps[(size_t) r.step];
            st.add (r.note);
            st.vel = (uint8_t) juce::jlimit (1, 127, (int) r.value);
            st.accent = r.accent;
            break;
        }
        case RecNote::NoteLength: pat.steps[(size_t) r.step].len = (uint16_t) juce::jlimit (10, 1600, (int) r.value); break;
        case RecNote::SlideOn:    pat.steps[(size_t) r.step].slide = true; break;
    }
    // the audio already has it; the next push carries the same
    dirty = true;
}

//==============================================================================
// Buttons
void Controller::buttonDown (int b)
{
    if (b < 0 || b >= NumButtons) return;
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "button"); o->setProperty ("b", b); o->setProperty ("down", true); sendRemote (o); if (b == BtnShift) shift = true; return; }
    btnDown[(size_t) b] = true;
    const bool sh = shift.load();

    if (b >= BtnTrack1 && b <= BtnTrack8)
    {
        const int t = b - BtnTrack1;
        if (sh) { setMute (t, ! project.tracks[(size_t) t].mute); return; }
        selectTrack (t);
        return;
    }
    switch (b)
    {
        case BtnShift: shift = true; break;
        case BtnSound:
            if (sh)   // Shift + Sound: start from Init
            {
                const auto& d = engineDef (cur().engine);
                int init = (int) d.presets.size() - 1;
                for (size_t i = 0; i < d.presets.size(); ++i) if (d.presets[i].name == "Init") init = (int) i;
                loadSound (track.load(), cur().engine, init);
                setFlash ("Init: a plain " + d.name + " sound");
                break;
            }
            setMode (mode.load() == ModeSound ? ModeBrowse : ModeSound);
            break;
        case BtnSeq:     setMode (ModeSeq); break;
        case BtnFx:      setMode (ModeFx); break;
        case BtnMix:
            if (sh) { for (auto& t : project.tracks) { t.mute = false; t.solo = false; } changed(); setFlash ("Everything unmuted"); break; }
            setMode (ModeMix);
            break;
        case BtnProject:
            if (assignPending >= 0) { assignPending = -1; setFlash ("Assign cancelled"); break; }
            if (mode.load() == ModeMenu) { setMode (ModeProject); break; }
            if (mode.load() == ModeProject) { menuPage = 0; setMode (ModeMenu); break; }
            setMode (ModeProject);
            break;
        case BtnPrev:
        case BtnNext:
        {
            const int dir = b == BtnNext ? 1 : -1;
            const int t = track.load();
            const int m = mode.load();
            if (sh && (m == ModeSound || m == ModeBrowse || m == ModeProject || m == ModeFx || m == ModeMix))
            {
                if (m == ModeFx)
                {
                    const auto& d = noteFxDef (cur().fxType);
                    if (d.presets.empty()) break;
                    static int fxPreset = 0;
                    fxPreset = (fxPreset + dir + (int) d.presets.size()) % (int) d.presets.size();
                    loadFxPreset (cur(), cur().fxType, fxPreset);
                    setFlash (d.name + ": " + d.presets[(size_t) fxPreset].name);
                    changed();
                    break;
                }
                octave[(size_t) t] = juce::jlimit (0, 7, octave[(size_t) t].load() + dir);
                setFlash ("Pads: octave " + juce::String (octave[(size_t) t].load()));
                break;
            }
            switch (m)
            {
                case ModeSound:
                {
                    const int n = (int) engineDef (cur().engine).pages.size() + numSoundExtras;
                    soundPage[(size_t) t] = (soundPage[(size_t) t] + dir + n) % n;
                    break;
                }
                case ModeBrowse:
                {
                    const auto list = browsePresets();
                    if (list.isEmpty()) break;
                    browsePos += dir;
                    const auto cats = browseCategories();
                    if (browsePos < 0 || browsePos >= list.size())   // past the end: the next / previous category
                    {
                        browseCat = (browseCat + dir + cats.size()) % juce::jmax (1, cats.size());
                        browsePos = dir > 0 ? 0 : juce::jmax (0, browsePresets().size() - 1);
                    }
                    browseApply (true);
                    break;
                }
                case ModeSeq:
                    if (sh)   // Shift + Prev = copy this bar, Shift + Next = paste it here
                    {
                        const int b0 = bar[(size_t) t] * 16;
                        auto& pat = cur().pat;
                        if (dir < 0)
                        {
                            for (int i = 0; i < 16; ++i)
                            {
                                clipSteps[(size_t) i] = pat.steps[(size_t) (b0 + i)];
                                for (int l = 0; l < numLanes; ++l) clipDrums[(size_t) l][(size_t) i] = pat.drums[(size_t) l][(size_t) (b0 + i)];
                                clipLocks[(size_t) i] = pat.locks[(size_t) (b0 + i)];
                                for (int l = 0; l < numLanes; ++l) clipRolls[(size_t) l][(size_t) i] = pat.rolls[(size_t) l][(size_t) (b0 + i)];
                            }
                            hasClip = true;
                            setFlash ("Bar " + juce::String (bar[(size_t) t] + 1) + " copied (Shift + Next = paste)");
                        }
                        else if (hasClip)
                        {
                            pushUndo ("Paste");
                            for (int i = 0; i < 16; ++i)
                            {
                                pat.steps[(size_t) (b0 + i)] = clipSteps[(size_t) i];
                                for (int l = 0; l < numLanes; ++l) pat.drums[(size_t) l][(size_t) (b0 + i)] = clipDrums[(size_t) l][(size_t) i];
                                pat.locks[(size_t) (b0 + i)] = clipLocks[(size_t) i];
                                for (int l = 0; l < numLanes; ++l) pat.rolls[(size_t) l][(size_t) (b0 + i)] = clipRolls[(size_t) l][(size_t) i];
                            }
                            changed();
                            setFlash ("Pasted into bar " + juce::String (bar[(size_t) t] + 1));
                        }
                        else setFlash ("Nothing copied yet (Shift + Prev = copy the bar)");
                        break;
                    }
                    {
                        const int bars = juce::jmax (1, (cur().pat.length + 15) / 16);
                        bar[(size_t) t] = (bar[(size_t) t] + dir + bars) % bars;
                    }
                    break;
                case ModeFx:
                {
                    const int ft = (cur().fxType + dir + NumNoteFx) % NumNoteFx;
                    loadFxPreset (cur(), ft, 0);
                    setFlash ("Note FX: " + noteFxNames()[ft]);
                    changed();
                    break;
                }
                case ModeMix:     mixPage = (mixPage + dir + 4) % 4; break;
                case ModeProject: projectPage = (projectPage + dir + 2) % 2; break;
                default: break;
            }
            if (onChange) onChange();
            break;
        }
        case BtnScene:
            sceneMode = ! sceneMode.load();
            setFlash (sceneMode.load() ? "Scenes: pad 1-8 = recall, Rec + pad = save, pads 9-16 = mute" : juce::String ("Scenes off"));
            break;
        case BtnRec:
            if (sceneMode.load()) break;   // a modifier for saving scenes
            if (sh)   // Shift + Rec: clear the track's pattern
            {
                clearPattern (track.load());
                setFlash ("Track " + juce::String (track.load() + 1) + ": pattern cleared");
                break;
            }
            if (mode.load() == ModeBrowse) { saveUserSound(); break; }
            if (mode.load() == ModeSound && cur().engine == EngineSampler)   // a sampler track: Rec records a sample
            {
                isSampling() ? stopSampling() : startSampling();
                break;
            }
            audio.setRecording (! audio.isRecording());
            setFlash (! audio.isRecording() ? "Rec off" : (audio.isPlaying() ? "Rec: play and it is written into the steps" : "Rec armed: press Play"));
            break;
        case BtnPlay:
            if (sh) { tapTempo(); break; }
            setPlaying (! audio.isPlaying());
            break;
        case BtnChaos:
            if (sh) { undo(); break; }
            chaosDownAt = juce::Time::getMillisecondCounter();
            chaosDone = false;
            break;
        default: break;
    }
    if (onChange) onChange();
}

void Controller::buttonUp (int b)
{
    if (b < 0 || b >= NumButtons) return;
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "button"); o->setProperty ("b", b); o->setProperty ("down", false); sendRemote (o); if (b == BtnShift) shift = false; return; }
    btnDown[(size_t) b] = false;
    if (b == BtnShift) shift = false;
    if (b == BtnChaos && ! chaosDone && ! shift.load() && juce::Time::getMillisecondCounter() - chaosDownAt < 600)
        setFlash ("Hold Chaos a moment for new random sound settings");
    if (onChange) onChange();
}

void Controller::selectTrack (int t)
{
    t = juce::jlimit (0, numTracks - 1, t);
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "track"); o->setProperty ("v", t); sendRemote (o); return; }
    track = t;
    audio.viewTrack = t;
    numHeldSteps = 0;
    if (mode.load() == ModeBrowse) browseSync();
    if (onChange) onChange();
}

void Controller::setMode (int m)
{
    inputWritten = false;
    menuConfirm = -1;
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "mode"); o->setProperty ("v", m); sendRemote (o); return; }
    mode = juce::jlimit (0, NumModes - 1, m);
    numHeldSteps = 0;
    if (m == ModeMenu) menuPage = 0;
    if (m == ModeBrowse) browseSync();
    if (onChange) onChange();
}

void Controller::setPlaying (bool p)
{
    if (isRemote()) return;
    audio.post ({ p ? Event::Play : Event::Stop, 0, 0, 0.0f });
    if (onChange) onChange();
}

void Controller::setMute (int t, bool m)
{
    project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)].mute = m;
    changed();
}

void Controller::setSolo (int t, bool s)
{
    project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)].solo = s;
    changed();
}

void Controller::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.isEmpty() && now - taps.getLast() > 2000.0) taps.clear();
    taps.add (now);
    if (taps.size() > 5) taps.remove (0);
    if (taps.size() >= 2)
    {
        const double avg = (taps.getLast() - taps.getFirst()) / (taps.size() - 1);
        project.m.tempo = (float) juce::jlimit (40.0, 240.0, 60000.0 / avg);
        changed();
        setFlash ("Tempo " + juce::String (juce::roundToInt (project.m.tempo)) + " BPM");
    }
    else setFlash ("Tap in time...");
}

//==============================================================================
// Sounds
void Controller::loadSound (int t, int engine, int preset)
{
    t = juce::jlimit (0, numTracks - 1, t);
    pushUndo ("Sound", true);
    auto& tr = project.tracks[(size_t) t];
    const bool wasDrums = tr.isDrums();
    if (tr.engine != engine)
    {
        for (auto& ls : tr.pat.locks) ls = {};   // recorded knob moves belong to the old engine
        tr.mods.dropEngineTargets();              // and so does modulation of its knobs
    }
    loadEnginePreset (tr, engine, preset);
    if (const auto& pd = engineDef (engine).presets; preset >= 0 && preset < (int) pd.size() && ! pd[(size_t) preset].samples.empty())
    {
        // a library sound: its samples must be there; a sampler plays it from the sample's own note
        int missing = 0;
        for (const auto& [id, name] : pd[(size_t) preset].samples) missing += SampleBank::get().find (name) < 0 ? 1 : 0;
        if (missing > 0) setFlash (juce::String (missing) + " sample(s) of this sound are not here - get the libraries (README: get_libraries)");
        if (engine == EngineSampler)
        {
            const auto& d = engineDef (engine);
            const int note = SampleBank::noteFromName (SampleBank::get().name (juce::roundToInt (tr.params[(size_t) d.index ("sample")])));
            if (note >= 0) tr.params[(size_t) d.index ("root")] = (float) note;
        }
    }
    if (const int chain = presetChain (engine, preset); chain >= 0)   // an FX Lab sound brings its effects
    {
        applyChainPreset (tr.mods, chain);
        for (auto& sl : tr.mods.slots) if (dest::isInsert (sl.dest)) sl = ModSlot();
    }
    if (wasDrums != tr.isDrums()) lane[(size_t) t] = 0;
    presetIdx[(size_t) t] = preset;
    userName[(size_t) t] = {};
    edited[(size_t) t] = false;
    soundPage[(size_t) t] = juce::jlimit (0, (int) engineDef (engine).pages.size() + numSoundExtras - 1, soundPage[(size_t) t]);
    changed();
}

void Controller::setParam (int t, int index, float value)
{
    auto& tr = project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)];
    const auto& d = engineDef (tr.engine);
    if (index < 0 || index >= (int) d.params.size()) return;
    tr.params[(size_t) index] = d.params[(size_t) index].clamp (value);
    edited[(size_t) t] = true;
    changed();
}

void Controller::setFxParam (int t, int index, float value)
{
    auto& tr = project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)];
    const auto& d = noteFxDef (tr.fxType);
    if (index < 0 || index >= (int) d.params.size()) return;
    tr.fx[(size_t) index] = d.params[(size_t) index].clamp (value);
    changed();
}

void Controller::setMaster (std::function<void (Master&)> change)
{
    change (project.m);
    changed();
}

juce::String Controller::presetName (int t) const
{
    t = juce::jlimit (0, numTracks - 1, t);
    if (userName[(size_t) t].isNotEmpty()) return userName[(size_t) t];
    const auto& d = engineDef (project.tracks[(size_t) t].engine);
    const int i = presetIdx[(size_t) t];
    return i >= 0 && i < (int) d.presets.size() ? d.presets[(size_t) i].name : juce::String ("Sound");
}

juce::String Controller::trackTitle (int t) const
{
    return engineDef (project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)].engine).name + " - " + presetName (t) + (isEdited (t) ? " *" : "");
}

// ---- the browser: knob 1 = engine, knob 2 = category, knob 3 = sound (heard right away)
juce::StringArray Controller::browseCategories() const
{
    auto c = engineDef (browseEngine).categories();
    const auto dir = dataDir.getChildFile ("sounds").getChildFile (engineDef (browseEngine).name);
    if (dir.isDirectory() && ! dir.findChildFiles (juce::File::findFiles, false, "*.json").isEmpty()) c.add ("User");
    if (browseEngine == EngineSampler || browseEngine == EngineGranular || browseEngine == EngineKit)
        for (const auto& f : SampleBank::get().libraryFolders()) c.add ("LIB " + f);   // every sample of the libraries
    return c;
}

juce::StringArray Controller::browsePresets() const
{
    const auto cats = browseCategories();
    if (cats.isEmpty()) return {};
    const auto cat = cats[juce::jlimit (0, cats.size() - 1, browseCat)];
    juce::StringArray out;
    if (cat.startsWith ("LIB "))
    {
        for (int s : SampleBank::get().librarySlotsIn (cat.substring (4))) out.add (SampleBank::get().shortName (s));
        return out;
    }
    if (cat == "User")
    {
        auto files = dataDir.getChildFile ("sounds").getChildFile (engineDef (browseEngine).name).findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        for (auto& f : files) out.add (f.getFileNameWithoutExtension());
        return out;
    }
    const auto& d = engineDef (browseEngine);
    for (int i : d.presetsIn (cat)) out.add (d.presets[(size_t) i].name);
    return out;
}

void Controller::browseSync()
{
    // the browser starts where the track's sound is
    browseEngine = cur().engine;
    const auto& d = engineDef (browseEngine);
    const auto cats = browseCategories();
    browseCat = 0; browsePos = 0;
    if (userName[(size_t) track.load()].isNotEmpty())
    {
        browseCat = juce::jmax (0, cats.indexOf ("User"));
        browsePos = juce::jmax (0, browsePresets().indexOf (userName[(size_t) track.load()]));
        return;
    }
    const int pi = presetIdx[(size_t) track.load()];
    if (pi >= 0 && pi < (int) d.presets.size())
    {
        browseCat = juce::jmax (0, cats.indexOf (d.presets[(size_t) pi].category));
        const auto list = d.presetsIn (d.presets[(size_t) pi].category);
        for (size_t k = 0; k < list.size(); ++k) if (list[k] == pi) browsePos = (int) k;
    }
}

void Controller::browseApply (bool preview)
{
    const auto cats = browseCategories();
    browseCat = juce::jlimit (0, juce::jmax (0, cats.size() - 1), browseCat);
    const auto list = browsePresets();
    browsePos = juce::jlimit (0, juce::jmax (0, list.size() - 1), browsePos);
    const int t = track.load();
    if (cats[browseCat].startsWith ("LIB "))
    {
        auto& bank = SampleBank::get();
        const auto slots = bank.librarySlotsIn (cats[browseCat].substring (4));
        if (slots.empty()) return;
        const int slot = slots[(size_t) juce::jlimit (0, (int) slots.size() - 1, browsePos)];
        if (! bank.ensureLoaded (slot)) { setFlash ("Can't read " + bank.shortName (slot)); return; }
        if (cur().engine != browseEngine)   // a fresh sound of the engine first
        {
            const auto& d = engineDef (browseEngine);
            int init = 0;
            for (size_t i = 0; i < d.presets.size(); ++i) if (d.presets[i].name == "Init") init = (int) i;
            loadSound (t, browseEngine, init);
        }
        pushUndo ("Sample", true);
        auto& tr = cur();
        const auto& d = engineDef (tr.engine);
        const auto folder = bank.folderOf (slot).toLowerCase();
        if (tr.engine == EngineKit)
        {
            const int l = lane[(size_t) t];
            tr.params[(size_t) (l * 6)] = (float) slot;
            setFlash ("Pad " + juce::String (l + 1) + ": " + bank.shortName (slot) + "  (hit a pad to pick another)");
            changed();
            if (preview) audio.post ({ Event::NoteOn, (int8_t) t, (int16_t) (36 + l), 0.9f });
            if (preview) audio.post ({ Event::NoteOff, (int8_t) t, (int16_t) (36 + l), 0.0f });
            return;
        }
        tr.params[(size_t) d.index ("sample")] = (float) slot;
        if (tr.engine == EngineSampler)
        {
            // a folder with many notes is an instrument: play all of it across the keys
            tr.params[(size_t) d.index ("multi")] = bank.buildZones (slot) ? 1.0f : 0.0f;
            const int note = SampleBank::noteFromName (bank.name (slot));
            tr.params[(size_t) d.index ("root")] = (float) (note >= 0 ? note : 60);
            const auto* smp = bank.at (slot);
            const bool sustained = folder.contains ("sus") || folder.contains ("vib") || folder.contains ("trem") || folder.contains ("organ")
                                || folder.contains ("pad") || folder.contains ("drone") || folder.contains ("roll");
            tr.params[(size_t) d.index ("mode")] = 0.0f;
            tr.params[(size_t) d.index ("start")] = 0.0f;
            tr.params[(size_t) d.index ("end")] = sustained ? 0.97f : 1.0f;
            tr.params[(size_t) d.index ("loop")] = sustained ? 1.0f : 0.0f;
            tr.params[(size_t) d.index ("loopstart")] = 0.4f;
            tr.params[(size_t) d.index ("s")] = 1.0f;
            tr.params[(size_t) d.index ("d")] = 4.0f;
            tr.params[(size_t) d.index ("r")] = sustained ? 0.45f : juce::jlimit (0.05f, 2.0f, smp != nullptr ? (float) (smp->length() / smp->sampleRate) : 0.5f);
        }
        presetIdx[(size_t) t] = -1;
        userName[(size_t) t] = bank.shortName (slot);
        edited[(size_t) t] = false;
        changed();
        if (preview && ! audio.isPlaying())
            audio.post ({ Event::Preview, (int8_t) t, (int16_t) (12 * (octave[(size_t) t].load() + 1) + project.m.key), 0.8f });
        return;
    }
    if (cats[browseCat] == "User")
    {
        const auto f = dataDir.getChildFile ("sounds").getChildFile (engineDef (browseEngine).name).getChildFile (list[browsePos] + ".json");
        if (! f.existsAsFile()) return;
        auto& tr = cur();
        const int fx = tr.fxType;
        const auto fxv = tr.fx;
        soundFromVar (juce::JSON::parse (f), tr);
        tr.fxType = fx; tr.fx = fxv;   // a sound doesn't change the track's note FX
        presetIdx[(size_t) t] = -1;
        userName[(size_t) t] = list[browsePos];
        edited[(size_t) t] = false;
        changed();
    }
    else
    {
        const auto& d = engineDef (browseEngine);
        const auto idx = d.presetsIn (cats[browseCat]);
        if (idx.empty()) return;
        loadSound (t, browseEngine, idx[(size_t) juce::jlimit (0, (int) idx.size() - 1, browsePos)]);
    }
    if (preview && ! audio.isPlaying())
        audio.post ({ Event::Preview, (int8_t) t, (int16_t) (12 * (octave[(size_t) t].load() + 1) + project.m.key), 0.8f });
}

void Controller::saveUserSound()
{
    const auto dir = dataDir.getChildFile ("sounds").getChildFile (engineDef (cur().engine).name);
    dir.createDirectory();
    const auto base = presetName (track.load()).upToFirstOccurrenceOf (" (", false, false).replaceCharacters ("/\\:*?\"<>|", "---------");
    auto f = dir.getChildFile (base + ".json");
    for (int i = 2; f.existsAsFile(); ++i) f = dir.getChildFile (base + " " + juce::String (i) + ".json");
    f.replaceWithText (juce::JSON::toString (soundToVar (cur())));
    userName[(size_t) track.load()] = f.getFileNameWithoutExtension();
    presetIdx[(size_t) track.load()] = -1;
    edited[(size_t) track.load()] = false;
    setFlash ("Saved: User / " + f.getFileNameWithoutExtension());
    if (mode.load() == ModeBrowse) browseSync();
    dirty = true;
}

void Controller::startSampling()
{
    if (isRemote()) return;
    if (enableMicrophone && ! enableMicrophone())
    {
        setFlash ("No input: connect a microphone (right-click > Sound card)");
        return;
    }
    audio.captureLength = 0;
    audio.capturing = true;
    setFlash ("Recording a sample... Rec = stop");
}

void Controller::stopSampling()
{
    audio.capturing = false;
    const int len = audio.captureLength.load();
    const auto& L = audio.captureL;
    const auto& R = audio.captureR;
    float peak = 0.0f;
    for (int i = 0; i < len; ++i) peak = juce::jmax (peak, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
    if (len < 2000 || peak < 0.003f) { setFlash ("Nothing came in (is the microphone on?)"); return; }
    // trim the silence before and after, normalise, short fades
    const float thr = peak * 0.05f;
    int a = 0, b = len - 1;
    while (a < len && std::abs (L[(size_t) a]) < thr && std::abs (R[(size_t) a]) < thr) ++a;
    while (b > a && std::abs (L[(size_t) b]) < thr && std::abs (R[(size_t) b]) < thr) --b;
    a = juce::jmax (0, a - (int) (audio.getSampleRate() * 0.005));
    b = juce::jmin (len - 1, b + (int) (audio.getSampleRate() * 0.2));
    std::vector<float> l (L.begin() + a, L.begin() + b + 1), r (R.begin() + a, R.begin() + b + 1);
    const float g = 0.9f / peak;
    const int fade = juce::jmin ((int) l.size() / 4, 240);
    for (size_t i = 0; i < l.size(); ++i)
    {
        float f = 1.0f;
        if ((int) i < fade) f = (float) i / (float) fade;
        if ((int) (l.size() - 1 - i) < fade) f = juce::jmin (f, (float) (l.size() - 1 - i) / (float) fade);
        l[i] *= g * f; r[i] *= g * f;
    }
    auto& bank = SampleBank::get();
    int n = 1;
    while (bank.find ("Rec " + juce::String (n)) >= 0 || bank.folder().getChildFile ("Rec " + juce::String (n) + ".wav").existsAsFile()) ++n;
    const int slot = bank.add ("Rec " + juce::String (n), std::move (l), std::move (r), audio.getSampleRate());
    if (slot < 0) { setFlash ("The sample memory is full (64 samples)"); return; }
    auto& tr = cur();
    const auto& d = engineDef (EngineSampler);
    tr.params[(size_t) d.index ("sample")] = (float) slot;
    tr.params[(size_t) d.index ("start")] = 0.0f;
    tr.params[(size_t) d.index ("end")] = 1.0f;
    edited[(size_t) track.load()] = true;
    changed();
    setFlash ("Sample \"Rec " + juce::String (n) + "\" recorded: play it on the keys (C4 = as recorded)");
}

void Controller::chaos()
{
    const int m = mode.load();
    auto& tr = cur();
    pushUndo ("Chaos");
    juce::Random rng;
    if (m == ModeFx && tr.fxType != FxOff)
    {
        const auto& d = noteFxDef (tr.fxType);
        for (size_t i = 0; i < d.params.size(); ++i)
            if (d.params[i].unit != ParamDef::Toggle) tr.fx[i] = d.params[i].fromNorm (rng.nextFloat());
        setFlash ("Chaos: new note FX settings (Shift + Chaos = back)");
        changed();
        return;
    }
    // a random factory sound of the engine, then every knob moved a little (volume stays)
    const auto& d = engineDef (tr.engine);
    const int pick = rng.nextInt ((int) d.presets.size());
    const float vol = d.index ("volume") >= 0 ? tr.params[(size_t) d.index ("volume")] : 0.0f;
    d.applyPreset (pick, tr.params.data());
    for (size_t i = 0; i < d.params.size(); ++i)
    {
        const auto& p = d.params[i];
        if (p.id == "volume" || p.id == "voices" || p.id.endsWith ("_level")) continue;
        if (p.stepped()) { if (p.unit == ParamDef::Choice && rng.nextFloat() < 0.15f) tr.params[i] = p.fromNorm (rng.nextFloat()); continue; }
        tr.params[i] = p.fromNorm (juce::jlimit (0.0f, 1.0f, p.toNorm (tr.params[i]) + (rng.nextFloat() - 0.5f) * 0.3f));
    }
    if (d.index ("volume") >= 0) tr.params[(size_t) d.index ("volume")] = vol;
    presetIdx[(size_t) track.load()] = pick;
    userName[(size_t) track.load()] = {};
    edited[(size_t) track.load()] = true;
    setFlash ("Chaos from " + d.presets[(size_t) pick].name + " (Shift + Chaos = back)");
    changed();
}

void Controller::pushUndo (const juce::String& what, bool merge)
{
    if (isRemote()) return;
    const auto now = juce::Time::getMillisecondCounter();
    if (merge && ! undoStack.empty() && undoStack.back()->what == what && now - undoStack.back()->at < 3000)
    {
        undoStack.back()->at = now;   // still the same edit
        return;
    }
    auto u = std::make_unique<UndoState>();
    u->project = project;
    u->presetIdx = presetIdx;
    u->edited = edited;
    u->userName = userName;
    u->what = what;
    u->at = now;
    undoStack.push_back (std::move (u));
    if (undoStack.size() > 40) undoStack.erase (undoStack.begin());
}

void Controller::undo()
{
    if (undoStack.empty()) { setFlash ("Nothing to take back"); return; }
    auto u = std::move (undoStack.back());
    undoStack.pop_back();
    if (u->what == "Factory reset")   // the scenes come back too
    {
        for (size_t i = 0; i < scenes.size(); ++i) if (scenesBeforeReset[i] != nullptr) scenes[i] = std::move (scenesBeforeReset[i]);
        saveScenes();
    }
    project = u->project;
    presetIdx = u->presetIdx;
    edited = u->edited;
    userName = u->userName;
    pagesClamp();
    numHeldSteps = 0;
    setFlash ("Undo: " + u->what + (undoStack.empty() ? juce::String() : "  (" + juce::String ((int) undoStack.size()) + " more)"));
    changed();
}

//==============================================================================
// Patterns, scenes
void Controller::clearPattern (int t)
{
    pushUndo ("Clear pattern");
    project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)].pat.clear();
    changed();
}

void Controller::editPattern (int t, std::function<void (Pattern&)> change)
{
    change (project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)].pat);
    changed();
}

void Controller::saveScene (int s)
{
    s = juce::jlimit (0, numScenes - 1, s);
    scenes[(size_t) s] = std::make_unique<Project> (project);
    loadedScene = s;
    saveScenes();
    setFlash ("Scene " + juce::String (s + 1) + " saved");
}

bool Controller::loadScene (int s)
{
    s = juce::jlimit (0, numScenes - 1, s);
    if (scenes[(size_t) s] == nullptr) { setFlash ("Scene " + juce::String (s + 1) + " is empty (Rec + pad saves it)"); return false; }
    pushUndo ("Scene " + juce::String (s + 1));
    const auto m = project.m;
    project = *scenes[(size_t) s];
    project.m.tempo = m.tempo;   // the tempo, key and master stay where they are
    project.m.key = m.key;
    project.m.scale = m.scale;
    project.m.volume = m.volume;
    project.m.djFilter = m.djFilter;
    loadedScene = s;
    for (int t = 0; t < numTracks; ++t) { edited[(size_t) t] = false; }
    changed (true);
    setFlash ("Scene " + juce::String (s + 1) + (audio.isPlaying() ? " (at the next bar)" : juce::String()));
    return true;
}

void Controller::saveScenes() const
{
    juce::Array<juce::var> a;
    for (const auto& s : scenes) a.add (s != nullptr ? s->toVar() : juce::var());
    dataDir.getChildFile ("scenes.json").replaceWithText (juce::JSON::toString (juce::var (a)));
}

void Controller::loadScenes()
{
    const auto f = dataDir.getChildFile ("scenes.json");
    if (! f.existsAsFile()) return;
    const auto parsed = juce::JSON::parse (f);
    if (auto* a = parsed.getArray())
        for (int i = 0; i < juce::jmin (numScenes, a->size()); ++i)
            if ((*a)[i].isObject())
            {
                scenes[(size_t) i] = std::make_unique<Project> (Project::makeDefault());
                scenes[(size_t) i]->fromVar ((*a)[i]);
            }
}

juce::var Controller::stateVar() const
{
    auto v = project.toVar();
    auto* ui = new juce::DynamicObject();
    juce::Array<juce::var> pi, ed, un, oc, sp;
    for (int t = 0; t < numTracks; ++t)
    {
        pi.add (presetIdx[(size_t) t]); ed.add (edited[(size_t) t]); un.add (userName[(size_t) t]);
        oc.add (octave[(size_t) t].load()); sp.add (soundPage[(size_t) t]);
    }
    ui->setProperty ("preset", pi); ui->setProperty ("edited", ed); ui->setProperty ("user", un);
    ui->setProperty ("octave", oc); ui->setProperty ("page", sp);
    const int m = mode.load();
    ui->setProperty ("track", track.load()); ui->setProperty ("mode", m == ModeBrowse || m == ModeMenu ? (int) ModeSound : m);
    ui->setProperty ("scalePads", scalePads.load());
    ui->setProperty ("scene", loadedScene);
    if (auto* o = v.getDynamicObject()) o->setProperty ("ui", juce::var (ui));
    return v;
}

void Controller::applyStateVar (const juce::var& v)
{
    if (! v.isObject()) return;
    project.fromVar (v);
    const auto ui = v["ui"];
    auto arr = [&ui] (const char* id) { return ui[id].getArray(); };
    for (int t = 0; t < numTracks; ++t)
    {
        if (auto* a = arr ("preset"); a != nullptr && t < a->size()) presetIdx[(size_t) t] = (int) (*a)[t];
        if (auto* a = arr ("edited"); a != nullptr && t < a->size()) edited[(size_t) t] = (bool) (*a)[t];
        if (auto* a = arr ("user"); a != nullptr && t < a->size()) userName[(size_t) t] = (*a)[t].toString();
        if (auto* a = arr ("octave"); a != nullptr && t < a->size()) octave[(size_t) t] = juce::jlimit (0, 7, (int) (*a)[t]);
        if (auto* a = arr ("page"); a != nullptr && t < a->size()) soundPage[(size_t) t] = (int) (*a)[t];
        selStep[(size_t) t] = -1;
    }
    track = juce::jlimit (0, numTracks - 1, (int) ui.getProperty ("track", 0));
    audio.viewTrack = track.load();
    mode = juce::jlimit (0, NumModes - 1, (int) ui.getProperty ("mode", 0));
    scalePads = (bool) ui.getProperty ("scalePads", false);
    loadedScene = (int) ui.getProperty ("scene", -1);
    pagesClamp();
}

void Controller::saveState()
{
    if (isRemote()) return;
    dirty = false;
    saveCountdown = 0;
    dataDir.getChildFile ("state2.json").replaceWithText (juce::JSON::toString (stateVar()));
}

void Controller::loadState()
{
    const auto f = dataDir.getChildFile ("state2.json");
    if (! f.existsAsFile()) return;
    applyStateVar (juce::JSON::parse (f));
}

bool Controller::projectExists (int slot) const
{
    return dataDir.getChildFile ("projects").getChildFile ("project" + juce::String (slot + 1) + ".json").existsAsFile();
}

void Controller::saveProject (int slot)
{
    const auto f = dataDir.getChildFile ("projects").getChildFile ("project" + juce::String (slot + 1) + ".json");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (stateVar()));
    setFlash ("Project " + juce::String (slot + 1) + " saved");
}

bool Controller::loadProject (int slot)
{
    const auto f = dataDir.getChildFile ("projects").getChildFile ("project" + juce::String (slot + 1) + ".json");
    if (! f.existsAsFile()) { setFlash ("Project " + juce::String (slot + 1) + " is empty"); return false; }
    pushUndo ("Load project");
    audio.post ({ Event::AllOff, 0, 0, 0.0f });
    applyStateVar (juce::JSON::parse (f));
    mode = ModeSound;
    changed();
    setFlash ("Project " + juce::String (slot + 1) + " loaded");
    return true;
}

void Controller::pagesClamp()
{
    for (int t = 0; t < numTracks; ++t)
        soundPage[(size_t) t] = juce::jlimit (0, (int) engineDef (project.tracks[(size_t) t].engine).pages.size() + numSoundExtras - 1, soundPage[(size_t) t]);
}

//==============================================================================
// Knobs
Controller::Target Controller::targetFor (int slot) const
{
    const int m = mode.load();
    const auto& tr = cur();
    switch (m)
    {
        case ModeSound:
        {
            const int t = track.load();
            switch (soundExtraPage())
            {
                case ExtraFx:
                    if (slot == 0) return { KFxSel, 0 };
                    if (slot == 1) return { KInsType, fxSlot[(size_t) t] };
                    if (slot >= 2 && slot < 10)
                    {
                        const int k = slot - 2;
                        const auto& in = tr.mods.inserts[(size_t) fxSlot[(size_t) t]];
                        return k < (int) insertDef (in.type).params.size() ? Target { KIns, fxSlot[(size_t) t] * insertParams + k } : Target {};
                    }
                    if (slot == 10) return tr.mods.inserts[(size_t) fxSlot[(size_t) t]].type != InsOff ? Target { KInsOn, fxSlot[(size_t) t] } : Target {};
                    if (slot == 11) return tr.mods.inserts[(size_t) fxSlot[(size_t) t]].type != InsOff ? Target { KInsPreset, fxSlot[(size_t) t] } : Target {};
                    if (slot >= 12 && slot < 16) return { KInsType, slot - 12 };
                    if (slot >= 16 && slot < 20) return tr.mods.inserts[(size_t) (slot - 16)].type != InsOff ? Target { KInsOn, slot - 16 } : Target {};
                    if (slot == 20) return { KChain, 0 };
                    return {};
                case ExtraMacro:
                {
                    if (slot < 8) return { KMacro, slot };
                    static const int quick[4] = { MsLfo1Rate, MsLfo1Shape, MsLfo2Rate, MsLfo2Shape };
                    if (slot < 12) return { KModSet, quick[slot - 8] };
                    return { KModSet, slot - 12 };
                }
                case ExtraMod:
                    if (slot == 0) return { KModSel, 0 };
                    if (slot <= 3) return { KModField, slot - 1 };
                    if (slot < 12) return { KModAmt, slot - 4 };
                    if (slot < 20) return { KModAmt, slot - 12 + 8 };
                    return {};
                case ExtraMy:
                    return destTarget (tr.mods.my[(size_t) slot]);
                default: break;
            }
            const auto& pages = engineDef (tr.engine).pages;
            if (pages.empty()) return {};
            const auto& pg = pages[(size_t) juce::jlimit (0, (int) pages.size() - 1, soundPage[(size_t) track.load()])];
            const int i = pg.slot[(size_t) slot];
            return i >= 0 ? Target { KParam, i } : Target {};
        }
        case ModeBrowse:
            if (slot <= 2) return { KBrowse, slot };
            return {};
        case ModeSeq: return { KSeq, slot };
        case ModeFx:
        {
            const auto& d = noteFxDef (tr.fxType);
            if (d.pages.empty()) return {};
            const int i = d.pages[0].slot[(size_t) slot];
            return i >= 0 ? Target { KFx, i } : Target {};
        }
        case ModeMix:
            switch (mixPage)
            {
                case 0:
                    if (slot < 8) return { KMix, slot * 4 + 0 };
                    if (slot < 12) return { KMaster, MVolume + (slot - 8) };
                    if (slot < 20) return { KMix, (slot - 12) * 4 + 1 };
                    return {};
                case 1:   // SENDS: reverb (Shift: delay) per track
                {
                    static const int a[4] = { MRevType, MRevSize, MDelayType, MDelayTime }, b[4] = { MDelayFb, MDelayTone, MPingPong, MDelayToRev };
                    if (slot < 8) return { KMix, slot * 4 + 2 };
                    if (slot < 12) return { KMaster, a[slot - 8] };
                    if (slot < 20) return { KMix, (slot - 12) * 4 + 3 };
                    return { KMaster, b[slot - 20] };
                }
                case 2:   // FX SEND: any effect as a send
                {
                    const auto& d = insertDef (project.m.fxSend.type);
                    if (slot < 8) return { KSendC, slot };
                    if (slot == 8) return { KMaster, MFxType };
                    if (slot == 9) return d.presets.empty() ? Target {} : Target { KSendFxPreset, 0 };
                    if (slot == 10) return { KMaster, MFxLevel };
                    if (slot == 11) return { KMaster, MFxDuck };
                    if (slot < 20) return slot - 12 < (int) d.params.size() ? Target { KSendFx, slot - 12 } : Target {};
                    return {};
                }
                default:  // RETURNS: the reverb and the delay in detail
                {
                    static const int a[24] = { MRevType, MRevSize, MRevDamp, MRevWidth, MRevLowCut, MRevPre, MRevDuck, MRevLevel,
                                               MDelayType, MDelayTime, MDelayFb, MDelayTone,
                                               MPingPong, MDelayWow, MDelayDuck, MDelayToRev, MDelayLevel, MFxLevel, MFxDuck, -1, -1, -1, -1, -1 };
                    return a[slot] >= 0 ? Target { KMaster, a[slot] } : Target {};
                }
            }
        case ModeProject:
        {
            if (projectPage == 1) return slot < (int) lookDefs().size() ? Target { KLook, slot } : Target {};
            static const int map[24] = { MTempo, MSwing, MKey, MScale, MMetronome, MVolume, MDj, MPads, MOctave, -1, -1, MReset,
                                         MGlue, MTape, MWidth, -1, MRevSize, MRevDamp, MDelayTime, MDelayFb, -1, -1, -1, -1 };
            return map[slot] >= 0 ? Target { KMaster, map[slot] } : Target {};
        }
        default: return {};
    }
}

const ParamDef* Controller::targetDef (const Target& t) const
{
    switch (t.kind)
    {
        case KParam: { const auto& d = engineDef (cur().engine); return t.index < (int) d.params.size() ? &d.params[(size_t) t.index] : nullptr; }
        case KFx:    { const auto& d = noteFxDef (cur().fxType); return t.index < (int) d.params.size() ? &d.params[(size_t) t.index] : nullptr; }
        case KMaster: return &masterDefs()[(size_t) t.index];
        case KMix:    return &mixDefs()[(size_t) t.index];
        case KIns:
        {
            const auto& d = insertDef (cur().mods.inserts[(size_t) (t.index / insertParams)].type);
            const int k = t.index % insertParams;
            return k < (int) d.params.size() ? &d.params[(size_t) k] : nullptr;
        }
        case KInsType: return &insTypeDef();
        case KInsOn:   return &insOnDef();
        case KFxSel:   return &fxSelDef();
        case KMacro:   return &macroDef();
        case KModSet:  return &modSetDefs()[(size_t) t.index];
        case KModSel:  return &modSelDef();
        case KModField: return t.index == 0 ? &modSourceDef() : t.index == 2 ? &modAmountDef() : nullptr;
        case KModAmt:  return &modAmountDef();
        case KLook:    return &lookDefs()[(size_t) t.index];
        case KSendC:
        {
            static const std::array<ParamDef, numTracks> d = [] { std::array<ParamDef, numTracks> a; for (int i = 0; i < numTracks; ++i) { a[(size_t) i] = defs::pct ("sc", "", 0.0f); a[(size_t) i].name = "FX " + juce::String (i + 1); } return a; }();
            return &d[(size_t) juce::jlimit (0, numTracks - 1, t.index)];
        }
        case KSendFx:
        {
            const auto& d = insertDef (project.m.fxSend.type);
            return t.index < (int) d.params.size() ? &d.params[(size_t) t.index] : nullptr;
        }
        default: return nullptr;
    }
}

float Controller::targetValue (const Target& t) const
{
    const auto& m = project.m;
    switch (t.kind)
    {
        case KParam: return cur().params[(size_t) t.index];
        case KFx:    return cur().fx[(size_t) t.index];
        case KIns:   return cur().mods.inserts[(size_t) (t.index / insertParams)].p[(size_t) (t.index % insertParams)];
        case KInsType: return (float) cur().mods.inserts[(size_t) t.index].type;
        case KInsOn: return cur().mods.inserts[(size_t) t.index].on ? 1.0f : 0.0f;
        case KFxSel: return (float) (fxSlot[(size_t) track.load()] + 1);
        case KMacro: return cur().mods.macros[(size_t) t.index];
        case KModSet: return cur().mods.set[(size_t) t.index];
        case KModSel: return (float) (modSlot[(size_t) track.load()] + 1);
        case KModField:
        {
            const auto& sl = cur().mods.slots[(size_t) modSlot[(size_t) track.load()]];
            return t.index == 0 ? (float) sl.source : t.index == 2 ? sl.amount : (float) sl.dest;
        }
        case KModAmt: return cur().mods.slots[(size_t) t.index].amount;
        case KSendC: return project.tracks[(size_t) t.index].sendC;
        case KSendFx: return project.m.fxSend.p[(size_t) t.index];
        case KLook:
            switch (t.index)
            {
                case 0: return (float) look.theme; case 1: return (float) look.accent; case 2: return (float) look.textSize;
                case 3: return (float) look.knobStyle; case 4: return look.trackColours ? 1.0f : 0.0f; default: return look.hintLine ? 1.0f : 0.0f;
            }
        case KMix:
        {
            const auto& tr = project.tracks[(size_t) (t.index / 4)];
            switch (t.index % 4) { case 0: return tr.volume; case 1: return tr.pan; case 2: return tr.sendA; default: return tr.sendB; }
        }
        case KMaster:
            switch (t.index)
            {
                case MVolume: return m.volume;       case MGlue: return m.glue;          case MTape: return m.tape;       case MWidth: return m.width;
                case MRevSize: return m.revSize;     case MRevDamp: return m.revDamp;    case MDelayTime: return (float) m.delayTime;
                case MDelayFb: return m.delayFb;     case MRevLowCut: return m.revLowCut; case MRevWidth: return m.revWidth;
                case MDelayTone: return m.delayTone; case MPingPong: return m.delayPingPong;
                case MTempo: return m.tempo;         case MSwing: return m.swing;        case MKey: return (float) m.key;
                case MScale: return (float) m.scale; case MMetronome: return m.metronome ? 1.0f : 0.0f;
                case MDj: return m.djFilter * 2.0f - 1.0f;
                case MPads: return scalePads.load() ? 1.0f : 0.0f;
                case MOctave: return (float) octave[(size_t) track.load()].load();
                case MReset: return resetProgress;
                case MRevType: return (float) m.revType;   case MRevPre: return m.revPre;     case MRevDuck: return m.revDuck;   case MRevLevel: return m.revLevel;
                case MDelayType: return (float) m.delayType; case MDelayWow: return m.delayWow; case MDelayDuck: return m.delayDuck;
                case MDelayToRev: return m.delayToRev;     case MDelayLevel: return m.delayLevel;
                case MFxType: return (float) m.fxSend.type; case MFxLevel: return m.fxLevel;  case MFxDuck: return m.fxDuck;
                default: return 0.0f;
            }
        default: return 0.0f;
    }
}

void Controller::changeTarget (const Target& t, float amount, bool fraction)
{
    if (t.kind == KBrowse)
    {
        const float steps = fraction ? amount * 24.0f : amount;
        static float acc = 0.0f;
        acc += steps;
        const int d = (int) acc;
        if (d == 0) return;
        acc -= (float) d;
        if (t.index == 0)
        {
            browseEngine = (browseEngine + d % NumEngineTypes + NumEngineTypes) % NumEngineTypes;
            browseCat = 0; browsePos = 0;
        }
        else if (t.index == 1)
        {
            const int n = juce::jmax (1, browseCategories().size());
            browseCat = ((browseCat + d) % n + n) % n;
            browsePos = 0;
        }
        else
        {
            const int n = juce::jmax (1, browsePresets().size());
            browsePos = juce::jlimit (0, n - 1, browsePos + d);
        }
        browseApply (true);
        return;
    }
    if (t.kind == KModField && t.index == 1)   // the destination: steps through everything a slot can move
    {
        static float acc = 0.0f;
        acc += fraction ? amount * 40.0f : amount;
        const int d = (int) acc;
        if (d == 0) return;
        acc -= (float) d;
        std::vector<int> list { dest::none };
        const auto& ed = engineDef (cur().engine);
        for (int i = 0; i < (int) ed.params.size(); ++i) list.push_back (i);
        for (int k = 0; k < numInserts; ++k)
            for (int q = 0; q < (int) insertDef (cur().mods.inserts[(size_t) k].type).params.size(); ++q) list.push_back (dest::insert0 + k * insertParams + q);
        for (int k = 0; k < 5; ++k) list.push_back (dest::mix0 + k);
        auto& sl = cur().mods.slots[(size_t) modSlot[(size_t) track.load()]];
        int pos = 0;
        for (size_t i = 0; i < list.size(); ++i) if (list[i] == sl.dest) pos = (int) i;
        pos = juce::jlimit (0, (int) list.size() - 1, pos + d);
        pushUndo ("Modulation", true);
        sl.dest = list[(size_t) pos];
        if (sl.source == SrcNone && sl.dest != dest::none) sl.source = SrcLfo1;
        if (sl.amount == 0.0f && sl.dest != dest::none) sl.amount = 0.25f;
        changed();
        return;
    }
    if (t.kind == KChain)
    {
        static float acc = 0.0f;
        acc += fraction ? amount * 12.0f : amount;
        const int d = (int) acc;
        if (d == 0) return;
        acc -= (float) d;
        const int n = chainPresetNames().size();
        chainIdx = ((chainIdx + d) % n + n) % n;
        pushUndo ("FX chain", true);
        applyChainPreset (cur().mods, chainIdx);
        for (auto& sl : cur().mods.slots) if (dest::isInsert (sl.dest)) sl = ModSlot();
        setFlash ("FX chain: " + chainPresetNames()[chainIdx]);
        changed();
        return;
    }
    if (t.kind == KSendFxPreset)
    {
        static float acc = 0.0f;
        acc += fraction ? amount * 12.0f : amount;
        const int d = (int) acc;
        if (d == 0) return;
        acc -= (float) d;
        auto& in = project.m.fxSend;
        const auto& idef = insertDef (in.type);
        if (idef.presets.empty()) return;
        const int np = (int) idef.presets.size();
        sendFxPreset = ((sendFxPreset + d) % np + np) % np;
        idef.applyPreset (sendFxPreset, in.p.data());
        for (size_t i = 0; i < idef.params.size(); ++i) if (idef.params[i].id == "mix") in.p[i] = 1.0f;
        setFlash ("FX send: " + idef.name + " - " + idef.presets[(size_t) sendFxPreset].name);
        changed();
        return;
    }
    if (t.kind == KInsPreset)
    {
        static float acc = 0.0f;
        acc += fraction ? amount * 12.0f : amount;
        const int d = (int) acc;
        if (d == 0) return;
        acc -= (float) d;
        auto& in = cur().mods.inserts[(size_t) t.index];
        const auto& idef = insertDef (in.type);
        if (idef.presets.empty()) return;
        auto& pi = insPreset[(size_t) track.load()];
        pi = ((pi + d) % (int) idef.presets.size() + (int) idef.presets.size()) % (int) idef.presets.size();
        pushUndo ("FX preset", true);
        idef.applyPreset (pi, in.p.data());
        setFlash (idef.name + ": " + idef.presets[(size_t) pi].name);
        changed();
        return;
    }
    const auto* def = targetDef (t);
    if (def == nullptr) return;
    float v = targetValue (t);
    if (def->stepped())
    {
        static std::array<float, 64> acc {};
        auto& a = acc[(size_t) ((t.kind * 13 + t.index) % 64)];
        a += fraction ? amount * (def->hi - def->lo + 1.0f) : amount;
        const int d = (int) a;
        if (d == 0) return;
        a -= (float) d;
        v = def->clamp (v + (float) d);
        if (def->unit == ParamDef::Choice && (t.kind == KParam || t.kind == KFx)) {}   // choices stop at the ends
    }
    else
    {
        const float n = def->toNorm (v) + (fraction ? amount : amount * 0.01f);
        v = def->fromNorm (n);
    }
    auto& m = project.m;
    switch (t.kind)
    {
        case KParam:
            if (audio.isRecording() && audio.isPlaying())   // recording: the knob move goes into the step that plays now
            {
                const int st = audio.stepNow[(size_t) track.load()].load();
                if (st >= 0)
                {
                    pushUndo ("Knob recording", true);
                    // continue from what the step already has (turning again moves on from there)
                    float from = cur().params[(size_t) t.index];
                    for (const auto& l : cur().pat.locks[(size_t) st]) if (l.param == t.index) from = l.value;
                    const auto* d = targetDef (t);
                    const float nv = d->stepped() ? d->clamp (from + (v - cur().params[(size_t) t.index]))
                                                  : d->fromNorm (d->toNorm (from) + (d->toNorm (v) - d->toNorm (cur().params[(size_t) t.index])));
                    cur().pat.setLock (st, t.index, nv);
                    changed();
                    setFlash ("Recorded on step " + juce::String (st + 1) + ": " + d->name + " " + d->format (nv));
                    return;
                }
            }
            pushUndo ("Sound edit", true); cur().params[(size_t) t.index] = v; edited[(size_t) track.load()] = true; break;
        case KFx:    cur().fx[(size_t) t.index] = v; break;
        case KIns:   pushUndo ("FX edit", true); cur().mods.inserts[(size_t) (t.index / insertParams)].p[(size_t) (t.index % insertParams)] = v; break;
        case KInsType:
        {
            pushUndo ("FX", true);
            auto& in = cur().mods.inserts[(size_t) t.index];
            const int nt = juce::roundToInt (v);
            if (nt == in.type) return;
            in.type = nt;
            in.on = true;
            insertDef (nt).defaults (in.p.data());
            for (auto& sl : cur().mods.slots)   // modulation of the old effect's knobs goes
                if (dest::isInsert (sl.dest) && (sl.dest - dest::insert0) / insertParams == t.index) sl = ModSlot();
            fxSlot[(size_t) track.load()] = t.index;
            insPreset[(size_t) track.load()] = 0;
            break;
        }
        case KInsOn: cur().mods.inserts[(size_t) t.index].on = v > 0.5f; break;
        case KFxSel: fxSlot[(size_t) track.load()] = juce::jlimit (0, numInserts - 1, juce::roundToInt (v) - 1); insPreset[(size_t) track.load()] = 0; if (onChange) onChange(); return;
        case KModSel: modSlot[(size_t) track.load()] = juce::jlimit (0, numModSlots - 1, juce::roundToInt (v) - 1); if (onChange) onChange(); return;
        case KMacro: cur().mods.macros[(size_t) t.index] = v; break;
        case KModSet: pushUndo ("Modulation", true); cur().mods.set[(size_t) t.index] = v; break;
        case KModField:
        {
            pushUndo ("Modulation", true);
            auto& sl = cur().mods.slots[(size_t) modSlot[(size_t) track.load()]];
            if (t.index == 0) sl.source = juce::roundToInt (v); else sl.amount = v;
            break;
        }
        case KModAmt: pushUndo ("Modulation", true); cur().mods.slots[(size_t) t.index].amount = v; break;
        case KSendC: project.tracks[(size_t) t.index].sendC = v; break;
        case KSendFx: project.m.fxSend.p[(size_t) t.index] = v; break;
        case KLook:
        {
            auto l = look;
            switch (t.index)
            {
                case 0: l.theme = juce::roundToInt (v); break; case 1: l.accent = juce::roundToInt (v); break; case 2: l.textSize = juce::roundToInt (v); break;
                case 3: l.knobStyle = juce::roundToInt (v); break; case 4: l.trackColours = v > 0.5f; break; default: l.hintLine = v > 0.5f; break;
            }
            setLook (l);
            return;
        }
        case KMix:
        {
            auto& tr = project.tracks[(size_t) (t.index / 4)];
            switch (t.index % 4) { case 0: tr.volume = v; break; case 1: tr.pan = v; break; case 2: tr.sendA = v; break; default: tr.sendB = v; break; }
            break;
        }
        case KMaster:
            switch (t.index)
            {
                case MVolume: m.volume = v; break;       case MGlue: m.glue = v; break;          case MTape: m.tape = v; break;     case MWidth: m.width = v; break;
                case MRevSize: m.revSize = v; break;     case MRevDamp: m.revDamp = v; break;    case MDelayTime: m.delayTime = juce::roundToInt (v); break;
                case MDelayFb: m.delayFb = v; break;     case MRevLowCut: m.revLowCut = v; break; case MRevWidth: m.revWidth = v; break;
                case MDelayTone: m.delayTone = v; break; case MPingPong: m.delayPingPong = v; break;
                case MTempo: m.tempo = v; break;         case MSwing: m.swing = v; break;        case MKey: m.key = juce::roundToInt (v); break;
                case MScale: m.scale = juce::roundToInt (v); break;
                case MMetronome: m.metronome = v > 0.5f; break;
                case MDj: m.djFilter = (v + 1.0f) * 0.5f; break;
                case MPads: scalePads = v > 0.5f; break;
                case MRevType: m.revType = juce::roundToInt (v); break;   case MRevPre: m.revPre = v; break;   case MRevDuck: m.revDuck = v; break;
                case MRevLevel: m.revLevel = v; break;                    case MDelayType: m.delayType = juce::roundToInt (v); break;
                case MDelayWow: m.delayWow = v; break;                    case MDelayDuck: m.delayDuck = v; break;
                case MDelayToRev: m.delayToRev = v; break;                case MDelayLevel: m.delayLevel = v; break;
                case MFxLevel: m.fxLevel = v; break;                      case MFxDuck: m.fxDuck = v; break;
                case MFxType:
                {
                    const int nt = juce::roundToInt (v);
                    if (nt == m.fxSend.type) return;
                    m.fxSend.type = nt;
                    m.fxSend.on = true;
                    insertDef (nt).applyPreset (0, m.fxSend.p.data());
                    // a send is all effect: the mix knob (if it has one) goes up
                    const auto& d = insertDef (nt);
                    for (size_t i = 0; i < d.params.size(); ++i) if (d.params[i].id == "mix") m.fxSend.p[i] = 1.0f;
                    sendFxPreset = 0;
                    break;
                }
                case MOctave: octave[(size_t) track.load()] = juce::roundToInt (v); break;
                case MReset:
                    resetProgress = juce::jlimit (0.0f, 100.0f, resetProgress + (v - resetProgress) * 4.0f);   // 25 clicks all the way
                    v = resetProgress;
                    resetTouched = juce::Time::getMillisecondCounter();
                    if (v >= 100.0f) { resetProgress = 0.0f; factoryReset(); return; }
                    if (onChange) onChange();
                    return;
                default: break;
            }
            break;
        default: break;
    }
    changed();
}

void Controller::turnKnob (int k, float clicks)
{
    if (k < 0 || k >= numKnobs || clicks == 0.0f) return;
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "knob"); o->setProperty ("k", k); o->setProperty ("c", clicks); sendRemote (o); return; }
    const int slot = k + (shift.load() ? numKnobs : 0);
    if (mode.load() == ModeSeq) { seqKnob (slot, clicks); return; }
    if (assignPending >= 0 && tryAssign (targetFor (slot))) return;
    changeTarget (targetFor (slot), clicks, false);
}

void Controller::turnKnobBy (int k, float fraction)
{
    if (k < 0 || k >= numKnobs || fraction == 0.0f) return;
    if (isRemote()) { turnKnob (k, fraction * 100.0f); return; }
    const int slot = k + (shift.load() ? numKnobs : 0);
    if (mode.load() == ModeSeq) { seqKnob (slot, fraction * 48.0f); return; }
    if (assignPending >= 0 && tryAssign (targetFor (slot))) return;
    changeTarget (targetFor (slot), fraction, true);
}

void Controller::resetKnob (int k)
{
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "reset"); o->setProperty ("k", k); sendRemote (o); return; }
    const auto t = targetFor (k + (shift.load() ? numKnobs : 0));
    const auto* def = targetDef (t);
    if (def == nullptr) return;
    const float cur0 = targetValue (t);
    if (def->stepped()) changeTarget (t, def->def - cur0, false);
    else changeTarget (t, def->toNorm (def->def) - def->toNorm (cur0), true);
}

KnobView Controller::knob (int k) const
{
    KnobView v;
    if (k < 0 || k >= numKnobs) return v;
    const int slot = k + (shift.load() ? numKnobs : 0);
    if (mode.load() == ModeSeq) return seqKnobView (slot);
    const auto t = targetFor (slot);
    if (t.kind == KBrowse)
    {
        v.used = true;
        if (t.index == 0) { v.name = "Engine"; v.value = engineDef (browseEngine).name; v.norm = (float) browseEngine / (NumEngineTypes - 1); }
        else if (t.index == 1) { const auto c = browseCategories(); v.name = "Category"; v.value = c[browseCat]; v.norm = c.size() > 1 ? (float) browseCat / (c.size() - 1) : 0.0f; }
        else { const auto l = browsePresets(); v.name = "Sound"; v.value = l[browsePos]; v.norm = l.size() > 1 ? (float) browsePos / (l.size() - 1) : 0.0f; }
        return v;
    }
    if (t.kind == KModField && t.index == 1)
    {
        const auto& sl = cur().mods.slots[(size_t) modSlot[(size_t) track.load()]];
        v.used = true; v.name = "Destination"; v.value = sl.dest == dest::none ? juce::String ("-") : destName (track.load(), sl.dest, true);
        v.norm = sl.dest == dest::none ? 0.0f : 0.5f;
        return v;
    }
    if (t.kind == KChain)
    {
        v.used = true; v.name = "Whole chain"; v.value = chainPresetNames()[chainIdx];
        v.norm = (float) chainIdx / (float) juce::jmax (1, chainPresetNames().size() - 1);
        return v;
    }
    if (t.kind == KSendFxPreset)
    {
        const auto& idef = insertDef (project.m.fxSend.type);
        v.used = true; v.name = "FX preset";
        v.value = idef.presets.empty() ? juce::String ("-") : idef.presets[(size_t) juce::jlimit (0, (int) idef.presets.size() - 1, sendFxPreset)].name;
        v.norm = idef.presets.size() > 1 ? (float) sendFxPreset / (float) (idef.presets.size() - 1) : 0.0f;
        return v;
    }
    if (t.kind == KInsPreset)
    {
        const auto& idef = insertDef (cur().mods.inserts[(size_t) t.index].type);
        v.used = true; v.name = "Preset";
        v.value = idef.presets.empty() ? juce::String ("-") : idef.presets[(size_t) juce::jlimit (0, (int) idef.presets.size() - 1, insPreset[(size_t) track.load()])].name;
        v.norm = idef.presets.size() > 1 ? (float) insPreset[(size_t) track.load()] / (float) (idef.presets.size() - 1) : 0.0f;
        return v;
    }
    const auto* def = targetDef (t);
    if (def == nullptr) return v;
    const float val = targetValue (t);
    v.used = true;
    v.name = def->name;
    if (t.kind == KInsType) v.name = "FX " + juce::String (t.index + 1);
    if (t.kind == KInsOn) v.name = "FX " + juce::String (t.index + 1) + " on";
    if (t.kind == KIns && soundExtraPage() == ExtraMy) v.name = destName (track.load(), dest::insert0 + t.index, true);
    if (t.kind == KMacro)
    {
        v.name = "Macro " + juce::String (t.index + 1);
        for (const auto& sl : cur().mods.slots)
            if (sl.active() && sl.source == SrcMacro1 + t.index) { v.name = "M" + juce::String (t.index + 1) + " " + destName (track.load(), sl.dest, true); break; }
    }
    if (t.kind == KModAmt)
    {
        const auto& sl = cur().mods.slots[(size_t) t.index];
        v.name = juce::String (t.index + 1) + " " + (sl.source == SrcNone || sl.dest == dest::none ? juce::String ("(empty)")
                                                     : modSourceNames()[sl.source].removeCharacters (" ") + ">" + destName (track.load(), sl.dest, true));
    }
    if (t.kind == KModSel) { v.value = modSlotText (track.load(), modSlot[(size_t) track.load()]); }
    v.value = def->format (val);
    if (t.kind == KMaster && t.index == MTempo) v.value = juce::String (val, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " BPM";
    if (t.kind == KMaster && t.index == MReset) v.value = resetProgress > 0.0f ? juce::String (juce::roundToInt (resetProgress)) + "% keep turning" : juce::String ("turn right");
    if (t.kind == KMaster && t.index == MWidth) v.value = juce::String (juce::roundToInt (val * 200.0f)) + "%";
    v.norm = def->toNorm (val);
    v.bipolar = def->unit == ParamDef::BiPct || (def->unit == ParamDef::Semi && def->lo < 0) || (def->unit == ParamDef::Cents && def->lo < 0)
             || (def->unit == ParamDef::Db && def->lo < 0 && def->hi > 0 && def->id != "volume");
    return v;
}

juce::StringArray Controller::pageNames() const
{
    juce::StringArray n;
    switch (mode.load())
    {
        case ModeSound: for (const auto& p : engineDef (cur().engine).pages) n.add (p.name); n.add ("FX"); n.add ("MACRO"); n.add ("MOD"); n.add ("MY"); break;
        case ModeBrowse: n.add ("BROWSE"); break;
        case ModeSeq: { const int bars = juce::jmax (1, (cur().pat.length + 15) / 16); for (int b = 0; b < bars; ++b) n.add ("BAR " + juce::String (b + 1)); break; }
        case ModeFx: for (const auto& s : noteFxNames()) n.add (s.toUpperCase()); break;
        case ModeMix: n.add ("LEVEL"); n.add ("SENDS"); n.add ("FX SEND"); n.add ("RETURNS"); break;
        case ModeProject: n.add ("PROJECT"); n.add ("LOOK"); break;
        case ModeMenu: n.add ("MENU"); break;
        default: break;
    }
    return n;
}

int Controller::pageIndex() const
{
    switch (mode.load())
    {
        case ModeSound: return soundPage[(size_t) track.load()];
        case ModeSeq: return bar[(size_t) track.load()];
        case ModeFx: return cur().fxType;
        case ModeMix: return mixPage;
        case ModeProject: return projectPage;
        default: return 0;
    }
}

int Controller::numPagesNow() const { return pageNames().size(); }

juce::String Controller::pageName() const
{
    const auto n = pageNames();
    return n[juce::jlimit (0, juce::jmax (0, n.size() - 1), pageIndex())];
}

juce::String Controller::hint() const
{
    if (sceneMode.load()) return "Pads 1-8: scenes (Rec + pad = save) | pads 9-16: mute | Scene: back";
    if (assignPending >= 0) return assignPending == assignMy ? "Turn the knob you want on your MY page (any page, any mode) | Project: cancel"
                                                             : "Turn the knob macro " + juce::String (assignPending + 1) + " should move | Project: cancel";
    if (mode.load() == ModeSound)
        switch (soundExtraPage())
        {
            case ExtraFx:    return "Knob 1: FX slot 1-4 | 2: the effect | 3-10: its knobs | 11: on | 12: presets | Shift: 4 effects, on / off, whole chain";
            case ExtraMacro: return "Knobs 1-8: macros (MENU > More > Assign) | 9-12: LFOs | Shift: LFO, envelope, random";
            case ExtraMod:   return "Knob 1: slot | 2: source | 3: destination | 4: amount | 5-12: amounts 1-8 (Shift: 9-16)";
            case ExtraMy:    return "Your own knobs | MENU > More > Add to MY knobs, then turn any knob";
            default: break;
        }
    if (mode.load() == ModeProject && projectPage == 1) return "How the screen looks: theme, colour, text size, knobs, track colours";
    switch (mode.load())
    {
        case ModeSound:   if (cur().engine == EngineSampler) return "Rec: record a sample (microphone) | knob 1: the sample | Play: Keys / Chop / One shot";
                          return "Knobs: the sound | Prev / Next: pages | Shift: 12 more | Sound: browse sounds";
        case ModeBrowse:  return "Knob 1 engine | 2 category | 3 sound | Prev / Next: next sound | Rec: save as User | Sound: back";
        case ModeSeq:     return cur().isDrums() ? "Hit a drum to pick it | pad = step on / off | knobs: the picked step | Shift + knobs: the pattern | Prev / Next: bar"
                                                 : (audio.isPlaying() ? "Pad = step on / off | knobs: the picked step | Shift + knobs: the pattern | Rec: record what you play"
                                                                       : "Play keys: they fill the steps one by one | pad = step on / off | knobs: the picked step | Shift + knobs: pattern");
        case ModeFx:      return "Prev / Next: Off / Arp / Chords / Orbit | Shift + Prev / Next: presets | play the keys";
        case ModeMix:
            switch (mixPage)
            {
                case 1:  return "Knobs 1-8: reverb send (Shift: delay send) | 9-12: reverb / delay type and size, time | Next: FX send";
                case 2:  return "Knobs 1-8: FX send per track | 9: the effect | 10: preset | 11: level | 12: duck | Shift: its knobs";
                case 3:  return "Reverb: type, size, damp, width, low cut, pre-delay, duck, level | delay: type, time ... (Shift: more)";
                default: return "Knobs: levels (Shift: pan) | Prev / Next: sends | pads 1-8 mute, 9-16 solo";
            }
        case ModeProject: return "Tempo, key, scale ... | Project again = MENU (save, load, patterns, help)";
        case ModeMenu:    return menuPage == 0 ? "A pad (or a touch) = do it | red = press again to be sure | Project = back" : "Pick with a pad | pad 16 = back";
        default: return {};
    }
}

//==============================================================================
// The sequencer
bool Controller::isStepHeld (int step) const
{
    for (int i = 0; i < numHeldSteps; ++i) if (heldSteps[(size_t) i] == step) return true;
    return false;
}

int Controller::heldStepCount() const { return numHeldSteps; }

int Controller::stepTargets (std::array<int, maxSteps>& out) const
{
    if (numHeldSteps > 0) { for (int i = 0; i < numHeldSteps; ++i) out[(size_t) i] = heldSteps[(size_t) i]; return numHeldSteps; }
    const int s = selStep[(size_t) track.load()];
    if (s >= 0 && s < cur().pat.length) { out[0] = s; return 1; }
    return 0;
}

void Controller::selectStep (int step)
{
    selStep[(size_t) track.load()] = step;
    inputWritten = false;   // step input continues from here
}

void Controller::stepTap (int step)
{
    // tap an empty step: on (and picked); tap a step that is on: picked; tap the picked one again: off
    pushUndo ("Steps", true);
    auto& tr = cur();
    auto& pat = tr.pat;
    if (step >= pat.length) return;
    const int t = track.load();
    if (tr.isDrums())
    {
        auto& v = pat.drums[(size_t) lane[(size_t) t]][(size_t) step];
        if (v == 0) { v = (uint8_t) juce::jlimit (1, 127, juce::jmax (90, lastVel)); selectStep (step); }
        else if (selStep[(size_t) t] != step) selectStep (step);
        else { v = 0; pat.rolls[(size_t) lane[(size_t) t]][(size_t) step] = 0; selectStep (-1); }
    }
    else
    {
        auto& st = pat.steps[(size_t) step];
        if (! st.on())
        {
            st.clear();
            st.notes[0] = (int8_t) lastNote;
            st.vel = (uint8_t) juce::jmax (90, lastVel);
            st.len = 75;
            selectStep (step);
        }
        else if (selStep[(size_t) t] != step) selectStep (step);
        else { st.clear(); selectStep (-1); }
    }
    changed();
}

void Controller::stepInput (int note, int velocity)
{
    // SEQ while stopped: every key goes into the next step (keys played together = a chord)
    auto& tr = cur();
    auto& pat = tr.pat;
    const int t = track.load();
    int cursor = selStep[(size_t) t];
    if (cursor < 0 || cursor >= pat.length) { cursor = juce::jmin (bar[(size_t) t] * 16, pat.length - 1); inputWritten = false; }
    if (inputKeysDown == 0 && inputWritten) cursor = (cursor + 1) % pat.length;
    pushUndo ("Steps", true);
    auto& st = pat.steps[(size_t) cursor];
    if (inputKeysDown == 0)
    {
        st.clear();
        st.notes[0] = (int8_t) note;
        st.vel = (uint8_t) juce::jlimit (1, 127, velocity);
        st.len = 75;
    }
    else st.add (note);
    selStep[(size_t) t] = cursor;
    bar[(size_t) t] = cursor / 16;
    inputWritten = true;
    changed();
}

void Controller::stepKey (int note, int velocity)
{
    pushUndo ("Steps", true);
    auto& tr = cur();
    if (tr.isDrums())
    {
        const int l = ((note - 36) % numLanes + numLanes) % numLanes;
        for (int i = 0; i < numHeldSteps; ++i) tr.pat.drums[(size_t) l][(size_t) heldSteps[(size_t) i]] = (uint8_t) juce::jlimit (1, 127, velocity);
    }
    else
        for (int i = 0; i < numHeldSteps; ++i)
        {
            auto& st = tr.pat.steps[(size_t) heldSteps[(size_t) i]];
            if (! stepEdited || ! st.on()) { st.notes = { (int8_t) note, -1, -1, -1 }; st.vel = (uint8_t) velocity; if (st.len == 0) st.len = 75; }
            else st.add (note);   // more keys while holding = a chord
        }
    stepEdited = true;
    changed();
}

void Controller::seqKnob (int slot, float clicks)
{
    pushUndo ("Steps", true);
    auto& tr = cur();
    auto& pat = tr.pat;
    const int t = track.load();
    static float acc[24] {};
    auto stepsOf = [&clicks] (float& a) { a += clicks; const int d = (int) a; a -= (float) d; return d; };
    const int d = stepsOf (acc[slot]);

    std::array<int, maxSteps> targets {};
    const int numTargets = slot < numKnobs ? stepTargets (targets) : 0;
    if (numTargets > 0)   // edit the picked (or held) steps; Shift = the whole pattern
    {
        if (numHeldSteps > 0) stepEdited = true;
        for (int i = 0; i < numTargets; ++i)
        {
            const int s = targets[(size_t) i];
            if (tr.isDrums())
            {
                auto& v = pat.drums[(size_t) lane[(size_t) t]][(size_t) s];
                if (slot == 0 || slot == 1) v = (uint8_t) juce::jlimit (v > 0 ? 1 : 0, 127, (int) v + d * 4);
                if (slot == 2) { auto& rl = pat.rolls[(size_t) lane[(size_t) t]][(size_t) s]; rl = (uint8_t) juce::jlimit (1, 4, juce::jmax (1, (int) rl) + d); }
                continue;
            }
            auto& st = pat.steps[(size_t) s];
            if (! st.on() && slot != 0) continue;
            switch (slot)
            {
                case 0:  if (! st.on()) { st.notes[0] = (int8_t) lastNote; break; }
                         for (auto& n : st.notes) if (n >= 0) n = (int8_t) music::moveInScale (n, project.m.key, project.m.scale, d);
                         lastNote = st.notes[0];
                         break;
                case 11: for (auto& n : st.notes) if (n >= 0) n = (int8_t) juce::jlimit (0, 127, n + d);   // semitones
                         lastNote = st.notes[0];
                         break;
                case 1:  st.vel = (uint8_t) juce::jlimit (1, 127, (int) st.vel + d * 3); break;
                case 2:  st.len = (uint16_t) juce::jlimit (10, 1600, (int) st.len + d * (st.len >= 100 ? 25 : 5)); break;
                case 3:  if (d != 0) st.slide = d > 0; break;
                case 4:  if (d != 0) st.accent = d > 0; break;
                case 5:  st.chance = (uint8_t) juce::jlimit (0, 100, (int) st.chance + d * 5); break;
                case 6:  st.nudge = (int8_t) juce::jlimit (-50, 50, (int) st.nudge + d * 2); break;
                case 10: st.ratchet = (uint8_t) juce::jlimit (1, 4, (int) st.ratchet + d); break;
                case 7: case 8: case 9:
                {
                    auto& n = st.notes[(size_t) (slot - 6)];
                    if (n < 0 && d > 0) n = (int8_t) music::moveInScale (st.notes[0], project.m.key, project.m.scale, 2 * (slot - 6));
                    else if (n >= 0)
                    {
                        const int nn = music::moveInScale (n, project.m.key, project.m.scale, d);
                        n = nn <= st.notes[0] ? (int8_t) -1 : (int8_t) nn;   // turned below the root: gone
                    }
                    break;
                }
                default: break;
            }
        }
        changed();
        return;
    }
    // nothing picked (or Shift): the whole pattern
    slot %= numKnobs;
    switch (slot)
    {
        case 0: pat.length = juce::jlimit (1, maxSteps, pat.length + d); bar[(size_t) t] = juce::jmin (bar[(size_t) t], (pat.length - 1) / 16); break;
        case 1: pat.rate = juce::jlimit (0, NumRates - 1, pat.rate + d); break;
        case 2: project.m.swing = juce::jlimit (0.0f, 0.75f, project.m.swing + d * 0.01f); break;
        case 3:   // transpose in the scale
            if (! tr.isDrums())
                for (auto& st : pat.steps) for (auto& n : st.notes) if (n >= 0) n = (int8_t) music::moveInScale (n, project.m.key, project.m.scale, d);
            break;
        case 4:   // rotate
            if (d != 0)
            {
                const int len = pat.length;
                auto rot = [len, d] (auto& arr) { auto copy = arr; for (int i = 0; i < len; ++i) arr[(size_t) (((i + d) % len + len) % len)] = copy[(size_t) i]; };
                rot (pat.steps);
                rot (pat.locks);
                for (auto& l : pat.drums) rot (l);
                for (auto& l : pat.rolls) rot (l);
            }
            break;
        case 5:   // all velocities
            for (auto& st : pat.steps) if (st.on()) st.vel = (uint8_t) juce::jlimit (1, 127, (int) st.vel + d * 3);
            for (auto& l : pat.drums) for (auto& v : l) if (v > 0) v = (uint8_t) juce::jlimit (1, 127, (int) v + d * 3);
            break;
        case 6:   // all lengths
            for (auto& st : pat.steps) if (st.on()) st.len = (uint16_t) juce::jlimit (10, 1600, (int) st.len + d * 5);
            break;
        case 7:
            if (tr.isDrums()) lane[(size_t) t] = (lane[(size_t) t] + d + numLanes * 8) % numLanes;
            else for (auto& st : pat.steps) for (auto& n : st.notes) if (n >= 0) n = (int8_t) juce::jlimit (0, 127, n + 12 * d);
            break;
        default: return;
    }
    changed();
}

KnobView Controller::seqKnobView (int slot) const
{
    KnobView v;
    const auto& tr = cur();
    const auto& pat = tr.pat;
    const int t = track.load();
    std::array<int, maxSteps> targets {};
    if (slot < numKnobs && stepTargets (targets) > 0)
    {
        const int s = targets[0];
        if (tr.isDrums())
        {
            if (slot == 2)
            {
                const int rl = juce::jmax (1, (int) pat.rolls[(size_t) lane[(size_t) t]][(size_t) s]);
                v.used = true; v.name = "Roll"; v.value = rl > 1 ? juce::String (rl) + " hits" : juce::String ("off"); v.norm = (rl - 1) / 3.0f;
                return v;
            }
            if (slot > 1) return v;
            const int vel = pat.drums[(size_t) lane[(size_t) t]][(size_t) s];
            v.used = true; v.name = slot == 0 ? "Step " + juce::String (s + 1) : juce::String ("Velocity"); v.value = vel > 0 ? juce::String (vel) + (vel >= 120 ? " accent" : "") : juce::String ("off"); v.norm = vel / 127.0f;
            return v;
        }
        const auto& st = pat.steps[(size_t) s];
        v.used = slot <= 11;
        switch (slot)
        {
            case 11: v.name = "Semitone"; v.value = st.on() ? music::noteName (st.notes[0]) : juce::String ("off"); v.norm = 0.5f; v.bipolar = true; break;
            case 0: v.name = "Step " + juce::String (s + 1) + " note"; v.value = st.on() ? music::noteName (st.notes[0]) : juce::String ("off"); v.norm = st.on() ? st.notes[0] / 127.0f : 0.0f; break;
            case 1: v.name = "Velocity"; v.value = juce::String ((int) st.vel); v.norm = st.vel / 127.0f; break;
            case 2: v.name = "Length"; v.value = st.len >= 100 ? juce::String (st.len / 100.0f, 2) + " steps" : juce::String ((int) st.len) + "%"; v.norm = juce::jmin (1.0f, st.len / 400.0f); break;
            case 3: v.name = "Slide"; v.value = st.slide ? "On" : "Off"; v.norm = st.slide ? 1.0f : 0.0f; break;
            case 4: v.name = "Accent"; v.value = st.accent ? "On" : "Off"; v.norm = st.accent ? 1.0f : 0.0f; break;
            case 5: v.name = "Chance"; v.value = juce::String ((int) st.chance) + "%"; v.norm = st.chance / 100.0f; break;
            case 6: v.name = "Nudge"; v.value = juce::String ((int) st.nudge) + "%"; v.norm = 0.5f + st.nudge / 100.0f; v.bipolar = true; break;
            case 10: v.name = "Ratchet"; v.value = st.ratchet > 1 ? juce::String ((int) st.ratchet) + " x" : juce::String ("off"); v.norm = (st.ratchet - 1) / 3.0f; break;
            case 7: case 8: case 9:
            {
                const int n = st.notes[(size_t) (slot - 6)];
                v.name = "Note " + juce::String (slot - 5); v.value = n >= 0 ? music::noteName (n) : juce::String ("-"); v.norm = n >= 0 ? n / 127.0f : 0.0f;
                break;
            }
            default: break;
        }
        return v;
    }
    v.used = true;
    slot %= numKnobs;
    switch (slot)
    {
        case 0: v.name = "Length"; v.value = juce::String (pat.length) + " steps"; v.norm = pat.length / (float) maxSteps; break;
        case 1: v.name = "Rate"; v.value = rateName (pat.rate); v.norm = pat.rate / (float) (NumRates - 1); break;
        case 2: v.name = "Swing"; v.value = juce::String (juce::roundToInt (project.m.swing * 100.0f)) + "%"; v.norm = project.m.swing / 0.75f; break;
        case 3: v.name = "Transpose"; v.value = tr.isDrums() ? juce::String ("-") : juce::String ("turn"); v.norm = 0.5f; v.bipolar = true; v.used = ! tr.isDrums(); break;
        case 4: v.name = "Rotate"; v.value = "turn"; v.norm = 0.5f; v.bipolar = true; break;
        case 5: v.name = "All velocity"; v.value = "turn"; v.norm = 0.5f; v.bipolar = true; break;
        case 6: v.name = "All length"; v.value = "turn"; v.norm = 0.5f; v.bipolar = true; v.used = ! tr.isDrums(); break;
        case 7:
            if (tr.isDrums()) { v.name = "Drum"; v.value = juce::String (lane[(size_t) t] + 1) + " " + laneName (t, lane[(size_t) t]); v.norm = lane[(size_t) t] / 15.0f; }
            else { v.name = "Octave"; v.value = "turn"; v.norm = 0.5f; v.bipolar = true; }
            break;
        default: v.used = false; break;
    }
    return v;
}

//==============================================================================
// Pads and keys
int Controller::padNote (int t, int pad) const
{
    t = juce::jlimit (0, numTracks - 1, t);
    if (pad < 0 || pad >= numPads) return -1;
    if (project.tracks[(size_t) t].isDrums()) return 36 + pad;
    const int base = 12 * (octave[(size_t) t].load() + 1);
    if (scalePads.load())
    {
        const int degree = pad >= 8 ? pad - 8 : pad + 8;   // bottom row up first, then the top row
        return juce::jlimit (0, 127, music::noteOfDegree (project.m.key, project.m.scale, octave[(size_t) t].load() + 1, degree));
    }
    // like a piano: the bottom row the white keys, the top row the black keys above them
    static const int white[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
    static const int black[8] = { 1, 3, -1, 6, 8, 10, -1, 13 };
    const int off = pad >= 8 ? white[pad - 8] : black[pad];
    return off < 0 ? -1 : juce::jlimit (0, 127, base + off);
}

bool Controller::padLit (int pad) const
{
    if (pad < 0 || pad >= numPads) return false;
    if (sceneMode.load()) return pad < 8 ? scenes[(size_t) pad] != nullptr : ! project.tracks[(size_t) (pad - 8)].mute;
    if (mode.load() == ModeMix) return pad < 8 ? project.tracks[(size_t) pad].mute : project.tracks[(size_t) (pad - 8)].solo;
    if (mode.load() == ModeSeq)
    {
        const auto& tr = cur();
        const int step = bar[(size_t) track.load()] * 16 + pad;
        if (step >= tr.pat.length) return false;
        return tr.isDrums() ? tr.pat.drums[(size_t) lane[(size_t) track.load()]][(size_t) step] > 0 : tr.pat.steps[(size_t) step].on();
    }
    const int n = padNote (track.load(), pad);
    return n >= 0 && ! cur().isDrums() && ((n - project.m.key) % 12 + 12) % 12 == 0;   // the root of the key glows
}

void Controller::padDown (int pad, int velocity)
{
    if (pad < 0 || pad >= numPads) return;
    if (isRemote())
    {
        runOnMessageThread ([this, pad, velocity] { auto* o = new juce::DynamicObject(); o->setProperty ("t", "pad"); o->setProperty ("p", pad); o->setProperty ("v", velocity); o->setProperty ("down", true); sendRemote (o); });
        return;
    }
    const int m = mode.load();
    if (m == ModeMenu && ! sceneMode.load()) { runOnMessageThread ([this, pad] { menuPress (pad); }); return; }
    if (sceneMode.load() || m == ModeSeq || m == ModeMix || shift.load())
    {
        runOnMessageThread ([this, pad, velocity] { if (! padDownMessageThread (pad, velocity)) padDown (pad, velocity); });
        if (! (m == ModeSeq && shift.load() && cur().isDrums())) return;
    }
    // play the track right away (the MIDI thread, no detour)
    const int t = track.load();
    const int note = padNote (t, pad);
    if (note < 0) return;
    padHeld[(size_t) pad] = true;
    if (project.tracks[(size_t) t].isDrums()) runOnMessageThread ([this, t, pad] { lane[(size_t) t] = pad; });
    audio.post ({ Event::NoteOn, (int8_t) t, (int16_t) note, juce::jlimit (1, 127, velocity) / 127.0f });
    lastVel = juce::jlimit (1, 127, velocity);
    if (! project.tracks[(size_t) t].isDrums()) lastNote = note;
}

bool Controller::padDownMessageThread (int pad, int velocity)
{
    const int t = track.load();
    if (sceneMode.load())
    {
        if (pad < 8) { btnDown[BtnRec] ? saveScene (pad) : (void) loadScene (pad); }
        else setMute (pad - 8, ! project.tracks[(size_t) (pad - 8)].mute);
        return true;
    }
    if (mode.load() == ModeMix)
    {
        if (pad < 8) setMute (pad, ! project.tracks[(size_t) pad].mute);
        else setSolo (pad - 8, ! project.tracks[(size_t) (pad - 8)].solo);
        return true;
    }
    if (mode.load() == ModeSeq)
    {
        if (shift.load() && cur().isDrums()) { lane[(size_t) t] = pad; if (onChange) onChange(); return true; }   // pick the drum (and it plays)
        const int step = bar[(size_t) t] * 16 + pad;
        if (step >= cur().pat.length) { setFlash ("The pattern is " + juce::String (cur().pat.length) + " steps (knob 1)"); return true; }
        if (numHeldSteps == 0) stepEdited = false;
        if (numHeldSteps < maxSteps && ! isStepHeld (step)) heldSteps[(size_t) numHeldSteps++] = step;
        padHeld[(size_t) pad] = true;
        if (onChange) onChange();
        return true;
    }
    juce::ignoreUnused (velocity);
    return shift.load();   // Shift + pad: nothing yet (it doesn't play)
}

void Controller::padUp (int pad)
{
    if (pad < 0 || pad >= numPads) return;
    if (isRemote())
    {
        runOnMessageThread ([this, pad] { auto* o = new juce::DynamicObject(); o->setProperty ("t", "pad"); o->setProperty ("p", pad); o->setProperty ("down", false); sendRemote (o); });
        return;
    }
    if (mode.load() == ModeMenu && ! sceneMode.load()) return;
    if (mode.load() == ModeSeq && ! sceneMode.load())
    {
        runOnMessageThread ([this, pad]
        {
            const int t = track.load();
            const int step = bar[(size_t) t] * 16 + pad;
            if (! isStepHeld (step))
            {
                if (padHeld[(size_t) pad]) { padHeld[(size_t) pad] = false; audio.post ({ Event::NoteOff, (int8_t) t, (int16_t) padNote (t, pad), 0.0f }); }
                return;
            }
            const bool tap = ! stepEdited && numHeldSteps == 1;
            int n = 0;
            for (int i = 0; i < numHeldSteps; ++i) if (heldSteps[(size_t) i] != step) heldSteps[(size_t) n++] = heldSteps[(size_t) i];
            numHeldSteps = n;
            padHeld[(size_t) pad] = false;
            if (tap) stepTap (step);
            if (numHeldSteps == 0) stepEdited = false;
            if (onChange) onChange();
        });
        return;
    }
    if (! padHeld[(size_t) pad]) return;
    padHeld[(size_t) pad] = false;
    const int t = track.load();
    const int note = padNote (t, pad);
    if (note >= 0) audio.post ({ Event::NoteOff, (int8_t) t, (int16_t) note, 0.0f });
}

void Controller::keyDown (int note, int velocity)
{
    if (isRemote()) { runOnMessageThread ([this, note, velocity] { auto* o = new juce::DynamicObject(); o->setProperty ("t", "key"); o->setProperty ("n", note); o->setProperty ("v", velocity); o->setProperty ("down", true); sendRemote (o); }); return; }
    const int t = track.load();
    lastVel = juce::jlimit (1, 127, velocity);
    if (! project.tracks[(size_t) t].isDrums()) lastNote = juce::jlimit (0, 127, note);
    if (project.tracks[(size_t) t].isDrums())   // hitting a drum picks it (SEQ shows and edits that drum)
        runOnMessageThread ([this, t, note] { lane[(size_t) t] = ((note - 36) % numLanes + numLanes) % numLanes; if (onChange) onChange(); });
    if (mode.load() == ModeSeq && numHeldSteps > 0)
        runOnMessageThread ([this, note, velocity] { if (numHeldSteps > 0) stepKey (note, velocity); });
    else if (mode.load() == ModeSeq && ! audio.isPlaying() && ! project.tracks[(size_t) t].isDrums())
        runOnMessageThread ([this, note, velocity] { stepInput (note, velocity); ++inputKeysDown; });
    audio.post ({ Event::NoteOn, (int8_t) t, (int16_t) juce::jlimit (0, 127, note), juce::jlimit (1, 127, velocity) / 127.0f });
}

void Controller::keyUp (int note)
{
    if (isRemote()) { runOnMessageThread ([this, note] { auto* o = new juce::DynamicObject(); o->setProperty ("t", "key"); o->setProperty ("n", note); o->setProperty ("down", false); sendRemote (o); }); return; }
    runOnMessageThread ([this] { inputKeysDown = juce::jmax (0, inputKeysDown - 1); });
    // the note goes off on every track (the track may have changed while it was held)
    for (int t = 0; t < numTracks; ++t) audio.post ({ Event::NoteOff, (int8_t) t, (int16_t) juce::jlimit (0, 127, note), 0.0f });
}

void Controller::setSlider (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "slider"); o->setProperty ("v", v); sendRemote (o); return; }
    // a dead zone in the middle: the filter is really off
    project.m.djFilter = std::abs (v - 0.5f) < 0.03f ? 0.5f : v;
    changed();
}

void Controller::setJoystick (float x, float y)
{
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "joy"); o->setProperty ("x", x); o->setProperty ("y", y); sendRemote (o); return; }
    const int t = track.load();
    audio.post ({ Event::Bend, (int8_t) t, 0, juce::jlimit (-1.0f, 1.0f, x) * 2.0f });
    audio.post ({ Event::Mod, (int8_t) t, 0, juce::jlimit (-1.0f, 1.0f, y) });
}

void Controller::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (isLearning())
    {
        if (m.isController())
            runOnMessageThread ([this, ch = m.getChannel(), n = m.getControllerNumber(), v = m.getControllerValue()] { learnInput (ch, n, false, v); });
        else if (m.isNoteOn())
            runOnMessageThread ([this, ch = m.getChannel(), n = m.getNoteNumber(), v = (int) m.getVelocity()] { learnInput (ch, n, true, v); });
        return;
    }
    if (handleMapped (m)) return;

    if (m.getChannel() == controllerChannel)   // the box's own knobs, buttons, pads, slider and joystick
    {
        if (m.isNoteOn() && m.getNoteNumber() >= firstPadNote && m.getNoteNumber() < firstPadNote + numPads)
            padDown (m.getNoteNumber() - firstPadNote, m.getVelocity());
        else if (m.isNoteOff() && m.getNoteNumber() >= firstPadNote && m.getNoteNumber() < firstPadNote + numPads)
            padUp (m.getNoteNumber() - firstPadNote);
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (cc >= firstKnobCC && cc < firstKnobCC + numKnobs)
            {
                const int clicks = v == 0 || v == 64 ? 0 : (v < 64 ? v : v - 128);
                runOnMessageThread ([this, k = cc - firstKnobCC, clicks] { turnKnob (k, (float) clicks); });
            }
            else if (cc >= firstButtonCC && cc < firstButtonCC + NumButtons)
            {
                const int b = cc - firstButtonCC;
                const bool down = v >= 64;
                if (b == BtnShift) shift = down;   // right away: a pad may follow immediately
                runOnMessageThread ([this, b, down] { down ? buttonDown (b) : buttonUp (b); });
            }
            else if (cc == sliderCC) runOnMessageThread ([this, v] { setSlider (v / 127.0f); });
            else if (cc == joyXCC || cc == joyYCC)
            {
                static std::atomic<int> jx { 64 }, jy { 64 };
                (cc == joyXCC ? jx : jy) = v;
                setJoystick ((jx.load() - 64) / 63.0f, (jy.load() - 64) / 63.0f);
            }
        }
        else if (m.isPitchWheel()) audio.post ({ Event::Bend, (int8_t) track.load(), 0, (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f });
        return;
    }

    // any other MIDI keyboard: its keys play the track, its wheels bend and modulate
    if (m.isNoteOn()) keyDown (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff()) keyUp (m.getNoteNumber());
    else if (m.isPitchWheel()) audio.post ({ Event::Bend, (int8_t) track.load(), 0, (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f });
    else if (m.isController() && m.getControllerNumber() == 1) audio.post ({ Event::Mod, (int8_t) track.load(), 0, m.getControllerValue() / 127.0f });
    else if (m.isAllNotesOff() || m.isAllSoundOff()) audio.post ({ Event::AllOff, 0, 0, 0.0f });
}


//==============================================================================
// The MENU: 16 tiles (one per pad); red ones need a second press
namespace
{
enum MenuMain { MSave = 0, MLoad, MNew, MClearTrack, MClearAll, MCopyTrack, MDrumPattern, MBassLine,
                MClick, MPadsMode, MLearn, MHelp, MTap, MSaveSound, MUndo, MMore };
enum MenuMore { XPin = 0, XClearMy, XAssign, XClearMods, XRandomMods, XChain, XClearFx, XCopyFx, XLook, XTheme, XAccent, XText, XTrackColours, XInit, XClose };
enum MenuPage { PageMain = 0, PageSave, PageLoad, PageCopy, PageDrums, PageBass, PageMore, PageMacro, PageChain, PageCopyFx };
}

juce::String Controller::menuTitle() const
{
    switch (menuPage)
    {
        case PageSave:  return "SAVE THE PROJECT INTO";
        case PageLoad:  return "LOAD A PROJECT";
        case PageCopy:  return "COPY TRACK " + juce::String (track.load() + 1) + " TO";
        case PageDrums: return "DRUM PATTERN FOR TRACK " + juce::String (track.load() + 1);
        case PageBass:  return "BASS LINE FOR TRACK " + juce::String (track.load() + 1) + " (in " + music::keyName (project.m.key) + " " + music::scaleName (project.m.scale) + ")";
        case PageMore:  return "MORE - make it yours";
        case PageMacro: return "WHICH MACRO? (then turn a knob)";
        case PageChain: return "FX CHAIN FOR TRACK " + juce::String (track.load() + 1);
        case PageCopyFx: return "COPY THE FX OF TRACK " + juce::String (track.load() + 1) + " TO";
        default:        return "MENU";
    }
}

int Controller::menuConfirming() const
{
    return menuConfirm >= 0 && juce::Time::getMillisecondCounter() < menuConfirmUntil ? menuConfirm : -1;
}

std::array<Controller::MenuTile, numPads> Controller::menuTiles() const
{
    std::array<MenuTile, numPads> m;
    auto back = [&m] { m[15] = { "Back", "", true, false, false }; };
    switch (menuPage)
    {
        case PageSave:
        case PageLoad:
            for (int i = 0; i < 8; ++i)
            {
                const bool has = projectExists (i);
                const auto f = dataDir.getChildFile ("projects").getChildFile ("project" + juce::String (i + 1) + ".json");
                m[(size_t) i] = { "Project " + juce::String (i + 1), has ? f.getLastModificationTime().formatted ("%d.%m. %H:%M") : juce::String ("empty"),
                                  menuPage == PageSave || has, menuPage == PageSave && has, false };
            }
            back();
            return m;
        case PageCopy:
            for (int i = 0; i < numTracks; ++i)
                m[(size_t) i] = { "Track " + juce::String (i + 1), engineDef (project.tracks[(size_t) i].engine).name, i != track.load(), true, false };
            back();
            return m;
        case PageDrums:
        {
            const auto n = drumTemplateNames();
            for (int i = 0; i < juce::jmin (15, n.size()); ++i) m[(size_t) i] = { n[i], "", true, false, false };
            back();
            return m;
        }
        case PageBass:
        {
            const auto n = bassTemplateNames();
            for (int i = 0; i < juce::jmin (15, n.size()); ++i) m[(size_t) i] = { n[i], "", true, false, false };
            back();
            return m;
        }
        case PageMacro:
            for (int i = 0; i < numMacros; ++i)
            {
                int used = 0;
                for (const auto& sl : cur().mods.slots) used += sl.active() && sl.source == SrcMacro1 + i ? 1 : 0;
                m[(size_t) i] = { "Macro " + juce::String (i + 1), used > 0 ? juce::String (used) + " linked" : juce::String ("free"), true, false, used > 0 };
            }
            back();
            return m;
        case PageChain:
        {
            const auto n = chainPresetNames();
            for (int i = 0; i < juce::jmin (15, n.size()); ++i) m[(size_t) i] = { n[i], "", true, false, false };
            back();
            return m;
        }
        case PageCopyFx:
            for (int i = 0; i < numTracks; ++i)
                m[(size_t) i] = { "Track " + juce::String (i + 1), engineDef (project.tracks[(size_t) i].engine).name, i != track.load(), true, false };
            back();
            return m;
        case PageMore:
        {
            const auto& md = cur().mods;
            int myUsed = 0, modsUsed = 0, fxUsed = 0;
            for (auto d : md.my) myUsed += d >= 0 ? 1 : 0;
            for (const auto& sl : md.slots) modsUsed += sl.active() ? 1 : 0;
            for (const auto& in : md.inserts) fxUsed += in.type != InsOff ? 1 : 0;
            m[XPin] = { "Add to MY knobs", juce::String (myUsed) + " of 24", myUsed < numMyKnobs, false, assignPending == assignMy };
            m[XClearMy] = { "Clear MY knobs", "", myUsed > 0, true, false };
            m[XAssign] = { "Assign to macro", "a macro moves it", true, false, assignPending >= 0 && assignPending < numMacros };
            m[XClearMods] = { "Clear modulation", juce::String (modsUsed) + " slots used", modsUsed > 0, true, false };
            m[XRandomMods] = { "Random modulation", "LFOs on the sound", true, false, false };
            m[XChain] = { "FX chain", "lo-fi, dub, space ...", true, false, false };
            m[XClearFx] = { "Clear FX", juce::String (fxUsed) + " effects", fxUsed > 0, true, false };
            m[XCopyFx] = { "Copy FX", "the chain to ...", fxUsed > 0, false, false };
            m[XLook] = { "Look", "all the screen settings", true, false, false };
            m[XTheme] = { "Theme", themeNames()[look.theme], true, false, false };
            m[XAccent] = { "Colour", accentNames()[look.accent], true, false, false };
            m[XText] = { "Text size", look.textSize == 0 ? "normal" : "big", true, false, look.textSize > 0 };
            m[XTrackColours] = { "Track colours", look.trackColours ? "on" : "off", true, false, look.trackColours };
            m[XInit] = { "Init track FX", "FX + mods + macros off", true, true, false };
            m[XClose] = { "Close", "", true, false, false };
            back();
            return m;
        }
        default: break;
    }
    const bool drums = cur().isDrums();
    m[MSave] = { "Save project", "8 places", true, false, false };
    m[MLoad] = { "Load project", "", true, false, false };
    m[MNew] = { "New project", "all from the start", true, true, false };
    m[MClearTrack] = { "Clear track", "the steps of track " + juce::String (track.load() + 1), true, true, false };
    m[MClearAll] = { "Clear all", "the steps of all tracks", true, true, false };
    m[MCopyTrack] = { "Copy track", "sound + steps to ...", true, false, false };
    m[MDrumPattern] = { "Drum pattern", drums ? "house, trap, breaks ..." : "(on a drum track)", drums, false, false };
    m[MBassLine] = { "Bass line", ! drums ? "acid, octaves, walking ..." : "(on a melody track)", ! drums, false, false };
    m[MClick] = { "Click", project.m.metronome ? "on" : "off", true, false, project.m.metronome };
    m[MPadsMode] = { "Pads", scalePads.load() ? "scale" : "piano", true, false, scalePads.load() };
    m[MLearn] = { "Learn keyboard", juce::String (numLearned()) + " controls learned", true, false, false };
    m[MHelp] = { "Help", "how it works", true, false, helpShown.load() };
    m[MTap] = { "Tap tempo", juce::String (juce::roundToInt (project.m.tempo)) + " BPM", true, false, false };
    m[MSaveSound] = { "Save sound", "as a User sound", true, false, false };
    m[MUndo] = { "Undo", juce::String ((int) undoStack.size()) + " steps back", ! undoStack.empty(), false, false };
    m[MMore] = { "More", "FX, macros, MY knobs, look", true, false, false };
    return m;
}

void Controller::menuPress (int tile)
{
    if (tile < 0 || tile >= numPads) return;
    const auto tiles = menuTiles();
    const auto& t = tiles[(size_t) tile];
    if (! t.enabled || t.label.isEmpty()) return;
    if (t.danger && menuConfirming() != tile)   // dangerous: a second press within 3 s
    {
        menuConfirm = tile;
        menuConfirmUntil = juce::Time::getMillisecondCounter() + 3000;
        setFlash (t.label + ": press again to be sure");
        return;
    }
    menuConfirm = -1;
    if (menuPage != PageMain)
    {
        const int page = menuPage;
        if (tile == 15) { menuPage = PageMain; if (onChange) onChange(); return; }
        switch (page)
        {
            case PageSave: saveProject (tile); break;
            case PageLoad: loadProject (tile); break;
            case PageCopy:
            {
                pushUndo ("Copy track");
                auto& to = project.tracks[(size_t) tile];
                const auto mix = std::make_tuple (to.volume, to.pan, to.sendA, to.sendB, to.mute, to.solo);
                to = cur();
                std::tie (to.volume, to.pan, to.sendA, to.sendB, to.mute, to.solo) = mix;
                presetIdx[(size_t) tile] = presetIdx[(size_t) track.load()];
                userName[(size_t) tile] = userName[(size_t) track.load()];
                edited[(size_t) tile] = edited[(size_t) track.load()];
                changed();
                setFlash ("Track " + juce::String (track.load() + 1) + " copied to track " + juce::String (tile + 1));
                break;
            }
            case PageDrums:
                pushUndo ("Drum pattern");
                applyDrumTemplate (cur().pat, tile);
                changed();
                setFlash ("Drum pattern: " + t.label + " (change it in SEQ)");
                if (! audio.isPlaying()) setPlaying (true);
                return;
            case PageMacro:
                startAssign (tile);
                return;
            case PageChain:
                pushUndo ("FX chain");
                applyChainPreset (cur().mods, tile);
                for (auto& sl : cur().mods.slots) if (dest::isInsert (sl.dest)) sl = ModSlot();
                fxSlot[(size_t) track.load()] = 0;
                changed();
                setFlash ("FX chain: " + t.label + " (SOUND > FX page)");
                return;
            case PageCopyFx:
                pushUndo ("Copy FX");
                project.tracks[(size_t) tile].mods.inserts = cur().mods.inserts;
                changed();
                setFlash ("FX of track " + juce::String (track.load() + 1) + " copied to track " + juce::String (tile + 1));
                break;
            case PageMore:
                switch (tile)
                {
                    case XPin: startAssign (assignMy); return;
                    case XClearMy: pushUndo ("MY knobs"); cur().mods.my.fill ((int16_t) dest::none); changed(); setFlash ("MY knobs cleared"); return;
                    case XAssign: menuPage = PageMacro; if (onChange) onChange(); return;
                    case XClearMods: pushUndo ("Modulation"); for (auto& sl : cur().mods.slots) sl = ModSlot(); cur().mods.macros.fill (0.0f); changed(); setFlash ("Modulation cleared"); return;
                    case XRandomMods: randomModulation(); return;
                    case XChain: menuPage = PageChain; if (onChange) onChange(); return;
                    case XClearFx: pushUndo ("Clear FX"); for (auto& in : cur().mods.inserts) in = Insert(); for (auto& sl : cur().mods.slots) if (dest::isInsert (sl.dest)) sl = ModSlot(); changed(); setFlash ("FX cleared"); return;
                    case XCopyFx: menuPage = PageCopyFx; if (onChange) onChange(); return;
                    case XLook: projectPage = 1; setMode (ModeProject); return;
                    case XTheme: { auto l = look; l.theme = (l.theme + 1) % themeNames().size(); setLook (l); return; }
                    case XAccent: { auto l = look; l.accent = (l.accent + 1) % accentNames().size(); setLook (l); return; }
                    case XText: { auto l = look; l.textSize = 1 - l.textSize; setLook (l); return; }
                    case XTrackColours: { auto l = look; l.trackColours = ! l.trackColours; setLook (l); return; }
                    case XInit: pushUndo ("Init track FX"); cur().mods = TrackMods(); fxSlot[(size_t) track.load()] = 0; modSlot[(size_t) track.load()] = 0; changed(); setFlash ("Track FX, macros and modulation: off"); return;
                    case XClose: setMode (ModeProject); return;
                    default: return;
                }
            case PageBass:
                pushUndo ("Bass line");
                applyBassTemplate (cur().pat, tile, project.m.key, project.m.scale);
                changed();
                setFlash ("Bass line: " + t.label + " (change it in SEQ)");
                if (! audio.isPlaying()) setPlaying (true);
                return;
            default: break;
        }
        menuPage = PageMain;
        if (onChange) onChange();
        return;
    }
    switch (tile)
    {
        case MSave:        menuPage = PageSave; break;
        case MLoad:        menuPage = PageLoad; break;
        case MNew:         factoryReset(); return;
        case MClearTrack:  clearPattern (track.load()); setFlash ("Track " + juce::String (track.load() + 1) + " cleared"); break;
        case MClearAll:    pushUndo ("Clear all"); for (auto& tr : project.tracks) tr.pat.clear(); changed(); setFlash ("All patterns cleared"); break;
        case MCopyTrack:   menuPage = PageCopy; break;
        case MDrumPattern: menuPage = PageDrums; break;
        case MBassLine:    menuPage = PageBass; break;
        case MClick:       project.m.metronome = ! project.m.metronome; changed(); break;
        case MPadsMode:    scalePads = ! scalePads.load(); break;
        case MLearn:       mode = ModeProject; startLearn(); return;
        case MHelp:        toggleHelp(); break;
        case MTap:         tapTempo(); break;
        case MSaveSound:   saveUserSound(); break;
        case MUndo:        undo(); break;
        case MMore:        menuPage = PageMore; break;
        default: break;
    }
    if (onChange) onChange();
}

juce::StringArray Controller::helpLines() const
{
    switch (mode.load())
    {
        case ModeSound: return {
            "SOUND - the knobs change the sound of the track",
            "Prev / Next: the sound's pages (e.g. OSC, FILTER, ENV)",
            "Shift held: 12 more knobs",
            "Sound again: the BROWSER - knob 1 engine, knob 2 category, knob 3 sound",
            "Shift + Sound: start from a plain sound (Init)",
            "Sampler track: Rec records a sample from the microphone" };
        case ModeBrowse: return {
            "BROWSER - every sound, heard the moment you pick it",
            "Knob 1: engine   Knob 2: category   Knob 3: the sound",
            "Prev / Next: the next / previous sound",
            "Rec: save the sound as yours (category User)",
            "Sound: back to the knobs" };
        case ModeSeq: return {
            "SEQ - the 16 pads are the 16 steps of a bar",
            "Tap an empty step: on. Tap it again: off",
            "The knobs change the picked step: note, velocity, length, slide, accent ...",
            "Stopped: play keys and they fill the steps one by one",
            "Drums: hit a drum (pad or key) to pick it, then tap its steps",
            "Shift + knobs: the whole pattern (length, swing, transpose ...)",
            "Prev / Next: bar 1-4   Shift + Prev / Next: copy / paste a bar",
            "Rec + Play: record what you play   MENU: ready-made drum patterns and bass lines" };
        case ModeFx: return {
            "NOTE FX - what happens to the notes of the track",
            "Prev / Next: Off / Arp / Chords / Orbit",
            "Arp: hold keys, it plays them in a rhythm   Chords: one key = a chord",
            "Orbit: every held key is a planet that plays when it crosses a line",
            "Shift + Prev / Next: ready-made settings" };
        case ModeMix: return {
            "MIX - all 8 tracks",
            "Knobs 1-8: volume (Shift: pan)   Knobs 9-12: master, glue, tape, width",
            "Next: SENDS - reverb (knobs 1-8) and delay (Shift + knobs 1-8)",
            "Pads 1-8: mute   Pads 9-16: solo" };
        case ModeMenu: return {
            "MENU - the 16 pads are the 16 buttons on the screen",
            "Red buttons (new, clear) ask first: press them twice",
            "Save / Load project: 8 places, the pad = the place",
            "Drum pattern / Bass line: a ready start for the track, then change it in SEQ",
            "Pad 16 or Project: back" };
        case ModeProject: return {
            "PROJECT - tempo, swing, key, scale, click, master",
            "Project again: the MENU - save / load, new project, clear,",
            "    copy a track, drum patterns, bass lines, help",
            "The slider: DJ filter   The joystick: bend and modulation",
            "Shift + Chaos: undo (40 steps)   Scene: 8 scenes on the pads" };
        default: return {};
    }
}

//==============================================================================
// MIDI learn
juce::String Controller::targetName (int t) const
{
    if (t >= LearnKnob0 && t < LearnKnob0 + numKnobs) return "Knob " + juce::String (t - LearnKnob0 + 1);
    if (t >= LearnButton0 && t < LearnButton0 + (int) NumButtons) return juce::String (buttonName (t - LearnButton0)) + " button";
    if (t >= LearnPad0 && t < LearnPad0 + numPads) return "Pad " + juce::String (t - LearnPad0 + 1);
    if (t == LearnKnobBank) return "Knob bank button";
    if (t == LearnSlider) return "Slider (DJ filter)";
    return {};
}

void Controller::startLearn()
{
    learnTargets.clear();
    for (int k = 0; k < numKnobs; ++k) learnTargets.push_back (LearnKnob0 + k);
    for (int b : { (int) BtnPlay, (int) BtnRec, (int) BtnShift, (int) BtnPrev, (int) BtnNext, (int) BtnSound, (int) BtnSeq, (int) BtnFx, (int) BtnMix,
                   (int) BtnProject, (int) BtnScene, (int) BtnChaos })
        learnTargets.push_back (LearnButton0 + b);
    for (int p = 0; p < numPads; ++p) learnTargets.push_back (LearnPad0 + p);
    learnTargets.push_back (LearnSlider);
    learnTargets.push_back (LearnKnobBank);
    learnLastText = {};
    learnCollecting = false;
    learnIndex = 0;
    if (onChange) onChange();
}

void Controller::learnSkip()
{
    if (learnCollecting) { finishLearnCandidate(); return; }
    const int i = learnIndex.load();
    if (i < 0) return;
    if (i + 1 >= (int) learnTargets.size()) learnDone();
    else learnIndex = i + 1;
    if (onChange) onChange();
}

void Controller::learnDone()
{
    if (learnCollecting) finishLearnCandidate();
    learnIndex = -1;
    saveMidiMap();
    setFlash (juce::String (numLearned()) + " controls learned");
}

void Controller::forgetMidiMap()
{
    {
        const juce::SpinLock::ScopedLockType lock (midiMapLock);
        midiMap.clear();
    }
    knobBank = false;
    saveMidiMap();
    setFlash ("The MIDI controller is forgotten");
}

int Controller::numLearned() const
{
    const juce::SpinLock::ScopedLockType lock (midiMapLock);
    return (int) midiMap.size();
}

juce::String Controller::learnPrompt() const
{
    const int i = learnIndex.load();
    if (i < 0 || i >= (int) learnTargets.size()) return {};
    return targetName (learnTargets[(size_t) i]);
}

juce::String Controller::learnHint() const
{
    const int i = learnIndex.load();
    if (i < 0 || i >= (int) learnTargets.size()) return {};
    const int t = learnTargets[(size_t) i];
    if (t < LearnButton0 || t == LearnSlider) return "Turn a knob or move a fader on your keyboard";
    if (t >= LearnPad0 && t < LearnPad0 + numPads) return "Hit a pad (switch the pad bank for pads 9-16)";
    return "Press a button on your keyboard";
}

void Controller::learnInput (int ch, int num, bool note, int value)
{
    const int i = learnIndex.load();
    if (i < 0 || i >= (int) learnTargets.size()) return;
    const int t = learnTargets[(size_t) i];
    const auto now = juce::Time::getMillisecondCounter();
    if (now < learnQuietUntil && ch == learnIgnoreCh && num == learnIgnoreNum && note == learnIgnoreNote) return;
    if (learnCollecting)
    {
        if (ch == learnCandidate.ch && num == learnCandidate.num && note == learnCandidate.note) learnValues.add (value);
        if (now - learnStartedAt > 300) finishLearnCandidate();
        return;
    }
    const bool knobLike = t < LearnButton0 || t == LearnSlider;
    if (knobLike && note) return;
    if (! knobLike && ! note && value == 0) return;
    learnCandidate = {};
    learnCandidate.ch = ch;
    learnCandidate.num = num;
    learnCandidate.note = note;
    learnCandidate.target = t;
    learnValues.clearQuick();
    learnValues.add (value);
    if (knobLike)
    {
        learnCollecting = true;
        learnStartedAt = now;
        juce::WeakReference<Controller> weak (this);
        juce::Timer::callAfterDelay (350, [weak] { if (auto* c = weak.get()) if (c->learnCollecting) c->finishLearnCandidate(); });
    }
    else finishLearnCandidate();
}

void Controller::finishLearnCandidate()
{
    learnCollecting = false;
    const int i = learnIndex.load();
    if (i < 0 || i >= (int) learnTargets.size()) return;
    auto c = learnCandidate;
    if (c.target < LearnButton0 || c.target == LearnSlider)
    {
        juce::Array<int> distinct;
        bool small = true;
        for (int v : learnValues)
        {
            distinct.addIfNotAlreadyThere (v);
            small = small && ((v >= 1 && v <= 15) || (v >= 113 && v <= 127));
        }
        c.relative = c.target != LearnSlider && small && distinct.size() <= 3 && learnValues.size() >= 2;
        c.last = c.relative ? -1 : learnValues.getLast();
    }
    {
        const juce::SpinLock::ScopedLockType lock (midiMapLock);
        midiMap.erase (std::remove_if (midiMap.begin(), midiMap.end(), [&c] (const MidiCtl& m)
                       { return (m.ch == c.ch && m.num == c.num && m.note == c.note) || m.target == c.target; }), midiMap.end());
        midiMap.push_back (c);
    }
    learnIgnoreCh = c.ch;
    learnIgnoreNum = c.num;
    learnIgnoreNote = c.note;
    learnQuietUntil = juce::Time::getMillisecondCounter() + 700;
    learnLastText = targetName (c.target) + " = " + (c.note ? "note " : "CC ") + juce::String (c.num) + ", channel " + juce::String (c.ch)
                  + (c.target < LearnButton0 ? (c.relative ? " (endless)" : " (knob / fader)") : juce::String());
    if (i + 1 >= (int) learnTargets.size()) learnDone();
    else learnIndex = i + 1;
    if (onChange) onChange();
}

bool Controller::handleMapped (const juce::MidiMessage& m)
{
    const bool isNote = m.isNoteOnOrOff();
    if (! isNote && ! m.isController()) return false;
    const int ch = m.getChannel(), num = isNote ? m.getNoteNumber() : m.getControllerNumber();
    const int value = isNote ? (m.isNoteOn() ? (int) m.getVelocity() : 0) : m.getControllerValue();
    int target = -1, delta = 0;
    bool relative = false, first = false;
    {
        const juce::SpinLock::ScopedLockType lock (midiMapLock);
        for (auto& c : midiMap)
            if (c.ch == ch && c.num == num && c.note == isNote)
            {
                target = c.target;
                relative = c.relative;
                first = c.last < 0;
                delta = first ? 0 : value - c.last;
                c.last = value;
                break;
            }
    }
    if (target < 0) return false;
    if (target == LearnSlider) { runOnMessageThread ([this, value] { setSlider (value / 127.0f); }); return true; }
    if (target < LearnButton0)
    {
        int k = target - LearnKnob0;
        if (knobBank.load() && k < 8) k += 4;
        if (relative)
        {
            const int clicks = value == 0 || value == 64 ? 0 : (value < 64 ? value : value - 128);
            runOnMessageThread ([this, k, clicks] { turnKnob (k, (float) clicks); });
        }
        else if (! first && delta != 0)
            runOnMessageThread ([this, k, delta] { turnKnobBy (k, (float) delta / 127.0f); });
        return true;
    }
    const bool down = value > 0;
    if (target >= LearnPad0 && target < LearnPad0 + numPads)
    {
        const int p = target - LearnPad0;
        down ? padDown (p, juce::jmax (1, value)) : padUp (p);
        return true;
    }
    if (target == LearnKnobBank)
    {
        if (down) runOnMessageThread ([this] { knobBank = ! knobBank.load(); setFlash (knobBank.load() ? "Keyboard knobs = knobs 5-12" : "Keyboard knobs = knobs 1-8"); });
        return true;
    }
    const int b = target - LearnButton0;
    if (b == BtnShift) shift = down;
    runOnMessageThread ([this, b, down] { down ? buttonDown (b) : buttonUp (b); });
    return true;
}

void Controller::loadMidiMap()
{
    const auto f = dataDir.getChildFile ("midimap2.json");
    std::vector<MidiCtl> list;
    const auto parsed = f.existsAsFile() ? juce::JSON::parse (f) : juce::var();
    if (f.existsAsFile())
        if (auto* a = parsed["controls"].getArray())
            for (const auto& v : *a)
            {
                MidiCtl c;
                c.ch = juce::jlimit (1, 16, (int) v.getProperty ("ch", 1));
                c.num = juce::jlimit (0, 127, (int) v.getProperty ("num", 0));
                c.note = (bool) v.getProperty ("note", false);
                c.target = (int) v.getProperty ("target", -1);
                c.relative = (bool) v.getProperty ("relative", false);
                if (targetName (c.target).isNotEmpty()) list.push_back (c);
            }
    const juce::SpinLock::ScopedLockType lock (midiMapLock);
    midiMap = std::move (list);
}

void Controller::saveMidiMap() const
{
    juce::Array<juce::var> a;
    {
        const juce::SpinLock::ScopedLockType lock (midiMapLock);
        for (const auto& c : midiMap)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("ch", c.ch); o->setProperty ("num", c.num); o->setProperty ("note", c.note);
            o->setProperty ("target", c.target); o->setProperty ("relative", c.relative);
            a.add (juce::var (o));
        }
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("controls", a);
    dataDir.getChildFile ("midimap2.json").replaceWithText (juce::JSON::toString (juce::var (root)));
}

//==============================================================================
// The link
void Controller::sendRemote (juce::DynamicObject* o)
{
    const juce::var v (o);
    if (remoteSink) remoteSink (v);
}

void Controller::setRemoteSink (std::function<void (const juce::var&)> sink)
{
    const bool was = isRemote();
    remoteSink = std::move (sink);
    audio.setSilent (isRemote());
    if (was && ! isRemote())   // back to playing by itself: the PC's own project again
    {
        project = Project::makeDefault();
        loadState();
        audio.mirrorTransport (false, false);
        prepareSamples(); audio.push (project);
    }
    if (onChange) onChange();
}

juce::var Controller::makeState() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "state");
    o->setProperty ("project", project.toVar());
    auto* ui = new juce::DynamicObject();
    juce::Array<juce::var> pi, ed, un, oc, sp, br, ln;
    for (int t = 0; t < numTracks; ++t)
    {
        pi.add (presetIdx[(size_t) t]); ed.add (edited[(size_t) t]); un.add (userName[(size_t) t]);
        oc.add (octave[(size_t) t].load()); sp.add (soundPage[(size_t) t]); br.add (bar[(size_t) t]); ln.add (lane[(size_t) t]);
    }
    ui->setProperty ("preset", pi); ui->setProperty ("edited", ed); ui->setProperty ("user", un);
    ui->setProperty ("octave", oc); ui->setProperty ("page", sp); ui->setProperty ("bar", br); ui->setProperty ("lane", ln);
    ui->setProperty ("track", track.load()); ui->setProperty ("mode", mode.load());
    ui->setProperty ("scene", sceneMode.load()); ui->setProperty ("mixPage", mixPage);
    ui->setProperty ("bEngine", browseEngine); ui->setProperty ("bCat", browseCat); ui->setProperty ("bPos", browsePos);
    juce::Array<juce::var> held;
    for (int i = 0; i < numHeldSteps; ++i) held.add (heldSteps[(size_t) i]);
    ui->setProperty ("held", held);
    ui->setProperty ("flash", flashText());
    o->setProperty ("ui", juce::var (ui));
    return juce::var (o);
}

juce::var Controller::makeLive()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "live");
    o->setProperty ("play", audio.isPlaying());
    o->setProperty ("rec", audio.isRecording());
    juce::Array<juce::var> st, lv, hb;
    for (int t = 0; t < numTracks; ++t)
    {
        st.add (audio.stepNow[(size_t) t].load());
        lv.add (std::round (audio.level[(size_t) t].exchange (0.0f) * 1000.0f) / 1000.0f);
        hb.add ((int) audio.hits[(size_t) t].exchange (0u));
    }
    o->setProperty ("steps", st);
    o->setProperty ("levels", lv);
    o->setProperty ("hits", hb);
    o->setProperty ("peak", audio.peak.exchange (0.0f));
    o->setProperty ("beats", audio.songBeats.load());
    o->setProperty ("wait", audio.sceneWaiting.load());
    juce::Array<juce::var> wave;
    const int w = audio.scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < Audio::scopeSize; i += 2)
        wave.add (std::round (audio.scope[(size_t) ((w + i) % Audio::scopeSize)].load (std::memory_order_relaxed) * 1000.0f) / 1000.0f);
    o->setProperty ("wave", wave);
    juce::Array<juce::var> orb;
    for (const auto& pv : audio.planets)
        if (pv.active.load())
            orb.add (juce::Array<juce::var> { pv.note.load(), std::round (pv.theta.load() * 1000.0f) / 1000.0f, std::round (pv.radius.load() * 100.0f) / 100.0f });
    o->setProperty ("orb", orb);
    return juce::var (o);
}

void Controller::applyState (const juce::var& v)
{
    if (! isRemote() || ! v.isObject()) return;
    project.fromVar (v["project"]);
    const auto ui = v["ui"];
    auto arr = [&ui] (const char* id) { return ui[id].getArray(); };
    for (int t = 0; t < numTracks; ++t)
    {
        if (auto* a = arr ("preset"); a != nullptr && t < a->size()) presetIdx[(size_t) t] = (int) (*a)[t];
        if (auto* a = arr ("edited"); a != nullptr && t < a->size()) edited[(size_t) t] = (bool) (*a)[t];
        if (auto* a = arr ("user"); a != nullptr && t < a->size()) userName[(size_t) t] = (*a)[t].toString();
        if (auto* a = arr ("octave"); a != nullptr && t < a->size()) octave[(size_t) t] = (int) (*a)[t];
        if (auto* a = arr ("page"); a != nullptr && t < a->size()) soundPage[(size_t) t] = (int) (*a)[t];
        if (auto* a = arr ("bar"); a != nullptr && t < a->size()) bar[(size_t) t] = (int) (*a)[t];
        if (auto* a = arr ("lane"); a != nullptr && t < a->size()) lane[(size_t) t] = (int) (*a)[t];
    }
    track = juce::jlimit (0, numTracks - 1, (int) ui.getProperty ("track", 0));
    mode = juce::jlimit (0, NumModes - 1, (int) ui.getProperty ("mode", 0));
    sceneMode = (bool) ui.getProperty ("scene", false);
    mixPage = (int) ui.getProperty ("mixPage", 0);
    browseEngine = (int) ui.getProperty ("bEngine", 0); browseCat = (int) ui.getProperty ("bCat", 0); browsePos = (int) ui.getProperty ("bPos", 0);
    numHeldSteps = 0;
    if (auto* h = ui["held"].getArray()) for (const auto& x : *h) if (numHeldSteps < maxSteps) heldSteps[(size_t) numHeldSteps++] = (int) x;
    const auto f = ui["flash"].toString();
    if (f.isNotEmpty() && f != flash) setFlash (f);
    pagesClamp();
    if (onChange) onChange();
}

void Controller::applyLive (const juce::var& v)
{
    if (! isRemote() || ! v.isObject()) return;
    audio.mirrorTransport ((bool) v["play"], (bool) v["rec"]);
    if (auto* a = v["steps"].getArray()) for (int t = 0; t < juce::jmin (numTracks, a->size()); ++t) audio.stepNow[(size_t) t] = (int) (*a)[t];
    if (auto* a = v["levels"].getArray()) for (int t = 0; t < juce::jmin (numTracks, a->size()); ++t) audio.level[(size_t) t] = (float) (*a)[t];
    if (auto* a = v["hits"].getArray()) for (int t = 0; t < juce::jmin (numTracks, a->size()); ++t) audio.hits[(size_t) t].fetch_or ((uint32_t) (int) (*a)[t]);
    audio.peak = (float) v.getProperty ("peak", 0.0);
    audio.songBeats = (double) v.getProperty ("beats", 0.0);
    audio.sceneWaiting = (bool) v["wait"];
    if (auto* w = v["wave"].getArray())
        for (int i = 0; i < juce::jmin (Audio::scopeSize / 2, w->size()); ++i)
        {
            audio.scope[(size_t) (2 * i)] = (float) (*w)[i];
            audio.scope[(size_t) (2 * i + 1)] = (float) (*w)[i];
        }
    audio.scopeWrite = 0;
    if (auto* orb = v["orb"].getArray())
        for (int i = 0; i < orbitcat::maxBodies; ++i)
        {
            auto& pv = audio.planets[(size_t) i];
            auto* a = i < orb->size() ? (*orb)[i].getArray() : nullptr;
            pv.active = a != nullptr && a->size() >= 3;
            if (! pv.active.load()) continue;
            pv.note = (int) (*a)[0]; pv.theta = (float) (*a)[1]; pv.radius = (float) (*a)[2];
        }
}

void Controller::handleLinkMessage (const juce::var& m, const std::function<void (const juce::var&)>& reply)
{
    if (isRemote() || ! m.isObject()) return;
    const auto t = m["t"].toString();
    if (t == "knob")        turnKnob ((int) m["k"], (float) m["c"]);
    else if (t == "reset")  resetKnob ((int) m["k"]);
    else if (t == "button")
    {
        const int b = (int) m["b"];
        const bool down = (bool) m["down"];
        if (b == BtnShift) shift = down;
        down ? buttonDown (b) : buttonUp (b);
    }
    else if (t == "pad")    (bool) m["down"] ? padDown ((int) m["p"], (int) m.getProperty ("v", 100)) : padUp ((int) m["p"]);
    else if (t == "key")    (bool) m["down"] ? keyDown ((int) m["n"], (int) m.getProperty ("v", 100)) : keyUp ((int) m["n"]);
    else if (t == "slider") setSlider ((float) m["v"]);
    else if (t == "joy")    setJoystick ((float) m["x"], (float) m["y"]);
    else if (t == "track")  selectTrack ((int) m["v"]);
    else if (t == "mode")   setMode ((int) m["v"]);
    else if (t == "reset!") factoryReset();
    else if (t == "hello")  reply (makeState());
}
//==============================================================================
// Customization
int Controller::soundExtraPage() const
{
    if (mode.load() != ModeSound) return -1;
    const int n = (int) engineDef (cur().engine).pages.size();
    const int p = soundPage[(size_t) track.load()];
    return p >= n ? juce::jlimit (0, numSoundExtras - 1, p - n) : -1;
}

Controller::Target Controller::destTarget (int d) const
{
    if (dest::isEngine (d)) return d < (int) engineDef (cur().engine).params.size() ? Target { KParam, d } : Target {};
    if (dest::isInsert (d))
    {
        const int k = d - dest::insert0;
        return (k % insertParams) < (int) insertDef (cur().mods.inserts[(size_t) (k / insertParams)].type).params.size() ? Target { KIns, k } : Target {};
    }
    if (dest::isMix (d)) return d - dest::mix0 == 4 ? Target { KSendC, track.load() } : Target { KMix, track.load() * 4 + (d - dest::mix0) };
    if (d >= dest::macro0 && d < dest::macro0 + numMacros) return { KMacro, d - dest::macro0 };
    if (d >= dest::modSet0 && d < dest::modSet0 + numModSet) return { KModSet, d - dest::modSet0 };
    return {};
}

int Controller::targetDest (const Target& t) const
{
    switch (t.kind)
    {
        case KParam: return t.index;
        case KIns: return dest::insert0 + t.index;
        case KMix: return t.index / 4 == track.load() ? dest::mix0 + t.index % 4 : -1;
        case KSendC: return t.index == track.load() ? dest::mix0 + 4 : -1;
        case KMacro: return dest::macro0 + t.index;
        case KModSet: return dest::modSet0 + t.index;
        default: return -1;
    }
}

juce::String Controller::destName (int t, int d, bool shortForm) const
{
    const auto& tr = project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)];
    if (dest::isEngine (d))
    {
        const auto& e = engineDef (tr.engine);
        return d < (int) e.params.size() ? e.params[(size_t) d].name : juce::String ("?");
    }
    if (dest::isInsert (d))
    {
        const int k = d - dest::insert0, slot = k / insertParams, q = k % insertParams;
        const auto& idef = insertDef (tr.mods.inserts[(size_t) slot].type);
        const auto pn = q < (int) idef.params.size() ? idef.params[(size_t) q].name : juce::String ("?");
        return shortForm ? juce::String (slot + 1) + " " + idef.shortName + " " + pn : "FX " + juce::String (slot + 1) + " " + idef.name + ": " + pn;
    }
    if (dest::isMix (d)) { static const char* n[] = { "Volume", "Pan", "Reverb send", "Delay send", "FX send" }; return n[d - dest::mix0]; }
    if (d >= dest::macro0 && d < dest::macro0 + numMacros) return "Macro " + juce::String (d - dest::macro0 + 1);
    if (d >= dest::modSet0 && d < dest::modSet0 + numModSet) return modSetDefs()[(size_t) (d - dest::modSet0)].name;
    return "-";
}

juce::String Controller::modSlotText (int t, int slot) const
{
    const auto& sl = project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)].mods.slots[(size_t) juce::jlimit (0, numModSlots - 1, slot)];
    if (sl.source == SrcNone || sl.dest == dest::none) return juce::String (slot + 1) + ": empty";
    const int pct = juce::roundToInt (sl.amount * 100.0f);
    return juce::String (slot + 1) + ": " + modSourceNames()[sl.source] + " > " + destName (t, sl.dest, true) + " " + (pct > 0 ? "+" : "") + juce::String (pct) + "%";
}

void Controller::startAssign (int what)
{
    assignPending = what;
    menuPage = 0;
    mode = ModeSound;   // back to the knobs: turn the one you mean
    setFlash (what == assignMy ? "Turn any knob: it goes onto your MY page" : "Turn the knob macro " + juce::String (what + 1) + " should move");
    if (onChange) onChange();
}

bool Controller::tryAssign (const Target& t)
{
    const int d = targetDest (t);
    if (d < 0) { setFlash ("That knob can't go there - turn a sound, FX, macro or mix knob"); return true; }
    auto& md = cur().mods;
    if (assignPending == assignMy)
    {
        for (auto x : md.my) if (x == d) { setFlash (destName (track.load(), d) + " is already on MY page"); assignPending = -1; return true; }
        for (auto& x : md.my)
            if (x < 0)
            {
                pushUndo ("MY knobs");
                x = (int16_t) d;
                assignPending = -1;
                changed();
                setFlash (destName (track.load(), d) + " is on your MY page now");
                return true;
            }
        setFlash ("MY page is full (24)");
        assignPending = -1;
        return true;
    }
    if (! (dest::isEngine (d) || dest::isInsert (d) || dest::isMix (d))) { setFlash ("A macro moves sound, FX or mix knobs"); return true; }
    const int m = assignPending;
    const int free = md.firstFreeSlot();
    if (free < 0) { setFlash ("All 16 modulation slots are used (SOUND > MOD)"); assignPending = -1; return true; }
    pushUndo ("Macro");
    md.slots[(size_t) free] = { SrcMacro1 + m, d, 0.5f };
    assignPending = -1;
    changed();
    setFlash ("Macro " + juce::String (m + 1) + " moves " + destName (track.load(), d) + " (amount: SOUND > MOD, slot " + juce::String (free + 1) + ")");
    return true;
}

void Controller::randomModulation()
{
    pushUndo ("Random modulation");
    auto& tr = cur();
    auto& md = tr.mods;
    const auto& e = engineDef (tr.engine);
    juce::Random rng;
    std::vector<int> candidates;
    for (int i = 0; i < (int) e.params.size(); ++i)
    {
        const auto& p = e.params[(size_t) i];
        if (p.stepped() || p.id == "volume" || p.id.endsWith ("_level") || p.unit == ParamDef::Sec) continue;
        candidates.push_back (i);
    }
    if (candidates.empty()) return;
    for (auto& sl : md.slots) if (sl.source == SrcLfo1 || sl.source == SrcLfo2 || sl.source == SrcRandom) sl = ModSlot();
    const int count = 2 + rng.nextInt (2);
    const int sources[] = { SrcLfo1, SrcLfo2, SrcRandom };
    for (int k = 0; k < count; ++k)
    {
        const int free = md.firstFreeSlot();
        if (free < 0) break;
        const int d = candidates[(size_t) rng.nextInt ((int) candidates.size())];
        md.slots[(size_t) free] = { sources[k % 3], d, (rng.nextBool() ? 1.0f : -1.0f) * (0.08f + rng.nextFloat() * 0.25f) };
    }
    static const int syncs[] = { 2, 3, 4, 10, 11, 0 };
    md.set[MsLfo1Sync] = (float) syncs[rng.nextInt (6)];
    md.set[MsLfo2Sync] = (float) syncs[rng.nextInt (6)];
    md.set[MsLfo1Shape] = (float) rng.nextInt (7);
    md.set[MsLfo2Shape] = (float) rng.nextInt (7);
    md.set[MsRandRate] = 5.0f;
    md.set[MsSmooth] = rng.nextFloat() * 0.8f;
    mode = ModeSound;   // show it: the MOD page
    soundPage[(size_t) track.load()] = (int) e.pages.size() + ExtraMod;
    changed();
    setFlash ("Random modulation: " + juce::String (count) + " knobs move (SOUND > MOD, Shift + Chaos = back)");
}

void Controller::setLook (const Look& l)
{
    look = l;
    look.theme = juce::jlimit (0, themeNames().size() - 1, look.theme);
    look.accent = juce::jlimit (0, accentNames().size() - 1, look.accent);
    look.textSize = juce::jlimit (0, 1, look.textSize);
    look.knobStyle = juce::jlimit (0, 1, look.knobStyle);
    saveLook();
    if (onChange) onChange();
}

void Controller::loadLook()
{
    const auto v = juce::JSON::parse (dataDir.getChildFile ("look.json"));
    if (! v.isObject()) return;
    Look l;
    l.theme = (int) v.getProperty ("theme", 0);
    l.accent = (int) v.getProperty ("accent", 0);
    l.textSize = (int) v.getProperty ("textSize", 0);
    l.knobStyle = (int) v.getProperty ("knobStyle", 0);
    l.trackColours = (bool) v.getProperty ("trackColours", false);
    l.hintLine = (bool) v.getProperty ("hintLine", true);
    look = l;
}

void Controller::saveLook() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("theme", look.theme);
    o->setProperty ("accent", look.accent);
    o->setProperty ("textSize", look.textSize);
    o->setProperty ("knobStyle", look.knobStyle);
    o->setProperty ("trackColours", look.trackColours);
    o->setProperty ("hintLine", look.hintLine);
    dataDir.getChildFile ("look.json").replaceWithText (juce::JSON::toString (juce::var (o)));
}

juce::String Controller::laneName (int t, int l) const
{
    const auto& tr = project.tracks[(size_t) juce::jlimit (0, numTracks - 1, t)];
    l = juce::jlimit (0, numLanes - 1, l);
    if (tr.engine == EngineKit)
    {
        auto n = SampleBank::get().shortName (juce::roundToInt (tr.params[(size_t) (l * 6)]));
        if (n.startsWith ("Kit ")) n = n.substring (4);
        return n.length() > 14 ? n.substring (0, 13) + "." : n;
    }
    return engineDef (tr.engine).params[(size_t) l * 4].name.upToFirstOccurrenceOf (" tune", false, false);
}

void Controller::prepareSamples()
{
    auto& bank = SampleBank::get();
    for (const auto& tr : project.tracks)
    {
        const auto& d = engineDef (tr.engine);
        for (size_t i = 0; i < d.params.size(); ++i)
            if (d.params[i].isSample) bank.ensureLoaded (juce::roundToInt (tr.params[i]));
        if (tr.engine == EngineSampler && tr.params[(size_t) d.index ("multi")] > 0.5f)
            bank.buildZones (juce::roundToInt (tr.params[(size_t) d.index ("sample")]));
    }
}

} // namespace catbox
