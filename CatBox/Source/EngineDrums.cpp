#include "Engine.h"
#include "Dsp.h"
#include <cmath>

/**
    DRUMS: a 16-pad drum synth (no samples). Each pad has its own voice; the kit model picks the
    character (808: round and long, 909: punchy with click, Lo-fi: crunchy 12-bit, Acoustic: woody
    and noisy). Every pad has tune, decay, tone and level; the kit has drive, a bus compressor,
    stereo spread and the open/closed hat choke.
    Pads: 1 Kick, 2 Snare, 3 Clap, 4 Closed hat, 5 Open hat, 6 Low tom, 7 Mid tom, 8 High tom,
          9 Rim, 10 Cowbell, 11 Crash, 12 Ride, 13 Shaker, 14 Conga, 15 Snap, 16 Zap.
    Notes 36..51 play pads 1..16 (any other note plays pad (note - 36) mod 16).
*/
namespace catbox
{
namespace
{
constexpr int numPads = 16;
enum Pad { Kick = 0, Snare, Clap, ClosedHat, OpenHat, LowTom, MidTom, HighTom, Rim, Cowbell, Crash, Ride, Shaker, Conga, Snap, Zap };
enum { pTune = 0, pDecay, pTone, pLevel };
enum { gModel = numPads * 4, gDrive, gComp, gVolume, gChoke, gSpread, gCount };
enum Model { M808 = 0, M909, MLofi, MAcoustic };

const char* padNames[numPads] = { "Kick", "Snare", "Clap", "Closed hat", "Open hat", "Low tom", "Mid tom", "High tom",
                                  "Rim", "Cowbell", "Crash", "Ride", "Shaker", "Conga", "Snap", "Zap" };
const char* padShort[numPads] = { "kick", "snare", "clap", "chh", "ohh", "ltom", "mtom", "htom", "rim", "cow", "crash", "ride", "shaker", "conga", "snap", "zap" };
}

EngineDef makeDrumsDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Drums";
    e.drums = true;
    const char* toneNames[numPads] = { "click", "snappy", "tone", "tone", "tone", "tone", "tone", "tone", "tone", "tone", "tone", "tone", "tone", "tone", "tone", "tone" };
    for (int i = 0; i < numPads; ++i)
    {
        const juce::String id (padShort[i]), nm (padNames[i]);
        e.params.push_back (semi ((id + "_tune").toRawUTF8(), "", -24, 24, 0));
        e.params.push_back (pct ((id + "_decay").toRawUTF8(), "", 0.5f));
        e.params.push_back (pct ((id + "_tone").toRawUTF8(), "", 0.5f));
        e.params.push_back (pct ((id + "_level").toRawUTF8(), "", 0.8f));
        // names (ParamDef copies the strings)
        auto* d = &e.params[e.params.size() - 4];
        d[0].name = nm + " tune"; d[1].name = nm + " decay"; d[2].name = nm + " " + toneNames[i]; d[3].name = nm + " level";
    }
    e.params.push_back (choice ("model", "Kit model", { "808", "909", "Lo-fi", "Acoustic" }, 0));
    e.params.push_back (pct ("drive", "Drive", 0.15f));
    e.params.push_back (pct ("comp", "Compressor", 0.3f));
    e.params.push_back (db ("volume", "Volume", -36.0f, 6.0f, -3.0f));
    e.params.push_back (toggle ("choke", "Hat choke", true));
    e.params.push_back (pct ("spread", "Stereo", 0.4f));
    jassert ((int) e.params.size() == gCount);

