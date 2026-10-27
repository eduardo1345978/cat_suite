#include "Project.h"
#include "Samples.h"

namespace catbox
{
juce::StringArray revTypeNames() { return { "Hall", "Room", "Plate", "Shimmer", "Cloud", "Spring", "Dark" }; }
juce::StringArray delayTypeNames() { return { "Clean", "Tape echo", "Dub", "Ping-pong", "Multi-tap", "Diffused", "Reverse" }; }

juce::StringArray delayNames() { return { "1/32", "1/16", "1/16 T", "1/8 T", "1/8", "1/8 .", "1/4 T", "1/4", "1/4 .", "1/2" }; }

double delayBeats (int i)
{
    static const double b[] = { 0.125, 0.25, 1.0 / 6.0, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
    return b[juce::jlimit (0, 9, i)];
}

void loadEnginePreset (Track& t, int engine, int preset)
{
    t.engine = juce::jlimit (0, NumEngineTypes - 1, engine);
    engineDef (t.engine).applyPreset (preset, t.params.data());
}

void loadFxPreset (Track& t, int fxType, int preset)
{
    t.fxType = juce::jlimit (0, NumNoteFx - 1, fxType);
    const auto& d = noteFxDef (t.fxType);
    float v[maxParams] {};
    d.applyPreset (preset, v);
    for (int i = 0; i < numFxParams; ++i) t.fx[(size_t) i] = v[i];
}

namespace
{
int presetIndex (int engine, const char* name)
{
    const auto& d = engineDef (engine);
    for (size_t i = 0; i < d.presets.size(); ++i) if (d.presets[i].name == name) return (int) i;
    return 0;
}
}

Project Project::makeDefault()
{
    Project p;
    auto setup = [&p] (int t, int engine, const char* preset, float vol)
    {
        auto& tr = p.tracks[(size_t) t];
        loadEnginePreset (tr, engine, presetIndex (engine, preset));
        loadFxPreset (tr, FxOff, 0);
        tr.volume = vol;
    };
    setup (0, EngineDrums, "808 Classic", 0.8f);
    setup (1, EngineAcid, "Classic 303", 0.7f);
    setup (2, EngineKeys, "Rhodes", 0.7f);
    setup (3, EngineAnalog, "Classic Lead", 0.7f);
    setup (4, EngineGlide, "Warm Analog Pad", 0.75f);
    setup (5, EngineDrums, "909 Classic", 0.7f);
    setup (6, EngineAnalog, "Pluck", 0.7f);
    setup (7, EngineKeys, "Grand Piano", 0.7f);
    p.tracks[2].sendA = 0.25f;
    p.tracks[3].sendB = 0.2f;
    p.tracks[4].sendA = 0.35f;
    p.tracks[6].sendB = 0.25f;

    // a beat and a bass line, so Play makes music right away
    auto& d = p.tracks[0].pat;
    for (int s : { 0, 4, 8, 12 }) d.drums[0][(size_t) s] = 110;          // kick
    for (int s : { 4, 12 }) d.drums[1][(size_t) s] = 100;                // snare
    for (int s = 2; s < 16; s += 4) d.drums[3][(size_t) s] = 80;         // closed hat
    d.drums[3][14] = 0;
    d.drums[4][14] = 85;                                                 // open hat
    auto& b = p.tracks[1].pat;
    const int line[16] = { 36, -1, 36, 48, -1, 36, 39, -1, 36, -1, 43, 36, -1, 46, 36, 39 };
    for (int s = 0; s < 16; ++s)
        if (line[s] >= 0)
        {
            b.steps[(size_t) s].notes[0] = (int8_t) line[s];
            b.steps[(size_t) s].accent = s == 3 || s == 10;
            b.steps[(size_t) s].slide = s == 6 || s == 13;
            b.steps[(size_t) s].len = 60;
        }
    return p;
}

//==============================================================================
juce::var soundToVar (const Track& t)
{
    auto* o = new juce::DynamicObject();
    const auto& d = engineDef (t.engine);
    o->setProperty ("engine", d.name);
    auto* vals = new juce::DynamicObject();
    for (size_t i = 0; i < d.params.size(); ++i)
        if (t.params[i] != d.params[i].def) vals->setProperty (d.params[i].id, t.params[i]);
    o->setProperty ("values", juce::var (vals));
    if (t.engine == EngineSampler)   // the sample by name (its slot can change when files are added)
        o->setProperty ("sampleName", SampleBank::get().name (juce::roundToInt (t.params[(size_t) d.index ("sample")])));
    o->setProperty ("fx", noteFxNames()[t.fxType]);
    juce::Array<juce::var> fx;
    for (auto v : t.fx) fx.add (v);
    o->setProperty ("fxValues", fx);
    o->setProperty ("mods", modsToVar (t.mods, t.engine));
    return juce::var (o);
}

void soundFromVar (const juce::var& v, Track& t)
{
    if (! v.isObject()) return;
    const int e = juce::jmax (0, engineNames().indexOf (v["engine"].toString()));
    t.engine = e;
    const auto& d = engineDef (e);
    d.defaults (t.params.data());
    if (auto* o = v["values"].getDynamicObject())
        for (const auto& prop : o->getProperties())
            if (const int i = d.index (prop.name.toString()); i >= 0) t.params[(size_t) i] = d.params[(size_t) i].clamp ((float) prop.value);
    if (e == EngineSampler && v.hasProperty ("sampleName"))
        if (const int slot = SampleBank::get().find (v["sampleName"].toString()); slot >= 0) t.params[(size_t) d.index ("sample")] = (float) slot;
    if (v.hasProperty ("fx"))
    {
        t.fxType = juce::jmax (0, noteFxNames().indexOf (v["fx"].toString()));
        loadFxPreset (t, t.fxType, 0);
        if (auto* a = v["fxValues"].getArray())
            for (int i = 0; i < juce::jmin (numFxParams, a->size()); ++i) t.fx[(size_t) i] = (float) (*a)[i];
    }
    modsFromVar (v["mods"], t.mods, e);
}

juce::var patternToVar (const Pattern& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("length", p.length);
    o->setProperty ("rate", p.rate);
    juce::Array<juce::var> steps;
    for (int i = 0; i < maxSteps; ++i)
    {
        const auto& s = p.steps[(size_t) i];
        if (! s.on()) continue;
        juce::Array<juce::var> notes;
        for (auto n : s.notes) if (n >= 0) notes.add ((int) n);
        auto* so = new juce::DynamicObject();
        so->setProperty ("i", i);
        so->setProperty ("n", notes);
        so->setProperty ("v", (int) s.vel);
        so->setProperty ("l", (int) s.len);
        if (s.slide) so->setProperty ("s", true);
        if (s.accent) so->setProperty ("a", true);
        if (s.chance < 100) so->setProperty ("c", (int) s.chance);
        if (s.nudge != 0) so->setProperty ("u", (int) s.nudge);
        if (s.ratchet > 1) so->setProperty ("r", (int) s.ratchet);
        steps.add (juce::var (so));
    }
    o->setProperty ("steps", steps);
    juce::Array<juce::var> lanes;
    for (const auto& lane : p.drums)
    {
        juce::String s;
        for (int i = 0; i < maxSteps; ++i) s << juce::String::toHexString ((int) lane[(size_t) i]).paddedLeft ('0', 2);
        lanes.add (s.trimCharactersAtEnd ("0"));
    }
    o->setProperty ("drums", lanes);
    juce::Array<juce::var> rolls;
    bool anyRoll = false;
    for (const auto& lane : p.rolls)
    {
        juce::String s;
        for (int i = 0; i < maxSteps; ++i) { s << juce::String (juce::jlimit (0, 4, (int) lane[(size_t) i])); anyRoll = anyRoll || lane[(size_t) i] > 1; }
        rolls.add (s.trimCharactersAtEnd ("0"));
    }
    if (anyRoll) o->setProperty ("rolls", rolls);
    juce::Array<juce::var> locks;
    for (int i = 0; i < maxSteps; ++i)
        for (const auto& l : p.locks[(size_t) i])
            if (l.param >= 0) locks.add (juce::Array<juce::var> { i, (int) l.param, l.value });
    if (! locks.isEmpty()) o->setProperty ("locks", locks);
    return juce::var (o);
}

void patternFromVar (const juce::var& v, Pattern& p)
{
    p = Pattern();
    if (! v.isObject()) return;
    if (auto* a = v["locks"].getArray())
        for (const auto& l : *a)
            if (auto* x = l.getArray(); x != nullptr && x->size() == 3)
            {
                const int i = (int) (*x)[0], param = (int) (*x)[1];
                if (i >= 0 && i < maxSteps && param >= 0 && param < maxParams) p.setLock (i, param, (float) (*x)[2]);
            }
    p.length = juce::jlimit (1, maxSteps, (int) v.getProperty ("length", 16));
    p.rate = juce::jlimit (0, NumRates - 1, (int) v.getProperty ("rate", (int) Rate16));
    if (auto* a = v["steps"].getArray())
        for (const auto& sv : *a)
        {
            const int i = (int) sv["i"];
            if (i < 0 || i >= maxSteps) continue;
            auto& s = p.steps[(size_t) i];
            if (auto* ns = sv["n"].getArray())
                for (int k = 0; k < juce::jmin (stepNotes, ns->size()); ++k) s.notes[(size_t) k] = (int8_t) juce::jlimit (0, 127, (int) (*ns)[k]);
            s.vel = (uint8_t) juce::jlimit (1, 127, (int) sv.getProperty ("v", 100));
            s.len = (uint16_t) juce::jlimit (10, 1600, (int) sv.getProperty ("l", 75));
            s.slide = (bool) sv.getProperty ("s", false);
            s.accent = (bool) sv.getProperty ("a", false);
            s.chance = (uint8_t) juce::jlimit (0, 100, (int) sv.getProperty ("c", 100));
            s.nudge = (int8_t) juce::jlimit (-50, 50, (int) sv.getProperty ("u", 0));
            s.ratchet = (uint8_t) juce::jlimit (1, 4, (int) sv.getProperty ("r", 1));
        }
    if (auto* rl = v["rolls"].getArray())
        for (int l = 0; l < juce::jmin (numLanes, rl->size()); ++l)
        {
            const auto s = (*rl)[l].toString();
            for (int i = 0; i < juce::jmin (maxSteps, s.length()); ++i) p.rolls[(size_t) l][(size_t) i] = (uint8_t) juce::jlimit (0, 4, (int) (s[i] - '0'));
        }
    if (auto* lanes = v["drums"].getArray())
        for (int l = 0; l < juce::jmin (numLanes, lanes->size()); ++l)
        {
            const auto s = (*lanes)[l].toString().paddedRight ('0', maxSteps * 2);
            for (int i = 0; i < maxSteps; ++i)
                p.drums[(size_t) l][(size_t) i] = (uint8_t) juce::jlimit (0, 127, s.substring (i * 2, i * 2 + 2).getHexValue32());
        }
}

juce::var Project::toVar() const
{
    auto* o = new juce::DynamicObject();
    auto* mo = new juce::DynamicObject();
    mo->setProperty ("tempo", m.tempo); mo->setProperty ("swing", m.swing);
    mo->setProperty ("key", m.key); mo->setProperty ("scale", m.scale);
    mo->setProperty ("volume", m.volume); mo->setProperty ("glue", m.glue); mo->setProperty ("tape", m.tape); mo->setProperty ("width", m.width);
    mo->setProperty ("revSize", m.revSize); mo->setProperty ("revDamp", m.revDamp); mo->setProperty ("revWidth", m.revWidth); mo->setProperty ("revLowCut", m.revLowCut);
    mo->setProperty ("delayTime", m.delayTime); mo->setProperty ("delayFb", m.delayFb); mo->setProperty ("delayTone", m.delayTone);
    mo->setProperty ("delayPingPong", m.delayPingPong);
    mo->setProperty ("metronome", m.metronome); mo->setProperty ("countIn", m.countIn);
    mo->setProperty ("revType", m.revType); mo->setProperty ("revPre", m.revPre); mo->setProperty ("revDuck", m.revDuck); mo->setProperty ("revLevel", m.revLevel);
    mo->setProperty ("delayType", m.delayType); mo->setProperty ("delayWow", m.delayWow); mo->setProperty ("delayDuck", m.delayDuck);
    mo->setProperty ("delayToRev", m.delayToRev); mo->setProperty ("delayLevel", m.delayLevel);
    mo->setProperty ("fxLevel", m.fxLevel); mo->setProperty ("fxDuck", m.fxDuck);
    {
        auto* fo = new juce::DynamicObject();
        fo->setProperty ("type", insertDef (m.fxSend.type).name);
        juce::Array<juce::var> pv;
        for (auto x : m.fxSend.p) pv.add (x);
        fo->setProperty ("p", pv);
        mo->setProperty ("fxSend", juce::var (fo));
    }
    o->setProperty ("master", juce::var (mo));
    juce::Array<juce::var> tr;
    for (const auto& t : tracks)
    {
        auto* to = new juce::DynamicObject();
        to->setProperty ("sound", soundToVar (t));
        to->setProperty ("pattern", patternToVar (t.pat));
        to->setProperty ("volume", t.volume); to->setProperty ("pan", t.pan);
        to->setProperty ("sendA", t.sendA); to->setProperty ("sendB", t.sendB); to->setProperty ("sendC", t.sendC);
        to->setProperty ("mute", t.mute); to->setProperty ("solo", t.solo);
        tr.add (juce::var (to));
    }
    o->setProperty ("tracks", tr);
    o->setProperty ("version", 2);
    return juce::var (o);
}

void Project::fromVar (const juce::var& v)
{
    if (! v.isObject() || (int) v.getProperty ("version", 0) != 2) return;
    const auto mv = v["master"];
    auto f = [&mv] (const char* id, float def) { return (float) mv.getProperty (id, def); };
    const Master d;
    m.tempo = juce::jlimit (40.0f, 240.0f, f ("tempo", d.tempo)); m.swing = f ("swing", d.swing);
    m.key = juce::jlimit (0, 11, (int) mv.getProperty ("key", d.key)); m.scale = (int) mv.getProperty ("scale", d.scale);
    m.volume = f ("volume", d.volume); m.glue = f ("glue", d.glue); m.tape = f ("tape", d.tape); m.width = f ("width", d.width);
    m.revSize = f ("revSize", d.revSize); m.revDamp = f ("revDamp", d.revDamp); m.revWidth = f ("revWidth", d.revWidth); m.revLowCut = f ("revLowCut", d.revLowCut);
    m.delayTime = (int) mv.getProperty ("delayTime", d.delayTime); m.delayFb = f ("delayFb", d.delayFb); m.delayTone = f ("delayTone", d.delayTone);
    m.delayPingPong = f ("delayPingPong", d.delayPingPong);
    m.metronome = (bool) mv.getProperty ("metronome", false); m.countIn = (int) mv.getProperty ("countIn", 0);
    m.revType = juce::jlimit (0, revTypeNames().size() - 1, (int) mv.getProperty ("revType", 0));
    m.revPre = f ("revPre", d.revPre); m.revDuck = f ("revDuck", d.revDuck); m.revLevel = f ("revLevel", d.revLevel);
    m.delayType = juce::jlimit (0, delayTypeNames().size() - 1, (int) mv.getProperty ("delayType", 0));
    m.delayWow = f ("delayWow", d.delayWow); m.delayDuck = f ("delayDuck", d.delayDuck);
    m.delayToRev = f ("delayToRev", d.delayToRev); m.delayLevel = f ("delayLevel", d.delayLevel);
    m.fxLevel = f ("fxLevel", d.fxLevel); m.fxDuck = f ("fxDuck", d.fxDuck);
    m.fxSend = d.fxSend;
    if (const auto fv = mv["fxSend"]; fv.isObject())
    {
        m.fxSend.type = juce::jmax (0, insertNames().indexOf (fv["type"].toString()));
        insertDef (m.fxSend.type).defaults (m.fxSend.p.data());
        if (auto* pa = fv["p"].getArray())
            for (int i = 0; i < juce::jmin (insertParams, pa->size()); ++i) m.fxSend.p[(size_t) i] = (float) (*pa)[i];
    }
    if (auto* a = v["tracks"].getArray())
        for (int i = 0; i < juce::jmin (numTracks, a->size()); ++i)
        {
            const auto tv = (*a)[i];
            auto& t = tracks[(size_t) i];
            soundFromVar (tv["sound"], t);
            patternFromVar (tv["pattern"], t.pat);
            t.volume = (float) tv.getProperty ("volume", 0.75f); t.pan = (float) tv.getProperty ("pan", 0.0f);
            t.sendA = (float) tv.getProperty ("sendA", 0.12f); t.sendB = (float) tv.getProperty ("sendB", 0.0f);
            t.sendC = (float) tv.getProperty ("sendC", 0.0f);
            t.mute = (bool) tv.getProperty ("mute", false); t.solo = (bool) tv.getProperty ("solo", false);
        }
}
} // namespace catbox
