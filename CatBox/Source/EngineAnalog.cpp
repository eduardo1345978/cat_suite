#include "Engine.h"
#include "Dsp.h"
#include <cmath>

/**
    ANALOG: a polyphonic subtractive synth with everything on knobs.
    Two band-limited oscillators (saw, square with PWM, triangle, sine, noise), sub oscillator,
    noise, unison up to 4 with detune, hard sync, FM and ring; a ladder (LP 24) or state variable
    filter (LP 12, band, high, notch) with drive, envelope, key tracking and velocity; amp and
    filter envelopes, a pitch envelope, two LFOs (synced or free), glide, poly / mono / legato;
    then drive, bit crush, sample-rate reduction, EQ, chorus and stereo width.
*/
namespace catbox
{
namespace
{
enum A
{
    // OSC
    aWave1 = 0, aWave2, aSemi2, aFine2, aMix, aSub, aNoise, aUnison, aDetune, aSpread, aPw, aPwm,
    aOctave, aTune, aSync, aFm, aRing, aDrift, aGlide, aMode, aVoices, aVelAmp, aBend, aPan,
    // FILTER
    aFType, aCutoff, aReso, aFDrive, aEnvAmt, aKeyTrack, aVelFilt, aFA, aFD, aFS, aFR, aHp,
    // ENV
    aA, aD, aS, aR, aPitchEnv, aPitchDecay,
    // LFO
    aL1Shape, aL1Rate, aL1Sync, aL1Dest, aL1Amt, aL1Delay, aL2Shape, aL2Rate, aL2Sync, aL2Dest, aL2Amt, aL2Delay,
    // FX
    aDrive, aChorus, aChorusRate, aChorusDepth, aWidth, aCrush, aDown, aLow, aHigh, aVolume,
    aCount
};
enum Dest { DestPitch = 0, DestFilter, DestAmp, DestPwm, DestPan, DestOsc2 };
}

EngineDef makeAnalogDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Analog";
    const juce::StringArray waves { "Saw", "Square", "Triangle", "Sine", "Noise" };
    const juce::StringArray shapes { "Sine", "Triangle", "Saw", "Square", "Random" };
    const juce::StringArray dests { "Pitch", "Filter", "Volume", "Pulse width", "Pan", "Osc 2 pitch" };
    auto& p = e.params;
    p = {
        choice ("wave1", "Osc 1", waves, 0), choice ("wave2", "Osc 2", waves, 0), semi ("semi2", "Osc 2 pitch", -24, 24, 0),
        cents ("fine2", "Osc 2 fine", -50, 50, 7), pct ("mix", "Osc mix", 0.5f), pct ("sub", "Sub osc", 0.0f), pct ("noise", "Noise", 0.0f),
        num ("unison", "Unison", 1, 4, 1), cents ("detune", "Detune", 0, 50, 12), pct ("spread", "Stereo", 0.5f),
        pct ("pw", "Pulse width", 0.5f), pct ("pwm", "PWM", 0.0f),
        num ("octave", "Octave", -2, 2, 0), cents ("tune", "Tune", -50, 50, 0), toggle ("sync", "Sync", false), pct ("fm", "FM", 0.0f),
        pct ("ring", "Ring", 0.0f), pct ("drift", "Drift", 0.15f), sec ("glide", "Glide", 0.0f, 2.0f, 0.0f),
        choice ("mode", "Voices mode", { "Poly", "Mono", "Legato" }, 0), num ("voices", "Voices", 1, 8, 8),
        pct ("velAmp", "Velocity", 0.5f), semi ("bend", "Bend range", 0, 24, 2), bi ("pan", "Pan", 0.0f),

        choice ("ftype", "Filter", { "LP 24", "LP 12", "Band", "High", "Notch" }, 0), hz ("cutoff", "Cutoff", 20.0f, 20000.0f, 2400.0f),
        pct ("reso", "Resonance", 0.2f), pct ("fdrive", "Drive", 0.1f), bi ("envamt", "Env amount", 0.35f), pct ("keytrack", "Key track", 0.5f),
        pct ("velfilt", "Vel > filter", 0.3f), sec ("fa", "F attack", 0.001f, 10.0f, 0.005f), sec ("fd", "F decay", 0.005f, 10.0f, 0.4f),
        pct ("fs", "F sustain", 0.3f), sec ("fr", "F release", 0.005f, 10.0f, 0.4f), hz ("hp", "Low cut", 20.0f, 2000.0f, 20.0f),

        sec ("a", "Attack", 0.001f, 10.0f, 0.005f), sec ("d", "Decay", 0.005f, 10.0f, 0.3f), pct ("s", "Sustain", 0.8f),
        sec ("r", "Release", 0.005f, 10.0f, 0.3f), semi ("penv", "Pitch env", -24, 24, 0), sec ("pdecay", "Pitch decay", 0.005f, 2.0f, 0.08f),

        choice ("l1shape", "LFO 1 shape", shapes, 0), hz ("l1rate", "LFO 1 rate", 0.02f, 20.0f, 3.0f), choice ("l1sync", "LFO 1 sync", syncNames(), 0),
        choice ("l1dest", "LFO 1 to", dests, DestFilter), pct ("l1amt", "LFO 1 amount", 0.0f), sec ("l1delay", "LFO 1 delay", 0.0f, 5.0f, 0.0f),
        choice ("l2shape", "LFO 2 shape", shapes, 1), hz ("l2rate", "LFO 2 rate", 0.02f, 20.0f, 5.0f), choice ("l2sync", "LFO 2 sync", syncNames(), 0),
        choice ("l2dest", "LFO 2 to", dests, DestPitch), pct ("l2amt", "LFO 2 amount", 0.0f), sec ("l2delay", "LFO 2 delay", 0.0f, 5.0f, 0.4f),

        pct ("drive", "Drive", 0.0f), pct ("chorus", "Chorus", 0.0f), hz ("chrate", "Chorus rate", 0.05f, 5.0f, 0.6f),
        pct ("chdepth", "Chorus depth", 0.5f), pct ("width", "Width", 1.0f), pct ("crush", "Bit crush", 0.0f), pct ("down", "Lo-fi", 0.0f),
        db ("low", "Low", -12.0f, 12.0f, 0.0f), db ("high", "High", -12.0f, 12.0f, 0.0f), db ("volume", "Volume", -36.0f, 6.0f, -6.0f),
    };
    jassert ((int) p.size() == aCount);
    e.pages = {
        page ("OSC",    { aWave1, aWave2, aSemi2, aFine2, aMix, aSub, aNoise, aUnison, aDetune, aSpread, aPw, aPwm,
                          aOctave, aTune, aSync, aFm, aRing, aDrift, aGlide, aMode, aVoices, aVelAmp, aBend, aPan }),
        page ("FILTER", { aFType, aCutoff, aReso, aFDrive, aEnvAmt, aKeyTrack, aVelFilt, aFA, aFD, aFS, aFR, aHp }),
        page ("ENV",    { aA, aD, aS, aR, aPitchEnv, aPitchDecay, aFA, aFD, aFS, aFR, aEnvAmt, aVelAmp }),
        page ("LFO",    { aL1Shape, aL1Rate, aL1Sync, aL1Dest, aL1Amt, aL1Delay, aL2Shape, aL2Rate, aL2Sync, aL2Dest, aL2Amt, aL2Delay }),
        page ("FX",     { aDrive, aChorus, aChorusRate, aChorusDepth, aWidth, aCrush, aDown, aLow, aHigh, aVolume, aPan, aSpread }),
    };

