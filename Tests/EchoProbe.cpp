// Echo-Cat sound probe: every preset with a 440 Hz tone burst (-6 dBFS), and a level check with a loud
// input: how much of the output is not the tone (noise, distortion, grain / click artefacts) and how
// often the safety limiter has to hold the level down.
#include "PluginProcessor.h"
#include <cstdio>
#include <complex>

static void fftInPlace (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; }
        }
    }
}
using namespace juce;

int main()
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int block = 480, len = (int) (sr * 6.0);
    EchoCatProcessor p;
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);
    const int N = 1 << 15;
    std::printf ("%-22s %8s %8s %8s %8s\n", "preset", "peak", "over.85", "junk%", "clicks");
    for (int prog = 0; prog < p.getNumPrograms(); ++prog)
    {
        p.setCurrentProgram (prog);
        p.releaseResources(); p.prepareToPlay (sr, block);
        AudioBuffer<float> b (2, len);
        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr;
            const double ph = std::fmod (t, 1.0);
            const float env = ph < 0.2 ? (float) std::min (1.0, std::min (ph / 0.005, (0.2 - ph) / 0.005)) : 0.0f;
            const float v = 0.5f * env * (float) std::sin (2.0 * MathConstants<double>::pi * 440.0 * t);
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        MidiBuffer m;
        for (int pos = 0; pos < len; pos += block)
        {
            AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, pos, std::min (block, len - pos));
            p.processBlock (v, m);
        }
        float peak = 0; int over = 0, clicks = 0;
        for (int i = 2; i < len; ++i)
        {
            const float x = b.getSample (0, i);
            peak = std::max (peak, std::abs (x));
            if (std::abs (x) > 0.85f) ++over;
            const float d2 = b.getSample (0, i) - 2 * b.getSample (0, i - 1) + b.getSample (0, i - 2);
            if (std::abs (d2) > 0.08f) ++clicks;          // a 440 Hz sine at 0.5 has d2 ~ 0.0021
        }
        // spectrum of the last N samples: energy far from 440 Hz and its harmonics / octaves
        std::vector<std::complex<double>> w ((size_t) N);
        for (int i = 0; i < N; ++i) w[(size_t) i] = b.getSample (0, len - N + i) * (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / N));
        fftInPlace (w);
        double tone = 0, junk = 0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double f = k * sr / N, e = std::norm (w[(size_t) k]);
            const double h = f / 220.0;                     // 220 Hz multiples (octave-down shimmer etc. too)
            if (std::abs (h - std::round (h)) * 220.0 < 12.0 && f < 4000.0) tone += e; else junk += e;
        }
        std::printf ("%-22s %8.3f %8d %8.2f %8d\n", p.getProgramName (prog).toRawUTF8(), peak, over, 100.0 * junk / std::max (1e-12, tone + junk), clicks);
    }
    return 0;
}
