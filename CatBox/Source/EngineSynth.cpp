#include "Engine.h"
#include "Dsp.h"
#include "Synth.h"
#include <cmath>

/**
    SYNTH: the "digital analog" synthesizer. Every voice: 3 oscillators (+ sub, noise, ring,
    cross mod, sync), a mixer that can be overloaded, a filter whose CIRCUIT is the model's
    (Moog ladder, Prophet / Jupiter / Juno OTA, Oberheim 12 dB state variable, 303 diode,
    MS-20 screaming 12 dB, Buchla lowpass gate), filter and amp envelopes, one LFO, poly-mod;
    then the model's chorus, drive and width. Mono, legato, poly or unison - always switchable.
*/
namespace catbox
{
namespace
{
enum P
{
    pModel = 0, pMode, pVoices, pUniDetune, pGlide, pBend, pDrift,
    pO1Range, pO1Wave, pO1Pw,
    pO2Range, pO2Semi, pO2Fine, pO2Wave, pO2Sync,
    pO3Range, pO3Semi, pO3Fine, pO3Wave, pO3Lfo,
    pO1Lvl, pO2Lvl, pO3Lvl, pSub, pNoise, pRing, pCross, pOverload,
    pCutoff, pReso, pEnvAmt, pKeyTrack, pFMode, pHpf, pVelFilt,
    pFA, pFD, pFS, pFR,
    pA, pD, pS, pR, pVelAmp,
    pLfoRate, pLfoShape, pLfoSync, pLfoDelay, pLfoPitch, pLfoFilter, pLfoPw, pLfoAmp,
    pPmEnv, pPmOsc,
    pChorus, pDrive, pWidth, pVolume, pModWheel,
    pCount
};
enum Mode { MMono = 0, MLegato, MPoly, MUnison };
enum Wave { WTri = 0, WShark, WSaw, WSquare, WWide, WNarrow, WSine, WFold };
enum FilterKind { FLadder = 0, FOta, FDiode, FSvf, FScream, FLpg };

struct Character
{
    int filter;
    float stageSat;   // drive inside the filter (warmth)
    float fbSat;      // resonance clipping
    float resMax;     // feedback at resonance 100 %
    float resComp;    // bass kept at high resonance (0 = Moog-like thinning)
    float drift;      // oscillator wander (cents at drift 100 %)
    float envSnap;    // faster envelopes (DCO-era snap)
    float noiseFloor; // a little hiss
    float chorusDepth;
};

const Character& character (int m)
{
    static const Character c[NumSynthModels] = {
        //  filter   stage  fb    resMax resComp drift snap  noise   chorus
        { FLadder, 1.6f, 1.0f, 4.1f, 0.25f, 9.0f, 1.0f, 0.0006f, 1.0f },   // Mini
        { FOta,    1.1f, 1.4f, 4.0f, 0.55f, 6.0f, 1.0f, 0.0004f, 1.0f },   // Prophet
        { FSvf,    1.2f, 1.2f, 1.0f, 1.0f,  7.0f, 1.0f, 0.0004f, 1.0f },   // Oberheim
        { FOta,    0.9f, 1.1f, 4.0f, 0.75f, 4.0f, 1.2f, 0.0002f, 1.0f },   // Jupiter
        { FOta,    0.8f, 1.0f, 4.0f, 0.85f, 0.6f, 1.5f, 0.0001f, 1.3f },   // Juno: DCO, stable
        { FOta,    1.0f, 1.3f, 4.1f, 0.6f,  1.0f, 1.6f, 0.0002f, 1.0f },   // SH
        { FDiode,  1.4f, 1.6f, 3.4f, 0.4f,  2.0f, 1.4f, 0.0003f, 1.0f },   // 303
        { FScream, 2.2f, 2.5f, 1.0f, 1.0f,  8.0f, 1.2f, 0.0008f, 1.0f },   // MS-20
        { FOta,    1.3f, 1.8f, 4.0f, 0.5f,  7.0f, 1.2f, 0.0005f, 1.0f },   // Odyssey
        { FLpg,    1.0f, 1.0f, 1.0f, 1.0f,  5.0f, 1.0f, 0.0003f, 1.0f },   // Buchla
    };
    return c[juce::jlimit (0, NumSynthModels - 1, m)];
}
} // namespace

//==============================================================================
const SynthModelLook& synthModelLook (int m)
{
    static const SynthModelLook looks[NumSynthModels] = {
        //                                                                                            panel       knob face   accent      ink         wood
        { "MINI",     "Moog Minimoog",        "3 oscillators, the 24 dB ladder: fat basses and leads",     0xff141414, 0xff1e1e1e, 0xffff9a3c, 0xffefe9dc, 0xff7a4a2a },
        { "PROPHET",  "Sequential Prophet-5", "poly-mod, warm pads and brass, 80s poly",                   0xff1b1712, 0xff111111, 0xffe8c06a, 0xfff1ead8, 0xff5e3a20 },
        { "OB",       "Oberheim SEM / OB-X",  "the 12 dB filter from low pass through notch to high pass", 0xff26364a, 0xff131a24, 0xffcfe3ff, 0xffe9f0fa, 0 },
        { "JUPITER",  "Roland Jupiter-8",     "lush, clean and wide",                                      0xff0c0c0c, 0xff1d1d1d, 0xffff4d3d, 0xffeeeeee, 0 },
        { "JUNO",     "Roland Juno-60",       "one DCO, sub, the famous chorus",                           0xff3b3f44, 0xff1c1d20, 0xffffb02e, 0xfff4f4f2, 0 },
        { "SH",       "Roland SH-101",        "one oscillator, punchy mono bass and leads",                0xff42566b, 0xff222b35, 0xff7fdcff, 0xfff0f6fb, 0 },
        { "ACID",     "Roland TB-303",        "the diode filter: squelch",                                 0xffc9c6bd, 0xff2b2b2b, 0xffc0392b, 0xff1c1c1c, 0 },
        { "MS",       "Korg MS-20",           "two filters that scream",                                   0xff101010, 0xff262626, 0xffffffff, 0xffdadada, 0 },
        { "ODYSSEY",  "ARP Odyssey",          "duophonic, sync and ring mod",                              0xffe8e3d6, 0xff222222, 0xffe0561f, 0xff1b1b1b, 0 },
        { "BUCHLA",   "Buchla Music Easel",   "wave folding and the lowpass gate: west coast",             0xff2b2240, 0xff3d3159, 0xffffc93d, 0xfff3eefb, 0 },
    };
    return looks[juce::jlimit (0, NumSynthModels - 1, m)];
}

juce::StringArray synthModelNames()
{
    juce::StringArray n;
    for (int i = 0; i < NumSynthModels; ++i) n.add (synthModelLook (i).name);
    return n;
}

const std::vector<SynthSection>& synthPanel()
{
    static const std::vector<SynthSection> s = {
        { "CONTROLLERS", { "glide", "mode", "voices", "unidetune", "bend", "drift", "modwheel" } },
        { "OSCILLATOR 1", { "o1range", "o1wave", "o1pw" } },
        { "OSCILLATOR 2", { "o2range", "o2wave", "o2semi", "o2fine", "o2sync" } },
        { "OSCILLATOR 3", { "o3range", "o3wave", "o3semi", "o3fine", "o3lfo" } },
        { "MIXER", { "o1lvl", "o2lvl", "o3lvl", "sub", "noise", "ring", "cross", "overload" } },
        { "FILTER", { "cutoff", "reso", "envamt", "keytrack", "fmode", "hpf", "velfilt" } },
        { "FILTER ENV", { "fa", "fd", "fs", "fr" } },
        { "AMP ENV", { "a", "d", "s", "r", "velamp" } },
        { "LFO", { "lforate", "lfoshape", "lfosync", "lfodelay", "lfopitch", "lfofilter", "lfopw", "lfoamp" } },
        { "POLY-MOD", { "pmenv", "pmosc" } },
        { "OUTPUT", { "chorus", "drive", "width", "volume" } },
    };
    return s;
}

EngineDef makeSynthDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Synth";
    const juce::StringArray ranges { "32'", "16'", "8'", "4'", "2'" };
    const juce::StringArray waves { "Triangle", "Shark", "Saw", "Square", "Wide pulse", "Narrow pulse", "Sine", "Fold" };
    auto& p = e.params;
    p = {
        choice ("model", "Model", synthModelNames(), 0), choice ("mode", "Voices", { "Mono", "Legato", "Poly", "Unison" }, 2),
        num ("voices", "Voice count", 1, 8, 8), cents ("unidetune", "Unison detune", 0, 50, 14), sec ("glide", "Glide", 0.0f, 2.0f, 0.0f),
        semi ("bend", "Bend range", 0, 12, 2), pct ("drift", "Analog drift", 0.35f),
        choice ("o1range", "Osc 1 range", ranges, 2), choice ("o1wave", "Osc 1 wave", waves, 2), pct ("o1pw", "Osc 1 shape", 0.5f),
        choice ("o2range", "Osc 2 range", ranges, 2), semi ("o2semi", "Osc 2 pitch", -12, 12, 0), cents ("o2fine", "Osc 2 fine", -50, 50, 7),
        choice ("o2wave", "Osc 2 wave", waves, 2), toggle ("o2sync", "Osc 2 sync", false),
        choice ("o3range", "Osc 3 range", ranges, 1), semi ("o3semi", "Osc 3 pitch", -12, 12, 0), cents ("o3fine", "Osc 3 fine", -50, 50, -5),
        choice ("o3wave", "Osc 3 wave", waves, 2), toggle ("o3lfo", "Osc 3 low", false),
        pct ("o1lvl", "Osc 1", 1.0f), pct ("o2lvl", "Osc 2", 0.7f), pct ("o3lvl", "Osc 3", 0.0f), pct ("sub", "Sub", 0.0f),
        pct ("noise", "Noise", 0.0f), pct ("ring", "Ring mod", 0.0f), pct ("cross", "Cross mod", 0.0f), pct ("overload", "Overload", 0.2f),
        hz ("cutoff", "Cutoff", 20.0f, 20000.0f, 1500.0f), pct ("reso", "Emphasis", 0.25f), bi ("envamt", "Contour", 0.4f),
        pct ("keytrack", "Key track", 0.5f), choice ("fmode", "Filter mode", { "Low pass", "Band pass", "High pass", "Notch" }, 0),
        hz ("hpf", "High pass", 20.0f, 1500.0f, 20.0f), pct ("velfilt", "Vel > filter", 0.3f),
        sec ("fa", "F attack", 0.001f, 10.0f, 0.004f), sec ("fd", "F decay", 0.005f, 10.0f, 0.45f), pct ("fs", "F sustain", 0.2f), sec ("fr", "F release", 0.005f, 10.0f, 0.3f),
        sec ("a", "Attack", 0.001f, 10.0f, 0.003f), sec ("d", "Decay", 0.005f, 10.0f, 0.3f), pct ("s", "Sustain", 0.85f), sec ("r", "Release", 0.005f, 10.0f, 0.25f),
        pct ("velamp", "Velocity", 0.5f),
        hz ("lforate", "LFO rate", 0.05f, 30.0f, 5.0f), choice ("lfoshape", "LFO shape", { "Triangle", "Sine", "Saw", "Square", "S & H", "Smooth" }, 0),
        choice ("lfosync", "LFO sync", syncNames(), 0), sec ("lfodelay", "LFO delay", 0.0f, 5.0f, 0.0f),
        pct ("lfopitch", "Vibrato", 0.0f), pct ("lfofilter", "LFO > filter", 0.0f), pct ("lfopw", "LFO > shape", 0.0f), pct ("lfoamp", "Tremolo", 0.0f),
        bi ("pmenv", "Env > osc 1", 0.0f), pct ("pmosc", "Osc 2 > filter", 0.0f),
        choice ("chorus", "Chorus", { "Off", "I", "II", "I + II" }, 0), pct ("drive", "Drive", 0.0f), pct ("width", "Width", 0.6f),
        db ("volume", "Volume", -36.0f, 6.0f, -6.0f), pct ("modwheel", "Mod wheel", 0.5f),
    };
    jassert ((int) p.size() == pCount);
    e.pages = {
        page ("OSC", { pO1Range, pO1Wave, pO1Lvl, pO2Range, pO2Wave, pO2Lvl, pO3Range, pO3Wave, pO3Lvl, pO2Fine, pCutoff, pReso,
                       pO2Semi, pO3Semi, pO3Fine, pO1Pw, pSub, pNoise, pRing, pCross, pO2Sync, pO3Lfo, pOverload, pGlide }),
        page ("FILTER", { pCutoff, pReso, pEnvAmt, pKeyTrack, pFA, pFD, pFS, pFR, pOverload, pFMode, pHpf, pVelFilt,
                          pA, pD, pS, pR, pVelAmp, pPmEnv, pPmOsc, pLfoFilter, pDrive, pModel, pMode, pVolume }),
        page ("ENV", { pFA, pFD, pFS, pFR, pA, pD, pS, pR, pCutoff, pEnvAmt, pVelAmp, pGlide,
                       pVelFilt, pKeyTrack, pReso, pFMode, pHpf, pOverload, pDrift, pBend, pMode, pVoices, pUniDetune, pVolume }),
        page ("LFO", { pLfoRate, pLfoShape, pLfoSync, pLfoDelay, pLfoPitch, pLfoFilter, pLfoPw, pLfoAmp, pPmEnv, pPmOsc, pModWheel, pCutoff,
                       pO3Lfo, pO3Range, pO3Wave, pCross, pO1Pw, pDrift, pGlide, pBend, pMode, pVoices, pUniDetune, pVolume }),
        page ("OUTPUT", { pModel, pMode, pVoices, pUniDetune, pChorus, pDrive, pWidth, pVolume, pGlide, pDrift, pBend, pModWheel,
                          pCutoff, pReso, pEnvAmt, pOverload, pA, pR, pFA, pFD, pVelAmp, pVelFilt, pHpf, pFMode }),
    };

