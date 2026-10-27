#include "TrackFx.h"
#include "Project.h"
#include <cmath>

namespace catbox
{
namespace
{
ParamDef mk (const char* id, const char* name, ParamDef::Unit u, float lo, float hi, float def, float skew = 1.0f, const char* suffix = "")
{
    ParamDef p; p.id = id; p.name = name; p.unit = u; p.lo = lo; p.hi = hi; p.def = def; p.skew = skew; p.suffix = suffix; return p;
}
ParamDef ch (const char* id, const char* name, juce::StringArray items, int def) { return defs::choice (id, name, std::move (items), def); }
Preset pr (const char* name, std::vector<std::pair<juce::String, float>> v) { Preset p; p.name = name; p.category = "FX"; p.values = std::move (v); return p; }

const juce::StringArray gateNames { "x.x.x.x.x.x.x.x.", "xx.xxx.xxx.x.xx.", "x..x..x...x..x..", "xxxx..xx..xxx.x.",
                                    "x.xxx.xxx.x.x.xx", "xxx.xxx.xxx.xxx.", "x...x...x.x.x...", "xx.x.xx.x.xxx.x." };

std::array<InsertDef, NumInsertTypes> makeInsertDefs()
{
    std::array<InsertDef, NumInsertTypes> d;
    d[InsOff] = { "Off", "-", {}, {} };
    d[InsFilter] = { "Filter", "FILT", {
        ch ("mode", "Type", { "Low pass", "Band pass", "High pass", "Notch", "Low pass 24" }, 0),
        defs::hz ("cutoff", "Cutoff", 20.0f, 20000.0f, 2000.0f),
        defs::pct ("res", "Resonance", 0.2f),
        defs::pct ("drive", "Drive", 0.0f),
        defs::bi ("follow", "Follow", 0.0f),
        defs::pct ("mix", "Mix", 1.0f) },
        { pr ("Warm low pass", { { "cutoff", 1800.0f }, { "res", 0.25f } }), pr ("Thin", { { "mode", 2 }, { "cutoff", 600.0f } }),
          pr ("Wah follow", { { "mode", 1 }, { "cutoff", 500.0f }, { "res", 0.6f }, { "follow", 0.8f } }),
          pr ("Acid squelch", { { "mode", 4 }, { "cutoff", 700.0f }, { "res", 0.8f }, { "drive", 0.4f }, { "follow", 0.5f } }),
          pr ("Telephone", { { "mode", 1 }, { "cutoff", 1500.0f }, { "res", 0.45f }, { "drive", 0.3f } }) } };
    d[InsDrive] = { "Drive", "DRV", {
        ch ("type", "Type", { "Soft", "Hard", "Tube", "Fold", "Fuzz", "Rectify" }, 0),
        defs::db ("drive", "Drive", 0.0f, 36.0f, 12.0f),
        defs::hz ("tone", "Tone", 500.0f, 20000.0f, 12000.0f),
        defs::bi ("bias", "Bias", 0.0f),
        defs::db ("out", "Output", -24.0f, 6.0f, 0.0f),
        defs::pct ("mix", "Mix", 1.0f) },
        { pr ("Warm", { { "type", 2 }, { "drive", 8.0f } }), pr ("Crunch", { { "type", 1 }, { "drive", 18.0f }, { "tone", 7000.0f } }),
          pr ("Fold", { { "type", 3 }, { "drive", 14.0f } }), pr ("Fuzz box", { { "type", 4 }, { "drive", 24.0f }, { "tone", 5000.0f } }),
          pr ("Parallel", { { "type", 0 }, { "drive", 24.0f }, { "mix", 0.35f } }) } };
    d[InsCrusher] = { "Crusher", "CRSH", {
        defs::num ("bits", "Bits", 1.0f, 16.0f, 8.0f),
        defs::hz ("rate", "Rate", 200.0f, 48000.0f, 12000.0f),
        defs::pct ("jitter", "Jitter", 0.0f),
        defs::pct ("mix", "Mix", 1.0f) },
        { pr ("8 bit", { { "bits", 8.0f }, { "rate", 16000.0f } }), pr ("Old sampler", { { "bits", 12.0f }, { "rate", 22050.0f } }),
          pr ("Broken", { { "bits", 4.0f }, { "rate", 3000.0f }, { "jitter", 0.4f } }), pr ("Dust", { { "bits", 6.0f }, { "rate", 9000.0f }, { "mix", 0.4f } }) } };
    d[InsDelay] = { "Delay", "DLY", {
        ch ("time", "Time", delayNames(), 4),
        defs::pct ("fb", "Feedback", 0.4f),
        defs::pct ("tone", "Tone", 0.5f),
        defs::pct ("pp", "Ping-pong", 0.0f),
        defs::pct ("wobble", "Wobble", 0.0f),
        defs::pct ("mix", "Mix", 0.35f) },
        { pr ("Eighth", { { "time", 4 } }), pr ("Dotted", { { "time", 5 }, { "fb", 0.5f } }),
          pr ("Ping-pong", { { "time", 4 }, { "pp", 1.0f } }), pr ("Dub", { { "time", 8 }, { "fb", 0.75f }, { "tone", 0.25f }, { "wobble", 0.3f } }),
          pr ("Slapback", { { "time", 0 }, { "fb", 0.1f }, { "mix", 0.3f } }) } };
    d[InsChorus] = { "Chorus", "CHOR", {
        mk ("rate", "Rate", ParamDef::Hz, 0.05f, 5.0f, 0.6f, 0.4f),
        defs::pct ("depth", "Depth", 0.5f),
        defs::pct ("width", "Width", 1.0f),
        defs::pct ("fb", "Feedback", 0.0f),
        defs::pct ("mix", "Mix", 0.5f) },
        { pr ("Gentle", { { "rate", 0.4f }, { "depth", 0.35f } }), pr ("Wide", { { "rate", 0.8f }, { "depth", 0.6f } }),
          pr ("Juno", { { "rate", 0.5f }, { "depth", 0.8f }, { "mix", 0.6f } }), pr ("Seasick", { { "rate", 3.0f }, { "depth", 0.9f }, { "fb", 0.4f } }) } };
    d[InsPhaser] = { "Phaser", "PHAS", {
        mk ("rate", "Rate", ParamDef::Hz, 0.05f, 8.0f, 0.4f, 0.4f),
        defs::pct ("depth", "Depth", 0.7f),
        defs::bi ("fb", "Feedback", 0.5f),
        ch ("stages", "Stages", { "2", "4", "6", "8", "12" }, 2),
        defs::hz ("center", "Center", 100.0f, 4000.0f, 800.0f),
        defs::pct ("mix", "Mix", 0.5f) },
        { pr ("Slow swirl", { { "rate", 0.15f } }), pr ("Jet", { { "rate", 0.5f }, { "fb", 0.8f }, { "stages", 4 } }),
          pr ("Small stone", { { "rate", 1.2f }, { "stages", 1 }, { "fb", 0.3f } }) } };
    d[InsFlanger] = { "Flanger", "FLNG", {
        mk ("rate", "Rate", ParamDef::Hz, 0.03f, 5.0f, 0.25f, 0.4f),
        defs::pct ("depth", "Depth", 0.6f),
        mk ("delay", "Delay", ParamDef::Ms, 0.5f, 10.0f, 2.0f, 1.0f, " ms"),
        defs::bi ("fb", "Feedback", 0.5f),
        defs::pct ("mix", "Mix", 0.5f) },
        { pr ("Classic", {}), pr ("Metal", { { "fb", 0.9f }, { "delay", 1.0f } }), pr ("Through zero", { { "fb", -0.7f }, { "rate", 0.1f } }) } };
    d[InsComp] = { "Compressor", "COMP", {
        defs::db ("thresh", "Threshold", -40.0f, 0.0f, -18.0f),
        mk ("ratio", "Ratio", ParamDef::Plain, 1.0f, 20.0f, 4.0f, 0.5f, ":1"),
        mk ("attack", "Attack", ParamDef::Ms, 0.1f, 100.0f, 10.0f, 0.4f, " ms"),
        mk ("release", "Release", ParamDef::Ms, 10.0f, 1000.0f, 120.0f, 0.4f, " ms"),
        defs::db ("makeup", "Makeup", 0.0f, 24.0f, 6.0f),
        defs::pct ("mix", "Mix", 1.0f) },
        { pr ("Glue", { { "thresh", -14.0f }, { "ratio", 2.0f }, { "attack", 30.0f }, { "makeup", 3.0f } }),
          pr ("Punch", { { "thresh", -20.0f }, { "ratio", 4.0f }, { "attack", 25.0f }, { "release", 80.0f } }),
          pr ("Squash", { { "thresh", -30.0f }, { "ratio", 12.0f }, { "attack", 1.0f }, { "makeup", 14.0f } }),
          pr ("New York", { { "thresh", -30.0f }, { "ratio", 10.0f }, { "attack", 3.0f }, { "makeup", 12.0f }, { "mix", 0.4f } }) } };
    d[InsEq] = { "EQ", "EQ", {
        defs::db ("lowGain", "Low", -15.0f, 15.0f, 0.0f),
        defs::hz ("lowFreq", "Low freq", 30.0f, 500.0f, 120.0f),
        defs::db ("midGain", "Mid", -15.0f, 15.0f, 0.0f),
        defs::hz ("midFreq", "Mid freq", 200.0f, 8000.0f, 1000.0f),
        mk ("midQ", "Mid Q", ParamDef::Plain, 0.3f, 8.0f, 1.0f, 0.5f),
        defs::db ("highGain", "High", -15.0f, 15.0f, 0.0f),
        defs::hz ("highFreq", "High freq", 1500.0f, 18000.0f, 6000.0f) },
        { pr ("Flat", {}), pr ("Smile", { { "lowGain", 5.0f }, { "highGain", 4.0f }, { "midGain", -2.0f } }),
          pr ("Air", { { "highGain", 6.0f }, { "highFreq", 10000.0f } }), pr ("No mud", { { "midGain", -5.0f }, { "midFreq", 350.0f }, { "midQ", 1.5f } }),
          pr ("Radio", { { "lowGain", -15.0f }, { "lowFreq", 400.0f }, { "highGain", -15.0f }, { "highFreq", 3000.0f }, { "midGain", 6.0f } }) } };
    d[InsTremolo] = { "Tremolo / Pan", "TREM", {
        ch ("mode", "Mode", { "Tremolo", "Auto-pan" }, 0),
        ch ("sync", "Rate", syncNames(), 4),
        mk ("free", "Free rate", ParamDef::Hz, 0.1f, 20.0f, 4.0f, 0.4f),
        defs::pct ("depth", "Depth", 0.6f),
        ch ("shape", "Shape", { "Sine", "Triangle", "Square", "Saw" }, 0) },
        { pr ("Vintage", { { "sync", 0 }, { "free", 5.0f }, { "depth", 0.5f } }), pr ("Chop 1/16", { { "sync", 5 }, { "shape", 2 }, { "depth", 1.0f } }),
          pr ("Auto-pan", { { "mode", 1 }, { "sync", 2 }, { "depth", 0.8f } }), pr ("Pulse 1/8", { { "sync", 4 }, { "shape", 3 }, { "depth", 0.8f } }) } };
    d[InsGate] = { "Trance gate", "GATE", {
        ch ("rate", "Rate", { "1/8", "1/16", "1/32" }, 1),
        ch ("pattern", "Pattern", gateNames, 0),
        defs::pct ("depth", "Depth", 1.0f),
        mk ("attack", "Attack", ParamDef::Ms, 0.5f, 50.0f, 2.0f, 0.5f, " ms"),
        mk ("release", "Release", ParamDef::Ms, 5.0f, 300.0f, 40.0f, 0.5f, " ms") },
        { pr ("Sixteenths", {}), pr ("Trance", { { "pattern", 1 } }), pr ("Dotted", { { "pattern", 2 } }), pr ("Stutter", { { "rate", 2 }, { "pattern", 3 } }) } };
    d[InsRing] = { "Ring mod", "RING", {
        defs::hz ("freq", "Frequency", 20.0f, 5000.0f, 440.0f),
        ch ("wave", "Wave", { "Sine", "Square", "Triangle" }, 0),
        defs::pct ("mix", "Mix", 0.5f) },
        { pr ("Bell", { { "freq", 600.0f } }), pr ("Robot", { { "freq", 80.0f }, { "wave", 1 }, { "mix", 0.7f } }), pr ("Shimmer", { { "freq", 2400.0f }, { "mix", 0.3f } }) } };
    d[InsReverb] = { "Reverb", "VERB", {
        defs::pct ("size", "Size", 0.6f),
        defs::pct ("damp", "Damp", 0.5f),
        defs::pct ("width", "Width", 1.0f),
        defs::pct ("mix", "Mix", 0.3f),
        defs::toggle ("freeze", "Freeze", false) },
        { pr ("Room", { { "size", 0.35f }, { "mix", 0.25f } }), pr ("Hall", { { "size", 0.8f }, { "mix", 0.35f } }),
          pr ("Huge", { { "size", 0.97f }, { "damp", 0.2f }, { "mix", 0.5f } }), pr ("Drone", { { "size", 1.0f }, { "freeze", 1.0f }, { "mix", 0.6f } }) } };
    d[InsWidth] = { "Width", "WIDE", {
        mk ("width", "Width", ParamDef::Pct, 0.0f, 2.0f, 1.0f),
        defs::hz ("monoBass", "Mono bass", 20.0f, 400.0f, 20.0f),
        mk ("haas", "Haas", ParamDef::Ms, 0.0f, 25.0f, 0.0f, 1.0f, " ms") },
        { pr ("Wide", { { "width", 1.6f } }), pr ("Mono", { { "width", 0.0f } }), pr ("Haas", { { "haas", 12.0f } }), pr ("Tight bass", { { "monoBass", 150.0f }, { "width", 1.3f } }) } };
    d[InsTape] = { "Tape / Lo-fi", "TAPE", {
        defs::pct ("wow", "Wow", 0.2f),
        defs::pct ("flutter", "Flutter", 0.15f),
        defs::pct ("sat", "Saturation", 0.3f),
        defs::pct ("noise", "Hiss", 0.05f),
        defs::hz ("tone", "Tone", 1000.0f, 20000.0f, 9000.0f),
        defs::pct ("mix", "Mix", 1.0f) },
        { pr ("Cassette", {}), pr ("Worn VHS", { { "wow", 0.6f }, { "flutter", 0.4f }, { "tone", 4500.0f }, { "noise", 0.15f } }),
          pr ("Warm tape", { { "wow", 0.05f }, { "flutter", 0.05f }, { "sat", 0.6f }, { "noise", 0.0f }, { "tone", 14000.0f } }),
          pr ("Dusty record", { { "wow", 0.15f }, { "sat", 0.2f }, { "noise", 0.3f }, { "tone", 6000.0f } }) } };
    auto more = [&d] (int type, std::initializer_list<Preset> ps) { for (const auto& x : ps) d[(size_t) type].presets.push_back (x); };
    more (InsFilter, { pr ("Dark", { { "cutoff", 600.0f }, { "res", 0.1f } }), pr ("Notch sweep", { { "mode", 3 }, { "cutoff", 1200.0f }, { "res", 0.5f } }),
                       pr ("Resonant HP", { { "mode", 2 }, { "cutoff", 900.0f }, { "res", 0.75f } }) });
    more (InsDrive, { pr ("Rectifier", { { "type", 5 }, { "drive", 10.0f }, { "tone", 6000.0f } }), pr ("Hot tube", { { "type", 2 }, { "drive", 22.0f }, { "bias", 0.3f } }) });
    more (InsDelay, { pr ("Quarter", { { "time", 7 }, { "fb", 0.35f } }), pr ("Triplet", { { "time", 6 }, { "fb", 0.45f }, { "pp", 0.6f } }),
                      pr ("Wobbly tape", { { "time", 5 }, { "fb", 0.55f }, { "tone", 0.35f }, { "wobble", 0.8f } }) });
    more (InsChorus, { pr ("Ensemble", { { "rate", 1.2f }, { "depth", 0.45f }, { "fb", 0.2f }, { "mix", 0.6f } }) });
    more (InsPhaser, { pr ("Barber", { { "rate", 0.08f }, { "depth", 1.0f }, { "fb", 0.7f }, { "stages", 3 } }), pr ("Negative", { { "fb", -0.7f }, { "rate", 0.6f } }) });
    more (InsFlanger, { pr ("Slow jet", { { "rate", 0.06f }, { "depth", 0.9f }, { "fb", 0.75f } }) });
    more (InsComp, { pr ("Drum bus", { { "thresh", -16.0f }, { "ratio", 4.0f }, { "attack", 10.0f }, { "release", 60.0f }, { "makeup", 5.0f } }),
                     pr ("Vocal level", { { "thresh", -24.0f }, { "ratio", 3.0f }, { "attack", 5.0f }, { "release", 200.0f }, { "makeup", 8.0f } }) });
    more (InsEq, { pr ("Warm", { { "lowGain", 3.0f }, { "highGain", -4.0f }, { "highFreq", 5000.0f } }), pr ("Presence", { { "midGain", 4.0f }, { "midFreq", 3000.0f } }) });
    more (InsTremolo, { pr ("Slow pan", { { "mode", 1 }, { "sync", 10 }, { "depth", 0.7f } }), pr ("Helicopter", { { "sync", 0 }, { "free", 14.0f }, { "shape", 2 }, { "depth", 0.9f } }) });
    more (InsGate, { pr ("Choppy", { { "pattern", 4 } }), pr ("Gallop", { { "pattern", 7 }, { "release", 20.0f } }), pr ("Soft pulse", { { "attack", 15.0f }, { "release", 120.0f }, { "depth", 0.7f } }) });
    more (InsRing, { pr ("Sci-fi", { { "freq", 1400.0f }, { "wave", 2 }, { "mix", 0.6f } }) });
    more (InsReverb, { pr ("Small booth", { { "size", 0.15f }, { "damp", 0.7f }, { "mix", 0.2f } }) });
    more (InsTape, { pr ("Broken walkman", { { "wow", 0.9f }, { "flutter", 0.6f }, { "sat", 0.5f }, { "noise", 0.2f }, { "tone", 3500.0f } }) });
    d[InsPitch] = { "Pitch shift", "PTCH", {
        defs::semi ("semi", "Pitch", -24.0f, 24.0f, 12.0f),
        defs::cents ("fine", "Fine", -50.0f, 50.0f, 0.0f),
        mk ("window", "Grain", ParamDef::Ms, 20.0f, 150.0f, 60.0f, 1.0f, " ms"),
        defs::pct ("fb", "Feedback", 0.0f),
        defs::pct ("mix", "Mix", 0.4f) },
        { pr ("Octave up", {}), pr ("Octave down", { { "semi", -12.0f } }), pr ("Fifth", { { "semi", 7.0f }, { "mix", 0.35f } }),
          pr ("Detune", { { "semi", 0.0f }, { "fine", 12.0f }, { "window", 40.0f }, { "mix", 0.5f } }),
          pr ("Shimmer climb", { { "semi", 12.0f }, { "fb", 0.55f }, { "mix", 0.35f }, { "window", 100.0f } }),
          pr ("Chipmunk", { { "semi", 7.0f }, { "mix", 1.0f }, { "window", 30.0f } }) } };
    d[InsComb] = { "Resonator", "RESO", {
        defs::hz ("freq", "Tune", 30.0f, 2000.0f, 220.0f),
        defs::bi ("fb", "Feedback", 0.85f),
        defs::pct ("damp", "Damp", 0.3f),
        defs::pct ("mix", "Mix", 0.5f) },
        { pr ("Metal string", {}), pr ("Hollow tube", { { "freq", 110.0f }, { "fb", -0.8f }, { "damp", 0.5f } }),
          pr ("Bell ring", { { "freq", 880.0f }, { "fb", 0.93f }, { "damp", 0.1f }, { "mix", 0.4f } }),
          pr ("Robot buzz", { { "freq", 60.0f }, { "fb", 0.9f }, { "damp", 0.2f } }) } };
    d[InsVowel] = { "Vowel", "VOWL", {
        mk ("vowel", "Vowel", ParamDef::Plain, 0.0f, 4.0f, 0.0f),
        defs::pct ("res", "Resonance", 0.6f),
        defs::semi ("shift", "Shift", -12.0f, 12.0f, 0.0f),
        defs::pct ("mix", "Mix", 0.8f) },
        { pr ("Aah", {}), pr ("Ooh", { { "vowel", 3.0f } }), pr ("Eee", { { "vowel", 2.0f }, { "res", 0.7f } }),
          pr ("Talk box", { { "vowel", 1.0f }, { "res", 0.85f }, { "mix", 1.0f } }), pr ("Small mouth", { { "vowel", 4.0f }, { "shift", 7.0f } }) } };
    d[InsVowel].params[0].formatFn = [] (float v) { static const char* n[] = { "A", "E", "I", "O", "U" }; const int i = juce::jlimit (0, 3, (int) v); const float f = v - (float) i;
                                                    return f < 0.08f ? juce::String (n[i]) : f > 0.92f ? juce::String (n[i + 1]) : juce::String (n[i]) + ">" + n[i + 1]; };
    d[InsStutter] = { "Stutter", "STUT", {
        ch ("rate", "Repeat", { "1/4", "1/8", "1/16", "1/32" }, 2),
        defs::pct ("chance", "Chance", 0.4f),
        ch ("hold", "Length", { "1/4", "1/2", "1 bar" }, 1),
        defs::pct ("mix", "Mix", 1.0f) },
        { pr ("Sometimes", {}), pr ("Glitchy", { { "rate", 3 }, { "chance", 0.6f }, { "hold", 0 } }),
          pr ("Build-up", { { "rate", 2 }, { "chance", 1.0f }, { "hold", 2 } }), pr ("Half beats", { { "rate", 1 }, { "chance", 0.3f }, { "hold", 1 } }) } };
    d[InsPump] = { "Pump", "PUMP", {
        ch ("rate", "Rate", { "1/2", "1/4", "1/8", "1 bar" }, 1),
        defs::pct ("depth", "Depth", 0.7f),
        defs::pct ("shape", "Release", 0.5f),
        defs::pct ("offset", "Offset", 0.0f) },
        { pr ("Sidechain", {}), pr ("Hard pump", { { "depth", 1.0f }, { "shape", 0.3f } }), pr ("Gentle breath", { { "depth", 0.4f }, { "shape", 0.8f } }),
          pr ("Eighths", { { "rate", 2 }, { "depth", 0.6f } }), pr ("Offbeat", { { "offset", 0.5f }, { "depth", 0.8f } }) } };
    d[InsTransient] = { "Transient", "TRNS", {
        defs::bi ("attack", "Attack", 0.4f),
        defs::bi ("sustain", "Sustain", 0.0f),
        defs::db ("out", "Output", -12.0f, 12.0f, 0.0f) },
        { pr ("Snappy", {}), pr ("Punchy & dry", { { "attack", 0.7f }, { "sustain", -0.6f } }), pr ("Soft & long", { { "attack", -0.6f }, { "sustain", 0.5f } }),
          pr ("Room boost", { { "attack", 0.0f }, { "sustain", 0.8f }, { "out", -3.0f } }) } };
    d[InsCabinet] = { "Amp & cabinet", "AMP", {
        ch ("type", "Cabinet", { "Combo", "Stack", "Radio", "Megaphone", "Telephone", "Tiny speaker" }, 0),
        defs::db ("drive", "Drive", 0.0f, 30.0f, 10.0f),
        defs::bi ("tone", "Tone", 0.0f),
        defs::db ("out", "Output", -24.0f, 6.0f, -3.0f),
        defs::pct ("mix", "Mix", 1.0f) },
        { pr ("Clean combo", { { "drive", 4.0f } }), pr ("Crunch stack", { { "type", 1 }, { "drive", 20.0f } }),
          pr ("Old radio", { { "type", 2 }, { "drive", 8.0f } }), pr ("Megaphone", { { "type", 3 }, { "drive", 16.0f } }),
          pr ("Phone call", { { "type", 4 }, { "drive", 6.0f } }), pr ("Laptop speaker", { { "type", 5 }, { "drive", 3.0f } }) } };
    return d;
}
} // namespace

void InsertDef::defaults (float* v) const
{
    for (int i = 0; i < insertParams; ++i) v[i] = i < (int) params.size() ? params[(size_t) i].def : 0.0f;
}

void InsertDef::applyPreset (int preset, float* v) const
{
    defaults (v);
    if (presets.empty()) return;
    for (const auto& [id, x] : presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, preset)].values)
        for (size_t i = 0; i < params.size(); ++i)
            if (params[i].id == id) v[i] = params[i].clamp (x);
}

