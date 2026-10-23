#include "BoxModel.h"
#include "CatSounds.h"
#include <cmath>

namespace catbox
{
namespace
{
ParamDesc percent (const char* name, float def, float min = 0.0f, float max = 1.0f)
{
    ParamDesc d; d.name = name; d.kind = Kind::Percent; d.min = min; d.max = max; d.def = def; return d;
}
ParamDesc choice (const char* name, juce::StringArray choices, float def)
{
    ParamDesc d; d.name = name; d.kind = Kind::Choice; d.min = 0.0f;
    d.max = (float) juce::jmax (0, choices.size() - 1); d.def = def; d.choices = std::move (choices); return d;
}
ParamDesc integer (const char* name, float min, float max, float def)
{
    ParamDesc d; d.name = name; d.kind = Kind::Integer; d.min = min; d.max = max; d.def = def; return d;
}
ParamDesc global (ParamDesc d, int g) { d.global = g; return d; }
ParamDesc perDrum (ParamDesc d) { d.perDrum = true; return d; }

using Table = std::array<std::array<ParamDesc, numSlots>, NumPages>;

Table buildTable()
{
    Table t;
    ParamDesc tempo; tempo.name = "Tempo"; tempo.kind = Kind::Bpm; tempo.min = 40; tempo.max = 240; tempo.def = 110;
    const auto tempoG  = global (tempo, GTempo);
    const auto keyG    = global (choice ("Key", keyNames(), 0), GKey);
    const auto scaleG  = global (choice ("Scale", scaleNames(), 1), GScale);
    const auto masterG = global (percent ("Master", 0.8f), GMaster);

    for (auto& inst : t)
    {
        inst[slot::Tempo] = tempoG;
        inst[slot::Key] = keyG;
        inst[slot::Scale] = scaleG;
        inst[slot::Master] = masterG;
    }

    auto& d = t[Drums];
    d[slot::DKit]    = choice ("Kit", hub::DrumKit::soundNames(), 0);
    d[slot::DSwing]  = percent ("Swing", 0.0f);
    d[slot::DLength] = choice ("Length", { "16 steps", "32 steps", "64 steps" }, 0);
    d[slot::DHuman]  = percent ("Human", 0.0f);
    d[slot::DLevel]  = perDrum (percent ("Drum level", 1.0f));
    d[slot::DChance] = perDrum (percent ("Drum chance", 1.0f));
    d[slot::DCrush]  = percent ("Crush", 0.0f);
    d[slot::DDrive]  = percent ("Drive", 0.0f);
    d[slot::DFilter] = percent ("Filter", 1.0f);
    d[slot::DEcho]   = percent ("Echo", 0.0f);
    d[slot::DVolume] = percent ("Volume", 0.85f);
    d[slot::DChorus] = percent ("Chorus", 0.0f);

    auto& a = t[Arp];
    a[slot::ADivision]    = choice ("Division", { "1/4", "1/8", "1/8 T", "1/8 .", "1/16", "1/16 T", "1/16 .", "1/32" }, 4);
    a[slot::AMode]        = choice ("Mode", { "Up", "Down", "Up-down", "Down-up", "As played", "Random", "Converge", "Chord" }, 0);
    a[slot::AOctaves]     = integer ("Octaves", 1, 4, 2);
    a[slot::AGate]        = percent ("Gate", 0.6f, 0.1f, 1.0f);
    a[slot::ASwing]       = percent ("Swing", 0.0f, 0.0f, 0.75f);
    a[slot::ALength]      = integer ("Length", 1, 16, 16);
    a[slot::ARatchet]     = integer ("Ratchet", 1, 4, 1);
    a[slot::AProbability] = percent ("Chance", 1.0f);
    a[slot::AAccent]      = percent ("Accent", 0.5f);
    a[slot::AFilter]      = percent ("Filter", 1.0f);
    a[slot::AVolume]      = percent ("Volume", 0.85f);
    a[slot::ASound]       = choice ("Sound", arpSoundNames(), 2);
    a[slot::AArpOn]       = choice ("Arp", { "On", "Off (plain synth)" }, 0);
    a[slot::AKeys]        = choice ("Pads", { "Piano", "Scale" }, 0);
    a[slot::AEcho]        = percent ("Echo", 0.25f);
    a[slot::AChorus]      = percent ("Chorus", 0.0f);
    a[slot::ADrive]       = percent ("Drive", 0.0f);

    auto& b = t[Bass];
    b[slot::BSound]     = choice ("Sound", bassSoundNames(), 0);
    b[slot::BCutoff]    = percent ("Cutoff", 0.35f);
    b[slot::BResonance] = percent ("Resonance", 0.6f);
    b[slot::BEnvMod]    = percent ("Env mod", 0.55f);
    b[slot::BDecay]     = percent ("Decay", 0.4f);
    b[slot::BLength]    = integer ("Length", 1, maxSteps, 16);
    b[slot::BAccent]    = percent ("Accent", 0.6f);
    b[slot::BDrive]     = percent ("Drive", 0.2f);
    b[slot::BSlide]     = percent ("Slide time", 0.35f);
    b[slot::BOctave]    = integer ("Octave", -1, 1, 0);
    b[slot::BVolume]    = percent ("Volume", 0.8f);
    b[slot::BEcho]      = percent ("Echo", 0.0f);
    b[slot::BChorus]    = percent ("Chorus", 0.0f);
    b[slot::BWave]      = choice ("Wave", { "Saw", "Square" }, 0);
    b[slot::BSwing]     = percent ("Swing", 0.0f);
    b[slot::BFollow]    = choice ("Follow arp", { "Off", "On" }, 0);
    b[slot::BKeys]      = choice ("Pads", { "Piano", "Scale" }, 0);

    auto& f = t[PageFx];
    f[slot::FReverb]     = percent ("Reverb", 0.6f);
    f[slot::FSize]       = percent ("Room size", 0.6f);
    f[slot::FDelay]      = percent ("Echo", 0.0f);
    f[slot::FPump]       = percent ("Pump", 0.0f);
    f[slot::FDjFilter]   = percent ("DJ filter", 0.5f);
    f[slot::FTape]       = percent ("Tape", 0.0f);
    f[slot::FVinyl]      = percent ("Vinyl", 0.0f);
    f[slot::FCrush]      = percent ("Crush", 0.0f);
    f[slot::FGlue]       = percent ("Glue", 0.0f);
    f[slot::FWidth]      = percent ("Width", 0.5f);
    f[slot::FPumpTarget] = choice ("Pump on", { "Arp + Bass", "Bass", "Arp", "Everything" }, 0);
    f[slot::FSendDrums]  = percent ("Drums reverb", 0.1f);
    f[slot::FSendArp]    = percent ("Arp reverb", 0.3f);
    f[slot::FSendBass]   = percent ("Bass reverb", 0.0f);
    return t;
}

const Table& table()
{
    static const Table t = buildTable();
    return t;
}
} // namespace

juce::String ParamDesc::format (float value) const
{
    switch (kind)
    {
        case Kind::Percent: return juce::String (juce::roundToInt (value * 100.0f)) + " %";
        case Kind::Choice:  return choices[juce::jlimit (0, choices.size() - 1, juce::roundToInt (value))];
        case Kind::Integer: return juce::String (juce::roundToInt (value));
        case Kind::Bpm:     return juce::String (juce::roundToInt (value)) + " BPM";
        case Kind::None:    break;
    }
    return {};
}

const ParamDesc& paramDesc (int page, int slotIndex)
{
    return table()[(size_t) juce::jlimit (0, NumPages - 1, page)][(size_t) juce::jlimit (0, numSlots - 1, slotIndex)];
}

juce::String instrumentName (int i)   { return i == Drums ? "Drums" : i == Arp ? "Arp" : i == Bass ? "Bass" : "FX"; }
juce::String instrumentFolder (int i) { return instrumentName (i); }

//==============================================================================
juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

juce::StringArray scaleNames()
{
    return { "Major", "Minor", "Dorian", "Mixolydian", "Harmonic minor", "Major penta", "Minor penta", "Blues" };
}

const std::vector<int>& scaleIntervals (int scale)
{
    static const std::vector<std::vector<int>> scales {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
        { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 }, { 0, 3, 5, 6, 7, 10 } };
    return scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, scale)];
}

