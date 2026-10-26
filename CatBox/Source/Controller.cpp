#include "Controller.h"
#include "Music.h"
#include "Samples.h"
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
    static const char* n[NumModes] = { "SOUND", "BROWSE", "SEQ", "NOTE FX", "MIX", "PROJECT" };
    return n[juce::jlimit (0, NumModes - 1, m)];
}

namespace
{
// ---- the MIX and PROJECT knobs
enum MasterId { MVolume = 0, MGlue, MTape, MWidth, MRevSize, MRevDamp, MDelayTime, MDelayFb, MRevLowCut, MRevWidth, MDelayTone, MPingPong,
                MTempo, MSwing, MKey, MScale, MMetronome, MDj, MPads, MOctave, NumMasterIds };

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
        };
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

} // namespace

//==============================================================================
juce::StringArray starterSampleNames();
void makeStarterSamples (SampleBank&);

Controller::Controller (const juce::File& dir) : dataDir (dir)
{
    dataDir.createDirectory();
    SampleBank::get().loadFolder (dataDir.getChildFile ("samples"), starterSampleNames(), [] (SampleBank& b) { makeStarterSamples (b); });
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
    }
    heldSteps.fill (-1);
    loadState();
    loadScenes();
    loadMidiMap();
    audio.push (project);
    startTimerHz (30);
}

Controller::~Controller()
{
    stopTimer();
}

void Controller::runOnMessageThread (std::function<void()> f)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread()) { f(); return; }
    juce::WeakReference<Controller> weak (this);
    juce::MessageManager::callAsync ([weak, f = std::move (f)] { if (weak.get() != nullptr) f(); });
}

void Controller::changed (bool atBar)
{
    if (! isRemote()) audio.push (project, atBar);
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
        case BtnProject: setMode (ModeProject); break;
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
                    const int n = (int) engineDef (cur().engine).pages.size();
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
                case ModeMix:     mixPage = (mixPage + dir + 2) % 2; break;
                case ModeProject: projectPage = 0; break;
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
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "mode"); o->setProperty ("v", m); sendRemote (o); return; }
    mode = juce::jlimit (0, NumModes - 1, m);
    numHeldSteps = 0;
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
    loadEnginePreset (tr, engine, preset);
    if (wasDrums != tr.isDrums()) lane[(size_t) t] = 0;
    presetIdx[(size_t) t] = preset;
    userName[(size_t) t] = {};
    edited[(size_t) t] = false;
    soundPage[(size_t) t] = juce::jlimit (0, juce::jmax (0, (int) engineDef (engine).pages.size() - 1), soundPage[(size_t) t]);
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
    return c;
}

juce::StringArray Controller::browsePresets() const
{
    const auto cats = browseCategories();
    if (cats.isEmpty()) return {};
    const auto cat = cats[juce::jlimit (0, cats.size() - 1, browseCat)];
    juce::StringArray out;
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

void Controller::saveState()
{
    if (isRemote()) return;
    dirty = false;
    saveCountdown = 0;
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
    ui->setProperty ("track", track.load()); ui->setProperty ("mode", mode.load() == ModeBrowse ? (int) ModeSound : mode.load());
    ui->setProperty ("scalePads", scalePads.load());
    ui->setProperty ("scene", loadedScene);
    if (auto* o = v.getDynamicObject()) o->setProperty ("ui", juce::var (ui));
    dataDir.getChildFile ("state2.json").replaceWithText (juce::JSON::toString (v));
}

void Controller::loadState()
{
    const auto f = dataDir.getChildFile ("state2.json");
    if (! f.existsAsFile()) return;
    const auto v = juce::JSON::parse (f);
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
    }
    track = juce::jlimit (0, numTracks - 1, (int) ui.getProperty ("track", 0));
    audio.viewTrack = track.load();
    mode = juce::jlimit (0, NumModes - 1, (int) ui.getProperty ("mode", 0));
    scalePads = (bool) ui.getProperty ("scalePads", false);
    loadedScene = (int) ui.getProperty ("scene", -1);
    pagesClamp();
}