    // ---- factory sounds, per model (the category is the model)
    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, int model, V v)
    {
        v.insert (v.begin(), { "model", (float) model });
        e.presets.push_back ({ name, synthModelLook (model).name, std::move (v) });
    };
    // MINI: mono basses and leads
    add ("Fat Bass", ModelMini, { { "volume", -3.3f }, { "mode", 0 }, { "o1range", 1 }, { "o2range", 1 }, { "o2fine", 9 }, { "o3range", 0 }, { "o3lvl", 0.6f }, { "o2lvl", 0.9f },
                                  { "cutoff", 260 }, { "reso", 0.2f }, { "envamt", 0.55f }, { "fd", 0.35f }, { "fs", 0.15f }, { "overload", 0.45f }, { "d", 0.5f }, { "s", 0.9f } });
    add ("Taurus Bass", ModelMini, { { "volume", -2.1f }, { "mode", 0 }, { "o1range", 0 }, { "o2range", 1 }, { "o2wave", 3 }, { "o3lvl", 0.0f }, { "cutoff", 180 }, { "reso", 0.1f },
                                     { "envamt", 0.35f }, { "fd", 0.9f }, { "fs", 0.3f }, { "overload", 0.6f }, { "glide", 0.04f } });
    add ("Funk Bass", ModelMini, { { "volume", -1.5f }, { "mode", 0 }, { "o1range", 1 }, { "o1wave", 3 }, { "o2range", 1 }, { "cutoff", 400 }, { "reso", 0.45f }, { "envamt", 0.7f },
                                   { "fd", 0.18f }, { "fs", 0.0f }, { "d", 0.25f }, { "s", 0.6f }, { "overload", 0.35f } });
    add ("Prog Lead", ModelMini, { { "volume", -1.7f }, { "mode", 1 }, { "glide", 0.18f }, { "o2fine", 12 }, { "o3range", 2 }, { "o3lvl", 0.5f }, { "cutoff", 1800 }, { "reso", 0.35f },
                                   { "envamt", 0.4f }, { "lfopitch", 0.12f }, { "lfodelay", 0.6f }, { "overload", 0.4f } });
    add ("Sub Bass", ModelMini, { { "volume", -4.1f }, { "mode", 0 }, { "o1range", 1 }, { "o1wave", 0 }, { "o2lvl", 0.0f }, { "sub", 0.6f }, { "cutoff", 300 }, { "envamt", 0.2f }, { "overload", 0.25f } });
    add ("Mini Brass", ModelMini, { { "volume", -3.7f }, { "mode", 2 }, { "voices", 6 }, { "cutoff", 700 }, { "envamt", 0.6f }, { "fa", 0.08f }, { "fd", 0.6f }, { "fs", 0.5f }, { "a", 0.04f }, { "r", 0.4f } });
    add ("Whistle Lead", ModelMini, { { "volume", 0.0f }, { "mode", 1 }, { "o1wave", 0 }, { "o2lvl", 0.0f }, { "cutoff", 2400 }, { "reso", 0.85f }, { "keytrack", 1.0f }, { "envamt", 0.1f }, { "lfopitch", 0.08f }, { "glide", 0.1f } });
    // PROPHET: poly
    add ("Poly Brass", ModelProphet, { { "volume", -8.0f }, { "mode", 2 }, { "o2wave", 3 }, { "o2fine", 8 }, { "cutoff", 900 }, { "reso", 0.15f }, { "envamt", 0.55f }, { "fa", 0.1f },
                                       { "fd", 0.7f }, { "fs", 0.45f }, { "a", 0.06f }, { "r", 0.45f }, { "chorus", 0 } });
    add ("Sync Lead", ModelProphet, { { "volume", -6.2f }, { "mode", 1 }, { "o2sync", 1 }, { "o2semi", 7 }, { "pmenv", 0.45f }, { "o1lvl", 0.0f }, { "o2lvl", 1.0f }, { "cutoff", 3500 },
                                      { "envamt", 0.3f }, { "fd", 0.6f }, { "glide", 0.05f } });
    add ("Prophet Pad", ModelProphet, { { "volume", -3.2f }, { "mode", 2 }, { "o1wave", 4 }, { "o2fine", 10 }, { "cutoff", 1200 }, { "envamt", 0.25f }, { "fa", 1.2f }, { "fd", 2.0f }, { "fs", 0.6f },
                                        { "a", 1.0f }, { "r", 1.8f }, { "lfopw", 0.3f }, { "lforate", 0.4f } });
    add ("Polymod Bell", ModelProphet, { { "volume", -1.2f }, { "mode", 2 }, { "o1wave", 6 }, { "o2wave", 3 }, { "o2semi", 12 }, { "cross", 0.45f }, { "pmenv", 0.3f }, { "cutoff", 4000 },
                                         { "a", 0.001f }, { "d", 1.6f }, { "s", 0.0f }, { "r", 1.2f } });
    add ("Prophet Strings", ModelProphet, { { "volume", -5.4f }, { "mode", 2 }, { "o1wave", 4 }, { "o2wave", 2 }, { "o2fine", 14 }, { "cutoff", 2600 }, { "envamt", 0.1f }, { "a", 0.4f }, { "r", 1.0f },
                                            { "lfopw", 0.25f }, { "lforate", 0.7f }, { "chorus", 1 } });
    // OB: the 12 dB filter
    add ("Bright Brass Stab", ModelOberheim, { { "volume", -4.8f }, { "mode", 3 }, { "voices", 4 }, { "unidetune", 18 }, { "cutoff", 1500 }, { "reso", 0.2f }, { "envamt", 0.5f },
                                               { "fa", 0.02f }, { "fd", 0.35f }, { "fs", 0.4f }, { "a", 0.01f }, { "r", 0.3f }, { "width", 1.0f } });
    add ("SEM Bass", ModelOberheim, { { "volume", -13.8f }, { "mode", 0 }, { "o1range", 1 }, { "o2range", 1 }, { "o2wave", 3 }, { "cutoff", 500 }, { "reso", 0.4f }, { "envamt", 0.5f }, { "fd", 0.3f }, { "fs", 0.1f } });
    add ("Notch Pad", ModelOberheim, { { "volume", -6.4f }, { "mode", 2 }, { "fmode", 3 }, { "cutoff", 900 }, { "reso", 0.45f }, { "lfofilter", 0.35f }, { "lforate", 0.25f }, { "a", 0.8f }, { "r", 1.5f } });
    add ("OB Strings", ModelOberheim, { { "volume", -8.4f }, { "mode", 2 }, { "o2fine", 12 }, { "cutoff", 3000 }, { "envamt", 0.05f }, { "a", 0.35f }, { "r", 0.9f }, { "chorus", 1 }, { "width", 1.0f } });
    // JUPITER: lush
    add ("Lush Pad", ModelJupiter, { { "volume", -1.6f }, { "mode", 2 }, { "o2fine", 11 }, { "o2wave", 4 }, { "cutoff", 1400 }, { "envamt", 0.2f }, { "fa", 1.5f }, { "fd", 2.5f }, { "fs", 0.5f },
                                     { "a", 1.4f }, { "r", 2.5f }, { "chorus", 2 }, { "width", 1.0f } });
    add ("Jupiter Arp", ModelJupiter, { { "volume", 0.0f }, { "mode", 2 }, { "o1wave", 3 }, { "cutoff", 1600 }, { "reso", 0.3f }, { "envamt", 0.5f }, { "fd", 0.22f }, { "fs", 0.0f },
                                        { "d", 0.3f }, { "s", 0.0f }, { "r", 0.3f } });
    add ("Cross Mod Bell", ModelJupiter, { { "volume", -4.0f }, { "mode", 2 }, { "o1wave", 3 }, { "o2wave", 0 }, { "o2semi", 7 }, { "cross", 0.6f }, { "cutoff", 5000 }, { "d", 1.5f }, { "s", 0.0f }, { "r", 1.5f } });
    add ("Jupiter Strings", ModelJupiter, { { "volume", -3.2f }, { "mode", 2 }, { "o2fine", 9 }, { "cutoff", 2400 }, { "hpf", 200 }, { "a", 0.5f }, { "r", 1.2f }, { "chorus", 1 } });
    // JUNO: DCO + chorus
    add ("Juno Pad", ModelJuno, { { "volume", -9.4f }, { "mode", 2 }, { "o1wave", 4 }, { "o2lvl", 0.0f }, { "sub", 0.4f }, { "cutoff", 1200 }, { "envamt", 0.25f }, { "lfopw", 0.4f }, { "lforate", 0.6f },
                                  { "a", 0.6f }, { "r", 1.2f }, { "chorus", 2 } });
    add ("Juno Bass", ModelJuno, { { "volume", -8.0f }, { "mode", 0 }, { "o1range", 1 }, { "o2lvl", 0.0f }, { "sub", 0.8f }, { "cutoff", 400 }, { "reso", 0.35f }, { "envamt", 0.6f }, { "fd", 0.2f },
                                   { "fs", 0.0f }, { "chorus", 0 } });
    add ("Juno Strings", ModelJuno, { { "volume", -3.9f }, { "mode", 2 }, { "o2lvl", 0.0f }, { "cutoff", 3000 }, { "hpf", 300 }, { "envamt", 0.1f }, { "a", 0.3f }, { "r", 0.9f }, { "chorus", 1 } });
    add ("PWM Choir", ModelJuno, { { "volume", -9.9f }, { "mode", 2 }, { "o1wave", 3 }, { "o2lvl", 0.0f }, { "lfopw", 0.7f }, { "lforate", 0.9f }, { "cutoff", 1800 }, { "a", 0.5f }, { "r", 1.0f }, { "chorus", 3 } });
    add ("Hoover", ModelJuno, { { "volume", -4.9f }, { "mode", 3 }, { "voices", 6 }, { "unidetune", 35 }, { "o1wave", 4 }, { "lfopw", 0.9f }, { "lforate", 3.0f }, { "glide", 0.15f },
                                { "pmenv", -0.2f }, { "cutoff", 4000 }, { "chorus", 3 } });
    // SH: mono punch
    add ("SH Bass", ModelSH, { { "volume", -7.5f }, { "mode", 0 }, { "o1range", 1 }, { "o1wave", 3 }, { "o2lvl", 0.0f }, { "sub", 0.6f }, { "cutoff", 350 }, { "reso", 0.4f }, { "envamt", 0.65f },
                               { "fd", 0.15f }, { "fs", 0.0f }, { "d", 0.2f }, { "s", 0.7f } });
    add ("SH Lead", ModelSH, { { "volume", -5.8f }, { "mode", 1 }, { "o2lvl", 0.0f }, { "sub", 0.3f }, { "cutoff", 2000 }, { "reso", 0.4f }, { "glide", 0.06f }, { "lfopitch", 0.1f } });
    add ("Zap", ModelSH, { { "volume", 0.0f }, { "mode", 0 }, { "o2lvl", 0.0f }, { "pmenv", 0.7f }, { "fd", 0.12f }, { "d", 0.35f }, { "s", 0.0f }, { "cutoff", 3000 }, { "envamt", 0.4f } });
    add ("Chip Arp", ModelSH, { { "volume", 0.0f }, { "mode", 0 }, { "o1wave", 5 }, { "o2lvl", 0.0f }, { "cutoff", 8000 }, { "envamt", 0.0f }, { "d", 0.12f }, { "s", 0.2f } });
    // ACID
    add ("Acid Squelch", ModelAcid, { { "volume", -2.2f }, { "mode", 1 }, { "o1range", 1 }, { "o2lvl", 0.0f }, { "cutoff", 450 }, { "reso", 0.8f }, { "envamt", 0.65f }, { "fd", 0.25f },
                                      { "fs", 0.0f }, { "glide", 0.06f }, { "overload", 0.5f }, { "d", 0.5f }, { "s", 0.6f } });
    add ("Acid Square", ModelAcid, { { "volume", -10.4f }, { "mode", 1 }, { "o1range", 1 }, { "o1wave", 3 }, { "o2lvl", 0.0f }, { "cutoff", 600 }, { "reso", 0.7f }, { "envamt", 0.5f }, { "fd", 0.3f }, { "fs", 0.0f }, { "glide", 0.06f } });
    add ("Acid Scream", ModelAcid, { { "volume", -10.6f }, { "mode", 1 }, { "o2lvl", 0.0f }, { "cutoff", 900 }, { "reso", 0.92f }, { "envamt", 0.8f }, { "fd", 0.4f }, { "drive", 0.6f }, { "glide", 0.06f } });
    // MS
    add ("Scream Lead", ModelMS, { { "volume", -16.0f }, { "mode", 1 }, { "o2semi", 7 }, { "cutoff", 1400 }, { "reso", 0.8f }, { "hpf", 180 }, { "envamt", 0.4f }, { "lfopitch", 0.1f }, { "overload", 0.5f } });
    add ("Dirty Bass", ModelMS, { { "volume", -16.0f }, { "mode", 0 }, { "o1range", 1 }, { "o2range", 1 }, { "o2wave", 3 }, { "cutoff", 500 }, { "reso", 0.6f }, { "envamt", 0.5f }, { "fd", 0.25f }, { "overload", 0.7f } });
    add ("MS Wah", ModelMS, { { "volume", -15.5f }, { "mode", 2 }, { "fmode", 1 }, { "cutoff", 600 }, { "reso", 0.75f }, { "envamt", 0.6f }, { "fa", 0.15f }, { "fd", 0.5f } });
    add ("Ring Mod Noise", ModelMS, { { "volume", -13.4f }, { "mode", 2 }, { "ring", 0.8f }, { "o2semi", 5 }, { "noise", 0.2f }, { "cutoff", 2500 }, { "reso", 0.5f } });
    // ODYSSEY
    add ("Odyssey Sync", ModelOdyssey, { { "volume", -2.8f }, { "mode", 1 }, { "o2sync", 1 }, { "o2semi", 12 }, { "lfopitch", 0.0f }, { "pmenv", 0.5f }, { "o1lvl", 0.0f }, { "cutoff", 3000 } });
    add ("Ring Bell", ModelOdyssey, { { "volume", 0.0f }, { "mode", 2 }, { "ring", 1.0f }, { "o1lvl", 0.0f }, { "o2lvl", 0.0f }, { "o2semi", 7 }, { "cutoff", 6000 }, { "d", 1.2f }, { "s", 0.0f }, { "r", 1.0f } });
    add ("Duo Lead", ModelOdyssey, { { "volume", -5.5f }, { "mode", 1 }, { "o2fine", 18 }, { "cutoff", 2200 }, { "reso", 0.5f }, { "lfopitch", 0.1f }, { "glide", 0.08f } });
    add ("Sample & Hold", ModelOdyssey, { { "volume", -3.9f }, { "mode", 2 }, { "lfoshape", 4 }, { "lforate", 8.0f }, { "lfofilter", 0.6f }, { "cutoff", 1200 }, { "reso", 0.75f }, { "envamt", 0.1f } });
    // BUCHLA
    add ("West Coast Pluck", ModelBuchla, { { "volume", -3.8f }, { "mode", 2 }, { "o1wave", 7 }, { "o1pw", 0.5f }, { "o2lvl", 0.0f }, { "cutoff", 3000 }, { "a", 0.001f }, { "d", 0.5f }, { "s", 0.0f }, { "r", 0.5f } });
    add ("Fold Lead", ModelBuchla, { { "volume", -11.6f }, { "mode", 1 }, { "o1wave", 7 }, { "o1pw", 0.8f }, { "o2lvl", 0.0f }, { "lfopw", 0.4f }, { "lforate", 2.0f }, { "cutoff", 5000 } });
    add ("Bongo", ModelBuchla, { { "volume", 0.0f }, { "mode", 2 }, { "o1wave", 6 }, { "o2lvl", 0.0f }, { "cross", 0.3f }, { "o3range", 2 }, { "pmenv", 0.25f }, { "cutoff", 2000 },
                                 { "a", 0.001f }, { "d", 0.25f }, { "s", 0.0f }, { "r", 0.25f } });
    add ("Gong Easel", ModelBuchla, { { "volume", -8.5f }, { "mode", 2 }, { "o1wave", 7 }, { "o1pw", 0.9f }, { "cross", 0.7f }, { "o3range", 3 }, { "o3semi", 5 }, { "cutoff", 4000 },
                                      { "d", 3.0f }, { "s", 0.0f }, { "r", 3.0f } });
    e.presets.push_back ({ "Init", "Init", {} });
    return e;
}