int scaleNote (int key, int scale, int degree, int baseNote)
{
    const auto& iv = scaleIntervals (scale);
    const int n = (int) iv.size();
    const int oct = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
    const int idx = degree - oct * n;
    return juce::jlimit (0, 127, baseNote + key + 12 * oct + iv[(size_t) idx]);
}

juce::String noteName (int midiNote)
{
    if (midiNote < 0) return "-";
    return keyNames()[midiNote % 12] + juce::String (midiNote / 12 - 1);
}

double arpDivisionPpq (int index)
{
    static const double v[] = { 1.0, 0.5, 1.0 / 3.0, 0.75, 0.25, 1.0 / 6.0, 0.375, 0.125 };
    return v[juce::jlimit (0, 7, index)];
}

int drumStepsFromChoice (float c)
{
    const int i = juce::jlimit (0, 2, juce::roundToInt (c));
    return i == 0 ? 16 : i == 1 ? 32 : 64;
}

int pianoPadNote (int pad, int lowC)
{
    static const int white[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
    static const int black[8] = { 1, 3, -1, 6, 8, 10, -1, 13 };   // between white key i and i + 1
    if (pad < 0 || pad >= numPads) return -1;
    const int off = pad >= 8 ? white[pad - 8] : black[pad];
    return off < 0 ? -1 : juce::jlimit (0, 127, lowC + off);
}

int drumPadNote (int pad)
{
    // first row: kick, snare, clap, rim / hats, ride, crash / toms, cowbell / shaker and spares
    static const int notes[numPads] = { 36, 38, 39, 37,  42, 46, 51, 49,  41, 45, 48, 56,  70, 35, 40, 57 };
    return notes[juce::jlimit (0, numPads - 1, pad)];
}

juce::String drumPadName (int pad)
{
    static const char* names[numPads] = { "Kick", "Snare", "Clap", "Rim", "Hat", "Open hat", "Ride", "Crash",
                                          "Tom 1", "Tom 2", "Tom 3", "Cowbell", "Shaker", "Kick 2", "Snare 2", "Crash 2" };
    return names[juce::jlimit (0, numPads - 1, pad)];
}

juce::StringArray arpSoundNames()
{
    auto s = hub::KeysSynth::soundNames();   // 8 sounds
    s.add ("Supersaw");
    s.add ("Acid");
    return s;
}

juce::StringArray bassSoundNames()
{
    juce::StringArray s { "Acid" };
    s.addArray (hub::BassSynth::soundNames());
    return s;
}

//==============================================================================
InstrumentState BoxData::defaults (int instrument)
{
    InstrumentState s;
    for (int i = 0; i < numSlots; ++i)
    {
        const auto& d = paramDesc (instrument, i);
        if (d.used() && d.global < 0 && ! d.perDrum) s.values[(size_t) i] = d.def;
    }
    s.drumLevel.fill (1.0f);
    s.drumChance.fill (1.0f);
    s.arpSteps.fill (StepOn);
    s.arpSteps[0] = s.arpSteps[8] = StepAccent;

    if (instrument == Drums)
    {
        // a plain starting beat so the box makes sound on its first start (change it on the pads)
        for (int st : { 0, 8 })        s.drumSteps[0][(size_t) st] = 110;   // kick
        for (int st : { 4, 12 })       s.drumSteps[1][(size_t) st] = 105;   // snare
        for (int st = 0; st < 16; st += 2) s.drumSteps[4][(size_t) st] = st % 4 == 0 ? 90 : 70;   // hat
    }
    if (instrument == Bass)
    {
        // C minor: a simple line to start from
        const int line[16] = { 36, -1, 36, -1, 48, -1, 36, 39, -1, 36, -1, 43, 36, -1, 46, 48 };
        for (int st = 0; st < 16; ++st) s.bassSteps[(size_t) st].note = line[st];
        s.bassSteps[7].slide = true;
        s.bassSteps[4].accent = s.bassSteps[12].accent = true;
    }
    return s;
}

std::array<float, numSlots> BoxData::fxDefaults()
{
    std::array<float, numSlots> v {};
    for (int i = 0; i < numSlots; ++i)
        if (const auto& d = paramDesc (PageFx, i); d.used() && d.global < 0) v[(size_t) i] = d.def;
    return v;
}

juce::var BoxData::fxToVar (const std::array<float, numSlots>& v)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("instrument", "FX");
    o->setProperty ("v", 2);
    juce::Array<juce::var> values;
    for (auto x : v) values.add (x);
    o->setProperty ("values", values);
    return juce::var (o);
}