    // ---- factory presets
    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    // Bass
    add ("Fat Saw Bass", "Bass", { { "mode", 2 }, { "octave", -1 }, { "sub", 0.5f }, { "cutoff", 600 }, { "reso", 0.25f }, { "envamt", 0.45f }, { "fd", 0.25f }, { "fs", 0.1f }, { "s", 1.0f }, { "r", 0.08f }, { "glide", 0.04f }, { "drive", 0.2f } });
    add ("Sub Bass", "Bass", { { "mode", 2 }, { "octave", -1 }, { "wave1", 3 }, { "wave2", 2 }, { "mix", 0.3f }, { "cutoff", 900 }, { "envamt", 0.0f }, { "s", 1.0f }, { "r", 0.1f }, { "drift", 0.0f } });
    add ("Reese", "Bass", { { "mode", 2 }, { "octave", -1 }, { "unison", 3 }, { "detune", 28 }, { "fine2", 14 }, { "cutoff", 700 }, { "reso", 0.15f }, { "envamt", 0.1f }, { "s", 1.0f }, { "chorus", 0.3f }, { "drive", 0.3f } });
    add ("Square Bass", "Bass", { { "mode", 2 }, { "octave", -1 }, { "wave1", 1 }, { "wave2", 1 }, { "semi2", -12 }, { "cutoff", 500 }, { "reso", 0.35f }, { "envamt", 0.5f }, { "fd", 0.18f }, { "fs", 0.0f }, { "s", 1.0f }, { "r", 0.06f } });
    add ("Rubber Bass", "Bass", { { "mode", 2 }, { "octave", -1 }, { "wave2", 1 }, { "cutoff", 300 }, { "reso", 0.55f }, { "envamt", 0.7f }, { "fd", 0.12f }, { "fs", 0.0f }, { "s", 0.9f }, { "r", 0.05f }, { "glide", 0.06f } });
    add ("Wobble Bass", "Bass", { { "mode", 2 }, { "octave", -1 }, { "unison", 2 }, { "detune", 18 }, { "cutoff", 350 }, { "reso", 0.5f }, { "envamt", 0.0f }, { "s", 1.0f }, { "l1dest", DestFilter }, { "l1amt", 0.8f }, { "l1sync", 4 }, { "l1shape", 1 }, { "drive", 0.45f } });
    add ("808 Sine", "Bass", { { "mode", 2 }, { "octave", -1 }, { "wave1", 3 }, { "mix", 0.0f }, { "cutoff", 2000 }, { "envamt", 0.0f }, { "a", 0.002f }, { "d", 1.2f }, { "s", 0.0f }, { "r", 0.3f }, { "penv", 7 }, { "pdecay", 0.06f }, { "drive", 0.35f }, { "glide", 0.08f } });
    add ("Pluck Bass", "Bass", { { "octave", -1 }, { "wave2", 1 }, { "cutoff", 400 }, { "reso", 0.3f }, { "envamt", 0.6f }, { "fd", 0.15f }, { "fs", 0.0f }, { "d", 0.4f }, { "s", 0.0f }, { "r", 0.15f } });
    add ("Synthwave Bass", "Bass", { { "mode", 2 }, { "octave", -1 }, { "unison", 2 }, { "detune", 10 }, { "cutoff", 1100 }, { "reso", 0.2f }, { "envamt", 0.4f }, { "fd", 0.2f }, { "fs", 0.2f }, { "s", 1.0f }, { "r", 0.1f }, { "chorus", 0.2f } });
    // Lead
    add ("Classic Lead", "Lead", { { "mode", 2 }, { "unison", 2 }, { "detune", 10 }, { "cutoff", 3500 }, { "reso", 0.2f }, { "envamt", 0.2f }, { "glide", 0.06f }, { "l2amt", 0.12f }, { "l2delay", 0.5f }, { "chorus", 0.2f } });
    add ("Square Lead", "Lead", { { "mode", 2 }, { "wave1", 1 }, { "wave2", 1 }, { "semi2", 12 }, { "pw", 0.35f }, { "pwm", 0.3f }, { "cutoff", 4000 }, { "glide", 0.05f }, { "l2amt", 0.1f } });
    add ("Sync Lead", "Lead", { { "mode", 2 }, { "sync", 1 }, { "semi2", 9 }, { "mix", 0.85f }, { "cutoff", 5000 }, { "penv", 12 }, { "pdecay", 0.15f }, { "drive", 0.25f } });
    add ("Sine Whistle", "Lead", { { "mode", 2 }, { "wave1", 3 }, { "wave2", 3 }, { "semi2", 12 }, { "mix", 0.2f }, { "noise", 0.05f }, { "cutoff", 8000 }, { "a", 0.04f }, { "glide", 0.1f }, { "l2amt", 0.18f }, { "l2delay", 0.3f } });
    add ("Screamer", "Lead", { { "mode", 2 }, { "unison", 3 }, { "detune", 16 }, { "cutoff", 1500 }, { "reso", 0.7f }, { "envamt", 0.5f }, { "fdrive", 0.6f }, { "drive", 0.6f } });
    add ("Chip Lead", "Lead", { { "mode", 2 }, { "wave1", 1 }, { "wave2", 1 }, { "pw", 0.25f }, { "mix", 0.0f }, { "cutoff", 16000 }, { "envamt", 0.0f }, { "crush", 0.6f }, { "down", 0.3f }, { "l2amt", 0.12f }, { "l2rate", 6 } });
    add ("Soft Lead", "Lead", { { "mode", 2 }, { "wave1", 2 }, { "wave2", 0 }, { "mix", 0.3f }, { "cutoff", 2200 }, { "a", 0.03f }, { "glide", 0.08f }, { "chorus", 0.35f } });
    add ("Fifth Lead", "Lead", { { "mode", 2 }, { "semi2", 7 }, { "unison", 2 }, { "detune", 8 }, { "cutoff", 3000 }, { "reso", 0.25f }, { "envamt", 0.3f } });
    // Pad
    add ("Warm Pad", "Pad", { { "unison", 3 }, { "detune", 14 }, { "cutoff", 1400 }, { "reso", 0.15f }, { "envamt", 0.1f }, { "a", 0.8f }, { "d", 1.5f }, { "s", 0.8f }, { "r", 1.6f }, { "chorus", 0.5f } });
    add ("Glass Pad", "Pad", { { "wave1", 2 }, { "wave2", 3 }, { "semi2", 12 }, { "ring", 0.25f }, { "cutoff", 5000 }, { "a", 1.0f }, { "r", 2.5f }, { "chorus", 0.4f } });
    add ("Strings Pad", "Strings", { { "unison", 4 }, { "detune", 16 }, { "cutoff", 2600 }, { "envamt", 0.05f }, { "a", 0.5f }, { "s", 0.9f }, { "r", 1.2f }, { "chorus", 0.6f }, { "l2amt", 0.05f } });
    add ("Slow Sunrise", "Pad", { { "unison", 3 }, { "detune", 12 }, { "cutoff", 300 }, { "reso", 0.3f }, { "envamt", 0.6f }, { "fa", 4.0f }, { "fd", 4.0f }, { "fs", 0.6f }, { "a", 2.5f }, { "r", 3.0f }, { "chorus", 0.4f } });
    add ("Dark Pad", "Pad", { { "unison", 3 }, { "detune", 20 }, { "cutoff", 600 }, { "a", 1.2f }, { "r", 2.0f }, { "sub", 0.3f }, { "chorus", 0.4f }, { "l1amt", 0.2f }, { "l1rate", 0.2f } });
    add ("PWM Pad", "Pad", { { "wave1", 1 }, { "wave2", 1 }, { "pwm", 0.6f }, { "fine2", 10 }, { "cutoff", 2000 }, { "a", 0.6f }, { "r", 1.5f }, { "chorus", 0.5f } });
    add ("Breathing Pad", "Pad", { { "unison", 2 }, { "detune", 10 }, { "noise", 0.1f }, { "cutoff", 900 }, { "a", 1.5f }, { "r", 2.0f }, { "l1amt", 0.35f }, { "l1rate", 0.12f }, { "chorus", 0.4f } });
    add ("Lofi Pad", "Pad", { { "unison", 2 }, { "detune", 14 }, { "cutoff", 1200 }, { "a", 0.7f }, { "r", 1.5f }, { "drift", 0.5f }, { "crush", 0.35f }, { "down", 0.25f }, { "chorus", 0.3f } });
    // Pluck / Keys
    add ("Pluck", "Pluck", { { "cutoff", 600 }, { "reso", 0.3f }, { "envamt", 0.7f }, { "fd", 0.18f }, { "fs", 0.0f }, { "d", 0.5f }, { "s", 0.0f }, { "r", 0.3f } });
    add ("Bell Pluck", "Pluck", { { "wave1", 3 }, { "wave2", 3 }, { "semi2", 19 }, { "fm", 0.35f }, { "cutoff", 6000 }, { "d", 0.9f }, { "s", 0.0f }, { "r", 0.8f } });
    add ("Marimba", "Pluck", { { "wave1", 3 }, { "wave2", 2 }, { "semi2", 24 }, { "mix", 0.25f }, { "cutoff", 4000 }, { "d", 0.35f }, { "s", 0.0f }, { "r", 0.3f }, { "penv", 2 }, { "pdecay", 0.02f } });
    add ("Stab", "Keys", { { "unison", 3 }, { "detune", 12 }, { "cutoff", 900 }, { "reso", 0.25f }, { "envamt", 0.6f }, { "fd", 0.22f }, { "fs", 0.1f }, { "d", 0.3f }, { "s", 0.2f }, { "r", 0.2f }, { "chorus", 0.3f } });
    add ("House Organ", "Keys", { { "wave1", 1 }, { "wave2", 3 }, { "semi2", 12 }, { "cutoff", 3000 }, { "envamt", 0.2f }, { "fd", 0.1f }, { "a", 0.002f }, { "d", 0.25f }, { "s", 0.6f }, { "r", 0.1f } });
    add ("Poly Keys", "Keys", { { "unison", 2 }, { "detune", 8 }, { "cutoff", 1800 }, { "envamt", 0.35f }, { "fd", 0.6f }, { "fs", 0.3f }, { "d", 1.0f }, { "s", 0.5f }, { "r", 0.5f }, { "chorus", 0.35f } });
    add ("Kalimba", "Pluck", { { "wave1", 3 }, { "wave2", 3 }, { "semi2", 31 }, { "mix", 0.2f }, { "fm", 0.15f }, { "cutoff", 7000 }, { "d", 0.5f }, { "s", 0.0f }, { "r", 0.4f } });
    // Brass
    add ("Synth Brass", "Brass", { { "unison", 2 }, { "detune", 10 }, { "cutoff", 900 }, { "reso", 0.1f }, { "envamt", 0.55f }, { "fa", 0.08f }, { "fd", 0.5f }, { "fs", 0.4f }, { "a", 0.04f }, { "s", 0.9f }, { "r", 0.3f } });
    add ("Brass Stab", "Brass", { { "unison", 3 }, { "detune", 14 }, { "cutoff", 800 }, { "envamt", 0.7f }, { "fd", 0.25f }, { "fs", 0.1f }, { "d", 0.35f }, { "s", 0.2f }, { "r", 0.25f } });
    // FX / Texture
    add ("Riser", "FX", { { "wave1", 4 }, { "noise", 1.0f }, { "cutoff", 300 }, { "reso", 0.5f }, { "envamt", 1.0f }, { "fa", 4.0f }, { "fs", 1.0f }, { "a", 2.0f }, { "r", 1.0f }, { "chorus", 0.4f } });
    add ("Laser Zap", "FX", { { "mode", 1 }, { "penv", 24 }, { "pdecay", 0.15f }, { "d", 0.2f }, { "s", 0.0f }, { "cutoff", 8000 } });
    add ("Wind", "Texture", { { "wave1", 4 }, { "wave2", 4 }, { "ftype", 2 }, { "cutoff", 800 }, { "reso", 0.6f }, { "a", 1.5f }, { "r", 2.0f }, { "l1amt", 0.6f }, { "l1rate", 0.15f } });
    add ("Space Drone", "Texture", { { "unison", 4 }, { "detune", 30 }, { "semi2", -12 }, { "cutoff", 500 }, { "reso", 0.4f }, { "a", 3.0f }, { "r", 4.0f }, { "l1amt", 0.3f }, { "l1rate", 0.05f }, { "chorus", 0.6f } });
    add ("Init", "Init", {});
    return e;
}

