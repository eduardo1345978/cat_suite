#include "GlideFX.h"

namespace glidecat
{
juce::StringArray ensembleModeNames() { return { "Off", "Chorus", "Strings" }; }
juce::StringArray roomTypeNames()     { return { "Room", "Hall", "Plate", "Cathedral", "Shimmer", "Bloom" }; }
juce::StringArray intervalNames()
{
    return { "-24", "-12", "-7", "-5", "0", "+3", "+4", "+5", "+7", "+12", "+19", "+24" };
}
float intervalSemis (int index)
{
    static const float v[] = { -24, -12, -7, -5, 0, 3, 4, 5, 7, 12, 19, 24 };
    return v[juce::jlimit (0, 11, index)];
}

//==============================================================================
void PitchShifter::prepare (double sr)
{
    buf.assign ((size_t) (sr * 0.2) + 4, 0.0f);
    window = (float) (sr * 0.07);   // 70 ms grains
    reset();
}

void PitchShifter::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    write = 0;
    phase = 0.0f;
}

float PitchShifter::process (float in, float semis) noexcept
{
    // Two read heads sweep through a short window at the pitch ratio, crossfaded (classic delay-line shifter)
    const int size = (int) buf.size();
    buf[(size_t) write] = in;
    const float ratio = std::pow (2.0f, semis / 12.0f);
    phase += (1.0f - ratio) / window;
    phase -= std::floor (phase);
    float out = 0.0f;
    for (int h = 0; h < 2; ++h)
    {
        const float ph = std::fmod (phase + 0.5f * (float) h, 1.0f);
        const float delay = ph * window + 2.0f;
        float rp = (float) write - delay;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const float s = buf[(size_t) (i0 % size)] * (1.0f - fr) + buf[(size_t) ((i0 + 1) % size)] * fr;
        out += s * std::sin (juce::MathConstants<float>::pi * ph);   // sine window: the two heads sum to constant power
    }
    write = (write + 1) % size;
    return out * 0.7071f;
}

//==============================================================================
void Effects::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    ensL.assign ((size_t) (sr * 0.05) + 4, 0.0f);
    ensR.assign (ensL.size(), 0.0f);
    prismBuf.assign ((size_t) (sr * 2.1) + 4, 0.0f);
    for (auto& s : shifters) s.prepare (sr);
    shimmer.prepare (sr);
    preDelay.assign ((size_t) (sr * 0.2) + 4, 0.0f);
    wetL.assign ((size_t) juce::jmax (512, maxBlock), 0.0f);
    wetR.assign (wetL.size(), 0.0f);
    reverb.setSampleRate (sr);
    reset();
}

void Effects::reset()
{
    std::fill (ensL.begin(), ensL.end(), 0.0f);
    std::fill (ensR.begin(), ensR.end(), 0.0f);
    std::fill (prismBuf.begin(), prismBuf.end(), 0.0f);
    std::fill (preDelay.begin(), preDelay.end(), 0.0f);
    for (auto& s : shifters) s.reset();
    shimmer.reset();
    reverb.reset();
    ensWrite = prismWrite = preWrite = 0;
    prismFbState = shimmerState = bloomEnv = 0.0f;
    lastRoom = -1;
}

