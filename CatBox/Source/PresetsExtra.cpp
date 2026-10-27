#include "Params.h"
#include "TrackFx.h"

/**
    More factory sounds: each starts from a sound of the engine, changes a few knobs and
    (most of them) brings its own chain of insert effects ("@chain"). Category "FX Lab"
    and a few new ones per engine.
*/
namespace catbox
{
namespace
{
using V = std::vector<std::pair<juce::String, float>>;
struct Extra { int engine; const char* name; const char* category; const char* base; V values; const char* chain; };

const std::vector<Extra>& extras()
{
    static const std::vector<Extra> x = {
        // ---- Analog
        { EngineAnalog, "Dub Chord Stab", "FX Lab", "Stab", { { "cutoff", 1400.0f }, { "d", 0.25f }, { "s", 0.0f } }, "Dub" },
        { EngineAnalog, "Pumping Saw Pad", "FX Lab", "Warm Pad", { { "unison", 4.0f }, { "detune", 22.0f } }, "Pumping" },
        { EngineAnalog, "Glitch Pluck", "FX Lab", "Pluck", {}, "Glitch" },
        { EngineAnalog, "Octave Monster", "FX Lab", "Fat Saw Bass", { { "cutoff", 900.0f } }, "Octaver" },
        { EngineAnalog, "Talking Lead", "FX Lab", "Classic Lead", { { "cutoff", 5000.0f } }, "Talk box" },
        { EngineAnalog, "Underwater Keys", "FX Lab", "Poly Keys", {}, "Underwater" },
        { EngineAnalog, "Shimmer Sunrise", "FX Lab", "Slow Sunrise", {}, "Shimmer up" },
        { EngineAnalog, "Radio Lead", "FX Lab", "Square Lead", {}, "Radio" },
        { EngineAnalog, "Space Bells", "FX Lab", "Bell Pluck", {}, "Space" },
        { EngineAnalog, "VHS Strings", "FX Lab", "Strings Pad", {}, "Old VHS" },
        { EngineAnalog, "Trance Gate Pad", "FX Lab", "Warm Pad", { { "unison", 4.0f }, { "detune", 18.0f }, { "cutoff", 4000.0f } }, "Trance gate" },
        { EngineAnalog, "Hoover", "Lead", "Reese", { { "mode", 2.0f }, { "glide", 0.12f }, { "pwm", 0.6f }, { "unison", 4.0f }, { "detune", 35.0f }, { "cutoff", 3500.0f } }, "" },
        { EngineAnalog, "Detroit Chord", "Keys", "Stab", { { "ftype", 1.0f }, { "cutoff", 900.0f }, { "reso", 0.45f }, { "envamt", 0.5f } }, "Dub" },
        { EngineAnalog, "Moog-ish Bass", "Bass", "Fat Saw Bass", { { "wave2", 1.0f }, { "semi2", -12.0f }, { "cutoff", 500.0f }, { "reso", 0.35f }, { "envamt", 0.6f }, { "fd", 0.25f } }, "" },
        { EngineAnalog, "Acid Lead Saw", "Lead", "Sync Lead", { { "reso", 0.6f }, { "envamt", 0.7f } }, "Acid" },
        { EngineAnalog, "Lofi Dream Pad", "Pad", "Lofi Pad", {}, "Lo-fi" },
        { EngineAnalog, "Robot Voice", "FX Lab", "Square Lead", {}, "Robot" },
        { EngineAnalog, "Wobble Machine", "FX Lab", "Wobble Bass", {}, "Wobble" },
        // ---- Acid
        { EngineAcid, "Acid Echo", "FX Lab", "Classic 303", {}, "Acid" },
        { EngineAcid, "Acid Destroyer", "FX Lab", "Hard Acid", {}, "Destroy" },
        { EngineAcid, "Dub Acid Space", "FX Lab", "Dub Acid", {}, "Dub" },
        { EngineAcid, "Talking 303", "FX Lab", "Squelch", {}, "Talk box" },
        { EngineAcid, "Glitch Acid", "FX Lab", "Bubble", {}, "Glitch" },
        { EngineAcid, "Amp Acid", "FX Lab", "Rubber", {}, "Guitar amp" },
        { EngineAcid, "Pumping Acid", "FX Lab", "Deep Acid", {}, "Pumping" },
        { EngineAcid, "Rave Siren", "Lead", "Acid Lead", { { "reso", 0.9f }, { "lfoamt", 0.5f }, { "lforate", 6.0f } }, "Space" },
        // ---- Drums
        { EngineDrums, "Big Room Kit", "FX Lab", "909 Classic", {}, "Big room" },
        { EngineDrums, "Punchy 808", "FX Lab", "808 Classic", {}, "Punchy" },
        { EngineDrums, "Lo-fi Tape Kit", "FX Lab", "Boom Bap", {}, "Lo-fi" },
        { EngineDrums, "Radio Kit", "FX Lab", "909 House", {}, "Radio" },
        { EngineDrums, "Glitch Kit", "FX Lab", "Minimal Click", {}, "Glitch" },
        { EngineDrums, "Dub Kit", "FX Lab", "Dub Echo Kit", {}, "Dub" },
        { EngineDrums, "Smashed Kit", "FX Lab", "Industrial", {}, "Destroy" },
        { EngineDrums, "Megaphone Kit", "FX Lab", "Breakbeat", {}, "Megaphone" },
        { EngineDrums, "Tight Acoustic", "FX Lab", "Acoustic Room", {}, "Tight drums" },
        { EngineDrums, "Pumping House", "FX Lab", "909 House", {}, "Pumping" },
        // ---- Keys
        { EngineKeys, "Dreamy Rhodes", "FX Lab", "Rhodes", {}, "Vintage keys" },
        { EngineKeys, "Shimmer Piano", "FX Lab", "Soft Piano", {}, "Shimmer up" },
        { EngineKeys, "VHS Piano", "FX Lab", "Grand Piano", {}, "Old VHS" },
        { EngineKeys, "Underwater EP", "FX Lab", "Chorus EP", {}, "Underwater" },
        { EngineKeys, "Space Harp", "FX Lab", "Harp Pluck", {}, "Space" },
        { EngineKeys, "Amp Guitar", "FX Lab", "Nylon Guitar", {}, "Guitar amp" },
        { EngineKeys, "Lofi Bedroom", "FX Lab", "Lofi Piano", {}, "Lo-fi" },
        { EngineKeys, "Music Box Dream", "FX Lab", "Music Box", {}, "Space" },
        { EngineKeys, "Gated Strings", "FX Lab", "Strings", {}, "Trance gate" },
        { EngineKeys, "Wurly Tremolo", "E-Piano", "Suitcase", { { "trem", 0.5f }, { "tremsync", 4.0f } }, "" },
        // ---- FM
        { EngineFM, "80s Ballad EP", "FX Lab", "DX Piano", {}, "Wide pad" },
        { EngineFM, "Crystal Bells", "FX Lab", "Glass Bell", {}, "Shimmer up" },
        { EngineFM, "Dub Marimba", "FX Lab", "Marimba", {}, "Dub" },
        { EngineFM, "Glitch Mallets", "FX Lab", "Vibraphone", {}, "Glitch" },
        { EngineFM, "Radio Organ", "FX Lab", "Drawbar Organ", {}, "Radio" },
        { EngineFM, "Rotary Organ", "Organ", "Percussive Organ", {}, "Vintage keys" },
        { EngineFM, "Octave Slap", "FX Lab", "Slap Bass", {}, "Octaver" },
        { EngineFM, "Metal Koto", "FX Lab", "Koto", {}, "Metal ring" },
        { EngineFM, "Space Flute", "FX Lab", "Flute", {}, "Space" },
        // ---- Wavetable
        { EngineWavetable, "Gated Morph", "FX Lab", "Morph Pad", {}, "Trance gate" },
        { EngineWavetable, "Shimmer Glass", "FX Lab", "Glass Pad", {}, "Shimmer up" },
        { EngineWavetable, "Growl Machine", "FX Lab", "Growl Bass", {}, "Destroy" },
        { EngineWavetable, "Talk Box Lead", "FX Lab", "Talking Lead", {}, "Talk box" },
        { EngineWavetable, "Pump Supersaw", "FX Lab", "Supersaw WT", {}, "Pumping" },
        { EngineWavetable, "Robot Choir", "FX Lab", "Choir Aah", {}, "Robot" },
        { EngineWavetable, "Underwater Organ", "FX Lab", "Organ WT", {}, "Underwater" },
        { EngineWavetable, "Glitch Pluck WT", "FX Lab", "Digi Pluck", {}, "Glitch" },
        // ---- Glide
        { EngineGlide, "Pad in Space", "FX Lab", "Warm Analog Pad", {}, "Space" },
        { EngineGlide, "Neon Shimmer", "FX Lab", "Neon Strings", {}, "Shimmer up" },
        { EngineGlide, "Velvet Tape", "FX Lab", "Velvet Keys", {}, "Old VHS" },
        { EngineGlide, "Brass Pump", "FX Lab", "Synth Brass Stab", {}, "Pumping" },
        { EngineGlide, "Cathedral Wash", "FX Lab", "Cathedral Drone", {}, "Underwater" },
        // ---- Choir
        { EngineChoir, "Choir in Space", "FX Lab", "Cathedral Ooh", {}, "Space" },
        { EngineChoir, "Robot Monks", "FX Lab", "Old Monks", {}, "Robot" },
        { EngineChoir, "Radio Diva", "FX Lab", "Soul Diva", {}, "Radio" },
        { EngineChoir, "Shimmer Kittens", "FX Lab", "Kitten Choir", {}, "Shimmer up" },
        { EngineChoir, "Megaphone Cat", "FX Lab", "Meow Lead", {}, "Megaphone" },
        { EngineChoir, "Gated Gospel", "FX Lab", "Gospel Mass", {}, "Trance gate" },
        // ---- Sampler / Granular
        { EngineSampler, "Glitch Break", "FX Lab", "Break Chop", {}, "Glitch" },
        { EngineSampler, "Dusty Break", "FX Lab", "Lofi Break", {}, "Lo-fi" },
        { EngineSampler, "Vox in Space", "FX Lab", "Vox Keys", {}, "Space" },
        { EngineSampler, "Octave 808", "FX Lab", "808 Bass", {}, "Octaver" },
        { EngineGranular, "Frozen Shimmer", "FX Lab", "Frozen Vox", {}, "Shimmer up" },
        { EngineGranular, "Glitch Cloud", "FX Lab", "Stutter Break", {}, "Glitch" },
        { EngineGranular, "Deep Space Drone", "FX Lab", "Deep Drone", {}, "Underwater" },
    };
    return x;
}
} // namespace

/** Called once per engine while its definition is built. */
void addExtraPresets (EngineDef& e, int engine)
{
    const auto chains = chainPresetNames();
    std::vector<Preset> add;
    for (const auto& x : extras())
    {
        if (x.engine != engine) continue;
        const Preset* base = nullptr;
        for (const auto& p : e.presets) if (p.name == x.base) { base = &p; break; }
        if (base == nullptr) continue;
        Preset p;
        p.name = x.name;
        p.category = x.category;
        p.values = base->values;
        for (const auto& [id, v] : x.values)
        {
            bool found = false;
            for (auto& pv : p.values) if (pv.first == id) { pv.second = v; found = true; }
            if (! found) p.values.push_back ({ id, v });
        }
        if (juce::String (x.chain).isNotEmpty())
            if (const int c = chains.indexOf (x.chain); c >= 0) p.values.push_back ({ "@chain", (float) c });
        add.push_back (std::move (p));
    }
    // before "Init" (it stays last)
    auto at = e.presets.end();
    for (auto it = e.presets.begin(); it != e.presets.end(); ++it) if (it->name == "Init") { at = it; break; }
    e.presets.insert (at, add.begin(), add.end());
}

int presetChain (int engine, int preset)
{
    const auto& d = engineDef (engine);
    if (preset < 0 || preset >= (int) d.presets.size()) return -1;
    for (const auto& [id, v] : d.presets[(size_t) preset].values) if (id == "@chain") return juce::roundToInt (v);
    return -1;
}
} // namespace catbox