//==============================================================================
namespace
{
class AnalogEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        for (auto& v : voices) { v = Voice(); }
        chorus.reset();
        volume.setTime (0.02f, sr);
        volume.y = 0.0f;
    }

    void setParams (const float* v) override
    {
        for (int i = 0; i < aCount; ++i) p[(size_t) i] = v[i];
        for (auto& vo : voices)
        {
            vo.amp.set (p[aA], p[aD], p[aS], p[aR], sr);
            vo.fenv.set (p[aFA], p[aFD], p[aFS], p[aFR], sr);
        }
    }

    void setTempo (double bpm) override { tempo = (float) bpm; }
    void setBend (float semis) override { bend = semis; }
    void setMod (float m) override { mod = m; }

    void noteOn (int note, float vel, bool slide, bool) override
    {
        const int mode = juce::roundToInt (p[aMode]);
        if (mode != 0)   // mono / legato: one voice
        {
            auto& v = voices[0];
            const bool legato = (mode == 2 && v.gate) || slide;
            for (int i = 1; i < maxVoices; ++i) voices[(size_t) i].amp.kill();
            held[(size_t) (numHeld < 16 ? numHeld++ : 15)] = note;
            v.target = (float) note;
            if (! v.active || (p[aGlide] <= 0.0005f && ! legato)) v.pitch = (float) note;
            v.note = note;
            v.vel = vel;
            v.gate = true;
            if (! v.active || ! legato) start (v);
            v.active = true;
            return;
        }
        const int limit = juce::jlimit (1, maxVoices, juce::roundToInt (p[aVoices]));
        Voice* pick = nullptr;
        for (int i = 0; i < limit && pick == nullptr; ++i) if (voices[(size_t) i].note == note && voices[(size_t) i].active) pick = &voices[(size_t) i];
        for (int i = 0; i < limit && pick == nullptr; ++i) if (! voices[(size_t) i].active) pick = &voices[(size_t) i];
        if (pick == nullptr)   // steal the oldest released, else the oldest
        {
            uint64_t best = ~(uint64_t) 0;
            for (int pass = 0; pass < 2 && pick == nullptr; ++pass)
                for (int i = 0; i < limit; ++i)
                {
                    auto& v = voices[(size_t) i];
                    if ((pass == 0 && v.gate) || v.age >= best) continue;
                    best = v.age; pick = &v;
                }
        }
        if (pick == nullptr) return;
        auto& v = *pick;
        v.pitch = p[aGlide] > 0.0005f ? lastPitch : (float) note;
        v.target = (float) note;
        v.note = note;
        v.vel = vel;
        v.gate = true;
        v.active = true;
        start (v);
        lastPitch = (float) note;
    }

    void noteOff (int note) override
    {
        const int mode = juce::roundToInt (p[aMode]);
        if (mode != 0)
        {
            int n = 0;
            for (int i = 0; i < numHeld; ++i) if (held[(size_t) i] != note) held[(size_t) n++] = held[(size_t) i];
            numHeld = n;
            auto& v = voices[0];
            if (numHeld > 0) { v.target = (float) held[(size_t) numHeld - 1]; v.note = held[(size_t) numHeld - 1]; return; }   // back to the last held key
            v.gate = false; v.amp.gateOff(); v.fenv.gateOff();
            return;
        }
        for (auto& v : voices)
            if (v.active && v.gate && v.note == note) { v.gate = false; v.amp.gateOff(); v.fenv.gateOff(); }
    }

    void allOff() override
    {
        for (auto& v : voices) { v.amp.kill(); v.fenv.kill(); v.active = v.gate = false; }
        numHeld = 0;
    }

    void render (float* outL, float* outR, int n) override
    {
        const float vol = dsp::dbToGain (p[aVolume]);
        bool any = false;
        for (auto& v : voices) any = any || v.active;
        if (! any && tailSamples <= 0) return;
        tailSamples = any ? (int) (sr * 0.5f) : tailSamples - n;

        // the LFOs run for the whole engine (synced to the tempo when chosen)
        const float lfoDt1 = lfoRate (aL1Rate, aL1Sync) / sr, lfoDt2 = lfoRate (aL2Rate, aL2Sync) / sr;
        const int shape1 = juce::roundToInt (p[aL1Shape]), shape2 = juce::roundToInt (p[aL2Shape]);
        const int dest1 = juce::roundToInt (p[aL1Dest]), dest2 = juce::roundToInt (p[aL2Dest]);
        const int ftype = juce::roundToInt (p[aFType]);
        const int unison = juce::jlimit (1, 4, juce::roundToInt (p[aUnison]));
        const int w1 = juce::roundToInt (p[aWave1]), w2 = juce::roundToInt (p[aWave2]);
        const bool sync = p[aSync] > 0.5f;
        const float uniGain = 1.0f / std::sqrt ((float) unison);
        const float glideCoef = p[aGlide] > 0.0005f ? std::exp (-1.0f / (p[aGlide] * sr / (float) control)) : 0.0f;
        const float crushBits = p[aCrush] > 0.001f ? std::exp2 (16.0f - 13.0f * p[aCrush]) : 0.0f;
        const int downStep = 1 + (int) (p[aDown] * p[aDown] * 24.0f);

        for (int s0 = 0; s0 < n; s0 += control)
        {
            const int m = juce::jmin (control, n - s0);
            const float l1 = lfo1.process (shape1, lfoDt1 * (float) m), l2 = lfo2.process (shape2, lfoDt2 * (float) m);
            for (auto& v : voices)
            {
                if (! v.active) continue;
                // control rate: pitch, cutoff, pulse width
                v.pitch = glideCoef > 0.0f ? v.target + (v.pitch - v.target) * glideCoef : v.target;
                v.lfoFade = juce::jmin (1.0f, v.lfoFade + (float) m / juce::jmax (1.0f, p[aL1Delay] * sr));
                v.lfoFade2 = juce::jmin (1.0f, v.lfoFade2 + (float) m / juce::jmax (1.0f, p[aL2Delay] * sr));
                v.driftNow += (v.driftTarget - v.driftNow) * 0.002f * (float) m / 16.0f;
                if ((v.age++ & 31) == 0) v.driftTarget = rng.next() * p[aDrift] * 0.25f;
                v.pitchEnv *= std::exp (-(float) m / juce::jmax (1.0f, p[aPitchDecay] * sr));

                const float lA = l1 * p[aL1Amt] * v.lfoFade, lB = l2 * p[aL2Amt] * v.lfoFade2;
                auto destSum = [&] (int d) { return (dest1 == d ? lA : 0.0f) + (dest2 == d ? lB : 0.0f); };
                const float basePitch = v.pitch + 12.0f * p[aOctave] + p[aTune] * 0.01f + bend + v.driftNow
                                      + destSum (DestPitch) * 12.0f + mod * 0.3f * std::sin (v.vibPh) + v.pitchEnv * p[aPitchEnv];
                v.vibPh += 0.03f * (float) m;
                const float f1 = dsp::midiToHz (basePitch);
                const float f2 = dsp::midiToHz (basePitch + p[aSemi2] + p[aFine2] * 0.01f + destSum (DestOsc2) * 12.0f);
                v.dt1 = f1 / sr; v.dt2 = f2 / sr;
                v.pw = juce::jlimit (0.05f, 0.95f, p[aPw] + destSum (DestPwm) * 0.45f + p[aPwm] * 0.4f * std::sin (v.pwmPh));
                v.pwmPh += 0.0035f * (float) m;
                const float fe = v.fenv.value;
                const float octaves = p[aEnvAmt] * fe * 7.0f + p[aKeyTrack] * (v.pitch - 60.0f) / 12.0f
                                    + p[aVelFilt] * (v.vel - 0.7f) * 3.0f + destSum (DestFilter) * 3.0f + mod * 2.0f;
                const float cut = juce::jlimit (20.0f, sr * 0.45f, p[aCutoff] * std::exp2 (octaves));
                if (ftype == 0) { v.ladder.set (cut, p[aReso], sr); v.ladder.drive = 1.0f + p[aFDrive] * 4.0f; }
                else v.svf.set (cut, 0.5f + p[aReso] * 12.0f, sr);
                v.hp.set (p[aHp], 0.7f, sr);
                v.ampMod = 1.0f + destSum (DestAmp);
                v.panNow = juce::jlimit (-1.0f, 1.0f, v.panBase + p[aPan] + destSum (DestPan));

                const float velGain = 1.0f - p[aVelAmp] + p[aVelAmp] * v.vel;
                const float gl = std::sqrt (0.5f * (1.0f - v.panNow)), gr = std::sqrt (0.5f * (1.0f + v.panNow));
                for (int i = 0; i < m; ++i)
                {
                    const float env = v.amp.process();
                    v.fenv.process();
                    // oscillators (unison copies detuned around the pitch)
                    float o1 = 0.0f, o2 = 0.0f;
                    for (int u = 0; u < unison; ++u)
                    {
                        const float det = unison > 1 ? (2.0f * (float) u / (float) (unison - 1) - 1.0f) * p[aDetune] * 0.01f : 0.0f;
                        const float mul = std::exp2 (det / 12.0f);
                        const float before = v.osc1[(size_t) u].phase;
                        const float a = v.osc1[(size_t) u].process (w1, v.dt1 * mul, v.pw, rng);
                        o1 += a;
                        const bool wrapped = v.osc1[(size_t) u].phase < before;
                        if (sync && wrapped) v.osc2[(size_t) u].reset();
                        o2 += v.osc2[(size_t) u].process (w2, juce::jmax (0.0f, v.dt2 * mul * (1.0f + p[aFm] * 2.0f * a)), v.pw, rng);
                    }
                    o1 *= uniGain; o2 *= uniGain;
                    const float subS = v.sub.process (dsp::WaveSquare, v.dt1 * 0.5f, 0.5f, rng);
                    float x = o1 * (1.0f - p[aMix]) + o2 * p[aMix] + p[aSub] * subS + p[aNoise] * rng.next() + p[aRing] * o1 * o2 * 1.5f;
                    if (ftype == 0) x = v.ladder.process (x);
                    else
                    {
                        const auto o = v.svf.process (x * (1.0f + p[aFDrive]));
                        x = ftype == 1 ? o.low : ftype == 2 ? o.band : ftype == 3 ? o.high : o.low + o.high;
                    }
                    x = v.hp.process (x).high;
                    x *= env * velGain * v.ampMod * 1.2f;
                    outBufL[i] += x * gl;
                    outBufR[i] += x * gr;
                }
                if (! v.amp.active()) { v.active = false; v.gate = false; }
            }

            // engine effects on this block
            for (int i = 0; i < m; ++i)
            {
                float l = outBufL[i], r = outBufR[i];
                outBufL[i] = outBufR[i] = 0.0f;
                if (p[aDrive] > 0.001f) { const float d = 1.0f + p[aDrive] * 8.0f; l = dsp::fastTanh (l * d) / std::sqrt (d); r = dsp::fastTanh (r * d) / std::sqrt (d); }
                if (crushBits > 0.0f) { l = std::round (l * crushBits) / crushBits; r = std::round (r * crushBits) / crushBits; }
                if (downStep > 1) { if (downCount++ % downStep == 0) { heldL = l; heldR = r; } l = heldL; r = heldR; }
                if (std::abs (p[aLow]) > 0.05f) { lowL = lowL + 0.02f * (l - lowL); lowR = lowR + 0.02f * (r - lowR); const float g = dsp::dbToGain (p[aLow]) - 1.0f; l += lowL * g; r += lowR * g; }
                if (std::abs (p[aHigh]) > 0.05f) { hiL = hiL + 0.25f * (l - hiL); hiR = hiR + 0.25f * (r - hiR); const float g = dsp::dbToGain (p[aHigh]) - 1.0f; l += (l - hiL) * g; r += (r - hiR) * g; }
                if (p[aChorus] > 0.001f) chorus.process (l, r, p[aChorus], p[aChorusRate], p[aChorusDepth], sr);
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * p[aWidth];
                const float g = volume.process (vol);
                outL[s0 + i] += (mid + side) * g;
                outR[s0 + i] += (mid - side) * g;
            }
        }
    }