//==============================================================================
namespace
{
inline float blep (float t, float dt) noexcept { return dsp::polyBlep (t, dt); }

/** The filters of the models. */
struct ModelFilter
{
    float s[4] {}, last = 0.0f, fbHp = 0.0f;
    dsp::Svf svf, hp2;
    float g = 0.1f;

    void reset() { for (auto& x : s) x = 0.0f; last = fbHp = 0.0f; svf.reset(); hp2.reset(); }

    inline float process (float x, int kind, int mode, float cutoff, float reso, const Character& c, float sr) noexcept
    {
        switch (kind)
        {
            case FSvf:
            case FScream:
            case FLpg:
            {
                const float q = kind == FScream ? 0.6f + reso * reso * 28.0f : 0.55f + reso * reso * 18.0f;
                svf.set (cutoff, q, sr);
                float in = dsp::fastTanh (x * c.stageSat) / c.stageSat;
                if (kind == FScream) in += dsp::fastTanh (last * reso * 1.2f) * 0.25f;   // the MS-20: the resonance clips and screams
                const auto o = svf.process (in);
                float y;
                switch (kind == FLpg ? 0 : mode)
                {
                    case 1: y = o.band; break;
                    case 2: y = o.high; break;
                    case 3: y = o.low + o.high; break;
                    default: y = o.low; break;
                }
                if (kind == FScream) y = dsp::fastTanh (y * c.fbSat) / c.fbSat * 1.3f;
                last = o.band;
                return y;
            }
            default:
            {
                // a 4-pole cascade with the feedback of the model (ladder, OTA, diode)
                const float wc = std::tan (dsp::pi * juce::jlimit (16.0f, sr * 0.42f, cutoff) / sr);
                g = wc / (1.0f + wc);
                const float k = reso * c.resMax;
                float fb = kind == FDiode ? last - fbHp : last;
                if (kind == FDiode) fbHp += (last - fbHp) * 0.01f;   // the 303 high-passes its resonance
                const float comp = 1.0f + c.resComp * k * 0.5f;
                float u = x * comp - k * dsp::fastTanh (fb * c.fbSat) / c.fbSat;
                float y[4];
                const int poles = kind == FDiode ? 3 : 4;
                for (int i = 0; i < poles; ++i)
                {
                    const float in = c.stageSat > 1.0f ? dsp::fastTanh (u * c.stageSat) / c.stageSat : u;
                    const float v = (in - s[i]) * g;
                    y[i] = v + s[i];
                    s[i] = y[i] + v;
                    u = y[i];
                }
                if (poles == 3) y[3] = y[2];
                last = y[3];
                if (! std::isfinite (last)) { reset(); return 0.0f; }
                switch (mode)
                {
                    case 1: return 2.0f * (y[1] - y[3]);
                    case 2: { const float in0 = x * comp - k * last; return in0 - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3]; }
                    case 3: { const float in0 = x * comp - k * last; return y[3] + (in0 - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3]); }
                    default: return y[3];
                }
            }
        }
    }
};

class SynthEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v = Voice();
        chorusL.assign ((size_t) (sr * 0.05f), 0.0f);
        chorusR.assign ((size_t) (sr * 0.05f), 0.0f);
        cw = 0;
        volume.setTime (0.03f, sr);
    }

    void setParams (const float* v) override
    {
        for (int i = 0; i < pCount; ++i) p[(size_t) i] = v[i];
        const auto& c = character (model());
        const float snap = c.envSnap;
        for (auto& vo : voices)
        {
            vo.fenv.set (p[pFA] / snap, p[pFD] / snap, p[pFS], p[pFR] / snap, sr);
            vo.aenv.set (p[pA] / snap, p[pD] / snap, p[pS], p[pR], sr);
        }
    }
    void setTempo (double bpm) override { tempo = (float) bpm; }
    void setBend (float s) override { bend = juce::jlimit (-1.0f, 1.0f, s / 2.0f) * p[pBend]; }
    void setMod (float m) override { modWheel = m; }

    void noteOn (int note, float vel, bool slide, bool) override
    {
        const int mode = juce::roundToInt (p[pMode]);
        // the held keys (mono: the last one plays; letting go returns to the one before)
        removeHeld (note);
        if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
        if (mode == MMono || mode == MLegato)
        {
            auto& v = voices[0];
            const bool legato = v.active && v.gate && (mode == MLegato || slide);
            startVoice (v, note, vel, ! legato, 0.0f, 0.0f);
            for (size_t i = 1; i < voices.size(); ++i) if (voices[i].active) voices[i].release();
            return;
        }
        if (mode == MUnison)
        {
            const int n = juce::jlimit (1, (int) voices.size(), juce::roundToInt (p[pVoices]));
            for (auto& v : voices) if (v.active) v.release();
            for (int i = 0; i < n; ++i)
            {
                const float spread = n > 1 ? ((float) i / (float) (n - 1) - 0.5f) * 2.0f : 0.0f;
                startVoice (voices[(size_t) i], note, vel, true, spread * p[pUniDetune] * 0.01f, spread * p[pWidth]);
                voices[(size_t) i].gain = 1.0f / std::sqrt ((float) n);   // a stack is fatter, not louder
            }
            return;
        }
        // poly: a free voice, else the oldest
        const int n = juce::jlimit (1, (int) voices.size(), juce::roundToInt (p[pVoices]));
        Voice* best = nullptr;
        for (int i = 0; i < n; ++i) if (voices[(size_t) i].active && voices[(size_t) i].note == note) best = &voices[(size_t) i];
        if (best == nullptr) for (int i = 0; i < n; ++i) if (! voices[(size_t) i].active) { best = &voices[(size_t) i]; break; }
        if (best == nullptr) for (int i = 0; i < n; ++i) if (best == nullptr || voices[(size_t) i].age < best->age) best = &voices[(size_t) i];
        const float pan = ((float) (counter % 7) / 3.0f - 1.0f) * 0.35f * p[pWidth];
        startVoice (*best, note, vel, true, 0.0f, pan);
    }

    void noteOff (int note) override
    {
        const int mode = juce::roundToInt (p[pMode]);
        removeHeld (note);
        if (mode == MMono || mode == MLegato)
        {
            auto& v = voices[0];
            if (v.note != note) return;
            if (numHeld > 0) { startVoice (v, held[(size_t) (numHeld - 1)], v.vel, mode == MMono, 0.0f, 0.0f); return; }   // back to the key still held
            v.release();
            return;
        }
        if (mode == MUnison) { if (numHeld == 0) for (auto& v : voices) if (v.active) v.release(); return; }
        for (auto& v : voices) if (v.active && v.gate && v.note == note) v.release();
    }

    void allOff() override { for (auto& v : voices) v = Voice(); numHeld = 0; }

    void render (float* outL, float* outR, int n) override
    {
        const int mdl = model();
        const auto& c = character (mdl);
        // the LFO (one for all voices, like the hardware)
        const int sync = juce::roundToInt (p[pLfoSync]);
        const float lfoHz = sync > 0 ? (float) (tempo / 60.0 / syncBeats (sync)) : p[pLfoRate];
        const int lfoShape = juce::roundToInt (p[pLfoShape]);
        const float modAmt = modWheel * p[pModWheel];
        const int fmode = juce::roundToInt (p[pFMode]);
        const float vol = dsp::dbToGain (p[pVolume]);
        const float drive = 1.0f + p[pDrive] * 8.0f;
        const float glideK = p[pGlide] > 0.0005f ? std::exp (-1.0f / (p[pGlide] * sr * 0.4f)) : 0.0f;
        const float overload = 1.0f + p[pOverload] * 5.0f * (mdl == ModelMini || mdl == ModelMS ? 1.3f : 1.0f);
        const float dcA = 1.0f - std::exp (-dsp::twoPi * 12.0f / sr);
        const float hpfA = p[pHpf] > 21.0f ? 1.0f - std::exp (-dsp::twoPi * p[pHpf] / sr) : 0.0f;
        const float ranges[] = { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f };
        const int r1 = juce::jlimit (0, 4, juce::roundToInt (p[pO1Range])), r2 = juce::jlimit (0, 4, juce::roundToInt (p[pO2Range])), r3 = juce::jlimit (0, 4, juce::roundToInt (p[pO3Range]));
        const int w1 = juce::roundToInt (p[pO1Wave]), w2 = juce::roundToInt (p[pO2Wave]), w3 = juce::roundToInt (p[pO3Wave]);
        const bool sync2 = p[pO2Sync] > 0.5f, osc3Low = p[pO3Lfo] > 0.5f;
        const float driftCents = p[pDrift] * c.drift;

        for (int i0 = 0; i0 < n; i0 += 16)
        {
            const int m = juce::jmin (16, n - i0);
            // control rate: the LFO
            lfoPhase += lfoHz * (float) m / sr;
            if (lfoPhase >= 1.0f) { lfoPhase -= std::floor (lfoPhase); lfoHoldPrev = lfoHold; lfoHold = rng.next(); }
            float lfo;
            switch (lfoShape)
            {
                case 1: lfo = std::sin (dsp::twoPi * lfoPhase); break;
                case 2: lfo = 1.0f - 2.0f * lfoPhase; break;
                case 3: lfo = lfoPhase < 0.5f ? 1.0f : -1.0f; break;
                case 4: lfo = lfoHold; break;
                case 5: lfo = lfoHoldPrev + (lfoHold - lfoHoldPrev) * (0.5f - 0.5f * std::cos (dsp::pi * lfoPhase)); break;
                default: lfo = 1.0f - 4.0f * std::abs (lfoPhase - 0.5f); break;
            }
            float bl[16] {}, br[16] {};
            for (auto& v : voices)
            {
                if (! v.active) continue;
                // LFO delay, drift (a slow random walk per voice and oscillator)
                v.lfoFade = juce::jmin (1.0f, v.lfoFade + (float) m / juce::jmax (1.0f, p[pLfoDelay] * sr));
                const float lf = lfo * (p[pLfoDelay] > 0.001f ? v.lfoFade : 1.0f);
                for (int o = 0; o < 3; ++o)
                {
                    v.drift[o] += (v.rng.next() * 0.02f - v.drift[o] * 0.0015f) * (float) m / 16.0f;
                    v.drift[o] = juce::jlimit (-1.0f, 1.0f, v.drift[o]);
                }
                const float vib = (p[pLfoPitch] + modAmt * 0.6f) * lf * 1.2f;   // semitones
                const float pwMod = p[pLfoPw] * lf * 0.45f;
                const float cutBase = p[pCutoff] * std::exp2 (p[pKeyTrack] * (v.pitch - 60.0f) / 12.0f + p[pLfoFilter] * lf * 3.0f
                                                              + p[pVelFilt] * (v.vel - 0.7f) * 2.0f + v.tolerance * 0.15f);
                const float trem = 1.0f - p[pLfoAmp] * (0.5f + 0.5f * lf);
                const float velGain = 1.0f - p[pVelAmp] + p[pVelAmp] * v.vel;
                for (int i = 0; i < m; ++i)
                {
                    v.pitch = glideK > 0.0f ? v.target + (v.pitch - v.target) * glideK : v.target;
                    const float fe = v.fenv.process();
                    float ae = v.aenv.process();
                    const float base = v.pitch + bend + vib + v.uni * 12.0f;
                    const float pm = p[pPmEnv] * fe * 24.0f;   // poly-mod: the filter envelope bends osc 1
                    // oscillator 3 first (it can modulate the others)
                    const float f3 = osc3Low ? 0.08f * std::exp2 ((ranges[r3] + p[pO3Semi] + p[pO3Fine] * 0.01f) / 12.0f + 3.0f)
                                             : dsp::midiToHz (base + ranges[r3] + p[pO3Semi] + p[pO3Fine] * 0.01f + v.drift[2] * driftCents * 0.01f);
                    const float o3 = wave (v.ph[2], f3 / sr, w3, p[pO1Pw]);
                    const float cross = p[pCross] * (osc3Low ? 12.0f : 0.0f);
                    const float fm = osc3Low ? 0.0f : p[pCross] * o3 * 0.9f;   // audio-rate cross mod (FM)
                    const float f1 = dsp::midiToHz (base + ranges[r1] + pm + cross * o3 + v.drift[0] * driftCents * 0.01f) * (1.0f + fm);
                    const float f2 = dsp::midiToHz (base + ranges[r2] + p[pO2Semi] + p[pO2Fine] * 0.01f + cross * o3 + v.drift[1] * driftCents * 0.01f);
                    const float before1 = v.ph[0];
                    const float pw = juce::jlimit (0.05f, 0.95f, p[pO1Pw] + pwMod);
                    const float o1 = wave (v.ph[0], f1 / sr, w1, pw);
                    if (sync2 && v.ph[0] < before1) v.ph[1] = v.ph[0] * f2 / juce::jmax (1.0f, f1);   // osc 2 restarts with osc 1
                    const float o2 = wave (v.ph[1], f2 / sr, w2, pw);
                    // sub: a square an octave under osc 1
                    if (v.ph[0] < before1) v.subFlip = ! v.subFlip;
                    const float sub = v.subFlip ? 1.0f : -1.0f;
                    float mix = o1 * p[pO1Lvl] + o2 * p[pO2Lvl] + (osc3Low ? 0.0f : o3 * p[pO3Lvl]) + sub * p[pSub] * 0.8f
                              + v.rng.next() * (p[pNoise] * 0.8f + c.noiseFloor) + o1 * o2 * p[pRing];
                    mix = dsp::fastTanh (mix * 0.45f * overload) / (0.45f * overload) * 0.5f;   // the mixer overloads into the filter (Moog warmth)
                    // the filter, moved by its envelope and osc 2 (poly-mod)
                    float cut = cutBase * std::exp2 (p[pEnvAmt] * fe * 7.0f + p[pPmOsc] * o2 * 2.0f);
                    if (c.filter == FLpg)   // Buchla: the lowpass gate - a vactrol opens light and sound together
                    {
                        v.vac += (ae - v.vac) * (ae > v.vac ? 0.02f : 0.0015f);
                        cut = juce::jmax (40.0f, cut * (0.04f + 0.96f * v.vac));
                        ae = v.vac;
                    }
                    cut = juce::jlimit (20.0f, sr * 0.45f, cut);
                    float y = v.filter.process (mix, c.filter, fmode, cut, p[pReso], c, sr);
                    if (hpfA > 0.0f) { v.hpState += (y - v.hpState) * hpfA; y -= v.hpState; }   // Juno / Jupiter / MS high pass
                    v.dc += (y - v.dc) * dcA; y -= v.dc;   // AC coupled, like the hardware (pulse waves carry DC)
                    y *= ae * velGain * trem * v.gain;
                    bl[i] += y * v.gl; br[i] += y * v.gr;
                    if (! v.aenv.active()) { v.active = false; break; }
                }
            }
            for (int i = 0; i < m; ++i)
            {
                float l = bl[i], r = br[i];
                if (drive > 1.01f) { l = dsp::fastTanh (l * drive) / std::sqrt (drive); r = dsp::fastTanh (r * drive) / std::sqrt (drive); }
                chorus (l, r, juce::roundToInt (p[pChorus]), c.chorusDepth);
                const float g = volume.process (vol) * 2.2f;
                outL[i0 + i] += l * g;
                outR[i0 + i] += r * g;
            }
        }
    }