bool BoxData::fxFromVar (const juce::var& v, std::array<float, numSlots>& out)
{
    if (! v.isObject() || v["instrument"].toString() != "FX") return false;
    auto fresh = fxDefaults();
    if (auto* values = v["values"].getArray())
        for (int i = 0; i < juce::jmin (numSlots, values->size()); ++i)
            if (const auto& d = paramDesc (PageFx, i); d.used() && d.global < 0)
                fresh[(size_t) i] = juce::jlimit (d.min, d.max, (float) (*values)[i]);
    out = fresh;
    return true;
}

juce::var BoxData::sceneToVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "catbox-scene-1");
    o->setProperty ("tempo", globals[GTempo]);
    o->setProperty ("key", globals[GKey]);
    o->setProperty ("scale", globals[GScale]);
    juce::Array<juce::var> insts;
    for (int i = 0; i < NumInstruments; ++i) insts.add (instrumentToVar (i, inst[(size_t) i]));
    o->setProperty ("instruments", insts);
    o->setProperty ("fx", fxToVar (fx));
    return juce::var (o);
}

bool BoxData::sceneFromVar (const juce::var& v)
{
    if (! v.isObject() || v["format"].toString() != "catbox-scene-1") return false;
    auto* insts = v["instruments"].getArray();
    if (insts == nullptr || insts->size() < NumInstruments) return false;
    std::array<InstrumentState, NumInstruments> fresh;
    for (int i = 0; i < NumInstruments; ++i)
        if (! instrumentFromVar (i, (*insts)[i], fresh[(size_t) i])) return false;
    inst = fresh;
    fxFromVar (v["fx"], fx);
    globals[GTempo] = juce::jlimit (40.0f, 240.0f, (float) v.getProperty ("tempo", globals[GTempo]));
    globals[GKey]   = (float) juce::jlimit (0, 11, (int) v.getProperty ("key", (int) globals[GKey]));
    globals[GScale] = (float) juce::jlimit (0, scaleNames().size() - 1, (int) v.getProperty ("scale", (int) globals[GScale]));
    return true;
}