    auto padPage = [] (const char* name, std::initializer_list<int> pads, std::initializer_list<int> extraTop, std::initializer_list<int> extraBottom,
                       std::initializer_list<int> extraShiftTop, std::initializer_list<int> extraShiftBottom)
    {
        KnobPage pg; pg.name = name;
        int col = 0;
        for (int pad : pads)
        {
            pg.slot[(size_t) col] = pad * 4 + pTune;
            pg.slot[(size_t) col + 6] = pad * 4 + pDecay;
            pg.slot[(size_t) col + 12] = pad * 4 + pTone;
            pg.slot[(size_t) col + 18] = pad * 4 + pLevel;
            ++col;
        }
        int c = col; for (int s : extraTop) pg.slot[(size_t) c++] = s;
        c = col; for (int s : extraBottom) pg.slot[(size_t) c++ + 6] = s;
        c = col; for (int s : extraShiftTop) pg.slot[(size_t) c++ + 12] = s;
        c = col; for (int s : extraShiftBottom) pg.slot[(size_t) c++ + 18] = s;
        return pg;
    };
    e.pages = {
        padPage ("PADS 1-6",   { 0, 1, 2, 3, 4, 5 }, {}, {}, {}, {}),
        padPage ("PADS 7-12",  { 6, 7, 8, 9, 10, 11 }, {}, {}, {}, {}),
        padPage ("PADS 13-16", { 12, 13, 14, 15 }, { gModel, gDrive }, { gComp, gVolume }, { gChoke }, { gSpread }),
    };

    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("808 Classic", "808", {});
    add ("808 Trap", "808", { { "kick_decay", 0.9f }, { "kick_tune", -3 }, { "kick_tone", 0.3f }, { "snare_tune", 3 }, { "snare_tone", 0.7f }, { "chh_decay", 0.3f }, { "chh_tone", 0.7f }, { "drive", 0.35f } });
    add ("808 Electro", "808", { { "kick_decay", 0.65f }, { "clap_decay", 0.6f }, { "cow_level", 0.9f }, { "drive", 0.25f } });
    add ("Miami Boom", "808", { { "kick_decay", 1.0f }, { "kick_tune", -5 }, { "drive", 0.5f }, { "comp", 0.5f } });
    add ("909 Classic", "909", { { "model", 1 } });
    add ("909 Techno", "909", { { "model", 1 }, { "kick_tone", 0.75f }, { "kick_decay", 0.55f }, { "drive", 0.45f }, { "comp", 0.6f }, { "ohh_decay", 0.45f }, { "ride_level", 0.7f } });
    add ("909 House", "909", { { "model", 1 }, { "kick_decay", 0.45f }, { "clap_level", 0.9f }, { "ohh_decay", 0.55f }, { "chh_tone", 0.6f }, { "comp", 0.4f } });
    add ("Industrial", "909", { { "model", 1 }, { "drive", 0.9f }, { "comp", 0.8f }, { "kick_tone", 0.9f }, { "snare_tone", 0.8f }, { "volume", -6 } });
    add ("Lo-fi Dusty", "Lo-fi", { { "model", 2 }, { "kick_decay", 0.4f }, { "snare_tone", 0.35f }, { "chh_tone", 0.3f }, { "comp", 0.5f } });
    add ("Boom Bap", "Lo-fi", { { "model", 2 }, { "kick_tune", -2 }, { "kick_decay", 0.5f }, { "snare_tune", -2 }, { "snare_decay", 0.6f }, { "drive", 0.4f }, { "comp", 0.6f } });
    add ("Chip Kit", "Lo-fi", { { "model", 2 }, { "kick_tone", 0.9f }, { "zap_level", 0.9f }, { "drive", 0.3f } });
    add ("Acoustic Room", "Acoustic", { { "model", 3 } });
    add ("Breakbeat", "Acoustic", { { "model", 3 }, { "snare_tone", 0.7f }, { "snare_decay", 0.55f }, { "comp", 0.7f }, { "drive", 0.3f } });
    add ("Jazz Brushes", "Acoustic", { { "model", 3 }, { "kick_level", 0.6f }, { "snare_tone", 0.85f }, { "snare_decay", 0.7f }, { "ride_level", 0.9f }, { "drive", 0.0f }, { "comp", 0.1f } });
    add ("Percussion", "Acoustic", { { "model", 3 }, { "conga_level", 0.9f }, { "shaker_level", 0.8f }, { "cow_level", 0.7f }, { "rim_level", 0.9f } });
    return e;
}

namespace
{
/** Six detuned square waves (the 808 / 909 metal for hats, cymbals and the cowbell). */
struct Metal
{
    float ph[6] {};
    inline float process (float baseHz, float sr, int count) noexcept
    {
        static const float ratio[6] = { 1.0f, 1.4827f, 1.8003f, 2.5464f, 2.6304f, 3.8968f };   // 205.3, 304.4, 369.6, 522.7, 540, 800 Hz
        float y = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            ph[i] += baseHz * ratio[i] / sr;
            ph[i] -= std::floor (ph[i]);
            y += ph[i] < 0.5f ? 1.0f : -1.0f;
        }
        return y / (float) count;
    }
};

struct DrumVoice
{
    int pad = 0;
    bool active = false;
    float t = 0.0f;                      // seconds since the hit
    float vel = 1.0f, ph = 0.0f, ph2 = 0.0f, amp = 0.0f, noiseAmp = 0.0f, pan = 0.0f;
    float hp = 0.0f, lp = 0.0f;
    dsp::Svf bp, bp2;
    Metal metal;
};

class DrumsEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (int i = 0; i < numPads; ++i) { voices[(size_t) i] = DrumVoice(); voices[(size_t) i].pad = i; }
        compEnv = 0.0f;
        volume.setTime (0.02f, sr);
    }

    void setParams (const float* v) override { for (int i = 0; i < gCount; ++i) p[(size_t) i] = v[i]; }

    void noteOn (int note, float vel, bool, bool accent) override
    {
        const int pad = ((note - 36) % numPads + numPads) % numPads;
        if (p[gChoke] > 0.5f && (pad == ClosedHat || pad == OpenHat))   // closed hat chokes the open one (and the other way)
            voices[(size_t) (pad == ClosedHat ? OpenHat : ClosedHat)].active = false;
        auto& v = voices[(size_t) pad];
        v.active = true;
        v.t = 0.0f;
        v.vel = juce::jlimit (0.05f, 1.0f, vel) * (accent ? 1.25f : 1.0f);
        v.ph = v.ph2 = 0.0f;
        v.hp = v.lp = 0.0f;
        v.bp.reset(); v.bp2.reset();
        const float spread = p[gSpread];
        static const float panOf[numPads] = { 0.0f, 0.05f, -0.1f, 0.35f, 0.35f, -0.5f, -0.2f, 0.2f, 0.25f, -0.3f, -0.4f, 0.45f, 0.55f, -0.55f, 0.15f, 0.0f };
        v.pan = panOf[pad] * spread;
    }

    void noteOff (int) override {}
    void allOff() override { for (auto& v : voices) v.active = false; }

    void render (float* outL, float* outR, int n) override
    {
        bool any = false;
        for (auto& v : voices) any = any || v.active;
        if (! any) return;
        const int model = juce::roundToInt (p[gModel]);
        const float dt = 1.0f / sr;
        const float vol = dsp::dbToGain (p[gVolume]);
        const float drive = 1.0f + p[gDrive] * 5.0f;
        const float comp = p[gComp];
        const float compAtt = 1.0f - std::exp (-dt / 0.003f), compRel = 1.0f - std::exp (-dt / 0.12f);

        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const float x = voice (v, model, dt);
                const float gl = std::sqrt (0.5f * (1.0f - v.pan)), gr = std::sqrt (0.5f * (1.0f + v.pan));
                l += x * gl; r += x * gr;
            }
            // lo-fi: 12 bits and half the sample rate
            if (model == MLofi)
            {
                if ((lofiCount++ & 1) == 0) { lofiL = std::round (l * 2048.0f) / 2048.0f; lofiR = std::round (r * 2048.0f) / 2048.0f; }
                l = lofiL; r = lofiR;
            }
            // bus compressor (peak follower, ratio grows with the knob), then drive
            if (comp > 0.001f)
            {
                const float lev = juce::jmax (std::abs (l), std::abs (r));
                compEnv += (lev - compEnv) * (lev > compEnv ? compAtt : compRel);
                const float thr = 0.5f - 0.4f * comp;
                const float over = compEnv > thr ? compEnv / thr : 1.0f;
                const float gain = std::pow (over, -comp * 0.8f) * (1.0f + comp * 1.2f);
                l *= gain; r *= gain;
            }
            if (drive > 1.01f) { const float k = 1.0f / std::sqrt (drive); l = dsp::fastTanh (l * drive) * k * 1.4f; r = dsp::fastTanh (r * drive) * k * 1.4f; }
            const float g = volume.process (vol);
            outL[i] += l * g;
            outR[i] += r * g;
        }
    }