private:
    static constexpr int maxVoices = 8, control = 16;
    struct Voice
    {
        bool active = false, gate = false;
        int note = 60;
        float vel = 0.8f, pitch = 60.0f, target = 60.0f, dt1 = 0.0f, dt2 = 0.0f, pw = 0.5f, pwmPh = 0.0f, vibPh = 0.0f;
        float driftNow = 0.0f, driftTarget = 0.0f, lfoFade = 0.0f, lfoFade2 = 0.0f, pitchEnv = 0.0f, ampMod = 1.0f, panBase = 0.0f, panNow = 0.0f;
        uint64_t age = 0;
        std::array<dsp::Osc, 4> osc1, osc2;
        dsp::Osc sub;
        dsp::Ladder ladder;
        dsp::Svf svf, hp;
        dsp::Adsr amp, fenv;
    };

    void start (Voice& v)
    {
        v.amp.set (p[aA], p[aD], p[aS], p[aR], sr);
        v.fenv.set (p[aFA], p[aFD], p[aFS], p[aFR], sr);
        v.amp.gateOn (true);
        v.fenv.gateOn (true);
        v.pitchEnv = 1.0f;
        v.lfoFade = p[aL1Delay] > 0.001f ? 0.0f : 1.0f;
        v.lfoFade2 = p[aL2Delay] > 0.001f ? 0.0f : 1.0f;
        v.age = ++counter;
        const float spread = p[aSpread];
        v.panBase = spread * ((float) (counter % 5) / 2.0f - 1.0f) * 0.6f;
        for (auto& o : v.osc1) o.phase = rng.uni();
        for (auto& o : v.osc2) o.phase = rng.uni();
    }

    float lfoRate (int rateIdx, int syncIdx) const
    {
        const double beats = syncBeats (juce::roundToInt (p[(size_t) syncIdx]));
        return beats > 0.0 ? (float) (tempo / 60.0 / beats) : p[(size_t) rateIdx];
    }

    std::array<float, aCount> p {};
    std::array<Voice, maxVoices> voices;
    std::array<int, 16> held {};
    int numHeld = 0;
    float sr = 48000.0f, tempo = 120.0f, bend = 0.0f, mod = 0.0f, lastPitch = 60.0f;
    uint64_t counter = 0;
    dsp::Lfo lfo1, lfo2;
    dsp::Rng rng;
    dsp::Chorus chorus;
    dsp::Smooth volume;
    float outBufL[control] {}, outBufR[control] {};
    float heldL = 0.0f, heldR = 0.0f, lowL = 0.0f, lowR = 0.0f, hiL = 0.0f, hiR = 0.0f;
    int downCount = 0, tailSamples = 0;
};
} // namespace

std::unique_ptr<Engine> makeAnalogEngine() { return std::make_unique<AnalogEngine>(); }
} // namespace catbox