BoxData::BoxData()
{
    for (int i = 0; i < NumInstruments; ++i) inst[(size_t) i] = defaults (i);
    fx = fxDefaults();
    globals[GTempo] = 110.0f;
    globals[GKey] = 0.0f;
    globals[GScale] = 1.0f;
    globals[GMaster] = 0.8f;
}

float BoxData::get (int instrument, int slotIndex) const
{
    const auto& d = paramDesc (instrument, slotIndex);
    if (! d.used()) return 0.0f;
    if (d.global >= 0) return globals[(size_t) d.global];
    if (instrument == PageFx) return fx[(size_t) slotIndex];
    const auto& s = inst[(size_t) juce::jlimit (0, NumInstruments - 1, instrument)];
    if (d.perDrum)
        return slotIndex == slot::DLevel ? s.drumLevel[(size_t) selectedDrum] : s.drumChance[(size_t) selectedDrum];
    return s.values[(size_t) slotIndex];
}

void BoxData::set (int instrument, int slotIndex, float value)
{
    const auto& d = paramDesc (instrument, slotIndex);
    if (! d.used()) return;
    value = juce::jlimit (d.min, d.max, value);
    if (d.kind != Kind::Percent) value = std::round (value);
    if (d.global >= 0) { globals[(size_t) d.global] = value; return; }
    if (instrument == PageFx) { fx[(size_t) slotIndex] = value; return; }
    auto& s = inst[(size_t) juce::jlimit (0, NumInstruments - 1, instrument)];
    if (d.perDrum)
        (slotIndex == slot::DLevel ? s.drumLevel : s.drumChance)[(size_t) selectedDrum] = value;
    else
        s.values[(size_t) slotIndex] = value;
}