private:
    struct Voice
    {
        bool active = false, gate = false, subFlip = false;
        int note = 60;
        float vel = 0.8f, pitch = 60.0f, target = 60.0f, uni = 0.0f, gl = 1.0f, gr = 1.0f, lfoFade = 0.0f, vac = 0.0f, hpState = 0.0f, tolerance = 0.0f, dc = 0.0f, gain = 1.0f;
        std::array<float, 3> ph { 0.0f, 0.33f, 0.66f }, drift {};
        dsp::Adsr fenv, aenv;
        ModelFilter filter;
        dsp::Rng rng;
        uint64_t age = 0;
        void release() { gate = false; fenv.gateOff(); aenv.gateOff(); }
    };

    int model() const noexcept { return juce::jlimit (0, NumSynthModels - 1, juce::roundToInt (p[pModel])); }

    void startVoice (Voice& v, int note, float vel, bool retrigger, float uniSemis, float pan)
    {
        const bool wasActive = v.active;
        v.note = note;
        v.target = (float) note;
        if (! wasActive || p[pGlide] < 0.0005f) v.pitch = v.target;
        v.vel = juce::jlimit (0.05f, 1.0f, vel);
        v.uni = uniSemis;
        const float pn = juce::jlimit (-1.0f, 1.0f, pan);
        v.gl = std::sqrt (0.5f * (1.0f - pn)) * 1.4142f;
        v.gr = std::sqrt (0.5f * (1.0f + pn)) * 1.4142f;
        v.age = ++counter;
        v.gate = true;
        if (! wasActive)
        {
            v.rng.s = 0x9e3779b9u ^ (uint32_t) (counter * 2654435761u);
            v.tolerance = v.rng.next();   // each voice a little different, like real parts
            v.filter.reset();
            v.vac = 0.0f;
            if (model() != ModelJuno) for (auto& ph : v.ph) ph = v.rng.uni();   // free-running oscillators (the Juno's DCOs restart)
            else v.ph = { 0.0f, 0.0f, 0.0f };
            v.lfoFade = 0.0f;
        }
        v.gain = 1.0f;
        const auto& c = character (model());
        v.fenv.set (p[pFA] / c.envSnap, p[pFD] / c.envSnap, p[pFS], p[pFR] / c.envSnap, sr);
        v.aenv.set (p[pA] / c.envSnap, p[pD] / c.envSnap, p[pS], p[pR], sr);
        if (retrigger || ! wasActive) { v.fenv.gateOn (true); v.aenv.gateOn (true); }
        v.active = true;
    }

    void removeHeld (int note)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note) { for (int k = i; k < numHeld - 1; ++k) held[(size_t) k] = held[(size_t) (k + 1)]; --numHeld; return; }
    }

    inline float wave (float& ph, float dt, int w, float pw) noexcept
    {
        dt = juce::jlimit (0.0f, 0.45f, dt);
        float y = 0.0f;
        switch (w)
        {
            case WTri:   y = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
            case WShark: { const float tri = 1.0f - 4.0f * std::abs (ph - 0.5f); const float saw = 2.0f * ph - 1.0f - blep (ph, dt); y = 0.5f * (tri + saw); break; }
            case WSaw:   y = 2.0f * ph - 1.0f - blep (ph, dt); break;
            case WSquare:
            case WWide:
            case WNarrow:
            {
                const float width = w == WSquare ? pw : w == WWide ? 0.3f + (pw - 0.5f) * 0.4f : 0.1f + (pw - 0.5f) * 0.12f;
                const float d = juce::jlimit (0.03f, 0.97f, width);
                y = ph < d ? 1.0f : -1.0f;
                y += blep (ph, dt);
                float t2 = ph - d; if (t2 < 0.0f) t2 += 1.0f;
                y -= blep (t2, dt);
                break;
            }
            case WSine:  y = std::sin (dsp::twoPi * ph); break;
            default:     // Fold: a sine folded back on itself (Shape = how much)
            {
                const float x = std::sin (dsp::twoPi * ph) * (1.0f + pw * 5.0f);
                y = std::sin (x * dsp::pi * 0.5f);
                break;
            }
        }
        ph += dt;
        if (ph >= 1.0f) ph -= 1.0f;
        return y;
    }

    /** The model's chorus (Juno I / II / I + II; a gentler ensemble on the others). */
    void chorus (float& l, float& r, int type, float depth)
    {
        if (type <= 0 || chorusL.empty()) return;
        static const float rates[] = { 0.0f, 0.513f, 0.863f, 9.75f };
        static const float depths[] = { 0.0f, 0.0019f, 0.0019f, 0.00035f };
        const int size = (int) chorusL.size();
        chPhase += rates[type] / sr;
        if (chPhase >= 1.0f) chPhase -= 1.0f;
        const float tri = 1.0f - 4.0f * std::abs (chPhase - 0.5f);
        const float base = 0.0035f * sr, dep = depths[type] * sr * depth;
        const float mono = 0.5f * (l + r);
        chorusL[(size_t) cw] = mono; chorusR[(size_t) cw] = mono;
        auto tap = [this, size] (const std::vector<float>& b, float d)
        {
            float pos = (float) cw - d; while (pos < 0.0f) pos += (float) size;
            const int i0 = (int) pos; const float f = pos - (float) i0;
            return b[(size_t) (i0 % size)] * (1.0f - f) + b[(size_t) ((i0 + 1) % size)] * f;
        };
        const float wl = tap (chorusL, base + dep * tri), wr = tap (chorusR, base - dep * tri);
        cw = (cw + 1) % size;
        l = 0.7f * l + 0.7f * wl;
        r = 0.7f * r + 0.7f * wr;
    }

    std::array<Voice, 8> voices {};
    std::array<int, 16> held {};
    int numHeld = 0;
    std::array<float, pCount> p {};
    float sr = 48000.0f, tempo = 120.0f, bend = 0.0f, modWheel = 0.0f;
    float lfoPhase = 0.0f, lfoHold = 0.0f, lfoHoldPrev = 0.0f;
    std::vector<float> chorusL, chorusR;
    int cw = 0;
    float chPhase = 0.0f;
    dsp::Smooth volume;
    dsp::Rng rng;
    uint64_t counter = 0;
};
} // namespace

std::unique_ptr<Engine> makeSynthEngine() { return std::make_unique<SynthEngine>(); }
} // namespace catbox
