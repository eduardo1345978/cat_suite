#include "BendCatEngine.h"
#include <algorithm>
#include <cmath>

namespace bendcat
{
namespace
{
    constexpr double twoPi = 6.283185307179586;

    /** Deterministic -1..1 random value per (seed, index) for Random / S&H waves. */
    float hashRandom (uint32_t seed, int64_t index)
    {
        uint64_t x = (uint64_t) seed * 0x9E3779B97F4A7C15ull + (uint64_t) index * 0xBF58476D1CE4E5B9ull;
        x ^= x >> 31; x *= 0x94D049BB133111EBull; x ^= x >> 29;
        return (float) ((double) (x & 0xFFFFFF) / (double) 0xFFFFFF) * 2.0f - 1.0f;
    }

    float smoothstep (float t) { t = juce::jlimit (0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }

    BendEnvelope fromFunction (const std::function<float (float)>& f, int numPoints)
    {
        BendEnvelope e;
        e.points.clear();
        for (int i = 0; i < numPoints; ++i)
        {
            const float t = (float) i / (float) (numPoints - 1);
            e.points.push_back ({ t, juce::jlimit (-1.0f, 1.0f, f (t)) });
        }
        e.curves.assign ((size_t) numPoints - 1, 0.0f);
        return e;
    }

    BendEnvelope fromPoints (std::vector<std::pair<float, float>> pts, std::vector<float> curves = {})
    {
        BendEnvelope e;
        e.points = std::move (pts);
        e.curves = std::move (curves);
        e.curves.resize (e.points.size() - 1, 0.0f);
        return e;
    }
}

juce::StringArray getModeNames()       { return { "Off", "Glide", "Envelope", "LFO", "Pattern" }; }
juce::StringArray getGlideCurveNames() { return { "Linear", "Exponential", "S-Curve" }; }
juce::StringArray getLfoWaveNames()    { return { "Sine", "Triangle", "Saw", "Random", "Sample & Hold" }; }
juce::StringArray getDivisionNames()   { return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4D", "1/8D" }; }

double divisionToPpq (int index)
{
    static const double values[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
    return values[juce::jlimit (0, 10, index)];
}

juce::StringArray getEnvelopeLengthNames()
{
    return { "Free (ms)", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" };
}

double envelopeLengthToPpq (int index)
{
    static const double values[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    return values[juce::jlimit (0, 7, index)];
}

//==============================================================================
void BendEnvelope::setDefaultADSR()
{
    // 5 nodes: attack up, decay, sustain, release back to pitch
    points = { { 0.0f, 0.0f }, { 0.1f, 1.0f }, { 0.3f, 0.5f }, { 0.8f, 0.5f }, { 1.0f, 0.0f } };
    curves = { -0.3f, 0.3f, 0.0f, 0.3f };
}

void BendEnvelope::sortAndClamp()
{
    if (points.size() < 2)
        setDefaultADSR();

    std::stable_sort (points.begin(), points.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    for (auto& [t, v] : points)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        v = juce::jlimit (-1.0f, 1.0f, v);
    }
    points.front().first = 0.0f;
    points.back().first = 1.0f;
    curves.resize (points.size() - 1, 0.0f);
    for (auto& c : curves) c = juce::jlimit (-1.0f, 1.0f, c);
}

float BendEnvelope::evaluate (float t) const
{
    if (points.empty()) return 0.0f;
    if (t <= points.front().first) return points.front().second;
    if (t >= points.back().first)  return points.back().second;

    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto& a = points[i];
        const auto& b = points[i + 1];
        if (t < b.first)
        {
            const float span = b.first - a.first;
            if (span <= 1.0e-6f) return b.second;
            float p = (t - a.first) / span;
            const float c = i < curves.size() ? curves[i] : 0.0f;
            if (c > 0.0f)      p = std::pow (p, 1.0f + c * 4.0f);                // slow start
            else if (c < 0.0f) p = 1.0f - std::pow (1.0f - p, 1.0f - c * 4.0f);  // fast start
            return a.second + (b.second - a.second) * p;
        }
    }
    return points.back().second;
}

juce::ValueTree BendEnvelope::toValueTree() const
{
    juce::ValueTree t ("ENVELOPE");
    t.setProperty ("duration", durationMs, nullptr);
    t.setProperty ("loop", loop, nullptr);
    t.setProperty ("sync", syncDivision, nullptr);
    for (size_t i = 0; i < points.size(); ++i)
    {
        juce::ValueTree p ("POINT");
        p.setProperty ("t", points[i].first, nullptr);
        p.setProperty ("v", points[i].second, nullptr);
        p.setProperty ("c", i < curves.size() ? curves[i] : 0.0f, nullptr);
        t.appendChild (p, nullptr);
    }
    return t;
}

BendEnvelope BendEnvelope::fromValueTree (const juce::ValueTree& t)
{
    BendEnvelope e;
    if (! t.isValid()) return e;
    e.durationMs = (float) (double) t.getProperty ("duration", 250.0);
    e.loop = t.getProperty ("loop", false);
    e.syncDivision = t.getProperty ("sync", 0);
    e.points.clear();
    e.curves.clear();
    for (const auto& p : t)
    {
        e.points.push_back ({ (float) (double) p.getProperty ("t", 0.0), (float) (double) p.getProperty ("v", 0.0) });
        e.curves.push_back ((float) (double) p.getProperty ("c", 0.0));
    }
    if (! e.curves.empty()) e.curves.pop_back();
    e.sortAndClamp();
    return e;
}

juce::StringArray getShapeNames()
{
    return { "Flat", "Ramp Up", "Curve Up",
             "Rise & Hold", "Ramp Down", "Curve Down",
             "Scoop", "Saw Up", "Saw Down",
             "Sine", "Bump", "S-Curve",
             "Small Wave", "Wave x2", "Wave x4",
             "Square", "Square x2", "Pulse",
             "Ripple", "Step Down", "Step Up",
             "Wobble Decay", "Vibrato", "Vibrato Swell",
             "Zigzag", "Spike", "Stutter" };
}

BendEnvelope makeShape (int index)
{
    const float pi = juce::MathConstants<float>::pi;
    switch (index)
    {
        case 0:  return fromPoints ({ { 0, 0 }, { 1, 0 } });
        case 1:  return fromPoints ({ { 0, 0 }, { 1, 1 } });
        case 2:  return fromPoints ({ { 0, 0 }, { 1, 1 } }, { -0.7f });
        case 3:  return fromPoints ({ { 0, 0 }, { 0.25f, 1 }, { 1, 1 } }, { -0.3f, 0.0f });
        case 4:  return fromPoints ({ { 0, 1 }, { 1, 0 } });
        case 5:  return fromPoints ({ { 0, 1 }, { 1, 0 } }, { -0.7f });
        case 6:  return fromPoints ({ { 0, -1 }, { 0.25f, 0 }, { 1, 0 } }, { -0.5f, 0.0f });
        case 7:  return fromPoints ({ { 0, -1 }, { 0.5f, 1 }, { 0.5f, -1 }, { 1, 1 } });
        case 8:  return fromPoints ({ { 0, 1 }, { 0.5f, -1 }, { 0.5f, 1 }, { 1, -1 } });
        case 9:  return fromFunction ([pi] (float t) { return std::sin (2 * pi * t); }, 17);
        case 10: return fromFunction ([pi] (float t) { return std::sin (pi * t); }, 13);
        case 11: return fromFunction ([] (float t) { return smoothstep (t) * 2.0f - 1.0f; }, 9);
        case 12: return fromFunction ([pi] (float t) { return 0.3f * std::sin (2 * pi * t); }, 17);
        case 13: return fromFunction ([pi] (float t) { return std::sin (4 * pi * t); }, 25);
        case 14: return fromFunction ([pi] (float t) { return 0.6f * std::sin (8 * pi * t); }, 41);
        case 15: return fromPoints ({ { 0, 1 }, { 0.5f, 1 }, { 0.5f, -1 }, { 1, -1 } });
        case 16: return fromPoints ({ { 0, 1 }, { 0.25f, 1 }, { 0.25f, -1 }, { 0.5f, -1 }, { 0.5f, 1 }, { 0.75f, 1 }, { 0.75f, -1 }, { 1, -1 } });
        case 17: return fromPoints ({ { 0, 1 }, { 0.2f, 1 }, { 0.2f, 0 }, { 1, 0 } });
        case 18:
        {
            std::vector<std::pair<float, float>> pts;
            for (int i = 0; i < 8; ++i)
            {
                const float v = i % 2 == 0 ? 0.5f : -0.5f;
                pts.push_back ({ i / 8.0f, v });
                pts.push_back ({ (i + 1) / 8.0f, v });
            }
            return fromPoints (pts);
        }
        case 19: return fromPoints ({ { 0, 1 }, { 0.25f, 1 }, { 0.25f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0 }, { 0.75f, 0 }, { 0.75f, -0.5f }, { 1, -0.5f } });
        case 20: return fromPoints ({ { 0, -0.5f }, { 0.25f, -0.5f }, { 0.25f, 0 }, { 0.5f, 0 }, { 0.5f, 0.5f }, { 0.75f, 0.5f }, { 0.75f, 1 }, { 1, 1 } });
        case 21: return fromFunction ([pi] (float t) { return std::sin (6 * pi * t) * (1.0f - t); }, 31);
        case 22: return fromFunction ([pi] (float t) { return 0.15f * std::sin (12 * pi * t); }, 49);
        case 23: return fromFunction ([pi] (float t) { return t * 0.3f * std::sin (12 * pi * t); }, 49);
        case 24: return fromPoints ({ { 0, 0 }, { 0.15f, 0.8f }, { 0.3f, -0.5f }, { 0.45f, 0.6f }, { 0.6f, -0.3f }, { 0.8f, 0.3f }, { 1, 0 } });
        case 25: return fromPoints ({ { 0, 0 }, { 0.08f, 1 }, { 0.3f, 0 }, { 1, 0 } }, { -0.4f, 0.5f, 0.0f });
        case 26:
        {
            static const float values[] = { 0.6f, -0.2f, 0.9f, -0.7f, 0.1f, 0.4f, -0.9f, 0.3f, -0.4f, 0.8f, -0.1f, 0.0f };
            std::vector<std::pair<float, float>> pts;
            for (int i = 0; i < 12; ++i)
            {
                pts.push_back ({ i / 12.0f, values[i] });
                pts.push_back ({ (i + 1) / 12.0f, values[i] });
            }
            return fromPoints (pts);
        }
        default: break;
    }
    return {};
}

//==============================================================================
float BendCatEngine::shapeGlide (float p, int curve)
{
    p = juce::jlimit (0.0f, 1.0f, p);
    switch (curve)
    {
        case CurveExponential: return (1.0f - std::exp (-5.0f * p)) / (1.0f - std::exp (-5.0f));
        case CurveSCurve:      return smoothstep (p);
        case CurveLinear:
        default:               return p;
    }
}

float BendCatEngine::lfoValue (int wave, double phase, uint32_t seed)
{
    const double cycle = std::floor (phase);
    const float frac = (float) (phase - cycle);

    switch (wave)
    {
        case WaveTriangle:   return frac < 0.5f ? frac * 4.0f - 1.0f : 3.0f - frac * 4.0f;
        case WaveSaw:        return frac * 2.0f - 1.0f;
        case WaveRandom:
        {
            const float a = hashRandom (seed, (int64_t) cycle), b = hashRandom (seed, (int64_t) cycle + 1);
            return a + (b - a) * smoothstep (frac);
        }
        case WaveSampleHold: return hashRandom (seed, (int64_t) cycle);
        case WaveSine:
        default:             return (float) std::sin (twoPi * phase);
    }
}

int BendCatEngine::bendToPitchWheel (float semitones, float range)
{
    range = juce::jmax (0.01f, range);
    return juce::jlimit (0, 16383, juce::roundToInt (8192.0f + semitones / range * 8192.0f));
}

void BendCatEngine::initVoice (BendVoice& v, int inChannel, int inNote, int outChannel, int64_t nowSample, double nowPpq,
                               float glideFromSemis, juce::Random& rng) const
{
    v = {};
    v.active = true;
    v.inChannel = inChannel;
    v.inNote = inNote;
    v.outChannel = outChannel;
    v.startSample = nowSample;
    v.startPpq = nowPpq;
    v.glideFromSemis = juce::jlimit (-rangeSemitones, rangeSemitones, glideFromSemis);
    v.detuneCents = (rng.nextFloat() * 2.0f - 1.0f) * std::abs (humanDetuneCents);
    v.lfoPhaseOffset = lfoPhaseRandom ? rng.nextFloat() : 0.0f;
    v.lfoDepthMul = 1.0f - rng.nextFloat() * lfoVoiceRandom;
    v.envDepthMul = 1.0f - rng.nextFloat() * envRandom;
    v.envEnabled = rng.nextFloat() <= envChance;
    v.driftPhase = rng.nextFloat() * (float) twoPi;
    v.seed = (uint32_t) rng.nextInt() | 1u;
}

float BendCatEngine::computeBend (BendVoice& v, int64_t now, double ppq, double bpm, double sr, bool hostRunning,
                                  const std::array<float, 128>& tuningCents) const
{
    const double elapsedSec = juce::jmax (0.0, (double) (now - v.startSample) / sr);
    const double elapsedPpq = elapsedSec * bpm / 60.0;
    float semis = 0.0f;

    switch (mode)
    {
        case ModeGlide:
        {
            if (glideTimeMs > 0.0f && std::abs (v.glideFromSemis) > 1.0e-4f)
            {
                const float p = (float) (elapsedSec * 1000.0 / glideTimeMs);
                semis = v.glideFromSemis * (1.0f - shapeGlide (p, glideCurve));
            }
            break;
        }

        case ModeEnvelope:
        {
            if (! v.envEnabled) break;
            const double lengthPpq = envelopeLengthToPpq (envelope.syncDivision);
            double phase;

            if (envPlayMode == PlayGlobal)
            {
                if (lengthPpq > 0.0 && hostRunning) phase = ppq / lengthPpq;
                else phase = (double) now / sr * 1000.0 / juce::jmax (1.0f, envelope.durationMs);
                phase -= std::floor (phase); // global mode always cycles
            }
            else
            {
                phase = lengthPpq > 0.0 ? elapsedPpq / lengthPpq : elapsedSec * 1000.0 / juce::jmax (1.0f, envelope.durationMs);
                if (envelope.loop) phase -= std::floor (phase);
                else phase = juce::jmin (1.0, phase);
            }

            const int cycles = juce::jlimit (1, 32, envCycles);
            double local = phase * cycles;
            local = phase >= 1.0 ? 1.0 : local - std::floor (local);
            v.envPhase = (float) phase;
            semis = envelope.evaluate ((float) local) * envDepth * v.envDepthMul * bendAmountSemitones;
            break;
        }

        case ModeLFO:
        {
            double phase;
            if (lfoSync && hostRunning) phase = ppq / divisionToPpq (lfoDivision);
            else if (lfoSync)           phase = elapsedPpq / divisionToPpq (lfoDivision);
            else                        phase = elapsedSec * lfoRateHz;
            phase += v.lfoPhaseOffset;
            semis = lfoValue (lfoWave, phase, v.seed) * lfoDepth * v.lfoDepthMul * bendAmountSemitones;
            break;
        }

        case ModePattern:
        {
            const double stepLen = divisionToPpq (patternDivision);
            const double pos = (hostRunning ? ppq : elapsedPpq) / stepLen;
            const double stepFloor = std::floor (pos);
            const int idx = (int) (((int64_t) stepFloor % 16 + 16) % 16);
            const float frac = (float) (pos - stepFloor);
            float cents = patternCents[(size_t) idx];
            if (patternSmoothing > 0.0f && frac > 1.0f - patternSmoothing)
            {
                const float t = (frac - (1.0f - patternSmoothing)) / patternSmoothing;
                cents += (patternCents[(size_t) ((idx + 1) % 16)] - cents) * smoothstep (t);
            }
            semis = cents / 100.0f;
            break;
        }

        case ModeOff:
        case NumModes:
        default:
            break;
    }

    semis += v.detuneCents / 100.0f;
    if (driftCents > 0.0f)
        semis += driftCents / 100.0f * (float) std::sin (twoPi * driftRateHz * ((double) now / sr) + v.driftPhase);
    if (scalaEnabled)
        semis += tuningCents[(size_t) juce::jlimit (0, 127, v.inNote)] / 100.0f;

    semis = juce::jlimit (-rangeSemitones, rangeSemitones, semis);
    v.currentBend = semis;
    return semis;
}

//==============================================================================
juce::ValueTree BendCatEngine::toValueTree() const
{
    juce::ValueTree t (treeId);
    t.appendChild (envelope.toValueTree(), nullptr);
    juce::StringArray pat;
    for (float c : patternCents) pat.add (juce::String (c, 2));
    t.setProperty ("pattern", pat.joinIntoString (","), nullptr);
    return t;
}

void BendCatEngine::fromValueTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    envelope = BendEnvelope::fromValueTree (t.getChildWithName ("ENVELOPE"));
    const auto pat = juce::StringArray::fromTokens (t.getProperty ("pattern", "").toString(), ",", "");
    for (int i = 0; i < 16; ++i)
        patternCents[(size_t) i] = i < pat.size() ? juce::jlimit (-50.0f, 50.0f, pat[i].getFloatValue()) : 0.0f;
}

} // namespace bendcat