const InsertDef& insertDef (int type)
{
    static const auto all = makeInsertDefs();
    return all[(size_t) juce::jlimit (0, NumInsertTypes - 1, type)];
}

juce::StringArray insertNames()
{
    juce::StringArray n;
    for (int i = 0; i < NumInsertTypes; ++i) n.add (insertDef (i).name);
    return n;
}

juce::StringArray modSourceNames()
{
    juce::StringArray n { "Off", "LFO 1", "LFO 2", "Envelope", "Velocity", "Note", "Mod wheel", "Random", "Bar" };
    for (int i = 1; i <= numMacros; ++i) n.add ("Macro " + juce::String (i));
    return n;
}

const std::vector<ParamDef>& modSetDefs()
{
    static const std::vector<ParamDef> d = [] {
        const juce::StringArray shapes { "Sine", "Triangle", "Saw up", "Saw down", "Square", "S & H", "Smooth random" };
        return std::vector<ParamDef> {
            ch ("lfo1Shape", "LFO 1 shape", shapes, 0), mk ("lfo1Rate", "LFO 1 rate", ParamDef::Hz, 0.02f, 20.0f, 1.0f, 0.35f),
            ch ("lfo1Sync", "LFO 1 sync", syncNames(), 0), defs::toggle ("lfo1Retrig", "LFO 1 restart", false),
            ch ("lfo2Shape", "LFO 2 shape", shapes, 1), mk ("lfo2Rate", "LFO 2 rate", ParamDef::Hz, 0.02f, 20.0f, 0.25f, 0.35f),
            ch ("lfo2Sync", "LFO 2 sync", syncNames(), 0), defs::toggle ("lfo2Retrig", "LFO 2 restart", false),
            defs::sec ("envA", "Env attack", 0.001f, 4.0f, 0.01f), defs::sec ("envD", "Env decay", 0.01f, 8.0f, 0.5f),
            ch ("randRate", "Random rate", syncNames(), 5), defs::pct ("smooth", "Random smooth", 0.0f) };
    }();
    return d;
}

