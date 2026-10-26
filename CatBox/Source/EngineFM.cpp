#include "Engine.h"
#include "Dsp.h"
#include <cmath>

/**
    FM: four sine operators in 8 algorithms (the DX7 idea, kept small): e-pianos, bells, basses,
    marimbas, brass and metal. Operator 1 always sounds; the others either sound or modulate
    (bend the pitch of) the one below them. Carriers follow the amp envelope, modulators their own
    envelope, so a sound can start bright and mellow out. Feedback on operator 4.
*/
namespace catbox
{
namespace
{
enum
{
    fAlgo = 0, fFeedback, fRatio1, fRatio2, fRatio3, fRatio4, fLevel1, fLevel2, fLevel3, fLevel4, fBright, fVolume,
    fA, fD, fS, fR, fMA, fMD, fMS, fMR, fVelMod, fKeyScale, fDetune, fOctave,
    fVibDepth, fVibRate, fPitchEnv, fPitchDecay, fGlide, fMode, fChorus, fDrive, fWidth, fFine1, fFine3, fTone,
    fCount
};

const float ratios[] = { 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 14.0f, 16.0f, 1.41f, 2.76f, 3.17f, 5.4f };
constexpr int numRatios = (int) (sizeof (ratios) / sizeof (ratios[0]));

juce::StringArray ratioNames()
{
    juce::StringArray s;
    for (float r : ratios) s.add (juce::String (r, r == std::floor (r) ? 0 : 2).trimCharactersAtEnd ("0") + " x");
    return s;
}

// which operators modulate which (mod[i] = bit mask of the operators that modulate i), and which are carriers
struct Algo { const char* name; int mod[4]; bool carrier[4]; };
const Algo algos[8] = {
    { "4>3>2>1",        { 1 << 1, 1 << 2, 1 << 3, 0 }, { true, false, false, false } },
    { "(3+4)>2>1",      { 1 << 1, (1 << 2) | (1 << 3), 0, 0 }, { true, false, false, false } },
    { "(2+3+4)>1",      { (1 << 1) | (1 << 2) | (1 << 3), 0, 0, 0 }, { true, false, false, false } },
    { "2>1  4>3",       { 1 << 1, 0, 1 << 3, 0 }, { true, false, true, false } },
    { "4>3>1  2>1",     { (1 << 1) | (1 << 2), 0, 1 << 3, 0 }, { true, false, false, false } },
    { "4>(1,2,3)",      { 1 << 3, 1 << 3, 1 << 3, 0 }, { true, true, true, false } },
    { "2>1  3  4",      { 1 << 1, 0, 0, 0 }, { true, false, true, true } },
    { "1 2 3 4 organ",  { 0, 0, 0, 0 }, { true, true, true, true } },
};

/** A fast sine (a table with linear steps). */
struct SineTable
{
    static constexpr int size = 4096;
    float t[size + 1];
    SineTable() { for (int i = 0; i <= size; ++i) t[i] = std::sin (dsp::twoPi * (float) i / size); }
    inline float operator() (float phase) const noexcept
    {
        phase -= std::floor (phase);
        const float x = phase * size;
        const int i = (int) x;
        const float f = x - (float) i;
        return t[i] + (t[i + 1] - t[i]) * f;
    }
};
const SineTable& sine() { static const SineTable s; return s; }
}

EngineDef makeFMDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "FM";
    juce::StringArray algoNames;
    for (const auto& a : algos) algoNames.add (a.name);
    const auto rn = ratioNames();
    auto& p = e.params;
    p = {
        choice ("algo", "Algorithm", algoNames, 0), pct ("feedback", "Feedback", 0.0f),
        choice ("ratio1", "Op 1 ratio", rn, 1), choice ("ratio2", "Op 2 ratio", rn, 1), choice ("ratio3", "Op 3 ratio", rn, 3), choice ("ratio4", "Op 4 ratio", rn, 1),
        pct ("level1", "Op 1 level", 1.0f), pct ("level2", "Op 2 level", 0.5f), pct ("level3", "Op 3 level", 0.3f), pct ("level4", "Op 4 level", 0.0f),
        pct ("bright", "Brightness", 0.5f), db ("volume", "Volume", -36.0f, 6.0f, -6.0f),
        sec ("a", "Attack", 0.001f, 8.0f, 0.002f), sec ("d", "Decay", 0.01f, 10.0f, 1.2f), pct ("s", "Sustain", 0.5f), sec ("r", "Release", 0.01f, 10.0f, 0.4f),
        sec ("ma", "Mod attack", 0.001f, 8.0f, 0.002f), sec ("md", "Mod decay", 0.01f, 10.0f, 0.6f), pct ("ms", "Mod sustain", 0.3f), sec ("mr", "Mod release", 0.01f, 10.0f, 0.4f),
        pct ("velmod", "Vel > bright", 0.6f), pct ("keyscale", "Key scaling", 0.3f), cents ("detune", "Detune", 0, 30, 4), num ("octave", "Octave", -2, 2, 0),
        pct ("vibdepth", "Vibrato", 0.0f), hz ("vibrate", "Vib rate", 0.5f, 10.0f, 5.5f), semi ("penv", "Pitch env", -24, 24, 0), sec ("pdecay", "Pitch decay", 0.005f, 1.0f, 0.05f),
        sec ("glide", "Glide", 0.0f, 1.0f, 0.0f), choice ("mode", "Voices mode", { "Poly", "Mono", "Legato" }, 0), pct ("chorus", "Chorus", 0.0f), pct ("drive", "Drive", 0.0f),
        pct ("width", "Width", 0.8f), cents ("fine1", "Op 1 fine", -50, 50, 0), cents ("fine3", "Op 3 fine", -50, 50, 0), bi ("tone", "Tone", 0.0f),
    };
    jassert ((int) p.size() == fCount);
    e.pages = {
        page ("FM",   { fAlgo, fFeedback, fRatio1, fRatio2, fRatio3, fRatio4, fLevel1, fLevel2, fLevel3, fLevel4, fBright, fVolume,
                        fFine1, fDetune, fFine3, fVelMod, fKeyScale, fOctave, fTone, fDrive, fChorus, fWidth, fMode, fGlide }),
        page ("ENV",  { fA, fD, fS, fR, fMA, fMD, fMS, fMR, fVelMod, fKeyScale, fPitchEnv, fPitchDecay,
                        fVibDepth, fVibRate, fGlide, fMode }),
    };

    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    // ratio indices: 0 = 0.5, 1 = 1, 2 = 1.5, 3 = 2, 4 = 2.5, 5 = 3, 6 = 3.5, 7 = 4, 8 = 5, 9 = 6, 10 = 7, 11 = 8, 13 = 10, 15 = 12, 16 = 14, 17 = 16
    add ("DX Piano", "E-Piano", { { "algo", 3 }, { "ratio2", 13 }, { "level2", 0.35f }, { "ratio4", 1 }, { "level4", 0.45f }, { "level3", 0.9f }, { "ratio3", 1 }, { "d", 2.5f }, { "s", 0.0f }, { "md", 0.35f }, { "ms", 0.1f }, { "detune", 6 }, { "chorus", 0.25f } });
    add ("Soft EP", "E-Piano", { { "algo", 3 }, { "ratio2", 1 }, { "level2", 0.3f }, { "level3", 0.7f }, { "ratio3", 1 }, { "level4", 0.2f }, { "d", 3.0f }, { "s", 0.1f }, { "md", 0.8f }, { "chorus", 0.35f } });
    add ("Tine Bell EP", "E-Piano", { { "algo", 3 }, { "ratio2", 15 }, { "level2", 0.45f }, { "level3", 0.8f }, { "ratio3", 1 }, { "level4", 0.5f }, { "d", 2.0f }, { "s", 0.0f }, { "md", 0.2f }, { "ms", 0.0f } });
    add ("Tubular Bell", "Bell", { { "algo", 3 }, { "ratio2", 9 }, { "level2", 0.6f }, { "ratio3", 1 }, { "level3", 0.7f }, { "ratio4", 19 }, { "level4", 0.5f }, { "d", 6.0f }, { "s", 0.0f }, { "r", 3.0f }, { "md", 4.0f }, { "ms", 0.0f } });
    add ("Glass Bell", "Bell", { { "algo", 0 }, { "ratio2", 20 }, { "level2", 0.5f }, { "ratio3", 7 }, { "level3", 0.2f }, { "d", 4.0f }, { "s", 0.0f }, { "r", 2.5f }, { "md", 2.0f }, { "ms", 0.0f } });
    add ("Church Bell", "Bell", { { "algo", 2 }, { "ratio2", 21 }, { "level2", 0.55f }, { "ratio3", 18 }, { "level3", 0.4f }, { "ratio4", 19 }, { "level4", 0.3f }, { "octave", -1 }, { "d", 8.0f }, { "s", 0.0f }, { "r", 5.0f }, { "md", 5.0f } });
    add ("Music Box", "Bell", { { "algo", 0 }, { "ratio2", 11 }, { "level2", 0.35f }, { "octave", 1 }, { "d", 1.5f }, { "s", 0.0f }, { "md", 0.3f }, { "ms", 0.0f } });
    add ("Marimba", "Mallet", { { "algo", 0 }, { "ratio2", 7 }, { "level2", 0.4f }, { "d", 0.5f }, { "s", 0.0f }, { "r", 0.3f }, { "md", 0.05f }, { "ms", 0.0f } });
    add ("Vibraphone", "Mallet", { { "algo", 3 }, { "ratio2", 7 }, { "level2", 0.3f }, { "ratio3", 1 }, { "level3", 0.6f }, { "ratio4", 13 }, { "level4", 0.2f }, { "d", 3.0f }, { "s", 0.0f }, { "md", 0.15f }, { "vibdepth", 0.15f }, { "vibrate", 5.0f } });
    add ("Kalimba", "Mallet", { { "algo", 0 }, { "ratio2", 9 }, { "level2", 0.45f }, { "d", 0.8f }, { "s", 0.0f }, { "md", 0.04f }, { "ms", 0.0f }, { "octave", 1 } });
    add ("Steel Drum", "Mallet", { { "algo", 0 }, { "ratio2", 4 }, { "level2", 0.5f }, { "d", 1.0f }, { "s", 0.0f }, { "md", 0.25f }, { "ms", 0.1f } });
    add ("FM Bass", "Bass", { { "algo", 0 }, { "mode", 2 }, { "octave", -1 }, { "ratio2", 1 }, { "level2", 0.6f }, { "feedback", 0.2f }, { "d", 0.6f }, { "s", 0.6f }, { "r", 0.08f }, { "md", 0.25f }, { "ms", 0.2f } });
    add ("Lately Bass", "Bass", { { "algo", 1 }, { "mode", 2 }, { "octave", -1 }, { "ratio2", 1 }, { "level2", 0.55f }, { "ratio3", 3 }, { "level3", 0.35f }, { "ratio4", 1 }, { "level4", 0.3f }, { "d", 0.8f }, { "s", 0.5f }, { "r", 0.1f }, { "md", 0.18f }, { "ms", 0.15f } });
    add ("Slap Bass", "Bass", { { "algo", 0 }, { "mode", 2 }, { "octave", -1 }, { "ratio2", 5 }, { "level2", 0.6f }, { "d", 0.4f }, { "s", 0.3f }, { "r", 0.08f }, { "md", 0.05f }, { "ms", 0.05f } });
    add ("Sub Sine Bass", "Bass", { { "algo", 7 }, { "mode", 2 }, { "octave", -1 }, { "level2", 0.0f }, { "level3", 0.0f }, { "s", 1.0f }, { "r", 0.1f }, { "drive", 0.3f } });
    add ("Wood Bass", "Bass", { { "algo", 0 }, { "mode", 2 }, { "octave", -1 }, { "ratio2", 3 }, { "level2", 0.3f }, { "d", 0.9f }, { "s", 0.0f }, { "r", 0.15f }, { "md", 0.1f }, { "ms", 0.0f } });
    add ("DX Brass", "Brass", { { "algo", 1 }, { "ratio2", 1 }, { "level2", 0.55f }, { "ratio3", 1 }, { "level3", 0.3f }, { "ratio4", 1 }, { "level4", 0.25f }, { "feedback", 0.35f }, { "a", 0.06f }, { "d", 1.0f }, { "s", 0.8f }, { "r", 0.3f }, { "ma", 0.1f }, { "md", 0.6f }, { "ms", 0.6f }, { "chorus", 0.3f } });
    add ("FM Strings", "Strings", { { "algo", 5 }, { "ratio1", 1 }, { "ratio2", 3 }, { "ratio3", 1 }, { "level2", 0.6f }, { "level3", 0.7f }, { "ratio4", 1 }, { "level4", 0.25f }, { "a", 0.6f }, { "s", 0.9f }, { "r", 1.2f }, { "ma", 0.8f }, { "ms", 0.8f }, { "detune", 12 }, { "chorus", 0.6f } });
    add ("Drawbar Organ", "Organ", { { "algo", 7 }, { "ratio1", 0 }, { "ratio2", 1 }, { "ratio3", 3 }, { "ratio4", 5 }, { "level1", 0.8f }, { "level2", 0.9f }, { "level3", 0.6f }, { "level4", 0.4f }, { "a", 0.003f }, { "s", 1.0f }, { "r", 0.05f }, { "chorus", 0.4f } });
    add ("Percussive Organ", "Organ", { { "algo", 6 }, { "ratio2", 3 }, { "level2", 0.3f }, { "ratio3", 3 }, { "level3", 0.6f }, { "ratio4", 5 }, { "level4", 0.4f }, { "d", 0.4f }, { "s", 0.7f }, { "r", 0.05f }, { "chorus", 0.3f } });
    add ("FM Pluck", "Pluck", { { "algo", 0 }, { "ratio2", 3 }, { "level2", 0.55f }, { "feedback", 0.15f }, { "d", 0.5f }, { "s", 0.0f }, { "r", 0.2f }, { "md", 0.12f }, { "ms", 0.0f } });
    add ("Harp", "Pluck", { { "algo", 0 }, { "ratio2", 1 }, { "level2", 0.35f }, { "d", 1.6f }, { "s", 0.0f }, { "r", 0.8f }, { "md", 0.3f }, { "ms", 0.0f }, { "chorus", 0.2f } });
    add ("Koto", "Pluck", { { "algo", 0 }, { "ratio2", 5 }, { "level2", 0.45f }, { "ratio3", 3 }, { "level3", 0.2f }, { "d", 0.9f }, { "s", 0.0f }, { "md", 0.1f }, { "ms", 0.0f }, { "pdecay", 0.02f }, { "penv", 1 } });
    add ("Metal Hit", "FX", { { "algo", 0 }, { "ratio2", 21 }, { "level2", 0.8f }, { "ratio3", 20 }, { "level3", 0.6f }, { "ratio4", 18 }, { "level4", 0.5f }, { "feedback", 0.4f }, { "d", 1.5f }, { "s", 0.0f }, { "md", 1.0f }, { "ms", 0.0f } });
    add ("Laser", "FX", { { "algo", 0 }, { "mode", 1 }, { "ratio2", 3 }, { "level2", 0.7f }, { "penv", 24 }, { "pdecay", 0.2f }, { "d", 0.3f }, { "s", 0.0f } });
    add ("Noise Sweep", "FX", { { "algo", 0 }, { "feedback", 1.0f }, { "level4", 1.0f }, { "level3", 0.8f }, { "level2", 0.6f }, { "a", 1.0f }, { "s", 1.0f }, { "ma", 2.0f }, { "ms", 1.0f }, { "r", 1.5f } });
    add ("Lead Sync-ish", "Lead", { { "algo", 0 }, { "mode", 2 }, { "ratio2", 3 }, { "level2", 0.6f }, { "feedback", 0.5f }, { "level4", 0.0f }, { "s", 0.9f }, { "ms", 0.7f }, { "glide", 0.06f }, { "vibdepth", 0.12f } });
    add ("Flute", "Lead", { { "algo", 0 }, { "mode", 2 }, { "ratio2", 1 }, { "level2", 0.15f }, { "feedback", 0.25f }, { "a", 0.06f }, { "s", 0.9f }, { "ma", 0.08f }, { "ms", 0.3f }, { "vibdepth", 0.15f }, { "vibrate", 5.0f } });
    add ("Init", "Init", {});
    return e;
}

