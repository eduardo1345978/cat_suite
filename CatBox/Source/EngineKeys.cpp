#include "Engine.h"
#include "Dsp.h"
#include "CatSounds.h"
#include <cmath>

/**
    KEYS: the Cat Suite's played instruments (piano with stiff-string partials, FM e-piano,
    Karplus-Strong nylon guitar, pads, strings, glass bell, chip, pluck), then a resonant
    filter, drive, tremolo, chorus, EQ and stereo width on top.
*/
namespace catbox
{
namespace
{
enum { kSound = 0, kCutoff, kReso, kDrive, kTrem, kTremRate, kChorus, kLow, kHigh, kWidth, kOctave, kVolume,
       kBend, kChorusRate, kChorusDepth, kTremSync, kCrush, kPan, kCount };
}

EngineDef makeKeysDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Keys";
    e.params = {
        choice ("sound", "Instrument", { "Soft Pad", "E-Piano", "Pluck", "Strings", "Chip", "Glass Bell", "Piano", "Nylon Guitar" }, 6),
        hz ("cutoff", "Cutoff", 100.0f, 20000.0f, 20000.0f), pct ("reso", "Resonance", 0.1f), pct ("drive", "Drive", 0.0f),
        pct ("trem", "Tremolo", 0.0f), hz ("tremrate", "Trem rate", 0.5f, 12.0f, 5.0f), pct ("chorus", "Chorus", 0.0f),
        db ("low", "Low", -12.0f, 12.0f, 0.0f), db ("high", "High", -12.0f, 12.0f, 0.0f), pct ("width", "Width", 1.0f),
        num ("octave", "Octave", -2, 2, 0), db ("volume", "Volume", -36.0f, 6.0f, -3.0f),
        semi ("bend", "Bend range", 0, 24, 2), hz ("chrate", "Chorus rate", 0.05f, 5.0f, 0.5f), pct ("chdepth", "Chorus depth", 0.5f),
        choice ("tremsync", "Trem sync", syncNames(), 0), pct ("crush", "Lo-fi", 0.0f), bi ("pan", "Pan", 0.0f),
    };
    jassert ((int) e.params.size() == kCount);
    e.pages = { page ("KEYS", { kSound, kCutoff, kReso, kDrive, kTrem, kTremRate, kChorus, kLow, kHigh, kWidth, kOctave, kVolume,
                                kBend, kChorusRate, kChorusDepth, kTremSync, kCrush, kPan }) };

    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Grand Piano", "Piano", {});
    add ("Bright Piano", "Piano", { { "high", 5 }, { "low", -2 } });
    add ("Soft Piano", "Piano", { { "cutoff", 2500 }, { "high", -3 } });
    add ("Dark Piano", "Piano", { { "cutoff", 1200 }, { "low", 3 } });
    add ("Lofi Piano", "Piano", { { "cutoff", 3500 }, { "crush", 0.45f }, { "chorus", 0.3f }, { "chdepth", 0.8f }, { "chrate", 0.3f } });
    add ("Honky Piano", "Piano", { { "chorus", 0.6f }, { "chrate", 3.0f }, { "chdepth", 0.25f }, { "high", 3 } });
    add ("Rhodes", "E-Piano", { { "sound", 1 } });
    add ("Suitcase", "E-Piano", { { "sound", 1 }, { "trem", 0.5f }, { "tremrate", 4.5f }, { "width", 1.0f } });
    add ("Bright EP", "E-Piano", { { "sound", 1 }, { "high", 6 }, { "drive", 0.2f } });
    add ("Chorus EP", "E-Piano", { { "sound", 1 }, { "chorus", 0.6f } });
    add ("Lofi EP", "E-Piano", { { "sound", 1 }, { "cutoff", 2800 }, { "crush", 0.4f }, { "chorus", 0.4f }, { "chrate", 0.25f }, { "chdepth", 0.9f } });
    add ("Nylon Guitar", "Guitar", { { "sound", 7 } });
    add ("Bright Nylon", "Guitar", { { "sound", 7 }, { "high", 5 } });
    add ("Lofi Guitar", "Guitar", { { "sound", 7 }, { "cutoff", 2400 }, { "crush", 0.35f }, { "chorus", 0.3f } });
    add ("Soft Pad", "Pad", { { "sound", 0 } });
    add ("Wide Pad", "Pad", { { "sound", 0 }, { "chorus", 0.6f }, { "width", 1.0f } });
    add ("Strings", "Strings", { { "sound", 3 } });
    add ("Chamber Strings", "Strings", { { "sound", 3 }, { "cutoff", 3500 }, { "chorus", 0.3f } });
    add ("Glass Bell", "Bell", { { "sound", 5 } });
    add ("Music Box", "Bell", { { "sound", 5 }, { "octave", 1 }, { "cutoff", 6000 } });
    add ("Pluck", "Pluck", { { "sound", 2 } });
    add ("Harp Pluck", "Pluck", { { "sound", 2 }, { "octave", 1 }, { "chorus", 0.3f } });
    add ("Chip Keys", "Chip", { { "sound", 4 } });
    add ("Gameboy", "Chip", { { "sound", 4 }, { "crush", 0.6f }, { "cutoff", 7000 } });
    add ("Init", "Init", {});
    return e;
}