const ParamDef& macroDef() { static const ParamDef d = defs::pct ("macro", "Macro", 0.0f); return d; }
const ParamDef& modAmountDef() { static const ParamDef d = defs::bi ("amount", "Amount", 0.0f); return d; }

TrackMods::TrackMods()
{
    my.fill ((int16_t) dest::none);
    for (size_t i = 0; i < (size_t) numModSet; ++i) set[i] = modSetDefs()[i].def;
}

void TrackMods::dropEngineTargets()
{
    for (auto& s : slots) if (dest::isEngine (s.dest)) s = ModSlot();
    for (auto& m : my) if (dest::isEngine (m)) m = (int16_t) dest::none;
}

//==============================================================================
namespace
{
struct ChainPreset { const char* name; std::array<std::pair<int, const char*>, numInserts> ins; };
const std::vector<ChainPreset>& chains()
{
    static const std::vector<ChainPreset> c = {
        { "Clean",        { { { InsOff, "" }, { InsOff, "" }, { InsOff, "" }, { InsOff, "" } } } },
        { "Lo-fi",        { { { InsCrusher, "Old sampler" }, { InsFilter, "Warm low pass" }, { InsTape, "Cassette" }, { InsOff, "" } } } },
        { "Dub",          { { { InsFilter, "Thin" }, { InsDelay, "Dub" }, { InsReverb, "Hall" }, { InsOff, "" } } } },
        { "Big room",     { { { InsComp, "Punch" }, { InsEq, "Smile" }, { InsReverb, "Hall" }, { InsWidth, "Wide" } } } },
        { "Wide pad",     { { { InsChorus, "Juno" }, { InsPhaser, "Slow swirl" }, { InsReverb, "Huge" }, { InsWidth, "Wide" } } } },
        { "Trance gate",  { { { InsGate, "Trance" }, { InsDelay, "Dotted" }, { InsReverb, "Room" }, { InsOff, "" } } } },
        { "Radio",        { { { InsEq, "Radio" }, { InsDrive, "Crunch" }, { InsCrusher, "Dust" }, { InsOff, "" } } } },
        { "Destroy",      { { { InsDrive, "Fuzz box" }, { InsCrusher, "Broken" }, { InsFilter, "Acid squelch" }, { InsComp, "Squash" } } } },
        { "Acid",         { { { InsDrive, "Warm" }, { InsDelay, "Dotted" }, { InsChorus, "Gentle" }, { InsOff, "" } } } },
        { "Robot",        { { { InsRing, "Robot" }, { InsFilter, "Telephone" }, { InsDelay, "Slapback" }, { InsOff, "" } } } },
        { "Vintage keys", { { { InsTremolo, "Vintage" }, { InsTape, "Warm tape" }, { InsReverb, "Room" }, { InsOff, "" } } } },
        { "Tight drums",  { { { InsComp, "Punch" }, { InsEq, "No mud" }, { InsDrive, "Parallel" }, { InsOff, "" } } } },
        { "Wobble",       { { { InsFilter, "Wah follow" }, { InsTremolo, "Chop 1/16" }, { InsDelay, "Ping-pong" }, { InsOff, "" } } } },
        { "Space",        { { { InsFlanger, "Classic" }, { InsDelay, "Ping-pong" }, { InsReverb, "Huge" }, { InsOff, "" } } } },
        { "Old VHS",      { { { InsTape, "Worn VHS" }, { InsChorus, "Seasick" }, { InsFilter, "Warm low pass" }, { InsOff, "" } } } },
        { "Talk box",     { { { InsDrive, "Warm" }, { InsVowel, "Talk box" }, { InsChorus, "Gentle" }, { InsOff, "" } } } },
        { "Pumping",      { { { InsPump, "Sidechain" }, { InsComp, "Glue" }, { InsReverb, "Room" }, { InsOff, "" } } } },
        { "Glitch",       { { { InsStutter, "Glitchy" }, { InsCrusher, "8 bit" }, { InsDelay, "Ping-pong" }, { InsOff, "" } } } },
        { "Octaver",      { { { InsPitch, "Octave down" }, { InsDrive, "Warm" }, { InsComp, "Punch" }, { InsOff, "" } } } },
        { "Guitar amp",   { { { InsDrive, "Crunch" }, { InsCabinet, "Crunch stack" }, { InsReverb, "Small booth" }, { InsOff, "" } } } },
        { "Metal ring",   { { { InsComb, "Metal string" }, { InsDelay, "Dotted" }, { InsReverb, "Hall" }, { InsOff, "" } } } },
        { "Shimmer up",   { { { InsPitch, "Shimmer climb" }, { InsReverb, "Huge" }, { InsWidth, "Wide" }, { InsOff, "" } } } },
        { "Punchy",       { { { InsTransient, "Punchy & dry" }, { InsComp, "Drum bus" }, { InsEq, "Smile" }, { InsOff, "" } } } },
        { "Megaphone",    { { { InsCabinet, "Megaphone" }, { InsDelay, "Slapback" }, { InsOff, "" }, { InsOff, "" } } } },
        { "Underwater",   { { { InsFilter, "Dark" }, { InsChorus, "Seasick" }, { InsPhaser, "Slow swirl" }, { InsReverb, "Hall" } } } },
    };
    return c;
}
} // namespace