void Controller::pagesClamp()
{
    for (int t = 0; t < numTracks; ++t)
        soundPage[(size_t) t] = juce::jlimit (0, juce::jmax (0, (int) engineDef (project.tracks[(size_t) t].engine).pages.size() - 1), soundPage[(size_t) t]);
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
            if (mixPage == 0)
            {
                if (slot < 8) return { KMix, slot * 4 + 0 };
                if (slot < 12) return { KMaster, MVolume + (slot - 8) };
                if (slot < 20) return { KMix, (slot - 12) * 4 + 1 };
                return {};
            }
            if (slot < 8) return { KMix, slot * 4 + 2 };
            if (slot < 12) return { KMaster, MRevSize + (slot - 8) };
            if (slot < 20) return { KMix, (slot - 12) * 4 + 3 };
            return { KMaster, MRevLowCut + (slot - 20) };
        case ModeProject:
        {
            static const int map[24] = { MTempo, MSwing, MKey, MScale, MMetronome, MVolume, MDj, MPads, MOctave, -1, -1, -1,
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
        case KParam: pushUndo ("Sound edit", true); cur().params[(size_t) t.index] = v; edited[(size_t) track.load()] = true; break;
        case KFx:    cur().fx[(size_t) t.index] = v; break;
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
                case MOctave: octave[(size_t) track.load()] = juce::roundToInt (v); break;
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
    changeTarget (targetFor (slot), clicks, false);
}

void Controller::turnKnobBy (int k, float fraction)
{
    if (k < 0 || k >= numKnobs || fraction == 0.0f) return;
    if (isRemote()) { turnKnob (k, fraction * 100.0f); return; }
    const int slot = k + (shift.load() ? numKnobs : 0);
    if (mode.load() == ModeSeq) { seqKnob (slot, fraction * 48.0f); return; }
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
    const auto* def = targetDef (t);
    if (def == nullptr) return v;
    const float val = targetValue (t);
    v.used = true;
    v.name = def->name;
    v.value = def->format (val);
    if (t.kind == KMaster && t.index == MTempo) v.value = juce::String (val, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " BPM";
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
        case ModeSound: for (const auto& p : engineDef (cur().engine).pages) n.add (p.name); break;
        case ModeBrowse: n.add ("BROWSE"); break;
        case ModeSeq: { const int bars = juce::jmax (1, (cur().pat.length + 15) / 16); for (int b = 0; b < bars; ++b) n.add ("BAR " + juce::String (b + 1)); break; }
        case ModeFx: for (const auto& s : noteFxNames()) n.add (s.toUpperCase()); break;
        case ModeMix: n.add ("LEVEL"); n.add ("SENDS"); break;
        case ModeProject: n.add ("PROJECT"); break;
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
    switch (mode.load())
    {
        case ModeSound:   if (cur().engine == EngineSampler) return "Rec: record a sample (microphone) | knob 1: the sample | Play: Keys / Chop / One shot";
                          return "Knobs: the sound | Prev / Next: pages | Shift: 12 more | Sound: browse sounds";
        case ModeBrowse:  return "Knob 1 engine | 2 category | 3 sound | Prev / Next: next sound | Rec: save as User | Sound: back";
        case ModeSeq:     return cur().isDrums() ? "Pad = step of the drum | hold a step + knob = edit | Shift + pad: pick drum | Prev / Next: bar | Shift + Prev / Next: copy / paste"
                                                 : "Pad = step | hold a step + key / knobs = edit | Prev / Next: bar | Shift + Prev / Next: copy / paste bar";
        case ModeFx:      return "Prev / Next: Off / Arp / Chords / Orbit | Shift + Prev / Next: presets | play the keys";
        case ModeMix:     return "Knobs: levels (Shift: pan) | Prev / Next: sends | pads 1-8 mute, 9-16 solo";
        case ModeProject: return "Tempo, swing, key, scale, click, master | Shift: glue, tape, width, reverb, delay";
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

void Controller::stepTap (int step)
{
    pushUndo ("Steps", true);
    auto& tr = cur();
    auto& pat = tr.pat;
    if (step >= pat.length) return;
    if (tr.isDrums())
    {
        auto& v = pat.drums[(size_t) lane[(size_t) track.load()]][(size_t) step];
        v = v > 0 ? 0 : (uint8_t) juce::jlimit (1, 127, lastVel);
    }
    else
    {
        auto& st = pat.steps[(size_t) step];
        if (st.on()) st.clear();
        else
        {
            st.notes[0] = (int8_t) lastNote;
            st.vel = (uint8_t) lastVel;
            st.len = 75;
        }
    }
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

    if (numHeldSteps > 0)   // edit the held steps
    {
        stepEdited = true;
        for (int i = 0; i < numHeldSteps; ++i)
        {
            const int s = heldSteps[(size_t) i];
            if (tr.isDrums())
            {
                auto& v = pat.drums[(size_t) lane[(size_t) t]][(size_t) s];
                if (slot == 0 || slot == 1) v = (uint8_t) juce::jlimit (v > 0 ? 1 : 0, 127, (int) v + d * 4);
                continue;
            }
            auto& st = pat.steps[(size_t) s];
            if (! st.on() && slot != 0) continue;
            switch (slot)
            {
                case 0:  if (! st.on()) { st.notes[0] = (int8_t) lastNote; break; }
                         for (auto& n : st.notes) if (n >= 0) n = (int8_t) (shift.load() ? juce::jlimit (0, 127, n + d) : music::moveInScale (n, project.m.key, project.m.scale, d));
                         break;
                case 1:  st.vel = (uint8_t) juce::jlimit (1, 127, (int) st.vel + d * 3); break;
                case 2:  st.len = (uint16_t) juce::jlimit (10, 1600, (int) st.len + d * (st.len >= 100 ? 25 : 5)); break;
                case 3:  if (d != 0) st.slide = d > 0; break;
                case 4:  if (d != 0) st.accent = d > 0; break;
                case 5:  st.chance = (uint8_t) juce::jlimit (0, 100, (int) st.chance + d * 5); break;
                case 6:  st.nudge = (int8_t) juce::jlimit (-50, 50, (int) st.nudge + d * 2); break;
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
    // nothing held: the whole pattern
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
                for (auto& l : pat.drums) rot (l);
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
    if (numHeldSteps > 0)
    {
        const int s = heldSteps[0];
        if (tr.isDrums())
        {
            if (slot > 1) return v;
            const int vel = pat.drums[(size_t) lane[(size_t) t]][(size_t) s];
            v.used = true; v.name = "Velocity"; v.value = vel > 0 ? juce::String (vel) + (vel >= 120 ? " accent" : "") : juce::String ("off"); v.norm = vel / 127.0f;
            return v;
        }
        const auto& st = pat.steps[(size_t) s];
        v.used = slot <= 9;
        switch (slot)
        {
            case 0: v.name = "Note"; v.value = st.on() ? music::noteName (st.notes[0]) : juce::String ("off"); v.norm = st.on() ? st.notes[0] / 127.0f : 0.0f; break;
            case 1: v.name = "Velocity"; v.value = juce::String ((int) st.vel); v.norm = st.vel / 127.0f; break;
            case 2: v.name = "Length"; v.value = st.len >= 100 ? juce::String (st.len / 100.0f, 2) + " steps" : juce::String ((int) st.len) + "%"; v.norm = juce::jmin (1.0f, st.len / 400.0f); break;
            case 3: v.name = "Slide"; v.value = st.slide ? "On" : "Off"; v.norm = st.slide ? 1.0f : 0.0f; break;
            case 4: v.name = "Accent"; v.value = st.accent ? "On" : "Off"; v.norm = st.accent ? 1.0f : 0.0f; break;
            case 5: v.name = "Chance"; v.value = juce::String ((int) st.chance) + "%"; v.norm = st.chance / 100.0f; break;
            case 6: v.name = "Nudge"; v.value = juce::String ((int) st.nudge) + "%"; v.norm = 0.5f + st.nudge / 100.0f; v.bipolar = true; break;
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
            if (tr.isDrums()) { v.name = "Drum"; v.value = juce::String (lane[(size_t) t] + 1) + " " + engineDef (tr.engine).params[(size_t) lane[(size_t) t] * 4].name.upToFirstOccurrenceOf (" tune", false, false); v.norm = lane[(size_t) t] / 15.0f; }
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
    if (mode.load() == ModeSeq && numHeldSteps > 0)
        runOnMessageThread ([this, note, velocity] { if (numHeldSteps > 0) stepKey (note, velocity); });
    audio.post ({ Event::NoteOn, (int8_t) t, (int16_t) juce::jlimit (0, 127, note), juce::jlimit (1, 127, velocity) / 127.0f });
}

void Controller::keyUp (int note)
{
    if (isRemote()) { runOnMessageThread ([this, note] { auto* o = new juce::DynamicObject(); o->setProperty ("t", "key"); o->setProperty ("n", note); o->setProperty ("down", false); sendRemote (o); }); return; }
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
        audio.push (project);
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
    else if (t == "hello")  reply (makeState());
}
} // namespace catbox
