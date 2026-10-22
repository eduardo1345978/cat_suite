#include "BoxModel.h"
#include "CatSounds.h"
#include "DrumCatEngine.h"
#include "BassEngine.h"
#include <cmath>

namespace catbox
{
namespace
{
ParamDesc percent (const char* name, float def, float min = 0.0f, float max = 1.0f)
{
    ParamDesc d; d.name = sk (name); d.kind = Kind::Percent; d.min = min; d.max = max; d.def = def; return d;
}
ParamDesc choice (const char* name, juce::StringArray choices, float def)
{
    ParamDesc d; d.name = sk (name); d.kind = Kind::Choice; d.min = 0.0f;
    d.max = (float) juce::jmax (0, choices.size() - 1); d.def = def; d.choices = std::move (choices); return d;
}
ParamDesc integer (const char* name, float min, float max, float def)
{
    ParamDesc d; d.name = sk (name); d.kind = Kind::Integer; d.min = min; d.max = max; d.def = def; return d;
}
ParamDesc global (ParamDesc d, int g) { d.global = g; return d; }

juce::StringArray barChoices() { return { sk ("1 takt"), sk ("2 takty"), sk ("4 takty") }; }

using Table = std::array<std::array<ParamDesc, numSlots>, NumInstruments>;

Table buildTable()
{
    Table t;
    ParamDesc tempo; tempo.name = sk ("Tempo"); tempo.kind = Kind::Bpm; tempo.min = 60; tempo.max = 180; tempo.def = 110;
    const auto tempoG  = global (tempo, GTempo);
    const auto keyG    = global (choice ("Tónina", keyNames(), 0), GKey);
    const auto scaleG  = global (choice ("Stupnica", scaleNames(), 1), GScale);
    const auto masterG = global (percent ("Master", 0.8f), GMaster);

    for (auto& inst : t)
    {
        inst[slot::Tempo] = tempoG;
        inst[slot::Key] = keyG;
        inst[slot::Scale] = scaleG;
        inst[slot::Master] = masterG;
    }

    auto& d = t[Drums];
    d[slot::DStyle]   = choice ("Štýl", drumcat::getStyleNames(), 0);
    d[slot::DDensity] = percent ("Hustota", 0.5f);
    d[slot::DFills]   = percent ("Breaky", 0.35f);
    d[slot::DSwing]   = percent ("Swing", 0.0f);
    d[slot::DKit]     = choice ("Súprava", hub::DrumKit::soundNames(), 0);
    d[slot::DBars]    = choice ("Dĺžka", barChoices(), 1);
    d[slot::DSeed]    = integer ("Variácia", 1, 99, 1);
    d[slot::DHuman]   = percent ("Human", 0.5f);
    d[slot::DCrush]   = percent ("Crush", 0.0f);
    d[slot::DFilter]  = percent ("Filter", 1.0f);
    d[slot::DVolume]  = percent ("Hlasitosť", 0.85f);
    d[slot::DEcho]    = percent ("Echo", 0.0f);
    d[slot::DDrive]   = percent ("Drive", 0.0f);
    d[slot::DChorus]  = percent ("Chorus", 0.0f);

    auto& a = t[Arp];
    a[slot::ADivision]    = choice ("Delenie", { "1/4", "1/8", "1/8 T", "1/8 .", "1/16", "1/16 T", "1/16 .", "1/32" }, 4);
    a[slot::AMode]        = choice ("Režim", { sk ("Hore"), sk ("Dole"), sk ("Hore-dole"), sk ("Dole-hore"), sk ("Ako hráš"),
                                               sk ("Náhodne"), sk ("Zbiehavo"), sk ("Akord") }, 0);
    a[slot::AOctaves]     = integer ("Oktávy", 1, 4, 2);
    a[slot::AGate]        = percent ("Gate", 0.6f, 0.1f, 1.0f);
    a[slot::ASwing]       = percent ("Swing", 0.0f, 0.0f, 0.75f);
    a[slot::ALength]      = integer ("Dĺžka", 1, 16, 16);
    a[slot::ARatchet]     = integer ("Ratchet", 1, 4, 1);
    a[slot::AProbability] = percent ("Šanca", 1.0f);
    a[slot::AAccent]      = percent ("Akcent", 0.5f);
    a[slot::AFilter]      = percent ("Filter", 1.0f);
    a[slot::AVolume]      = percent ("Hlasitosť", 0.6f);
    a[slot::ASound]       = choice ("Zvuk", hub::KeysSynth::soundNames(), 2);
    a[slot::AEcho]        = percent ("Echo", 0.25f);
    a[slot::AChorus]      = percent ("Chorus", 0.0f);
    a[slot::ADrive]       = percent ("Drive", 0.0f);

    auto& b = t[Bass];
    b[slot::BStyle]   = choice ("Štýl", basscat::getStyleNames(), 0);
    b[slot::BDensity] = percent ("Hustota", 0.5f);
    b[slot::BGhosts]  = percent ("Ghost tóny", 0.3f);
    b[slot::BSlides]  = percent ("Slidy", 0.3f);
    b[slot::BSound]   = choice ("Zvuk", hub::BassSynth::soundNames(), 1);
    b[slot::BBars]    = choice ("Dĺžka", barChoices(), 1);
    b[slot::BSeed]    = integer ("Variácia", 1, 99, 1);
    b[slot::BDrive]   = percent ("Drive", 0.1f);
    b[slot::BFilter]  = percent ("Filter", 1.0f);
    b[slot::BOctave]  = integer ("Oktáva", -1, 1, 0);
    b[slot::BVolume]  = percent ("Hlasitosť", 0.8f);
    b[slot::BEcho]    = percent ("Echo", 0.0f);
    b[slot::BChorus]  = percent ("Chorus", 0.0f);
    b[slot::BFills]   = percent ("Breaky", 0.35f);
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

const ParamDesc& paramDesc (int instrument, int slotIndex)
{
    return table()[(size_t) juce::jlimit (0, NumInstruments - 1, instrument)][(size_t) juce::jlimit (0, numSlots - 1, slotIndex)];
}

juce::String instrumentName (int i)   { return i == Drums ? "Bubny" : i == Arp ? "Arp" : "Bass"; }
juce::String instrumentFolder (int i) { return i == Drums ? "Drums" : i == Arp ? "Arp" : "Bass"; }

//==============================================================================
juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" }; }

juce::StringArray scaleNames()
{
    return { sk ("Dur"), sk ("Mol"), sk ("Dórska"), sk ("Mixolýdická"), sk ("Harmonická mol"),
             sk ("Pentatonika dur"), sk ("Pentatonika mol"), sk ("Blues") };
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

juce::String degreeChordName (int key, int scale, int degree)
{
    const int root = scaleNote (key, scale, degree, 48);
    const int third = scaleNote (key, scale, degree + 2, 48) - root;
    const int fifth = scaleNote (key, scale, degree + 4, 48) - root;
    juce::String name = keyNames()[root % 12];
    if (third == 3) name << (fifth == 6 ? "dim" : "m");
    else if (fifth == 8) name << "aug";
    return name;
}

double arpDivisionPpq (int index)
{
    static const double v[] = { 1.0, 0.5, 1.0 / 3.0, 0.75, 0.25, 1.0 / 6.0, 0.375, 0.125 };
    return v[juce::jlimit (0, 7, index)];
}

int barsFromChoice (float c)
{
    const int i = juce::jlimit (0, 2, juce::roundToInt (c));
    return i == 0 ? 1 : i == 1 ? 2 : 4;
}

int drumPadNote (int pad)
{
    // bottom row first, like a drum machine: kick, snare, clap, rim / hats, ride, crash / toms / extras
    static const int notes[numPads] = { 36, 38, 39, 37,  42, 46, 51, 49,  41, 45, 48, 56,  70, 35, 40, 57 };
    return notes[juce::jlimit (0, numPads - 1, pad)];
}

juce::String drumPadName (int pad)
{
    static const char* names[numPads] = { "Kick", "Snare", "Clap", "Rim", "Hat", "Open hat", "Ride", "Crash",
                                          "Tom 1", "Tom 2", "Tom 3", "Cowbell", "Shaker", "Kick 2", "Snare 2", "Crash 2" };
    return sk (names[juce::jlimit (0, numPads - 1, pad)]);
}

//==============================================================================
BoxData::BoxData()
{
    for (int i = 0; i < NumInstruments; ++i)
    {
        for (int s = 0; s < numSlots; ++s)
        {
            const auto& d = paramDesc (i, s);
            if (d.used() && d.global < 0) inst[(size_t) i].values[(size_t) s] = d.def;
        }
        inst[(size_t) i].arpSteps.fill (StepOn);
    }
    auto& steps = inst[Arp].arpSteps;
    steps[0] = steps[8] = StepAccent;

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
    return inst[(size_t) juce::jlimit (0, NumInstruments - 1, instrument)].values[(size_t) slotIndex];
}

void BoxData::set (int instrument, int slotIndex, float value)
{
    const auto& d = paramDesc (instrument, slotIndex);
    if (! d.used()) return;
    value = juce::jlimit (d.min, d.max, value);
    if (d.kind != Kind::Percent) value = std::round (value);
    if (d.global >= 0) globals[(size_t) d.global] = value;
    else inst[(size_t) juce::jlimit (0, NumInstruments - 1, instrument)].values[(size_t) slotIndex] = value;
}

juce::var BoxData::instrumentToVar (int instrument, const InstrumentState& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("instrument", instrumentFolder (instrument));
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
        juce::Array<juce::var> hits;
        for (const auto& h : s.drumHits) hits.add (juce::Array<juce::var> { h.step, h.note, h.velocity });
        o->setProperty ("hits", hits);
    }
    return juce::var (o);
}

bool BoxData::instrumentFromVar (int instrument, const juce::var& v, InstrumentState& s)
{
    if (! v.isObject() || v["instrument"].toString() != instrumentFolder (instrument)) return false;
    InstrumentState fresh = BoxData().inst[(size_t) instrument];   // defaults for anything missing
    if (v.hasProperty ("tempo")) fresh.tempo = juce::jlimit (60.0f, 180.0f, (float) v["tempo"]);
    if (auto* values = v["values"].getArray())
        for (int i = 0; i < juce::jmin (numSlots, values->size()); ++i)
        {
            const auto& d = paramDesc (instrument, i);
            if (d.used() && d.global < 0)
                fresh.values[(size_t) i] = juce::jlimit (d.min, d.max, (float) (*values)[i]);
        }
    if (auto* steps = v["steps"].getArray())
        for (int i = 0; i < juce::jmin (16, steps->size()); ++i)
            fresh.arpSteps[(size_t) i] = (uint8_t) juce::jlimit (0, NumStepStates - 1, (int) (*steps)[i]);
    if (auto* hits = v["hits"].getArray())
        for (const auto& h : *hits)
            if (auto* a = h.getArray(); a != nullptr && a->size() >= 3)
                fresh.drumHits.push_back ({ juce::jlimit (0, 63, (int) (*a)[0]), juce::jlimit (0, 127, (int) (*a)[1]),
                                            juce::jlimit (1, 127, (int) (*a)[2]) });
    s = std::move (fresh);
    return true;
}

juce::var BoxData::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "catbox-1");
    o->setProperty ("tempo", globals[GTempo]);
    o->setProperty ("key", globals[GKey]);
    o->setProperty ("scale", globals[GScale]);
    o->setProperty ("master", globals[GMaster]);
    o->setProperty ("page", page);
    o->setProperty ("bassChord", bassChordDegree);
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
    if (! v.isObject()) return;
    *this = BoxData();
    globals[GTempo]  = juce::jlimit (60.0f, 180.0f, (float) v.getProperty ("tempo", 110.0f));
    globals[GKey]    = (float) juce::jlimit (0, 11, (int) v.getProperty ("key", 0));
    globals[GScale]  = (float) juce::jlimit (0, scaleNames().size() - 1, (int) v.getProperty ("scale", 1));
    globals[GMaster] = juce::jlimit (0.0f, 1.0f, (float) v.getProperty ("master", 0.8f));
    page = juce::jlimit (0, NumInstruments - 1, (int) v.getProperty ("page", 0));
    bassChordDegree = juce::jlimit (0, 15, (int) v.getProperty ("bassChord", 0));
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