juce::StringArray chainPresetNames()
{
    juce::StringArray n;
    for (const auto& c : chains()) n.add (c.name);
    return n;
}

void applyChainPreset (TrackMods& m, int index)
{
    const auto& c = chains()[(size_t) juce::jlimit (0, (int) chains().size() - 1, index)];
    for (int i = 0; i < numInserts; ++i)
    {
        auto& in = m.inserts[(size_t) i];
        const auto& [type, preset] = c.ins[(size_t) i];
        in.type = type;
        in.on = true;
        const auto& d = insertDef (type);
        int pi = 0;
        for (size_t k = 0; k < d.presets.size(); ++k) if (d.presets[k].name == preset) pi = (int) k;
        d.applyPreset (pi, in.p.data());
    }
}

namespace
{
juce::String destCode (int d, const EngineDef& e)
{
    if (dest::isEngine (d)) return d < (int) e.params.size() ? "e:" + e.params[(size_t) d].id : juce::String();
    if (dest::isInsert (d)) return "i:" + juce::String ((d - dest::insert0) / insertParams) + ":" + juce::String ((d - dest::insert0) % insertParams);
    if (dest::isMix (d)) return "x:" + juce::String (d - dest::mix0);
    if (d >= dest::macro0 && d < dest::macro0 + numMacros) return "m:" + juce::String (d - dest::macro0);
    if (d >= dest::modSet0 && d < dest::modSet0 + numModSet) return "s:" + juce::String (d - dest::modSet0);
    return {};
}

int destFromCode (const juce::String& c, const EngineDef& e)
{
    const auto kind = c.upToFirstOccurrenceOf (":", false, false);
    const auto rest = c.fromFirstOccurrenceOf (":", false, false);
    if (kind == "e") { const int i = e.index (rest); return i; }
    if (kind == "i")
    {
        const int s = rest.upToFirstOccurrenceOf (":", false, false).getIntValue(), p = rest.fromFirstOccurrenceOf (":", false, false).getIntValue();
        return s >= 0 && s < numInserts && p >= 0 && p < insertParams ? dest::insert0 + s * insertParams + p : -1;
    }
    if (kind == "x") return juce::isPositiveAndBelow (rest.getIntValue(), 5) ? dest::mix0 + rest.getIntValue() : -1;
    if (kind == "m") return juce::isPositiveAndBelow (rest.getIntValue(), numMacros) ? dest::macro0 + rest.getIntValue() : -1;
    if (kind == "s") return juce::isPositiveAndBelow (rest.getIntValue(), numModSet) ? dest::modSet0 + rest.getIntValue() : -1;
    return -1;
}
} // namespace

juce::var modsToVar (const TrackMods& m, int engine)
{
    const auto& e = engineDef (engine);
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> ins;
    bool anyInsert = false;
    for (const auto& i : m.inserts)
    {
        auto* io = new juce::DynamicObject();
        io->setProperty ("type", insertDef (i.type).name);
        if (! i.on) io->setProperty ("on", false);
        juce::Array<juce::var> p;
        for (auto v : i.p) p.add (v);
        io->setProperty ("p", p);
        ins.add (juce::var (io));
        anyInsert = anyInsert || i.type != InsOff;
    }
    if (anyInsert) o->setProperty ("inserts", ins);
    juce::Array<juce::var> mac;
    bool anyMacro = false;
    for (auto v : m.macros) { mac.add (v); anyMacro = anyMacro || v != 0.0f; }
    if (anyMacro) o->setProperty ("macros", mac);
    juce::Array<juce::var> slots;
    for (const auto& s : m.slots)
        if (s.source != SrcNone && s.dest != dest::none)
            if (const auto c = destCode (s.dest, e); c.isNotEmpty())
                slots.add (juce::Array<juce::var> { modSourceNames()[s.source], c, s.amount });
    if (! slots.isEmpty()) o->setProperty ("slots", slots);
    auto* so = new juce::DynamicObject();
    bool anySet = false;
    for (size_t i = 0; i < (size_t) numModSet; ++i)
        if (m.set[i] != modSetDefs()[i].def) { so->setProperty (modSetDefs()[i].id, m.set[i]); anySet = true; }
    if (anySet) o->setProperty ("set", juce::var (so));
    else delete so;
    juce::Array<juce::var> my;
    bool anyMy = false;
    for (auto d : m.my) { const auto c = d >= 0 ? destCode (d, e) : juce::String(); my.add (c); anyMy = anyMy || c.isNotEmpty(); }
    if (anyMy) o->setProperty ("my", my);
    return juce::var (o);
}