//==============================================================================
juce::var BoxData::instrumentToVar (int instrument, const InstrumentState& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("instrument", instrumentFolder (instrument));
    o->setProperty ("v", 2);
    o->setProperty ("tempo", s.tempo);
    juce::Array<juce::var> values;
    for (auto v : s.values) values.add (v);
    o->setProperty ("values", values);
    if (instrument == Arp)
    {
        juce::Array<juce::var> steps;
        for (auto v : s.arpSteps) steps.add ((int) v);
        o->setProperty ("steps", steps);
    }
    if (instrument == Drums)
    {
        juce::Array<juce::var> hits, level, chance;
        for (int p = 0; p < numPads; ++p)
        {
            level.add (s.drumLevel[(size_t) p]);
            chance.add (s.drumChance[(size_t) p]);
            for (int st = 0; st < maxSteps; ++st)
                if (const int v = s.drumSteps[(size_t) p][(size_t) st]; v > 0)
                    hits.add (juce::Array<juce::var> { p, st, v });
        }
        o->setProperty ("hits", hits);
        o->setProperty ("level", level);
        o->setProperty ("chance", chance);
    }
    if (instrument == Bass)
    {
        juce::Array<juce::var> notes;
        for (int st = 0; st < maxSteps; ++st)
            if (const auto& b = s.bassSteps[(size_t) st]; b.note >= 0)
                notes.add (juce::Array<juce::var> { st, b.note, b.slide ? 1 : 0, b.accent ? 1 : 0 });
        o->setProperty ("notes", notes);
    }
    return juce::var (o);
}

bool BoxData::instrumentFromVar (int instrument, const juce::var& v, InstrumentState& s)
{
    if (! v.isObject() || v["instrument"].toString() != instrumentFolder (instrument) || (int) v.getProperty ("v", 1) < 2)
        return false;
    InstrumentState fresh = defaults (instrument);
    if (instrument != Arp) { for (auto& p : fresh.drumSteps) p.fill (0); for (auto& b : fresh.bassSteps) b = {}; }
    if (v.hasProperty ("tempo")) fresh.tempo = juce::jlimit (40.0f, 240.0f, (float) v["tempo"]);
    if (auto* values = v["values"].getArray())
        for (int i = 0; i < juce::jmin (numSlots, values->size()); ++i)
        {
            const auto& d = paramDesc (instrument, i);
            if (d.used() && d.global < 0 && ! d.perDrum)
                fresh.values[(size_t) i] = juce::jlimit (d.min, d.max, (float) (*values)[i]);
        }
    if (auto* steps = v["steps"].getArray())
        for (int i = 0; i < juce::jmin (16, steps->size()); ++i)
            fresh.arpSteps[(size_t) i] = (uint8_t) juce::jlimit (0, NumStepStates - 1, (int) (*steps)[i]);
    if (auto* hits = v["hits"].getArray())
        for (const auto& h : *hits)
            if (auto* a = h.getArray(); a != nullptr && a->size() >= 3)
            {
                const int p = (int) (*a)[0], st = (int) (*a)[1];
                if (p >= 0 && p < numPads && st >= 0 && st < maxSteps)
                    fresh.drumSteps[(size_t) p][(size_t) st] = (uint8_t) juce::jlimit (1, 127, (int) (*a)[2]);
            }
    if (auto* lv = v["level"].getArray())
        for (int p = 0; p < juce::jmin (numPads, lv->size()); ++p) fresh.drumLevel[(size_t) p] = juce::jlimit (0.0f, 1.0f, (float) (*lv)[p]);
    if (auto* ch = v["chance"].getArray())
        for (int p = 0; p < juce::jmin (numPads, ch->size()); ++p) fresh.drumChance[(size_t) p] = juce::jlimit (0.0f, 1.0f, (float) (*ch)[p]);
    if (auto* notes = v["notes"].getArray())
        for (const auto& n : *notes)
            if (auto* a = n.getArray(); a != nullptr && a->size() >= 4)
            {
                const int st = (int) (*a)[0];
                if (st < 0 || st >= maxSteps) continue;
                auto& b = fresh.bassSteps[(size_t) st];
                b.note = juce::jlimit (0, 127, (int) (*a)[1]);
                b.slide = (int) (*a)[2] != 0;
                b.accent = (int) (*a)[3] != 0;
            }
    s = std::move (fresh);
    return true;
}

