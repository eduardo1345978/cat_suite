#include "Engine.h"
#include "Dsp.h"
#include <cmath>

/**
    ACID: a TB-303 model with every hidden trim pot on a knob.
    One oscillator (the 303's curved saw, or the square it shapes from that saw), a diode-ladder
    style 4-pole low-pass whose resonance loop has a high-pass in it (so the bass stays tight
    when the resonance is high), the filter envelope (MEG) with the "decay" knob, accents that
    force a short sweep and charge the accent capacitor (the "wow" of repeated accents),
    the fixed RC slide, the soft attack and the coupling high-pass, then drive and four
    distortion types (tube, fuzz, the classic Rat pedal).
*/
namespace catbox
{
namespace
{
enum
{
    cWave = 0, cCutoff, cReso, cEnvMod, cDecay, cAccent, cSlide, cDrive, cTune, cSub, cDist, cVolume,
    cAccDecay, cSweep, cSoftAtt, cFilterFm, cKeyTrack, cOctave, cLfoRate, cLfoAmt, cLfoSync, cHpf, cTone, cAmpDecay,
    cCount
};
}

EngineDef makeAcidDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Acid";
    e.params = {
        choice ("wave", "Wave", { "Saw", "Square" }, 0), hz ("cutoff", "Cutoff", 60.0f, 6000.0f, 520.0f), pct ("reso", "Resonance", 0.7f),
        pct ("envmod", "Env mod", 0.55f), sec ("decay", "Decay", 0.05f, 2.5f, 0.45f), pct ("accent", "Accent", 0.6f),
        sec ("slide", "Slide time", 0.01f, 0.5f, 0.06f), pct ("drive", "Drive", 0.25f), cents ("tune", "Tune", -100, 100, 0),
        pct ("sub", "Sub osc", 0.0f), choice ("dist", "Distortion", { "Off", "Tube", "Fuzz", "Rat" }, 0), db ("volume", "Volume", -36.0f, 6.0f, -4.0f),

        sec ("accdecay", "Accent decay", 0.03f, 1.0f, 0.2f), pct ("sweep", "Accent wow", 0.5f), sec ("softatt", "Soft attack", 0.0005f, 0.05f, 0.003f),
        pct ("filterfm", "Filter FM", 0.0f), pct ("keytrack", "Key track", 0.2f), num ("octave", "Octave", -2, 2, 0),
        hz ("lforate", "LFO rate", 0.05f, 20.0f, 2.0f), bi ("lfoamt", "LFO > cutoff", 0.0f), choice ("lfosync", "LFO sync", syncNames(), 0),
        hz ("hpf", "Low cut", 20.0f, 400.0f, 45.0f), bi ("tone", "Tone", 0.0f), sec ("ampdecay", "Amp decay", 0.08f, 5.0f, 3.0f),
    };
    jassert ((int) e.params.size() == cCount);
    e.pages = {
        page ("ACID", { cWave, cCutoff, cReso, cEnvMod, cDecay, cAccent, cSlide, cDrive, cTune, cSub, cDist, cVolume,
                        cAccDecay, cSweep, cSoftAtt, cFilterFm, cKeyTrack, cOctave, cLfoRate, cLfoAmt, cLfoSync, cHpf, cTone, cAmpDecay }),
        page ("MOD",  { cAccDecay, cSweep, cSoftAtt, cFilterFm, cKeyTrack, cOctave, cLfoRate, cLfoAmt, cLfoSync, cHpf, cTone, cAmpDecay }),
    };

    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Classic 303", "Acid", {});
    add ("Squelch", "Acid", { { "cutoff", 380 }, { "reso", 0.88f }, { "envmod", 0.75f }, { "decay", 0.35f }, { "accent", 0.8f }, { "sweep", 0.7f } });
    add ("Rubber", "Acid", { { "cutoff", 260 }, { "reso", 0.6f }, { "envmod", 0.5f }, { "decay", 0.25f }, { "drive", 0.1f } });
    add ("Deep Acid", "Acid", { { "cutoff", 180 }, { "reso", 0.78f }, { "envmod", 0.4f }, { "decay", 0.9f }, { "octave", -1 }, { "sub", 0.35f } });
    add ("Hard Acid", "Acid", { { "cutoff", 600 }, { "reso", 0.85f }, { "envmod", 0.7f }, { "decay", 0.3f }, { "dist", 3 }, { "drive", 0.6f }, { "volume", -2 } });
    add ("Square Acid", "Acid", { { "wave", 1 }, { "cutoff", 420 }, { "reso", 0.72f }, { "envmod", 0.6f } });
    add ("Phuture", "Acid", { { "cutoff", 300 }, { "reso", 0.92f }, { "envmod", 0.65f }, { "decay", 0.6f }, { "accent", 1.0f }, { "sweep", 0.9f }, { "drive", 0.35f } });
    add ("Screamer", "Acid", { { "cutoff", 900 }, { "reso", 0.97f }, { "envmod", 0.8f }, { "decay", 0.4f }, { "dist", 2 }, { "drive", 0.7f }, { "volume", -2 } });
    add ("Liquid", "Acid", { { "cutoff", 700 }, { "reso", 0.65f }, { "envmod", 0.3f }, { "decay", 1.4f }, { "slide", 0.15f }, { "tone", -0.3f } });
    add ("Wobbly", "Acid", { { "cutoff", 350 }, { "reso", 0.8f }, { "lfoamt", 0.5f }, { "lfosync", 4 } });
    add ("Dub Acid", "Acid", { { "cutoff", 220 }, { "reso", 0.75f }, { "envmod", 0.45f }, { "decay", 1.2f }, { "dist", 1 }, { "drive", 0.4f }, { "tone", -0.4f } });
    add ("Plastic", "Acid", { { "wave", 1 }, { "cutoff", 800 }, { "reso", 0.5f }, { "envmod", 0.35f }, { "decay", 0.2f }, { "ampdecay", 0.3f } });
    add ("Bubble", "Acid", { { "cutoff", 450 }, { "reso", 0.9f }, { "envmod", 0.25f }, { "decay", 0.12f }, { "filterfm", 0.2f }, { "ampdecay", 0.25f } });
    add ("Sub Acid", "Bass", { { "cutoff", 160 }, { "reso", 0.5f }, { "envmod", 0.35f }, { "octave", -1 }, { "sub", 0.7f }, { "hpf", 20 } });
    add ("Acid Lead", "Lead", { { "cutoff", 1200 }, { "reso", 0.8f }, { "envmod", 0.5f }, { "octave", 1 }, { "dist", 1 }, { "drive", 0.5f }, { "slide", 0.1f } });
    add ("Fuzz Bass", "Bass", { { "wave", 1 }, { "cutoff", 400 }, { "reso", 0.4f }, { "dist", 2 }, { "drive", 0.8f }, { "octave", -1 }, { "volume", -2 } });
    add ("Init", "Init", {});
    return e;
}