void modsFromVar (const juce::var& v, TrackMods& m, int engine)
{
    m = TrackMods();
    if (! v.isObject()) return;
    const auto& e = engineDef (engine);
    if (auto* a = v["inserts"].getArray())
        for (int i = 0; i < juce::jmin (numInserts, a->size()); ++i)
        {
            const auto iv = (*a)[i];
            auto& ins = m.inserts[(size_t) i];
            ins.type = juce::jmax (0, insertNames().indexOf (iv["type"].toString()));
            ins.on = (bool) iv.getProperty ("on", true);
            insertDef (ins.type).defaults (ins.p.data());
            if (auto* p = iv["p"].getArray())
                for (int k = 0; k < juce::jmin (insertParams, p->size()); ++k)
                {
                    const auto& defs = insertDef (ins.type).params;
                    ins.p[(size_t) k] = k < (int) defs.size() ? defs[(size_t) k].clamp ((float) (*p)[k]) : 0.0f;
                }
        }
    if (auto* a = v["macros"].getArray())
        for (int i = 0; i < juce::jmin (numMacros, a->size()); ++i) m.macros[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) (*a)[i]);
    if (auto* a = v["slots"].getArray())
    {
        int k = 0;
        for (const auto& s : *a)
            if (auto* x = s.getArray(); x != nullptr && x->size() == 3 && k < numModSlots)
            {
                const int src = modSourceNames().indexOf ((*x)[0].toString());
                const int d = destFromCode ((*x)[1].toString(), e);
                if (src > 0 && d >= 0) m.slots[(size_t) k++] = { src, d, juce::jlimit (-1.0f, 1.0f, (float) (*x)[2]) };
            }
    }
    if (auto* o = v["set"].getDynamicObject())
        for (size_t i = 0; i < (size_t) numModSet; ++i)
            if (o->hasProperty (modSetDefs()[i].id)) m.set[i] = modSetDefs()[i].clamp ((float) o->getProperty (modSetDefs()[i].id));
    if (auto* a = v["my"].getArray())
        for (int i = 0; i < juce::jmin (numMyKnobs, a->size()); ++i)
            m.my[(size_t) i] = (int16_t) destFromCode ((*a)[i].toString(), e);
}

//==============================================================================
void InsertFx::prepare (double sampleRate)
{
    sr = sampleRate;
    bufL.assign ((size_t) (sr * 2.1), 0.0f);
    bufR.assign ((size_t) (sr * 2.1), 0.0f);
    reverb.setSampleRate (sr);
    reset();
}

void InsertFx::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    w = 0;
    svfL.reset(); svfR.reset(); svf2L.reset(); svf2R.reset(); svf3L.reset(); svf3R.reset();
    env2 = 0.0f; pitchPhase = 0.0f; stutterOn = false; stutterGrid = -1; segStart = 0; segLen = 1; segPos = 0;
    env = 0.0f; holdL = holdR = holdCount = 0.0f; lpL = lpR = dcL = dcR = dcInL = dcInR = 0.0f;
    gain = 1.0f; smoothTime = -1.0f; gateEnv = 0.0f;
    apL.fill (0.0f); apR.fill (0.0f); fbL = fbR = 0.0f;
    for (auto& b : eq) b.z1 = b.z2 = 0.0f;
    eqLast.fill (-999.0f);
    reverb.reset();
}

float InsertFx::readDelay (const std::vector<float>& buf, float back) const noexcept
{
    const int size = (int) buf.size();
    back = juce::jlimit (1.0f, (float) (size - 2), back);
    float pos = (float) w - back;
    while (pos < 0.0f) pos += (float) size;
    const int i0 = (int) pos;
    const float f = pos - (float) i0;
    const int i1 = (i0 + 1) % size;
    return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
}

void InsertFx::setEq (const float* p)
{
    bool same = true;
    for (int i = 0; i < 7; ++i) same = same && eqLast[(size_t) i] == p[i];
    if (same) return;
    for (int i = 0; i < 7; ++i) eqLast[(size_t) i] = p[i];
    auto shelf = [this] (Biquad& b, float f0, float db, bool high)
    {
        const float A = std::pow (10.0f, db / 40.0f), w0 = dsp::twoPi * juce::jlimit (10.0f, (float) sr * 0.45f, f0) / (float) sr;
        const float cs = std::cos (w0), sn = std::sin (w0), alpha = sn / 2.0f * std::sqrt (2.0f), sq = 2.0f * std::sqrt (A) * alpha;
        float b0, b1, b2, a0, a1, a2;
        if (! high)
        {
            b0 = A * ((A + 1) - (A - 1) * cs + sq); b1 = 2 * A * ((A - 1) - (A + 1) * cs); b2 = A * ((A + 1) - (A - 1) * cs - sq);
            a0 = (A + 1) + (A - 1) * cs + sq; a1 = -2 * ((A - 1) + (A + 1) * cs); a2 = (A + 1) + (A - 1) * cs - sq;
        }
        else
        {
            b0 = A * ((A + 1) + (A - 1) * cs + sq); b1 = -2 * A * ((A - 1) + (A + 1) * cs); b2 = A * ((A + 1) + (A - 1) * cs - sq);
            a0 = (A + 1) - (A - 1) * cs + sq; a1 = 2 * ((A - 1) - (A + 1) * cs); a2 = (A + 1) - (A - 1) * cs - sq;
        }
        b.b0 = b0 / a0; b.b1 = b1 / a0; b.b2 = b2 / a0; b.a1 = a1 / a0; b.a2 = a2 / a0;
    };
    auto peak = [this] (Biquad& b, float f0, float db, float q)
    {
        const float A = std::pow (10.0f, db / 40.0f), w0 = dsp::twoPi * juce::jlimit (10.0f, (float) sr * 0.45f, f0) / (float) sr;
        const float alpha = std::sin (w0) / (2.0f * juce::jmax (0.1f, q)), cs = std::cos (w0);
        const float a0 = 1 + alpha / A;
        b.b0 = (1 + alpha * A) / a0; b.b1 = -2 * cs / a0; b.b2 = (1 - alpha * A) / a0; b.a1 = -2 * cs / a0; b.a2 = (1 - alpha / A) / a0;
    };
    shelf (eq[0], p[1], p[0], false); shelf (eq[1], p[1], p[0], false);
    peak (eq[2], p[3], p[2], p[4]); peak (eq[3], p[3], p[2], p[4]);
    shelf (eq[4], p[6], p[5], true); shelf (eq[5], p[6], p[5], true);
}

void InsertFx::setCab (int type, float tone)
{
    const float key[7] = { (float) type, tone, 0, 0, 0, 0, -77.0f };
    bool same = true;
    for (int i = 0; i < 7; ++i) same = same && eqLast[(size_t) i] == key[i];
    if (same) return;
    for (int i = 0; i < 7; ++i) eqLast[(size_t) i] = key[i];
    // high-pass, low-pass, a peak: the speaker's shape
    static const float hp[] = { 80.0f, 60.0f, 250.0f, 500.0f, 400.0f, 600.0f };
    static const float lp[] = { 5000.0f, 4200.0f, 3500.0f, 3000.0f, 3200.0f, 6500.0f };
    static const float pf[] = { 1800.0f, 1200.0f, 1000.0f, 1800.0f, 1500.0f, 2500.0f };
    static const float pg[] = { 3.0f, 4.0f, 5.0f, 9.0f, 6.0f, 6.0f };
    const int t = juce::jlimit (0, 5, type);
    const float tm = std::exp2 (tone * 1.2f);
    auto pass = [this] (Biquad& b, float f0, bool high)
    {
        const float w0 = dsp::twoPi * juce::jlimit (10.0f, (float) sr * 0.45f, f0) / (float) sr, cs = std::cos (w0), alpha = std::sin (w0) / (2.0f * 0.8f);
        const float a0 = 1 + alpha;
        if (high) { b.b0 = (1 + cs) / 2 / a0; b.b1 = -(1 + cs) / a0; b.b2 = b.b0; }
        else { b.b0 = (1 - cs) / 2 / a0; b.b1 = (1 - cs) / a0; b.b2 = b.b0; }
        b.a1 = -2 * cs / a0; b.a2 = (1 - alpha) / a0;
    };
    auto peak = [this] (Biquad& b, float f0, float db, float q)
    {
        const float A = std::pow (10.0f, db / 40.0f), w0 = dsp::twoPi * juce::jlimit (10.0f, (float) sr * 0.45f, f0) / (float) sr;
        const float alpha = std::sin (w0) / (2.0f * q), cs = std::cos (w0), a0 = 1 + alpha / A;
        b.b0 = (1 + alpha * A) / a0; b.b1 = -2 * cs / a0; b.b2 = (1 - alpha * A) / a0; b.a1 = -2 * cs / a0; b.a2 = (1 - alpha / A) / a0;
    };
    pass (eq[0], hp[t], true); pass (eq[1], hp[t], true);
    pass (eq[2], lp[t] * tm, false); pass (eq[3], lp[t] * tm, false);
    peak (eq[4], pf[t] * tm, pg[t], 1.2f); peak (eq[5], pf[t] * tm, pg[t], 1.2f);
}