private:
    float par (int pad, int which) const noexcept { return p[(size_t) (pad * 4 + which)]; }

    /** One sample of one pad. */
    float voice (DrumVoice& v, int model, float dt) noexcept
    {
        const int pad = v.pad;
        const float tune = std::exp2 (par (pad, pTune) / 12.0f);
        const float decayMul = std::exp2 ((par (pad, pDecay) - 0.5f) * 4.0f);   // 1/4 .. 4 x
        const float tone = par (pad, pTone);
        const float level = par (pad, pLevel) * par (pad, pLevel) * v.vel * 1.6f;
        const float t = v.t;
        v.t += dt;
        float y = 0.0f;
        float len = 1.0f;   // when the voice is done

        auto env = [] (float tt, float dec) { return std::exp (-tt * 4.6f / juce::jmax (0.001f, dec)); };
        auto sine = [&] (float& ph, float hz) { ph += hz * dt; ph -= std::floor (ph); return std::sin (dsp::twoPi * ph); };
        const float noise = rng.next();

        switch (pad)
        {
            case Kick:
            {
                // base pitch, sweep depth and time, decay, click per model
                float base = 50.0f, sweep = 3.2f, sweepT = 0.025f, dec = 0.55f, click = 0.35f;
                if (model == M909) { base = 52.0f; sweep = 5.0f; sweepT = 0.018f; dec = 0.32f; click = 0.8f; }
                if (model == MLofi) { base = 55.0f; sweep = 2.5f; sweepT = 0.03f; dec = 0.3f; click = 0.5f; }
                if (model == MAcoustic) { base = 62.0f; sweep = 1.6f; sweepT = 0.012f; dec = 0.22f; click = 0.6f; }
                dec *= decayMul; len = dec * 1.2f;
                const float hz = base * tune * (1.0f + sweep * env (t, sweepT * 4.6f));
                float body = sine (v.ph, hz) * env (t, dec);
                if (model == M909) body = dsp::fastTanh (body * 1.8f) * 0.8f;   // the 909 kick is a bit squarer
                // click: a short filtered noise / pulse transient (the tone knob)
                v.lp += (noise - v.lp) * (model == MAcoustic ? 0.15f : 0.5f);
                const float clk = (v.lp * 0.6f + (t < 0.0015f ? 1.0f : 0.0f)) * env (t, 0.006f) * click * (0.3f + tone * 1.4f);
                const float thump = model == MAcoustic ? v.lp * env (t, 0.04f) * 0.3f : 0.0f;
                y = body * 1.1f + clk + thump;
                break;
            }
            case Snare:
            {
                float f1 = 180.0f, f2 = 330.0f, dec = 0.16f, ndec = 0.2f, bpHz = 3000.0f;
                if (model == M909) { f1 = 190.0f; f2 = 345.0f; dec = 0.12f; ndec = 0.24f; bpHz = 5000.0f; }
                if (model == MAcoustic) { f1 = 200.0f; f2 = 290.0f; dec = 0.1f; ndec = 0.32f; bpHz = 2500.0f; }
                if (model == MLofi) { ndec = 0.18f; bpHz = 2200.0f; }
                dec *= decayMul; ndec *= decayMul; len = juce::jmax (dec, ndec) * 1.2f;
                const float pitchDrop = 1.0f + 0.4f * env (t, 0.02f);
                const float body = (sine (v.ph, f1 * tune * pitchDrop) * 0.6f + sine (v.ph2, f2 * tune * pitchDrop) * 0.4f) * env (t, dec);
                v.bp.set (bpHz * tune, 0.7f, sr);
                const auto nb = v.bp.process (noise);
                const float snap = (nb.band + nb.high * 0.5f) * env (t, ndec);
                y = body * (1.1f - tone * 0.6f) + snap * (0.4f + tone * 1.6f);
                break;
            }
            case Clap:
            {
                // three quick bursts, then the tail
                const float dec = 0.25f * decayMul;
                len = 0.03f + dec * 1.2f;
                float e = 0.0f;
                for (int b = 0; b < 3; ++b) { const float tb = t - (float) b * 0.011f; if (tb >= 0.0f && tb < 0.011f) e = juce::jmax (e, env (tb, 0.012f)); }
                if (t > 0.033f) e = juce::jmax (e, 0.7f * env (t - 0.033f, dec));
                v.bp.set ((model == M909 ? 1200.0f : 1000.0f) * tune * (0.7f + tone * 0.8f), 1.6f, sr);
                y = v.bp.process (noise).band * e * 2.2f;
                break;
            }
            case ClosedHat:
            case OpenHat:
            {
                const bool open = pad == OpenHat;
                float dec = (open ? 0.45f : 0.05f) * decayMul;
                if (model == M909) dec *= 0.85f;
                len = dec * 1.2f;
                const float m = v.metal.process (205.3f * tune * (model == MAcoustic ? 1.3f : 1.0f), sr, 6);
                const float src = model == M909 || model == MAcoustic ? m * 0.45f + noise * 0.55f : m * 0.8f + noise * 0.2f;
                v.bp.set (8000.0f + tone * 4000.0f, 1.2f, sr);
                v.bp2.set (6500.0f, 0.7f, sr);
                const float x = v.bp.process (src).band + 0.5f * v.bp2.process (src).high;
                y = x * env (t, dec) * (1.0f - std::exp (-t * 3000.0f)) * 1.6f;
                break;
            }
            case LowTom: case MidTom: case HighTom:
            {
                const float base = (pad == LowTom ? 90.0f : pad == MidTom ? 130.0f : 175.0f) * (model == MAcoustic ? 1.15f : 1.0f);
                const float dec = (model == M909 ? 0.35f : 0.45f) * decayMul;
                len = dec * 1.2f;
                const float hz = base * tune * (1.0f + 0.35f * env (t, 0.12f));
                v.lp += (noise - v.lp) * 0.2f;
                y = sine (v.ph, hz) * env (t, dec) + v.lp * env (t, 0.03f) * (0.2f + tone * 0.8f);
                break;
            }
            case Rim:
            {
                const float dec = 0.03f * decayMul; len = dec * 1.5f + 0.01f;
                v.bp.set (1700.0f * tune, 4.0f, sr);
                const float x = sine (v.ph, 1700.0f * tune) * 0.5f + sine (v.ph2, 480.0f * tune) * 0.5f + noise * 0.3f * tone;
                y = v.bp.process (x).band * env (t, dec) * 3.0f;
                break;
            }
            case Cowbell:
            {
                const float dec = 0.25f * decayMul; len = dec * 1.2f;
                v.ph += 540.0f * tune * dt; v.ph -= std::floor (v.ph);
                v.ph2 += 800.0f * tune * dt; v.ph2 -= std::floor (v.ph2);
                const float sq = (v.ph < 0.5f ? 1.0f : -1.0f) + (v.ph2 < 0.5f ? 1.0f : -1.0f);
                v.bp.set (900.0f + tone * 1500.0f, 1.5f, sr);
                const float e = 0.6f * env (t, 0.02f) + 0.4f * env (t, dec);
                y = v.bp.process (sq).band * e * 1.3f;
                break;
            }
            case Crash:
            case Ride:
            {
                const bool ride = pad == Ride;
                const float dec = (ride ? 1.6f : 1.4f) * decayMul; len = dec * 1.2f;
                const float m = v.metal.process ((ride ? 300.0f : 240.0f) * tune, sr, 6);
                const float src = ride ? m * 0.85f + noise * 0.15f : m * 0.4f + noise * 0.6f;
                v.bp.set ((ride ? 5000.0f : 4000.0f) + tone * 4000.0f, 0.8f, sr);
                const float x = v.bp.process (src).high;
                const float e = ride ? 0.4f * env (t, 0.03f) + 0.6f * env (t, dec) : env (t, dec);
                y = x * e * (ride ? 0.8f : 1.1f);
                break;
            }
            case Shaker:
            {
                const float dec = 0.08f * decayMul; len = 0.02f + dec * 1.3f;
                const float att = juce::jmin (1.0f, t / 0.015f);
                v.bp.set (6000.0f + tone * 4000.0f, 1.0f, sr);
                y = v.bp.process (noise).high * att * env (juce::jmax (0.0f, t - 0.015f), dec) * 1.2f;
                break;
            }
            case Conga:
            {
                const float dec = 0.22f * decayMul; len = dec * 1.2f;
                const float hz = 250.0f * tune * (1.0f + 0.15f * env (t, 0.02f));
                v.lp += (noise - v.lp) * 0.4f;
                y = sine (v.ph, hz) * env (t, dec) + v.lp * env (t, 0.006f) * tone * 0.8f;
                break;
            }
            case Snap:
            {
                const float dec = 0.04f * decayMul; len = dec * 1.4f + 0.01f;
                v.bp.set (2500.0f * tune, 2.0f, sr);
                y = v.bp.process (noise).band * env (t, dec) * 3.0f + (t < 0.001f ? 0.6f : 0.0f) * tone;
                break;
            }
            case Zap:
            default:
            {
                const float dec = 0.18f * decayMul; len = dec * 1.2f;
                const float hz = 80.0f * tune * (1.0f + 30.0f * env (t, 0.03f + tone * 0.08f));
                y = sine (v.ph, hz) * env (t, dec);
                break;
            }
        }
        if (model == MAcoustic) { v.hp += (y - v.hp) * 0.3f; y = v.hp; }   // a bit softer on top
        if (t > len + 0.01f) v.active = false;
        return y * level * 0.45f;
    }

    std::array<float, gCount> p {};
    std::array<DrumVoice, numPads> voices;
    dsp::Rng rng;
    dsp::Smooth volume;
    float sr = 48000.0f, compEnv = 0.0f, lofiL = 0.0f, lofiR = 0.0f;
    int lofiCount = 0;
};
} // namespace

std::unique_ptr<Engine> makeDrumsEngine() { return std::make_unique<DrumsEngine>(); }

/** Pad names for the screen. */
juce::String drumPadName (int pad) { return padNames[((pad % numPads) + numPads) % numPads]; }
} // namespace catbox
