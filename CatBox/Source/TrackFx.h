#pragma once

#include "Params.h"
#include "Dsp.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

/**
    Cat Box 3: what each track can have after its sound engine.
    - 4 insert effects in a chain (filter, drive, crusher, delay, chorus, phaser, flanger,
      compressor, EQ, tremolo / auto-pan, trance gate, ring mod, reverb, width, tape),
    - 8 macro knobs, 2 LFOs, a note envelope, random, and 16 modulation slots
      (source > any engine / effect / mixer parameter, amount),
    - "MY" knobs: 24 knobs you put together from anything.
*/
namespace catbox
{
constexpr int numInserts = 4, insertParams = 8, numMacros = 8, numModSlots = 16, numModSet = 12, numMyKnobs = 24;

enum InsertType
{
    InsOff = 0, InsFilter, InsDrive, InsCrusher, InsDelay, InsChorus, InsPhaser, InsFlanger, InsComp,
    InsEq, InsTremolo, InsGate, InsRing, InsReverb, InsWidth, InsTape,
    InsPitch, InsComb, InsVowel, InsStutter, InsPump, InsTransient, InsCabinet, NumInsertTypes
};

struct InsertDef
{
    juce::String name, shortName;
    std::vector<ParamDef> params;   // up to 8
    std::vector<Preset> presets;
    void defaults (float* v) const;
    void applyPreset (int preset, float* v) const;
};
const InsertDef& insertDef (int type);
juce::StringArray insertNames();

/** One insert on a track (plain data). */
struct Insert
{
    int type = InsOff;
    bool on = true;
    std::array<float, insertParams> p {};
};

/** Where a modulation (or a MY knob) goes. */
namespace dest
{
    constexpr int none = -1;
    constexpr int insert0 = 200;   // 200 + slot * 8 + param
    constexpr int mix0 = 300;      // 300 volume, 301 pan, 302 send A (reverb), 303 send B (delay), 304 send C (FX)
    constexpr int macro0 = 400;    // MY knobs only: 400 + macro
    constexpr int modSet0 = 500;   // MY knobs only: 500 + LFO / envelope setting
    inline bool isEngine (int d) { return d >= 0 && d < maxParams; }
    inline bool isInsert (int d) { return d >= insert0 && d < insert0 + numInserts * insertParams; }
    inline bool isMix (int d) { return d >= mix0 && d < mix0 + 5; }
}

enum ModSource
{
    SrcNone = 0, SrcLfo1, SrcLfo2, SrcEnv, SrcVelocity, SrcNote, SrcModWheel, SrcRandom, SrcBar,
    SrcMacro1, SrcMacro8 = SrcMacro1 + 7, NumModSources
};
juce::StringArray modSourceNames();

struct ModSlot
{
    int source = SrcNone;
    int dest = dest::none;
    float amount = 0.0f;   // -1..1 of the knob's travel
    bool active() const noexcept { return source != SrcNone && dest != dest::none && amount != 0.0f; }
};

/** The LFOs, the envelope and random (12 settings, the MACRO page with Shift). */
enum ModSetIndex
{
    MsLfo1Shape = 0, MsLfo1Rate, MsLfo1Sync, MsLfo1Retrig, MsLfo2Shape, MsLfo2Rate, MsLfo2Sync, MsLfo2Retrig,
    MsEnvAttack, MsEnvDecay, MsRandRate, MsSmooth
};
const std::vector<ParamDef>& modSetDefs();
const ParamDef& macroDef();   // 0..100 %
const ParamDef& modAmountDef();

/** Everything after the engine on a track (plain data, part of Track). */
struct TrackMods
{
    std::array<Insert, numInserts> inserts {};
    std::array<float, numMacros> macros {};
    std::array<ModSlot, numModSlots> slots {};
    std::array<float, numModSet> set {};
    std::array<int16_t, numMyKnobs> my {};

    TrackMods();
    bool anyActive() const noexcept { for (const auto& s : slots) if (s.active()) return true; return false; }
    /** The engine changed: what pointed at the old engine's parameters goes. */
    void dropEngineTargets();
    int firstFreeSlot() const noexcept { for (int i = 0; i < numModSlots; ++i) if (slots[(size_t) i].source == SrcNone || slots[(size_t) i].dest == dest::none) return i; return -1; }
};

/** As JSON (engine parameters by id, so it survives the engine's list changing). */
/** Ready-made chains of the 4 inserts (the MENU). */
juce::StringArray chainPresetNames();
void applyChainPreset (TrackMods&, int index);

juce::var modsToVar (const TrackMods&, int engine);
void modsFromVar (const juce::var&, TrackMods&, int engine);

//==============================================================================
/** One insert effect running (the audio thread). */
class InsertFx
{
public:
    void prepare (double sampleRate);
    void reset();
    /** In place. ppq: the song position at the first sample, ppqPerSample for tempo-synced effects. */
    void process (int type, const float* p, float* l, float* r, int n, double ppq, double ppqPerSample, float bpm);
    int lastType = -1;

private:
    double sr = 48000.0;
    // shared state
    dsp::Svf svfL, svfR, svf2L, svf2R, svf3L, svf3R;
    float env2 = 0.0f, pitchPhase = 0.0f;
    // stutter
    bool stutterOn = false;
    long long stutterGrid = -1;
    int segStart = 0, segLen = 1, segPos = 0;
    void setCab (int type, float tone);
    float env = 0.0f, lfoPhase = 0.0f, lfoPhase2 = 0.25f, holdL = 0.0f, holdR = 0.0f, holdCount = 0.0f;
    float lpL = 0.0f, lpR = 0.0f, dcL = 0.0f, dcR = 0.0f, dcInL = 0.0f, dcInR = 0.0f;
    float gain = 1.0f, smoothTime = -1.0f, gateEnv = 0.0f;
    std::vector<float> bufL, bufR;
    int w = 0;
    std::array<float, 12> apL {}, apR {};
    float fbL = 0.0f, fbR = 0.0f;
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline float process (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    std::array<Biquad, 6> eq;
    std::array<float, 7> eqLast {};
    juce::Reverb reverb;
    juce::Reverb::Parameters revLast;
    dsp::Rng rng;
    float wowPhase = 0.0f, flutterPhase = 0.0f;

    float readDelay (const std::vector<float>& buf, float samplesBack) const noexcept;
    void setEq (const float* p);
};

/** The LFO / envelope / random sources of a track (the audio thread). */
struct ModRuntime
{
    float lfoPhase[2] { 0.0f, 0.0f }, lfoHold[2] { 0.0f, 0.0f }, lfoPrevHold[2] { 0.0f, 0.0f };
    float env = 0.0f;
    int envStage = 0;   // 0 off, 1 attack, 2 decay
    float velocity = 0.8f, note = 60.0f, random = 0.0f, randomTarget = 0.0f, randomSmooth = 0.0f;
    double nextRandom = 0.0;
    dsp::Rng rng;
    std::array<float, NumModSources> value {};

    void noteOn (int n, float vel, const std::array<float, numModSet>& set);
    /** Advances by n samples and fills value[]. */
    void advance (int n, double sr, double ppq, double ppqPerSample, bool playing, double songPpq, float modWheel,
                  const std::array<float, numModSet>& set, const std::array<float, numMacros>& macros);
};
} // namespace catbox