void Effects::process (float* left, float* right, int numSamples, const FxSettings& fx)
{
    const float srf = (float) sr;

    // ---- Ensemble
    if (fx.ensembleMode > 0 && fx.ensembleMix > 0.0f)
    {
        const int size = (int) ensL.size();
        const bool strings = fx.ensembleMode == 2;
        const int taps = strings ? 3 : 2;
        const float base = (strings ? 0.008f : 0.012f) * srf;
        const float depthSlow = (strings ? 0.0025f : 0.003f) * srf * fx.ensembleDepth;
        const float depthFast = strings ? 0.0003f * srf * fx.ensembleDepth : 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            ensL[(size_t) ensWrite] = left[i];
            ensR[(size_t) ensWrite] = right[i];
            ensPhaseSlow += fx.ensembleRate / srf;
            ensPhaseFast += 6.0f / srf;
            if (ensPhaseSlow > 1.0f) ensPhaseSlow -= 1.0f;
            if (ensPhaseFast > 1.0f) ensPhaseFast -= 1.0f;
            float wl = 0.0f, wr = 0.0f;
            for (int t = 0; t < taps; ++t)
            {
                const float off = (float) t / (float) taps;
                for (int side = 0; side < 2; ++side)
                {
                    const float p = ensPhaseSlow + off + (side == 1 ? 0.25f : 0.0f);
                    const float d = base + depthSlow * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * p))
                                    + depthFast * std::sin (juce::MathConstants<float>::twoPi * (ensPhaseFast + off));
                    float rp = (float) ensWrite - d;
                    while (rp < 0.0f) rp += (float) size;
                    const int i0 = (int) rp;
                    const float fr = rp - (float) i0;
                    const auto& b = side == 0 ? ensL : ensR;
                    const float s = b[(size_t) (i0 % size)] * (1.0f - fr) + b[(size_t) ((i0 + 1) % size)] * fr;
                    (side == 0 ? wl : wr) += s / (float) taps;
                }
            }
            ensWrite = (ensWrite + 1) % size;
            left[i] = left[i] * (1.0f - 0.5f * fx.ensembleMix) + wl * fx.ensembleMix;
            right[i] = right[i] * (1.0f - 0.5f * fx.ensembleMix) + wr * fx.ensembleMix;
        }
    }

    // ---- Prism
    if (fx.prismMix > 0.0f)
    {
        const int size = (int) prismBuf.size();
        const int delay = juce::jlimit (1, size - 2, (int) (fx.prismTimeMs * 0.001f * srf));
        const float i1 = intervalSemis (fx.prismI1), i2 = intervalSemis (fx.prismI2), i3 = intervalSemis (fx.prismI3);
        for (int i = 0; i < numSamples; ++i)
        {
            int r = prismWrite - delay;
            if (r < 0) r += size;
            const float delayed = prismBuf[(size_t) r];
            const float a = shifters[0].process (delayed, i1);
            const float b = shifters[1].process (delayed, i2);
            const float c = shifters[2].process (delayed, i3);
            const float shifted = (a + b + c) * 0.45f;
            prismFbState += 0.3f * (shifted - prismFbState);      // darker repeats
            prismBuf[(size_t) prismWrite] = 0.5f * (left[i] + right[i]) + fx.prismFeedback * prismFbState;
            prismWrite = (prismWrite + 1) % size;
            left[i] += fx.prismMix * (a * 0.8f + c * 0.4f) * 0.6f;
            right[i] += fx.prismMix * (b * 0.8f + c * 0.4f) * 0.6f;
        }
    }

    // ---- Rooms
    if (fx.roomMix > 0.0f)
    {
        if (fx.roomType != lastRoom || std::abs (fx.roomSize - lastSize) > 0.01f)
        {
            lastRoom = fx.roomType;
            lastSize = fx.roomSize;
            static const float size[] = { 0.45f, 0.75f, 0.6f, 0.93f, 0.88f, 0.92f };
            static const float damp[] = { 0.5f, 0.35f, 0.15f, 0.2f, 0.25f, 0.45f };
            juce::Reverb::Parameters p;
            p.roomSize = juce::jlimit (0.05f, 0.98f, size[juce::jlimit (0, 5, fx.roomType)] * (0.6f + 0.6f * fx.roomSize));
            p.damping = damp[juce::jlimit (0, 5, fx.roomType)];
            p.wetLevel = 1.0f;
            p.dryLevel = 0.0f;
            p.width = 1.0f;
            reverb.setParameters (p);
        }
        const int n = juce::jmin (numSamples, (int) wetL.size());
        const int pre = (int) ((fx.roomType == 5 ? 0.12f : (fx.roomType == 3 ? 0.04f : 0.01f)) * srf);
        const int psize = (int) preDelay.size();
        for (int i = 0; i < n; ++i)
        {
            float in = 0.5f * (left[i] + right[i]);
            if (fx.roomType == 5)
            {
                // Bloom: the reverb input swells in slowly
                bloomEnv += (std::abs (in) - bloomEnv) * (1.0f / (0.6f * srf));
                in *= juce::jmin (1.0f, bloomEnv * 8.0f);
            }
            if (fx.roomType == 4)
                in += 0.35f * shimmerState;           // octave-up feedback into the reverb
            preDelay[(size_t) preWrite] = in;
            int r = preWrite - pre;
            if (r < 0) r += psize;
            wetL[(size_t) i] = wetR[(size_t) i] = preDelay[(size_t) r];
            preWrite = (preWrite + 1) % psize;
        }
        reverb.processStereo (wetL.data(), wetR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            if (fx.roomType == 4)
                shimmerState = shimmer.process (0.5f * (wetL[(size_t) i] + wetR[(size_t) i]), 12.0f);
            left[i] = left[i] * (1.0f - 0.4f * fx.roomMix) + wetL[(size_t) i] * fx.roomMix;
            right[i] = right[i] * (1.0f - 0.4f * fx.roomMix) + wetR[(size_t) i] * fx.roomMix;
        }
    }
}

} // namespace glidecat