void InsertFx::process (int type, const float* p, float* l, float* r, int n, double ppq, double ppqPerSample, float bpm)
{
    if (type != lastType) { reset(); lastType = type; }
    const float fs = (float) sr;
    const int size = (int) bufL.size();
    auto wet = [] (float dry, float x, float mix) { return dry + (x - dry) * mix; };
    switch (type)
    {
        case InsFilter:
        {
            const int mode = juce::roundToInt (p[0]);
            const float res = p[2], drive = 1.0f + p[3] * 8.0f, follow = p[4], mix = p[5];
            const float q = 0.5f + res * res * 14.0f;
            for (int i = 0; i < n; ++i)
            {
                const float a = std::abs (l[i]) + std::abs (r[i]);
                env += (a - env) * (a > env ? 0.01f : 0.0005f);
                if ((i & 7) == 0)
                {
                    const float cut = juce::jlimit (20.0f, fs * 0.45f, p[1] * std::exp2 (follow * juce::jmin (1.0f, env * 2.0f) * 5.0f));
                    svfL.rampTo (cut, q, fs, 8); svfR.rampTo (cut, q, fs, 8);
                    if (mode == 4) { svf2L.rampTo (cut, 0.6f, fs, 8); svf2R.rampTo (cut, 0.6f, fs, 8); }
                }
                float x[2] { l[i] * drive, r[i] * drive };
                dsp::Svf* f1[2] { &svfL, &svfR };
                dsp::Svf* f2[2] { &svf2L, &svf2R };
                for (int c = 0; c < 2; ++c)
                {
                    float in = drive > 1.001f ? dsp::fastTanh (x[c]) / std::sqrt (drive) : x[c];
                    const auto o = f1[c]->process (in);
                    float y = 0.0f;
                    switch (mode)
                    {
                        case 0: y = o.low; break;
                        case 1: y = o.band; break;
                        case 2: y = o.high; break;
                        case 3: y = o.low + o.high; break;
                        default: y = f2[c]->process (o.low).low; break;
                    }
                    x[c] = y;
                }
                l[i] = wet (l[i], x[0], mix); r[i] = wet (r[i], x[1], mix);
            }
            break;
        }
        case InsDrive:
        {
            const int t = juce::roundToInt (p[0]);
            const float g = dsp::dbToGain (p[1]), bias = p[3] * 0.5f, out = dsp::dbToGain (p[4]) / std::sqrt (juce::jmax (1.0f, g * 0.5f)), mix = p[5];
            const float lpA = 1.0f - std::exp (-dsp::twoPi * p[2] / fs);
            auto shape = [t, bias] (float x)
            {
                switch (t)
                {
                    case 0: return dsp::fastTanh (x);
                    case 1: return juce::jlimit (-1.0f, 1.0f, x);
                    case 2: return dsp::fastTanh (x + 0.3f + bias) - dsp::fastTanh (0.3f + bias);
                    case 3: return std::sin (x * 1.2f + bias);
                    case 4: { const float s = x + bias; return (s >= 0 ? 1.0f : -1.0f) * (1.0f - std::exp (-std::abs (s) * 2.0f)); }
                    default: return std::abs (x + bias) * 1.5f - 0.5f;
                }
            };
            for (int i = 0; i < n; ++i)
            {
                float a = shape (l[i] * g), b = shape (r[i] * g);
                // DC out (fold / tube / rectify shift it)
                const float ya = a - dcInL + 0.995f * dcL; dcInL = a; dcL = ya;
                const float yb = b - dcInR + 0.995f * dcR; dcInR = b; dcR = yb;
                lpL += (ya - lpL) * lpA; lpR += (yb - lpR) * lpA;
                l[i] = wet (l[i], lpL * out, mix); r[i] = wet (r[i], lpR * out, mix);
            }
            break;
        }
        case InsCrusher:
        {
            const float levels = std::exp2 (juce::jlimit (1.0f, 16.0f, p[0])) * 0.5f, step = juce::jmax (1.0f, fs / p[1]), mix = p[3];
            for (int i = 0; i < n; ++i)
            {
                holdCount -= 1.0f;
                if (holdCount <= 0.0f)
                {
                    holdCount += step * (1.0f + p[2] * rng.next() * 0.8f);
                    holdL = std::round (l[i] * levels) / levels;
                    holdR = std::round (r[i] * levels) / levels;
                }
                l[i] = wet (l[i], holdL, mix); r[i] = wet (r[i], holdR, mix);
            }
            break;
        }
        case InsDelay:
        {
            const float target = juce::jlimit (16.0f, (float) size - 4.0f, (float) (delayBeats (juce::roundToInt (p[0])) * 60.0 / juce::jmax (20.0f, bpm) * sr));
            if (smoothTime < 0.0f) smoothTime = target;
            const float fb = juce::jmin (0.95f, p[1]), lpA = 0.05f + 0.9f * p[2], pp = p[3], wob = p[4], mix = p[5];
            const float wobInc = 0.6f / fs;
            for (int i = 0; i < n; ++i)
            {
                smoothTime += (target - smoothTime) * 0.0005f;
                lfoPhase += wobInc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                const float t = smoothTime * (1.0f + wob * 0.01f * std::sin (dsp::twoPi * lfoPhase));
                const float dl = readDelay (bufL, t), dr = readDelay (bufR, t);
                lpL += (dl - lpL) * lpA; lpR += (dr - lpR) * lpA;
                const float inMono = 0.5f * (l[i] + r[i]);
                // ping-pong: the input enters on the left only and the repeats cross over
                const float inL = l[i] + (inMono - l[i]) * pp, inR = r[i] * (1.0f - pp);
                const float nl = inL + (lpL + (lpR - lpL) * pp) * fb;
                const float nr = inR + (lpR + (lpL - lpR) * pp) * fb;
                bufL[(size_t) w] = dsp::fastTanh (nl);
                bufR[(size_t) w] = dsp::fastTanh (nr);
                w = (w + 1) % size;
                l[i] += dl * mix; r[i] += dr * mix;
            }
            break;
        }
        case InsChorus:
        case InsFlanger:
        {
            const bool fl = type == InsFlanger;
            const float rate = p[0], depth = p[1];
            const float baseMs = fl ? p[2] : 12.0f, depthMs = fl ? p[2] * 0.95f * depth : 8.0f * depth;
            const float fb = fl ? p[3] * 0.95f : p[3] * 0.6f, mix = fl ? p[4] : p[4];
            const float width = fl ? 1.0f : p[2];
            const float inc = rate / fs;
            for (int i = 0; i < n; ++i)
            {
                lfoPhase += inc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                const float ml = std::sin (dsp::twoPi * lfoPhase), mr = std::sin (dsp::twoPi * (lfoPhase + 0.25f * width));
                const float tl = (baseMs + depthMs * 0.5f * (1.0f + ml)) * 0.001f * fs;
                const float tr = (baseMs + depthMs * 0.5f * (1.0f + mr)) * 0.001f * fs;
                const float dl = readDelay (bufL, tl), dr = readDelay (bufR, tr);
                bufL[(size_t) w] = l[i] + dl * fb;
                bufR[(size_t) w] = r[i] + dr * fb;
                w = (w + 1) % size;
                l[i] = wet (l[i], 0.5f * (l[i] + dl) * 1.4f, mix);
                r[i] = wet (r[i], 0.5f * (r[i] + dr) * 1.4f, mix);
            }
            break;
        }
        case InsPhaser:
        {
            static const int stagesOf[] = { 2, 4, 6, 8, 12 };
            const int stages = stagesOf[juce::jlimit (0, 4, juce::roundToInt (p[3]))];
            const float inc = p[0] / fs, depth = p[1], fb = p[2] * 0.9f, mix = p[5];
            for (int i = 0; i < n; ++i)
            {
                lfoPhase += inc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                const float m = 0.5f + 0.5f * std::sin (dsp::twoPi * lfoPhase);
                const float f = juce::jlimit (30.0f, fs * 0.4f, p[4] * std::exp2 ((m - 0.5f) * depth * 6.0f));
                const float t = std::tan (dsp::pi * f / fs);
                const float a = (t - 1.0f) / (t + 1.0f);
                float x[2] { l[i] + fbL * fb, r[i] + fbR * fb };
                std::array<float, 12>* st[2] { &apL, &apR };
                for (int c = 0; c < 2; ++c)
                    for (int s = 0; s < stages; ++s)
                    {
                        auto& z = (*st[c])[(size_t) s];
                        const float y = a * x[c] + z;
                        z = x[c] - a * y;
                        x[c] = y;
                    }
                fbL = dsp::fastTanh (x[0]); fbR = dsp::fastTanh (x[1]);
                l[i] = wet (l[i], 0.5f * (l[i] + x[0]) * 1.4f, mix);
                r[i] = wet (r[i], 0.5f * (r[i] + x[1]) * 1.4f, mix);
            }
            break;
        }
        case InsComp:
        {
            const float thr = p[0], ratio = juce::jmax (1.0f, p[1]);
            const float att = std::exp (-1.0f / (juce::jmax (0.1f, p[2]) * 0.001f * fs)), rel = std::exp (-1.0f / (juce::jmax (1.0f, p[3]) * 0.001f * fs));
            const float makeup = dsp::dbToGain (p[4]), mix = p[5];
            for (int i = 0; i < n; ++i)
            {
                const float a = juce::jmax (std::abs (l[i]), std::abs (r[i]));
                env = a > env ? a + (env - a) * att : a + (env - a) * rel;
                const float db = juce::Decibels::gainToDecibels (env + 1.0e-9f, -120.0f);
                const float over = db - thr;
                const float red = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                gain = dsp::dbToGain (-red);
                l[i] = wet (l[i], l[i] * gain * makeup, mix); r[i] = wet (r[i], r[i] * gain * makeup, mix);
            }
            break;
        }
        case InsEq:
        {
            setEq (p);
            for (int i = 0; i < n; ++i)
            {
                l[i] = eq[4].process (eq[2].process (eq[0].process (l[i])));
                r[i] = eq[5].process (eq[3].process (eq[1].process (r[i])));
            }
            break;
        }
        case InsTremolo:
        {
            const int mode = juce::roundToInt (p[0]), sync = juce::roundToInt (p[1]), shape = juce::roundToInt (p[4]);
            const double beats = syncBeats (sync);
            const float depth = p[3];
            for (int i = 0; i < n; ++i)
            {
                float ph;
                if (beats > 0.0) { const double x = (ppq + i * ppqPerSample) / beats; ph = (float) (x - std::floor (x)); }
                else { lfoPhase += p[2] / fs; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f; ph = lfoPhase; }
                float m;
                switch (shape)
                {
                    case 1: m = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
                    case 2: m = ph < 0.5f ? 1.0f : -1.0f; break;
                    case 3: m = 1.0f - 2.0f * ph; break;
                    default: m = std::cos (dsp::twoPi * ph); break;
                }
                gateEnv += (m - gateEnv) * 0.02f;   // no clicks on square / saw
                if (mode == 0)
                {
                    const float g = 1.0f - depth * 0.5f * (1.0f - gateEnv);
                    l[i] *= g; r[i] *= g;
                }
                else
                {
                    const float pan = gateEnv * depth;
                    l[i] *= std::sqrt (0.5f * (1.0f - pan)) * 1.4142f;
                    r[i] *= std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
                }
            }
            break;
        }
        case InsGate:
        {
            static const double rates[] = { 0.5, 0.25, 0.125 };
            const double stepLen = rates[juce::jlimit (0, 2, juce::roundToInt (p[0]))];
            const auto pat = gateNames[juce::jlimit (0, gateNames.size() - 1, juce::roundToInt (p[1]))];
            const float depth = p[2];
            const float att = 1.0f - std::exp (-1.0f / (juce::jmax (0.5f, p[3]) * 0.001f * fs));
            const float rel = 1.0f - std::exp (-1.0f / (juce::jmax (1.0f, p[4]) * 0.001f * fs));
            for (int i = 0; i < n; ++i)
            {
                const double s = (ppq + i * ppqPerSample) / stepLen;
                const long long step = (long long) std::floor (s);
                const float inStep = (float) (s - (double) step);
                const bool open = pat[(int) (((step % 16) + 16) % 16)] == 'x' && inStep < 0.75f;
                const float target = open ? 1.0f : 0.0f;
                gateEnv += (target - gateEnv) * (target > gateEnv ? att : rel);
                const float g = 1.0f - depth * (1.0f - gateEnv);
                l[i] *= g; r[i] *= g;
            }
            break;
        }
        case InsRing:
        {
            const float inc = p[0] / fs, mix = p[2];
            const int wave = juce::roundToInt (p[1]);
            for (int i = 0; i < n; ++i)
            {
                lfoPhase += inc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                const float m = wave == 1 ? (lfoPhase < 0.5f ? 1.0f : -1.0f) : wave == 2 ? 1.0f - 4.0f * std::abs (lfoPhase - 0.5f) : std::sin (dsp::twoPi * lfoPhase);
                l[i] = wet (l[i], l[i] * m, mix); r[i] = wet (r[i], r[i] * m, mix);
            }
            break;
        }
        case InsReverb:
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.0f, 1.0f, p[0]);
            rp.damping = p[1];
            rp.width = p[2];
            rp.wetLevel = p[3] * 0.6f;
            rp.dryLevel = 1.0f - p[3] * 0.5f;
            rp.freezeMode = p[4] > 0.5f ? 1.0f : 0.0f;
            if (rp.roomSize != revLast.roomSize || rp.damping != revLast.damping || rp.width != revLast.width
                || rp.wetLevel != revLast.wetLevel || rp.freezeMode != revLast.freezeMode)
            {
                reverb.setParameters (rp);
                revLast = rp;
            }
            reverb.processStereo (l, r, n);
            break;
        }
        case InsWidth:
        {
            const float wdt = p[0];
            const float haas = p[2] * 0.001f * fs;
            const bool monoBass = p[1] > 25.0f;
            if (monoBass) { svfL.set (p[1], 0.7f, fs); }
            for (int i = 0; i < n; ++i)
            {
                float a = l[i], b = r[i];
                if (haas >= 1.0f)
                {
                    bufR[(size_t) w] = b;
                    b = readDelay (bufR, haas);
                    w = (w + 1) % size;
                }
                float mid = 0.5f * (a + b), side = 0.5f * (a - b) * wdt;
                if (monoBass) side -= svfL.process (side).low;
                l[i] = mid + side; r[i] = mid - side;
            }
            break;
        }
        case InsTape:
        {
            const float wow = p[0], flutter = p[1], sat = p[2], hiss = p[3], mix = p[5];
            const float lpA = 1.0f - std::exp (-dsp::twoPi * p[4] / fs);
            const float drive = 1.0f + sat * 4.0f;
            for (int i = 0; i < n; ++i)
            {
                wowPhase += 0.5f / fs; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
                flutterPhase += 7.0f / fs; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
                const float t = (6.0f + wow * 4.0f * (1.0f + std::sin (dsp::twoPi * wowPhase)) + flutter * 0.6f * (1.0f + std::sin (dsp::twoPi * flutterPhase))) * 0.001f * fs;
                bufL[(size_t) w] = l[i]; bufR[(size_t) w] = r[i];
                float a = readDelay (bufL, t), b = readDelay (bufR, t);
                w = (w + 1) % size;
                a = dsp::fastTanh (a * drive) / std::sqrt (drive);
                b = dsp::fastTanh (b * drive) / std::sqrt (drive);
                lpL += (a - lpL) * lpA; lpR += (b - lpR) * lpA;
                const float nz = hiss * 0.02f;
                l[i] = wet (l[i], lpL + rng.next() * nz, mix);
                r[i] = wet (r[i], lpR + rng.next() * nz, mix);
            }
            break;
        }
        case InsPitch:
        {
            const float ratio = std::exp2 ((p[0] + p[1] * 0.01f) / 12.0f);
            const float window = juce::jlimit (64.0f, (float) size * 0.4f, p[2] * 0.001f * fs);
            const float fb = p[3] * 0.85f, mix = p[4];
            const float inc = (ratio - 1.0f) / window;
            for (int i = 0; i < n; ++i)
            {
                pitchPhase -= inc;
                pitchPhase -= std::floor (pitchPhase);
                float ol = 0.0f, orr = 0.0f;
                for (int h = 0; h < 2; ++h)
                {
                    float ph = pitchPhase + 0.5f * (float) h;
                    if (ph >= 1.0f) ph -= 1.0f;
                    const float d = ph * window + 2.0f, win = std::sin (dsp::pi * ph);
                    ol += readDelay (bufL, d) * win;
                    orr += readDelay (bufR, d) * win;
                }
                bufL[(size_t) w] = l[i] + dsp::fastTanh (ol * fb);
                bufR[(size_t) w] = r[i] + dsp::fastTanh (orr * fb);
                w = (w + 1) % size;
                l[i] = wet (l[i], ol, mix); r[i] = wet (r[i], orr, mix);
            }
            break;
        }
        case InsComb:
        {
            const float d = juce::jlimit (2.0f, (float) size - 4.0f, fs / juce::jmax (20.0f, p[0]));
            const float fb = p[1] * 0.98f, damp = p[2] * 0.9f, mix = p[3];
            for (int i = 0; i < n; ++i)
            {
                const float dl = readDelay (bufL, d), dr = readDelay (bufR, d * 1.003f);
                lpL += (dl - lpL) * (1.0f - damp); lpR += (dr - lpR) * (1.0f - damp);
                bufL[(size_t) w] = dsp::fastTanh (l[i] + lpL * fb);
                bufR[(size_t) w] = dsp::fastTanh (r[i] + lpR * fb);
                w = (w + 1) % size;
                l[i] = wet (l[i], dl * 0.6f, mix); r[i] = wet (r[i], dr * 0.6f, mix);
            }
            break;
        }
        case InsVowel:
        {
            static const float f[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
            const float v = juce::jlimit (0.0f, 4.0f, p[0]);
            const int i0 = juce::jmin (3, (int) v);
            const float fr = v - (float) i0, shift = std::exp2 (p[2] / 12.0f), q = 2.0f + p[1] * 14.0f, mix = p[3];
            float F[3];
            for (int k = 0; k < 3; ++k) F[k] = (f[i0][k] + (f[i0 + 1][k] - f[i0][k]) * fr) * shift;
            svfL.set (F[0], q, fs); svfR.set (F[0], q, fs);
            svf2L.set (F[1], q, fs); svf2R.set (F[1], q, fs);
            svf3L.set (F[2], q, fs); svf3R.set (F[2], q, fs);
            const float g = 0.5f + q * 0.12f;
            for (int i = 0; i < n; ++i)
            {
                const float a = (svfL.process (l[i]).band + 0.7f * svf2L.process (l[i]).band + 0.4f * svf3L.process (l[i]).band) * g;
                const float b = (svfR.process (r[i]).band + 0.7f * svf2R.process (r[i]).band + 0.4f * svf3R.process (r[i]).band) * g;
                l[i] = wet (l[i], dsp::fastTanh (a), mix); r[i] = wet (r[i], dsp::fastTanh (b), mix);
            }
            break;
        }
        case InsStutter:
        {
            static const double reps[] = { 1.0, 0.5, 0.25, 0.125 }, holds[] = { 1.0, 2.0, 4.0 };
            const double rep = reps[juce::jlimit (0, 3, juce::roundToInt (p[0]))], hold = holds[juce::jlimit (0, 2, juce::roundToInt (p[2]))];
            const float chance = p[1], mix = p[3];
            const double samplesPerBeat = 1.0 / juce::jmax (1.0e-9, ppqPerSample);
            for (int i = 0; i < n; ++i)
            {
                const double pos = ppq + i * ppqPerSample;
                const long long grid = (long long) std::floor (pos / hold);
                if (grid != stutterGrid)   // a new stretch: stutter this time?
                {
                    stutterGrid = grid;
                    stutterOn = rng.uni() < chance;
                    segStart = w;
                    segLen = juce::jlimit (32, size / 2, (int) (rep * samplesPerBeat));
                    segPos = 0;
                }
                bufL[(size_t) w] = l[i]; bufR[(size_t) w] = r[i];
                w = (w + 1) % size;
                if (! stutterOn || segPos < segLen) { ++segPos; continue; }   // the first piece plays as it comes
                const int k = (segPos - segLen) % segLen;
                const int idx = (segStart + k) % size;
                const float fade = juce::jmin (1.0f, juce::jmin ((float) k, (float) (segLen - k)) / 48.0f);
                l[i] = wet (l[i], bufL[(size_t) idx] * fade, mix);
                r[i] = wet (r[i], bufR[(size_t) idx] * fade, mix);
                ++segPos;
            }
            break;
        }
        case InsPump:
        {
            static const double rates[] = { 2.0, 1.0, 0.5, 4.0 };
            const double period = rates[juce::jlimit (0, 3, juce::roundToInt (p[0]))];
            const float depth = p[1], k = 2.0f + (1.0f - p[2]) * 12.0f;
            for (int i = 0; i < n; ++i)
            {
                double x = (ppq + i * ppqPerSample) / period + p[3];
                const float ph = (float) (x - std::floor (x));
                const float duck = std::exp (-ph * k);
                const float target = 1.0f - depth * duck * juce::jmin (1.0f, ph * 400.0f + 0.2f);
                gain += (target - gain) * 0.05f;
                l[i] *= gain; r[i] *= gain;
            }
            break;
        }
        case InsTransient:
        {
            const float fA = 1.0f - std::exp (-1.0f / (0.001f * fs)), fR = 1.0f - std::exp (-1.0f / (0.05f * fs));
            const float sA = 1.0f - std::exp (-1.0f / (0.025f * fs)), sR = 1.0f - std::exp (-1.0f / (0.25f * fs));
            const float out = dsp::dbToGain (p[2]);
            for (int i = 0; i < n; ++i)
            {
                const float a = juce::jmax (std::abs (l[i]), std::abs (r[i]));
                env += (a - env) * (a > env ? fA : fR);
                env2 += (a - env2) * (a > env2 ? sA : sR);
                const float t = juce::jlimit (0.0f, 3.0f, (env - env2) / (env2 + 1.0e-4f));
                const float sus = juce::jlimit (0.0f, 1.0f, 1.0f - t);
                const float target = std::exp2 (p[0] * t * 1.2f + p[1] * sus * 1.5f * juce::jmin (1.0f, env2 * 20.0f));
                gain += (juce::jlimit (0.1f, 8.0f, target) - gain) * 0.2f;
                l[i] *= gain * out; r[i] *= gain * out;
            }
            break;
        }
        case InsCabinet:
        {
            setCab (juce::roundToInt (p[0]), p[2]);
            const float g = dsp::dbToGain (p[1]), out = dsp::dbToGain (p[3]) / std::sqrt (juce::jmax (1.0f, g * 0.5f)), mix = p[4];
            for (int i = 0; i < n; ++i)
            {
                float a = dsp::fastTanh (l[i] * g), b = dsp::fastTanh (r[i] * g);
                a = eq[4].process (eq[2].process (eq[0].process (a)));
                b = eq[5].process (eq[3].process (eq[1].process (b)));
                l[i] = wet (l[i], a * out, mix); r[i] = wet (r[i], b * out, mix);
            }
            break;
        }
        default: break;
    }
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) { reset(); lastType = type; std::fill (l, l + n, 0.0f); std::fill (r, r + n, 0.0f); return; }
    }
}