namespace
{
class KeysEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        synth.prepare (sampleRate);
        chorus.reset();
        volume.setTime (0.02f, sr);
        held.fill (-1);
    }

    void setParams (const float* v) override
    {
        for (int i = 0; i < kCount; ++i) p[(size_t) i] = v[i];
        const int s = juce::roundToInt (p[kSound]);
        if (s != sound) { synth.allOff(); sound = s; synth.setSound (s); }
        synth.setBendRange (p[kBend]);
    }

    void setTempo (double bpm) override { tempo = (float) bpm; }
    void setBend (float semis) override
    {
        const float range = juce::jmax (0.01f, p[kBend]);
        synth.handle (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, juce::roundToInt (8192.0f + 8191.0f * semis / range))));
    }

    void noteOn (int note, float vel, bool, bool accent) override
    {
        if (note < 0 || note > 127) return;
        const int n = juce::jlimit (0, 127, note + 12 * juce::roundToInt (p[kOctave]));
        if (held[(size_t) note] >= 0) synth.handle (juce::MidiMessage::noteOff (1, held[(size_t) note]));
        held[(size_t) note] = n;
        synth.handle (juce::MidiMessage::noteOn (1, n, juce::jlimit (0.01f, 1.0f, vel * (accent ? 1.2f : 1.0f))));
        tail = (int) (sr * 6.0f);
    }

    void noteOff (int note) override
    {
        if (note < 0 || note > 127 || held[(size_t) note] < 0) return;
        synth.handle (juce::MidiMessage::noteOff (1, held[(size_t) note]));
        held[(size_t) note] = -1;
    }

    void allOff() override { synth.allOff(); held.fill (-1); }

    void render (float* outL, float* outR, int n) override
    {
        if (tail <= 0) return;
        tail -= n;
        float l[256], r[256];
        for (int s0 = 0; s0 < n; s0 += 256)
        {
            const int m = juce::jmin (256, n - s0);
            std::fill (l, l + m, 0.0f);
            std::fill (r, r + m, 0.0f);
            synth.render (l, r, m);
            process (l, r, m);
            for (int i = 0; i < m; ++i) { outL[s0 + i] += l[i]; outR[s0 + i] += r[i]; }
        }
    }

private:
    void process (float* l, float* r, int m)
    {
        const bool filt = p[kCutoff] < 19500.0f || p[kReso] > 0.15f;
        if (filt) { fl.set (p[kCutoff], 0.6f + p[kReso] * 8.0f, sr); fr.set (p[kCutoff], 0.6f + p[kReso] * 8.0f, sr); }
        const double sync = syncBeats (juce::roundToInt (p[kTremSync]));
        const float tremHz = sync > 0.0 ? (float) (tempo / 60.0 / sync) : p[kTremRate];
        const float drive = 1.0f + p[kDrive] * 6.0f;
        const float crushBits = p[kCrush] > 0.001f ? std::exp2 (14.0f - 9.0f * p[kCrush]) : 0.0f;
        const int down = 1 + (int) (p[kCrush] * 4.0f);
        const float vol = dsp::dbToGain (p[kVolume]) * 1.4f;
        const float pan = p[kPan];
        const float gl = std::sqrt (0.5f * (1.0f - pan)) * 1.4142f, gr = std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
        for (int i = 0; i < m; ++i)
        {
            float a = l[i], b = r[i];
            if (filt) { a = fl.process (a).low; b = fr.process (b).low; }
            if (drive > 1.01f) { const float k = 1.0f / std::sqrt (drive); a = dsp::fastTanh (a * drive) * k; b = dsp::fastTanh (b * drive) * k; }
            if (crushBits > 0.0f)
            {
                if (count++ % down == 0) { hl = std::round (a * crushBits) / crushBits; hr = std::round (b * crushBits) / crushBits; }
                a = hl; b = hr;
            }
            if (p[kTrem] > 0.001f)
            {
                trPh += tremHz / sr; if (trPh >= 1.0f) trPh -= 1.0f;
                const float ta = 1.0f - p[kTrem] * (0.5f + 0.5f * std::sin (dsp::twoPi * trPh));
                const float tb = 1.0f - p[kTrem] * (0.5f + 0.5f * std::sin (dsp::twoPi * trPh + 1.2f));   // the two sides a bit apart
                a *= ta; b *= tb;
            }
            if (std::abs (p[kLow]) > 0.05f) { lowL += 0.02f * (a - lowL); lowR += 0.02f * (b - lowR); const float g = dsp::dbToGain (p[kLow]) - 1.0f; a += lowL * g; b += lowR * g; }
            if (std::abs (p[kHigh]) > 0.05f) { hiL += 0.25f * (a - hiL); hiR += 0.25f * (b - hiR); const float g = dsp::dbToGain (p[kHigh]) - 1.0f; a += (a - hiL) * g; b += (b - hiR) * g; }
            if (p[kChorus] > 0.001f) chorus.process (a, b, p[kChorus], p[kChorusRate], p[kChorusDepth], sr);
            const float mid = 0.5f * (a + b), side = 0.5f * (a - b) * p[kWidth];
            const float g = volume.process (vol);
            l[i] = (mid + side) * g * gl;
            r[i] = (mid - side) * g * gr;
        }
    }

    hub::KeysSynth synth;
    std::array<float, kCount> p {};
    std::array<int, 128> held {};
    int sound = -1, tail = 0, count = 0;
    float sr = 48000.0f, tempo = 120.0f, trPh = 0.0f, hl = 0.0f, hr = 0.0f, lowL = 0.0f, lowR = 0.0f, hiL = 0.0f, hiR = 0.0f;
    dsp::Svf fl, fr;
    dsp::Chorus chorus;
    dsp::Smooth volume;
};
} // namespace

std::unique_ptr<Engine> makeKeysEngine() { return std::make_unique<KeysEngine>(); }
} // namespace catbox