namespace
{
class AcidEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        allOff();
        amp = fenv = accEnv = wow = 0.0f;
        for (auto& z : s) z = 0.0f;
        fbHp = postHp = 0.0f;
        volume.setTime (0.02f, sr);
    }

    void setParams (const float* v) override { for (int i = 0; i < cCount; ++i) p[(size_t) i] = v[i]; }
    void setTempo (double bpm) override { tempo = (float) bpm; }
    void setBend (float semis) override { bend = semis; }
    void setMod (float m) override { mod = m; }

    void noteOn (int note, float vel, bool slide, bool accent) override
    {
        const bool legato = gate && (slide || numHeld > 0);
        if (numHeld < 16) held[(size_t) numHeld++] = note;
        target = (float) note;
        accentNote = accent || vel > 0.9f;
        if (! legato)
        {
            pitch = (float) note;
            fenv = 1.0f;
            attack = 0.0f;
            if (accentNote) { accEnv = 1.0f; }
            ampEnv = 1.0f;
        }
        gate = true;
        active = true;
    }

    void noteOff (int note) override
    {
        int n = 0;
        for (int i = 0; i < numHeld; ++i) if (held[(size_t) i] != note) held[(size_t) n++] = held[(size_t) i];
        numHeld = n;
        if (numHeld > 0) { target = (float) held[(size_t) numHeld - 1]; return; }   // slide back to the key still held
        gate = false;
    }

    void allOff() override { gate = false; numHeld = 0; }

    void render (float* outL, float* outR, int n) override
    {
        if (! active) return;
        const float dt = 1.0f / sr;
        const float slideK = 1.0f - std::exp (-dt / juce::jmax (0.002f, p[cSlide] * 0.4f));
        const float fdec = std::exp (-dt * 4.6f / (accentNote ? p[cAccDecay] : p[cDecay]));
        const float adec = std::exp (-dt * 4.6f / p[cAccDecay]);
        const float ampDec = std::exp (-dt * 4.6f / p[cAmpDecay]);
        const float rel = std::exp (-dt / 0.008f);
        const float attK = 1.0f - std::exp (-dt / p[cSoftAtt]);
        // the accent capacitor: resonance up = it charges slower and swells more (the 303's "wow")
        const float wowK = 1.0f - std::exp (-dt / (0.02f + p[cSweep] * 0.25f));
        const float res = juce::jlimit (0.0f, 1.04f, p[cReso] * 1.04f) * 4.0f;
        const float fbHpK = 1.0f - std::exp (-dsp::twoPi * 150.0f * dt);
        const float postHpK = 1.0f - std::exp (-dsp::twoPi * p[cHpf] * dt);
        const float lfoDt = lfoHz() * dt;
        const float vol = dsp::dbToGain (p[cVolume]);
        const int dist = juce::roundToInt (p[cDist]);
        const float drive = 1.0f + p[cDrive] * 6.0f;
        const float accAmt = accentNote ? p[cAccent] : 0.0f;
        const float toneK = p[cTone];

        for (int i = 0; i < n; ++i)
        {
            pitch += (target - pitch) * slideK;
            const float note = pitch + 12.0f * p[cOctave] + p[cTune] * 0.01f + bend;
            const float freq = dsp::midiToHz (note);
            const float inc = juce::jmin (0.45f, freq * dt);
            // the 303 saw is slightly curved (capacitor charge)
            const float before = phase;
            phase += inc; if (phase >= 1.0f) phase -= 1.0f;
            float saw = 2.0f * phase - 1.0f - dsp::polyBlep (phase, inc);
            saw = saw - 0.12f * (saw * saw - 0.33f);
            float osc = saw;
            if (p[cWave] > 0.5f)   // the square is shaped out of the saw: a pulse with soft edges
                osc = dsp::fastTanh ((saw + 0.12f) * 6.0f) * 0.9f;
            if (p[cSub] > 0.001f)
            {
                if (phase < before) subFlip = ! subFlip;
                osc += p[cSub] * (subFlip ? 0.7f : -0.7f);
            }

            // envelopes
            fenv *= fdec;
            accEnv *= adec;
            wow += (accEnv * accAmt - wow) * wowK;
            ampEnv *= ampDec;
            attack += (1.0f - attack) * attK;
            amp = gate ? amp + (1.0f - amp) * attK : amp * rel;

            // cutoff: knob, envelope, accent (sharp + "wow"), key track, LFO, filter FM
            const float lfo = std::sin (dsp::twoPi * lfoPh); lfoPh += lfoDt; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
            const float oct = p[cEnvMod] * 4.5f * fenv + accAmt * 1.6f * fenv + wow * 2.2f
                            + p[cKeyTrack] * (note - 48.0f) / 12.0f + p[cLfoAmt] * 2.5f * lfo + mod * 2.0f
                            + p[cFilterFm] * 2.0f * osc;
            const float fc = juce::jlimit (20.0f, sr * 0.42f, p[cCutoff] * std::exp2 (oct));
            const float wc = std::tan (dsp::pi * fc / sr);
            const float g = wc / (1.0f + wc);

            // resonance loop: the fed-back signal is high-passed (tight lows), then saturated
            fbHp += (last - fbHp) * fbHpK;
            const float fb = last - fbHp;
            float u = dsp::fastTanh (osc * 0.8f - res * fb);
            for (int k = 0; k < 4; ++k)
            {
                const float v = (u - s[(size_t) k]) * g;
                const float y = v + s[(size_t) k];
                s[(size_t) k] = y + v;
                u = k < 3 ? dsp::fastTanh (y * 1.05f) : y;   // diode ladder: each stage a little nonlinear
            }
            if (! std::isfinite (u)) { for (auto& z : s) z = 0.0f; u = 0.0f; }
            last = u;
            // gain makeup: high resonance eats the level on a real 303 less than on a moog
            float x = u * (1.0f + res * 0.35f);
            postHp += (x - postHp) * postHpK;
            x -= postHp;

            x *= amp * attack * (0.35f + 0.65f * ampEnv) * (1.0f + accAmt * 0.8f * accEnv);

            // drive and distortion
            x *= drive;
            switch (dist)
            {
                case 1: x = dsp::fastTanh (x + 0.15f) - 0.149f; break;                                  // tube: asymmetric, warm
                case 2: x = juce::jlimit (-1.0f, 1.0f, x * 3.0f) * 0.8f + dsp::fastTanh (x) * 0.2f; break; // fuzz: square-ish
                case 3: { ratLp += (x * 8.0f - ratLp) * 0.35f; x = dsp::fastTanh (ratLp) * 0.9f; break; }   // rat: hard clip after a gain stage with low-pass
                default: x = dsp::fastTanh (x); break;
            }
            x /= std::sqrt (drive);
            // tone: tilt (darker or brighter)
            toneLp += (x - toneLp) * 0.15f;
            x = toneK < 0.0f ? x + (toneLp - x) * (-toneK) : x + (x - toneLp) * toneK;

            const float gn = volume.process (vol) * 0.9f;
            outL[i] += x * gn;
            outR[i] += x * gn;
        }
        if (! gate && amp < 1.0e-5f) active = false;
    }

private:
    float lfoHz() const
    {
        const double b = syncBeats (juce::roundToInt (p[cLfoSync]));
        return b > 0.0 ? (float) (tempo / 60.0 / b) : p[cLfoRate];
    }

    std::array<float, cCount> p {};
    float sr = 48000.0f, tempo = 120.0f, bend = 0.0f, mod = 0.0f;
    float pitch = 48.0f, target = 48.0f, phase = 0.0f, lfoPh = 0.0f;
    float amp = 0.0f, fenv = 0.0f, accEnv = 0.0f, wow = 0.0f, ampEnv = 0.0f, attack = 0.0f;
    float s[4] {}, last = 0.0f, fbHp = 0.0f, postHp = 0.0f, ratLp = 0.0f, toneLp = 0.0f;
    bool gate = false, active = false, accentNote = false, subFlip = false;
    std::array<int, 16> held {};
    int numHeld = 0;
    dsp::Smooth volume;
};
} // namespace

std::unique_ptr<Engine> makeAcidEngine() { return std::make_unique<AcidEngine>(); }
} // namespace catbox