juce::var BoxData::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "catbox-2");
    o->setProperty ("tempo", globals[GTempo]);
    o->setProperty ("key", globals[GKey]);
    o->setProperty ("scale", globals[GScale]);
    o->setProperty ("master", globals[GMaster]);
    o->setProperty ("page", page);
    o->setProperty ("selectedDrum", selectedDrum);
    o->setProperty ("light", lightScreen);
    o->setProperty ("fx", fxToVar (fx));
    o->setProperty ("fxPreset", loadedPreset[PageFx]);
    o->setProperty ("scene", loadedScene);
    juce::Array<juce::var> insts;
    for (int i = 0; i < NumInstruments; ++i)
    {
        auto v = instrumentToVar (i, inst[(size_t) i]);
        v.getDynamicObject()->setProperty ("muted", muted[(size_t) i]);
        v.getDynamicObject()->setProperty ("padOctave", padOctave[(size_t) i]);
        v.getDynamicObject()->setProperty ("preset", loadedPreset[(size_t) i]);
        insts.add (v);
    }
    o->setProperty ("instruments", insts);
    return juce::var (o);
}

void BoxData::fromVar (const juce::var& v)
{
    if (! v.isObject() || v["format"].toString() != "catbox-2") return;   // older memory: start fresh
    *this = BoxData();
    globals[GTempo]  = juce::jlimit (40.0f, 240.0f, (float) v.getProperty ("tempo", 110.0f));
    globals[GKey]    = (float) juce::jlimit (0, 11, (int) v.getProperty ("key", 0));
    globals[GScale]  = (float) juce::jlimit (0, scaleNames().size() - 1, (int) v.getProperty ("scale", 1));
    globals[GMaster] = juce::jlimit (0.0f, 1.0f, (float) v.getProperty ("master", 0.8f));
    page = juce::jlimit (0, NumPages - 1, (int) v.getProperty ("page", 0));
    fxFromVar (v["fx"], fx);
    loadedPreset[PageFx] = juce::jlimit (-1, numPresetSlots - 1, (int) v.getProperty ("fxPreset", -1));
    loadedScene = juce::jlimit (-1, numPresetSlots - 1, (int) v.getProperty ("scene", -1));
    selectedDrum = juce::jlimit (0, numPads - 1, (int) v.getProperty ("selectedDrum", 0));
    lightScreen = (bool) v.getProperty ("light", false);
    if (auto* insts = v["instruments"].getArray())
        for (int i = 0; i < juce::jmin ((int) NumInstruments, insts->size()); ++i)
        {
            const auto& iv = (*insts)[i];
            instrumentFromVar (i, iv, inst[(size_t) i]);
            muted[(size_t) i] = (bool) iv.getProperty ("muted", false);
            padOctave[(size_t) i] = juce::jlimit (-2, 2, (int) iv.getProperty ("padOctave", 0));
            loadedPreset[(size_t) i] = juce::jlimit (-1, numPresetSlots - 1, (int) iv.getProperty ("preset", -1));
        }
}

} // namespace catbox