//==============================================================================
namespace
{
class FMEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v = Voice();
        chorus.reset();
        volume.setTime (0.02f, sr);
    }

    void setParams (const float* v) override
    {
        for (int i = 0; i < fCount; ++i) p[(size_t) i] = v[i];
        for (auto& vo : voices)
        {
            vo.amp.set (p[fA], p[fD], p[fS], p[fR], sr);
            vo.menv.set (p[fMA], p[fMD], p[fMS], p[fMR], sr);
        }
    }
    void setBend (float s) override { bend = s; }
    void setMod (float m) override { mod = m; }
    void setTempo (double) override {}

    void noteOn (int note, float vel, bool slide, bool) override
    {
        const int mode = juce::roundToInt (p[fMode]);
        Voice* v = nullptr;
        bool legato = false;
        if (mode != 0)
        {
            v = &voices[0];
            legato = (mode == 2 && v->gate) || slide;
            if (numHeld < 16) held[(size_t) numHeld++] = note;
        }
        else
        {
            for (auto& x : voices) if (x.active && x.note == note) v = &x;
            if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
            if (v == nullptr)   // the oldest released, else the oldest
            {
                uint64_t best = ~(uint64_t) 0;
                for (int pass = 0; pass < 2 && v == nullptr; ++pass)
                    for (auto& x : voices) if ((pass == 1 || ! x.gate) && x.age < best) { best = x.age; v = &x; }
            }
        }
        if (v == nullptr) return;
        v->target = (float) note;
        if (! v->active || (p[fGlide] < 0.001f && ! legato)) v->pitch = (float) note;
        v->note = note;
        v->gate = true;
        if (! legato)
        {
            v->vel = vel;
            v->amp.set (p[fA], p[fD], p[fS], p[fR], sr);
            v->menv.set (p[fMA], p[fMD], p[fMS], p[fMR], sr);
            v->amp.gateOn (true);
            v->menv.gateOn (true);
            v->pitchEnv = 1.0f;
            if (v->amp.value < 0.001f) for (auto& ph : v->phase) ph = 0.0f;   // a fresh note starts in phase (sharper attack)
        }
        v->active = true;
        v->age = ++counter;
        v->pan = ((float) (counter % 5) - 2.0f) * 0.25f * p[fWidth];
    }

    void noteOff (int note) override
    {
        if (juce::roundToInt (p[fMode]) != 0)
        {
            int n = 0;
            for (int i = 0; i < numHeld; ++i) if (held[(size_t) i] != note) held[(size_t) n++] = held[(size_t) i];
            numHeld = n;
            auto& v = voices[0];
            if (numHeld > 0) { v.target = (float) held[(size_t) numHeld - 1]; v.note = held[(size_t) numHeld - 1]; return; }
            v.gate = false; v.amp.gateOff(); v.menv.gateOff();
            return;
        }
        for (auto& v : voices) if (v.active && v.gate && v.note == note) { v.gate = false; v.amp.gateOff(); v.menv.gateOff(); }
    }

    void allOff() override { for (auto& v : voices) { v.amp.kill(); v.menv.kill(); v.active = v.gate = false; } numHeld = 0; }

    void render (float* outL, float* outR, int n) override
    {
        bool any = false;
        for (auto& v : voices) any = any || v.active;
        if (! any && tail <= 0) return;
        tail = any ? (int) (sr * 0.3f) : tail - n;

        const auto& algo = algos[juce::jlimit (0, 7, juce::roundToInt (p[fAlgo]))];
        float ratio[4], level[4];
        for (int o = 0; o < 4; ++o)
        {
            ratio[o] = ratios[juce::jlimit (0, numRatios - 1, juce::roundToInt (p[(size_t) (fRatio1 + o)]))];
            level[o] = p[(size_t) (fLevel1 + o)];
        }
        ratio[0] *= std::exp2 (p[fFine1] / 1200.0f);
        ratio[2] *= std::exp2 (p[fFine3] / 1200.0f);
        const float detune = std::exp2 (p[fDetune] / 1200.0f);
        ratio[1] *= detune; ratio[3] /= detune;
        const float vol = dsp::dbToGain (p[fVolume]);
        const float fb = p[fFeedback] * p[fFeedback] * 0.9f;
        const float glideK = p[fGlide] > 0.001f ? std::exp (-1.0f / (p[fGlide] * sr)) : 0.0f;
        const float pdk = std::exp (-1.0f / juce::jmax (1.0f, p[fPitchDecay] * sr));
        const auto& sn = sine();
        int carriers = 0;
        for (bool c : algo.carrier) carriers += c ? 1 : 0;
        const float carrierGain = 1.0f / std::sqrt ((float) juce::jmax (1, carriers));
        const float drive = 1.0f + p[fDrive] * 6.0f;

        for (int i = 0; i < n; ++i)
        {
            vibPh += p[fVibRate] / sr; if (vibPh >= 1.0f) vibPh -= 1.0f;
            const float vib = (p[fVibDepth] + mod * 0.5f) * 0.4f * sn (vibPh);
            float l = 0.0f, r = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                v.pitch = glideK > 0.0f ? v.target + (v.pitch - v.target) * glideK : v.target;
                v.pitchEnv *= pdk;
                const float note = v.pitch + 12.0f * p[fOctave] + bend + vib + v.pitchEnv * p[fPitchEnv];
                const float f0 = dsp::midiToHz (note) / sr;
                const float a = v.amp.process(), m = v.menv.process();
                // how hard the modulators hit: level, the brightness knob, velocity, key scaling (high notes get less)
                const float keyScale = 1.0f - p[fKeyScale] * juce::jlimit (0.0f, 1.0f, (v.pitch - 48.0f) / 48.0f);
                const float bright = (0.25f + 1.5f * p[fBright]) * (1.0f - p[fVelMod] + p[fVelMod] * v.vel) * keyScale * (1.0f + mod);
                float out[4];
                for (int o = 3; o >= 0; --o)
                {
                    float pm = 0.0f;
                    for (int k = o + 1; k < 4; ++k) if (algo.mod[o] & (1 << k)) pm += out[k];
                    if (o == 3) pm += fb * (v.fb1 + v.fb2) * 0.5f;
                    v.phase[(size_t) o] += f0 * ratio[o];
                    if (v.phase[(size_t) o] >= 1.0f) v.phase[(size_t) o] -= std::floor (v.phase[(size_t) o]);
                    const float s = sn (v.phase[(size_t) o] + pm);
                    if (o == 3) { v.fb2 = v.fb1; v.fb1 = s; }
                    out[o] = algo.carrier[o] ? s * level[o] * a : s * level[o] * m * bright * 1.6f;   // modulators: in cycles of phase
                }
                float x = 0.0f;
                for (int o = 0; o < 4; ++o) if (algo.carrier[o]) x += out[o];
                x *= carrierGain * (0.4f + 0.6f * v.vel) * 0.5f;
                const float gl = std::sqrt (0.5f * (1.0f - v.pan)), gr = std::sqrt (0.5f * (1.0f + v.pan));
                l += x * gl; r += x * gr;
                if (! v.amp.active()) v.active = v.gate = false;
            }
            if (drive > 1.01f) { const float k = 1.0f / std::sqrt (drive); l = dsp::fastTanh (l * drive) * k; r = dsp::fastTanh (r * drive) * k; }
            if (std::abs (p[fTone]) > 0.02f)
            {
                toneL += (l - toneL) * 0.12f; toneR += (r - toneR) * 0.12f;
                if (p[fTone] < 0.0f) { l += (toneL - l) * -p[fTone]; r += (toneR - r) * -p[fTone]; }
                else { l += (l - toneL) * p[fTone]; r += (r - toneR) * p[fTone]; }
            }
            if (p[fChorus] > 0.001f) chorus.process (l, r, p[fChorus], 0.5f, 0.6f, sr);
            const float g = volume.process (vol) * 1.4f;
            outL[i] += l * g;
            outR[i] += r * g;
        }
    }

private:
    struct Voice
    {
        bool active = false, gate = false;
        int note = 60;
        float vel = 0.8f, pitch = 60.0f, target = 60.0f, pitchEnv = 0.0f, pan = 0.0f, fb1 = 0.0f, fb2 = 0.0f;
        std::array<float, 4> phase {};
        uint64_t age = 0;
        dsp::Adsr amp, menv;
    };
    std::array<float, fCount> p {};
    std::array<Voice, 8> voices;
    std::array<int, 16> held {};
    int numHeld = 0, tail = 0;
    uint64_t counter = 0;
    float sr = 48000.0f, bend = 0.0f, mod = 0.0f, vibPh = 0.0f, toneL = 0.0f, toneR = 0.0f;
    dsp::Chorus chorus;
    dsp::Smooth volume;
};
} // namespace

std::unique_ptr<Engine> makeFMEngine() { return std::make_unique<FMEngine>(); }
} // namespace catbox
