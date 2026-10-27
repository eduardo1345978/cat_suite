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

struct LibPreset { int engine; const char* name; const char* category; std::vector<std::pair<juce::String, juce::String>> samples; V values; };
/** Sounds made from the libraries (VSCO 2 CE, Dirt-Samples): the samples by name, found when the library is there. */
const std::vector<LibPreset>& libraryPresets()
{
    static const std::vector<LibPreset> x = {
        { EngineKit, "TR-808", "Library", { { "s1", "808bd/BD0025" }, { "s2", "808sd/SD0010" }, { "s3", "808/CP" }, { "s4", "808hc/HC00" }, { "s5", "808oh/OH25" }, { "s6", "808lt/LT25" }, { "s7", "808mt/MT25" }, { "s8", "808ht/HT25" }, { "s9", "808/RS" }, { "s10", "808/CB" }, { "s11", "808cy/CY0050" }, { "s12", "808cy/CY0010" }, { "s13", "808/MA" }, { "s14", "808/CL" }, { "s15", "808/CH" }, { "s16", "808sd/SD0050" } }, {} },
        { EngineKit, "TR-909 Style", "Library", { { "s1", "bd/BT0A0A7" }, { "s2", "sn/ST0T0S3" }, { "s3", "cp/HANDCLP0" }, { "s4", "hc/HHCD0" }, { "s5", "ho/HHOD4" }, { "s6", "lt/LT0D3" }, { "s7", "mt/MT0D3" }, { "s8", "ht/HT0D3" }, { "s9", "rm/RIM0" }, { "s10", "808/CB" }, { "s11", "808cy/CY0025" }, { "s12", "cr/RIDED2" }, { "s13", "drumtraks/000_DT Cabasa" }, { "s14", "east/003_shime_hi" }, { "s15", "realclaps/1" }, { "s16", "808/MA" } }, {} },
        { EngineKit, "Jazz Kit", "Library", { { "s1", "jazz/000_BD" }, { "s2", "jazz/007_SN" }, { "s3", "realclaps/2" }, { "s4", "jazz/003_HH" }, { "s5", "jazz/004_OH" }, { "s6", "gretsch/001_brushlotom" }, { "s7", "gretsch/000_brushhitom" }, { "s8", "gretsch/012_hitom" }, { "s9", "gretsch/009_flam" }, { "s10", "jazz/001_CB" }, { "s11", "gretsch/007_cymbalgrab" }, { "s12", "gretsch/019_ridecymbal" }, { "s13", "gretsch/010_foothat" }, { "s14", "jazz/005_P1" }, { "s15", "gretsch/021_snareghost" }, { "s16", "jazz/002_FX" } }, {} },
        { EngineKit, "House Kit", "Library", { { "s1", "house/000_BD" }, { "s2", "house/007_SN" }, { "s3", "realclaps/3" }, { "s4", "house/003_HH" }, { "s5", "house/004_OH" }, { "s6", "lt/LT0D7" }, { "s7", "mt/MT0D7" }, { "s8", "ht/HT0D7" }, { "s9", "rm/RIMA" }, { "s10", "house/001_CB" }, { "s11", "808cy/CY0025" }, { "s12", "cr/RIDED4" }, { "s13", "drumtraks/010_DT Tambourine" }, { "s14", "house/005_P1" }, { "s15", "house/006_P2" }, { "s16", "house/002_FX" } }, {} },
        { EngineKit, "Techno Kit", "Library", { { "s1", "tech/tn1kick1" }, { "s2", "tech/tn1snare1" }, { "s3", "cp/HANDCLPA" }, { "s4", "tech/tn1closedhh" }, { "s5", "tech/tn1openhh" }, { "s6", "tech/tn1hit1" }, { "s7", "tech/tn1hit2" }, { "s8", "tech/tn1hit3" }, { "s9", "tech/tn1perc1" }, { "s10", "tech/tn1perc2" }, { "s11", "tech/tn1crash" }, { "s12", "tech/tn1ride" }, { "s13", "tech/tn1kick2" }, { "s14", "tech/tn1snare2" }, { "s15", "techno/000_0" }, { "s16", "techno/001_1" } }, {} },
        { EngineKit, "Electro Kit", "Library", { { "s1", "electro1/005_et1kick1" }, { "s2", "electro1/011_et1snare1" }, { "s3", "realclaps/4" }, { "s4", "electro1/000_et1closedhh" }, { "s5", "electro1/007_et1openhh" }, { "s6", "electro1/002_et1hit1" }, { "s7", "electro1/003_et1hit2" }, { "s8", "electro1/004_et1hit3" }, { "s9", "electro1/008_et1perc1" }, { "s10", "electro1/009_et1perc2" }, { "s11", "electro1/001_et1crash" }, { "s12", "electro1/010_et1ride" }, { "s13", "electro1/006_et1kick2" }, { "s14", "electro1/012_et1snare2" }, { "s15", "sid/003_blipp01" }, { "s16", "sid/000_bas2" } }, {} },
        { EngineKit, "Hip Hop Kit", "Library", { { "s1", "hh/005_hh3kick1" }, { "s2", "hh/011_hh3snare1" }, { "s3", "realclaps/1" }, { "s4", "hh/000_hh3closedhh" }, { "s5", "hh/007_hh3openhh" }, { "s6", "hh/002_hh3hit1" }, { "s7", "hh/003_hh3hit2" }, { "s8", "hh/004_hh3hit3" }, { "s9", "hh/008_hh3rerc1" }, { "s10", "hh/009_hh3rerc2" }, { "s11", "hh/001_hh3crash" }, { "s12", "hh/010_hh3ride" }, { "s13", "hh/006_hh3kick2" }, { "s14", "hh/012_hh3snare2" }, { "s15", "click/000_click0" }, { "s16", "click/001_click1" } }, {} },
        { EngineKit, "DrumTraks", "Library", { { "s1", "drumtraks/006_DT Kick" }, { "s2", "drumtraks/009_DT Snare" }, { "s3", "drumtraks/001_DT Claps" }, { "s4", "drumtraks/004_DT Hat Closed" }, { "s5", "drumtraks/005_DT Hat Open" }, { "s6", "drumtraks/012_DT Tom2" }, { "s7", "drumtraks/011_DT Tom1" }, { "s8", "ht/HT0DA" }, { "s9", "drumtraks/008_DT Rimshot" }, { "s10", "drumtraks/002_DT Cowbell" }, { "s11", "drumtraks/003_DT Crash" }, { "s12", "drumtraks/007_DT Ride" }, { "s13", "drumtraks/000_DT Cabasa" }, { "s14", "drumtraks/010_DT Tambourine" }, { "s15", "realclaps/2" }, { "s16", "tok/000_0" } }, {} },
        { EngineKit, "Vintage DR", "Library", { { "s1", "dr2/003_DR110KIK" }, { "s2", "dr2/005_DR110SNR" }, { "s3", "dr2/001_DR110CLP" }, { "s4", "dr2/000_DR110CHT" }, { "s5", "dr2/004_DR110OHT" }, { "s6", "lt/LT0DA" }, { "s7", "mt/MT0DA" }, { "s8", "ht/HT0DA" }, { "s9", "dr55/002_DR55 rimshot" }, { "s10", "808/CB" }, { "s11", "dr2/002_DR110CYM" }, { "s12", "cr/RIDED6" }, { "s13", "dr55/000_DR55 hi hat" }, { "s14", "dr55/003_DR55 snare" }, { "s15", "co/CLOP1" }, { "s16", "dr55/001_DR55 kick" } }, {} },
        { EngineKit, "Sequential Tom", "Library", { { "s1", "sequential/003_Tom Kick" }, { "s2", "sequential/005_Tom Snare" }, { "s3", "sequential/000_Tom Clap" }, { "s4", "sequential/002_Tom Hat Closed" }, { "s5", "sequential/004_Tom Openhat" }, { "s6", "sequential/007_Tom Tom2" }, { "s7", "sequential/006_Tom Tom1" }, { "s8", "ht/HT0D0" }, { "s9", "rs/rytm-rs" }, { "s10", "808/CB" }, { "s11", "sequential/001_Tom Crash" }, { "s12", "cr/RIDED0" }, { "s13", "drumtraks/000_DT Cabasa" }, { "s14", "east/001_ohkawa_mute" }, { "s15", "realclaps/3" }, { "s16", "sid/003_blipp01" } }, {} },
        { EngineKit, "Brush Kit", "Library", { { "s1", "gretsch/013_kick" }, { "s2", "gretsch/002_brushsnare" }, { "s3", "gretsch/009_flam" }, { "s4", "gretsch/004_closedhat" }, { "s5", "gretsch/017_openhat" }, { "s6", "gretsch/015_lotom" }, { "s7", "gretsch/001_brushlotom" }, { "s8", "gretsch/012_hitom" }, { "s9", "gretsch/003_brushsnareghost" }, { "s10", "gretsch/006_cowbell" }, { "s11", "gretsch/007_cymbalgrab" }, { "s12", "gretsch/019_ridecymbal" }, { "s13", "gretsch/010_foothat" }, { "s14", "gretsch/018_ridebell" }, { "s15", "gretsch/021_snareghost" }, { "s16", "gretsch/008_cymbalrub" } }, {} },
        { EngineKit, "Taiko & East", "Library", { { "s1", "east/006_taiko_1" }, { "s2", "east/003_shime_hi" }, { "s3", "east/005_shime_mute" }, { "s4", "east/000_nipon_wood_block" }, { "s5", "east/002_ohkawa_open" }, { "s6", "east/007_taiko_2" }, { "s7", "east/008_taiko_3" }, { "s8", "east/004_shime_hi_2" }, { "s9", "east/001_ohkawa_mute" }, { "s10", "tok/001_1" }, { "s11", "tok/002_2" }, { "s12", "tok/003_3" }, { "s13", "tabla/000_bass_flick1" }, { "s14", "tabla/009_hi_hit1" }, { "s15", "tabla/003_dead_hit1" }, { "s16", "tabla/007_hi_flick1" } }, {} },
        { EngineKit, "Tabla", "Library", { { "s1", "tabla/000_bass_flick1" }, { "s2", "tabla/001_bass_flick2" }, { "s3", "tabla/002_bass_lick1" }, { "s4", "tabla/003_dead_hit1" }, { "s5", "tabla/004_dead_hit2" }, { "s6", "tabla/005_dead_hit3" }, { "s7", "tabla/006_d_sharp_hit" }, { "s8", "tabla/007_hi_flick1" }, { "s9", "tabla/008_hi_flick2" }, { "s10", "tabla/009_hi_hit1" }, { "s11", "tabla/010_hi_hit10" }, { "s12", "tabla/011_hi_hit2" }, { "s13", "tabla/012_hi_hit3" }, { "s14", "tabla/013_hi_hit4" }, { "s15", "tabla/014_hi_hit5" }, { "s16", "tabla/015_hi_hit6" } }, {} },
        { EngineKit, "Lo-fi Feel", "Library", { { "s1", "feel/BD 04 d" }, { "s2", "feel/Sd 139" }, { "s3", "realclaps/1" }, { "s4", "feel/HH 003b" }, { "s5", "feel/hihat029a" }, { "s6", "lt/LT0D0" }, { "s7", "mt/MT0D0" }, { "s8", "ht/HT0D0" }, { "s9", "rm/RIM0" }, { "s10", "808/CB" }, { "s11", "808cy/CY0000" }, { "s12", "cr/RIDED2" }, { "s13", "drumtraks/010_DT Tambourine" }, { "s14", "feel/Sd 180" }, { "s15", "feel/Sd 223" }, { "s16", "feel/sub" } }, {} },
        { EngineKit, "Orchestra Perc", "Library", { { "s1", "VSCO 1 Percussion/drums/bass@C2" }, { "s2", "VSCO 1 Percussion/drums/snare/drum1@C4" }, { "s3", "VSCO 1 Percussion/drums/snare/drum3_marching@C4" }, { "s4", "VSCO 1 Percussion/varWood/Camo's Shaker@C4" }, { "s5", "VSCO 1 Percussion/varMetal/triangle/1@C4" }, { "s6", "VSCO 1 Percussion/drums/tenor/tenor_lower@C4" }, { "s7", "VSCO 1 Percussion/drums/tenor/tenor_higher@C4" }, { "s8", "VSCO 1 Percussion/drums/other/Bongos@C4" }, { "s9", "VSCO 1 Percussion/varWood@C4" }, { "s10", "VSCO 1 Percussion/varMetal/various@C4" }, { "s11", "VSCO 1 Percussion/varMetal/Cymbals/clash@C4" }, { "s12", "VSCO 1 Percussion/varMetal/Cymbals/susp@C4" }, { "s13", "VSCO 1 Percussion/varWood/log_drum@C4" }, { "s14", "VSCO 1 Percussion/drums/other/ethnic/congo/open@C4" }, { "s15", "VSCO 1 Percussion/varMetal/Gong@C4" }, { "s16", "Percussion/Timpani@C3" } }, {} },
        { EngineSampler, "Violins", "Library Strings", { { "sample", "Strings/Violin Section/susVib@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Violin Pizzicato", "Library Strings", { { "sample", "Strings/Violin Section/Pizz@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Violin Tremolo", "Library Strings", { { "sample", "Strings/Violin Section/Trem@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Violin Spiccato", "Library Strings", { { "sample", "Strings/Violin Section/Spic@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Violas", "Library Strings", { { "sample", "Strings/Viola Section/susvib@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Viola Pizzicato", "Library Strings", { { "sample", "Strings/Viola Section/pizz@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Viola Tremolo", "Library Strings", { { "sample", "Strings/Viola Section/trem@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Cellos", "Library Strings", { { "sample", "Strings/Cello Section/susvib@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Cello Pizzicato", "Library Strings", { { "sample", "Strings/Cello Section/pizzT@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Cello Tremolo", "Library Strings", { { "sample", "Strings/Cello Section/trem@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Cello Spiccato", "Library Strings", { { "sample", "Strings/Cello Section/spic@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Solo Violin", "Library Strings", { { "sample", "Strings/Solo Violin/Arco Vib@A4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Solo Violin Pizz", "Library Strings", { { "sample", "Strings/Solo Violin/Pizz@A4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Contrabass", "Library Strings", { { "sample", "Strings/Solo Contrabass/SusVib@E2" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Contrabass Pizz", "Library Strings", { { "sample", "Strings/Solo Contrabass/Pizz@E2" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Harp", "Library Strings", { { "sample", "Strings/Harp@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "French Horn", "Library Brass", { { "sample", "Brass/F Horn/sus@F3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Horn Staccato", "Library Brass", { { "sample", "Brass/F Horn/stac@F3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Muted Horn", "Library Brass", { { "sample", "Brass/F Horn/mute@F3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Trumpet", "Library Brass", { { "sample", "Brass/Trumpet/sus@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Trumpet Vibrato", "Library Brass", { { "sample", "Brass/Trumpet/susvib@C5" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Trumpet Staccato", "Library Brass", { { "sample", "Brass/Trumpet/stac@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Harmon Trumpet", "Library Brass", { { "sample", "Brass/Trumpet/harmonM-sus@D4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Trombone", "Library Brass", { { "sample", "Brass/Tenor Trombone/sus@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Trombone Staccato", "Library Brass", { { "sample", "Brass/Tenor Trombone/stac@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Trombone Buzz", "Library Brass", { { "sample", "Brass/OldTrombone/Buzz@D3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Trombone Fall", "Library Brass", { { "sample", "Brass/OldTrombone/Fall@D3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Tuba", "Library Brass", { { "sample", "Brass/Tuba/sus@F1" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Tuba Staccato", "Library Brass", { { "sample", "Brass/Tuba/stac@F1" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Flute", "Library Woodwinds", { { "sample", "Woodwinds/Flute/susvib@C5" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Flute Straight", "Library Woodwinds", { { "sample", "Woodwinds/Flute/susNV@C5" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Flute Staccato", "Library Woodwinds", { { "sample", "Woodwinds/Flute/stac@C5" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Clarinet", "Library Woodwinds", { { "sample", "Woodwinds/Clarinet/susLong@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Clarinet Staccato", "Library Woodwinds", { { "sample", "Woodwinds/Clarinet/stac@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Oboe", "Library Woodwinds", { { "sample", "Woodwinds/Oboe/Vib@C5" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Oboe Staccato", "Library Woodwinds", { { "sample", "Woodwinds/Oboe/Stacc@C5" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Bassoon", "Library Woodwinds", { { "sample", "Woodwinds/Bassoon/sus@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Bassoon Staccato", "Library Woodwinds", { { "sample", "Woodwinds/Bassoon/stac@C3" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Piccolo", "Library Woodwinds", { { "sample", "Woodwinds/Piccolo/Sus@C6" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Glockenspiel", "Library Mallets", { { "sample", "Percussion/Glock@C5" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Marimba Real", "Library Mallets", { { "sample", "Percussion/Marimba@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Xylophone", "Library Mallets", { { "sample", "Percussion/Xylo@C6" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Upright Piano Real", "Library Keys", { { "sample", "Keys/Upright Nr1@C4" } }, { { "multi", 1 }, { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Timpani", "Library Percussion", { { "sample", "Percussion/Timpani@C3" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Timpani Roll", "Library Percussion", { { "sample", "Percussion/Timpani/Rolls@C3" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Gong", "Library Percussion", { { "sample", "VSCO 1 Percussion/varMetal/Gong@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Nepalese Bells", "Library Percussion", { { "sample", "Miscellania Raw/Misc 2/NepaleseBells@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Glock Glissando", "Library Percussion", { { "sample", "Miscellania Raw/Misc 2/glock_glisses@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Juno Stab", "Library Synth", { { "sample", "juno@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Moog Bass", "Library Synth", { { "sample", "moog@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Jungle Bass", "Library Synth", { { "sample", "jungbass@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Hoover Rave", "Library Synth", { { "sample", "hoover@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Rave Stab", "Library Synth", { { "sample", "rave@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Arpy", "Library Synth", { { "sample", "arpy@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Casio Tone", "Library Synth", { { "sample", "casio@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Sitar", "Library World", { { "sample", "sitar@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Sax Hit", "Library World", { { "sample", "sax@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Speak & Spell", "Library Vocal", { { "sample", "speakspell@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Yeah!", "Library Vocal", { { "sample", "yeah@C4" } }, { { "mode", 0 }, { "loop", 0 }, { "r", 0.6f }, { "d", 4.0f }, { "s", 1.0f } } },
        { EngineSampler, "Pad Long", "Library Synth", { { "sample", "padlong@C4" } }, { { "mode", 0 }, { "loop", 1 }, { "loopstart", 0.4f }, { "end", 0.97f }, { "a", 0.02f }, { "r", 0.45f } } },
        { EngineSampler, "Amen Chop", "Library Breaks", { { "sample", "amencutup@C4" } }, { { "mode", 1 }, { "slices", 16 } } },
        { EngineSampler, "Break 125", "Library Breaks", { { "sample", "breaks125@C4" } }, { { "mode", 1 }, { "slices", 16 } } },
        { EngineSampler, "Break 152", "Library Breaks", { { "sample", "breaks152@C4" } }, { { "mode", 1 }, { "slices", 16 } } },
        { EngineSampler, "Break 157", "Library Breaks", { { "sample", "breaks157@C4" } }, { { "mode", 1 }, { "slices", 16 } } },
        { EngineSampler, "Break 165", "Library Breaks", { { "sample", "breaks165@C4" } }, { { "mode", 1 }, { "slices", 16 } } },
        { EngineGranular, "Violin Cloud", "Library", { { "sample", "Strings/Violin Section/susVib@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Cello Fog", "Library", { { "sample", "Strings/Cello Section/susvib@C3" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Brass Haze", "Library", { { "sample", "Brass/Tenor Trombone/sus@C3" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Flute Air", "Library", { { "sample", "Woodwinds/Flute/susvib@C5" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Harp Glitter", "Library", { { "sample", "Strings/Harp@C5" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Gong Drone", "Library", { { "sample", "VSCO 1 Percussion/varMetal/Gong@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Organ Breath", "Library", { { "sample", "Keys/Organ/Quiet@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Tremolo Swarm", "Library", { { "sample", "Strings/Violin Section/Trem@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Bells Dust", "Library", { { "sample", "Miscellania Raw/Misc 2/NepaleseBells@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Speech Grains", "Library", { { "sample", "speech@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Bird Cloud", "Library", { { "sample", "birds@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
        { EngineGranular, "Wind Texture", "Library", { { "sample", "wind@C4" } }, { { "pos", 0.35f }, { "size", 0.12f }, { "density", 18 }, { "spray", 0.2f } } },
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
    for (const auto& x : libraryPresets())
    {
        if (x.engine != engine) continue;
        Preset p;
        p.name = x.name;
        p.category = x.category;
        p.values = x.values;
        p.samples = x.samples;
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