//==============================================================================
void ModRuntime::noteOn (int n, float vel, const std::array<float, numModSet>& set)
{
    note = (float) n;
    velocity = vel;
    envStage = 1;
    if (set[MsLfo1Retrig] > 0.5f) lfoPhase[0] = 0.0f;
    if (set[MsLfo2Retrig] > 0.5f) lfoPhase[1] = 0.0f;
}

void ModRuntime::advance (int n, double sr, double ppq, double ppqPerSample, bool playing, double songPpq, float modWheel,
                          const std::array<float, numModSet>& set, const std::array<float, numMacros>& macros)
{
    const double bpm = ppqPerSample * sr * 60.0;
    for (int k = 0; k < 2; ++k)
    {
        const int shape = juce::roundToInt (set[(size_t) (k == 0 ? MsLfo1Shape : MsLfo2Shape)]);
        const double beats = syncBeats (juce::roundToInt (set[(size_t) (k == 0 ? MsLfo1Sync : MsLfo2Sync)]));
        const float before = lfoPhase[k];
        if (beats > 0.0 && playing && set[(size_t) (k == 0 ? MsLfo1Retrig : MsLfo2Retrig)] < 0.5f)
        {
            const double x = songPpq / beats;
            lfoPhase[k] = (float) (x - std::floor (x));
        }
        else
        {
            const double hz = beats > 0.0 ? bpm / 60.0 / beats : (double) set[(size_t) (k == 0 ? MsLfo1Rate : MsLfo2Rate)];
            lfoPhase[k] += (float) (hz * n / sr);
            lfoPhase[k] -= std::floor (lfoPhase[k]);
        }
        const float ph = lfoPhase[k];
        if (ph < before) { lfoPrevHold[k] = lfoHold[k]; lfoHold[k] = rng.next(); }   // a new cycle: S & H / smooth random
        float v;
        switch (shape)
        {
            case 1: v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
            case 2: v = 2.0f * ph - 1.0f; break;
            case 3: v = 1.0f - 2.0f * ph; break;
            case 4: v = ph < 0.5f ? 1.0f : -1.0f; break;
            case 5: v = lfoHold[k]; break;
            case 6: v = lfoPrevHold[k] + (lfoHold[k] - lfoPrevHold[k]) * (0.5f - 0.5f * std::cos (dsp::pi * ph)); break;
            default: v = std::sin (dsp::twoPi * ph); break;
        }
        value[(size_t) (k == 0 ? SrcLfo1 : SrcLfo2)] = v;
    }
    // the note envelope (attack, decay to 0)
    const float dt = (float) (n / sr);
    if (envStage == 1)
    {
        env += dt / juce::jmax (0.001f, set[MsEnvAttack]);
        if (env >= 1.0f) { env = 1.0f; envStage = 2; }
    }
    else if (envStage == 2)
    {
        env *= std::exp (-dt * 4.6f / juce::jmax (0.01f, set[MsEnvDecay]));
        if (env < 0.0005f) { env = 0.0f; envStage = 0; }
    }
    value[SrcEnv] = env;
    value[SrcVelocity] = velocity;
    value[SrcNote] = juce::jlimit (-1.0f, 1.0f, (note - 60.0f) / 24.0f);
    value[SrcModWheel] = modWheel;
    // random: a new value every step of its rate
    double beats = syncBeats (juce::roundToInt (set[MsRandRate]));
    if (beats <= 0.0) beats = 0.25;
    if (ppq >= nextRandom || nextRandom - ppq > beats * 2.0)
    {
        randomTarget = rng.next();
        nextRandom = (std::floor (ppq / beats) + 1.0) * beats;
    }
    const float smooth = set[MsSmooth];
    randomSmooth = smooth <= 0.001f ? randomTarget : randomSmooth + (randomTarget - randomSmooth) * juce::jmin (1.0f, dt / (smooth * (float) (beats * 60.0 / juce::jmax (1.0, bpm)) + 1.0e-4f));
    value[SrcRandom] = randomSmooth;
    value[SrcBar] = playing ? (float) (std::fmod (songPpq, 4.0) / 4.0) : 0.0f;
    for (int m = 0; m < numMacros; ++m) value[(size_t) (SrcMacro1 + m)] = macros[(size_t) m];
    value[SrcNone] = 0.0f;
}
} // namespace catbox
