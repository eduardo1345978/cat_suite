#include "CryptDsp.h"

namespace tomb
{
juce::StringArray waveNames()
{
    return { "Pulse 12%", "Pulse 25%", "Square", "Triangle", "Organ", "Bells", "Ghost", "Noise", "Metal" };
}

namespace
{
Table makeTable (std::initializer_list<std::pair<int, float>> harmonics, float bend = 0.0f)
{
    std::array<float, 32> v {};
    for (int i = 0; i < 32; ++i)
    {
        const float x = (float) i / 32.0f * juce::MathConstants<float>::twoPi;
        for (auto [h, a] : harmonics) v[(size_t) i] += a * std::sin ((float) h * x + bend * (float) h);
    }
    float lo = 1.0e9f, hi = -1.0e9f;
    for (float x : v) { lo = juce::jmin (lo, x); hi = juce::jmax (hi, x); }
    Table t {};
    for (int i = 0; i < 32; ++i) t[(size_t) i] = (uint8_t) juce::jlimit (0, 15, juce::roundToInt ((v[(size_t) i] - lo) / (hi - lo) * 15.0f));
    return t;
}
}

const Table& tableFor (Wave w)
{
    static const Table organ = makeTable ({ { 1, 1.0f }, { 2, 0.5f }, { 4, 0.35f }, { 8, 0.2f } });
    static const Table bells = makeTable ({ { 1, 1.0f }, { 3, 0.0f }, { 5, 0.7f }, { 7, 0.45f }, { 11, 0.3f } }, 0.4f);
    static const Table ghost = makeTable ({ { 1, 1.0f }, { 2, 0.15f }, { 6, 0.35f }, { 7, 0.3f } }, 1.1f);
    static const Table tri = []
    {
        Table t {};
        for (int i = 0; i < 32; ++i) t[(size_t) i] = (uint8_t) (i < 16 ? 15 - i : i - 16);     // the NES triangle: 15 .. 0 .. 15
        return t;
    }();
    switch (w)
    {
        case Wave::Organ: return organ;
        case Wave::Bells: return bells;
        case Wave::Ghost: return ghost;
        default:          return tri;
    }
}

float ChipOsc::tick (Wave w, double hz, double sampleRate, float duty) noexcept
{
    const double step = juce::jlimit (0.0, 0.5, hz / sampleRate);
    const double before = phase;
    phase += step;
    if (phase >= 1.0) phase -= 1.0;
    switch (w)
    {
        case Wave::Pulse12: return phase < (duty > 0.0f ? duty : 0.125f) ? 1.0f : -1.0f;
        case Wave::Pulse25: return phase < (duty > 0.0f ? duty : 0.25f) ? 1.0f : -1.0f;
        case Wave::Square:  return phase < (duty > 0.0f ? duty : 0.5f) ? 1.0f : -1.0f;
        case Wave::Triangle: case Wave::Organ: case Wave::Bells: case Wave::Ghost:
            return (float) tableFor (w)[(size_t) juce::jlimit (0, 31, (int) (phase * 32.0))] / 7.5f - 1.0f;
        case Wave::Noise: case Wave::Metal:
        {
            // the register is clocked 16 times a period, so the note still sets the "pitch" of the noise
            lfsr.metal = w == Wave::Metal;
            const int a = (int) (before * 16.0), b = (int) ((before + step) * 16.0);
            for (int k = a; k < b; ++k) noiseValue = lfsr.clock();
            return noiseValue;
        }
    }
    return 0.0f;
}

void Crusher::process (float* l, float* r, int n, float bits, float rateHz) noexcept
{
    const bool clean = bits >= 15.9f && rateHz >= sr * 0.99;
    if (clean) return;
    const float levels = std::exp2 (juce::jlimit (2.0f, 16.0f, bits) - 1.0f);
    const double step = juce::jlimit (0.0, 1.0, rateHz / sr);
    for (int i = 0; i < n; ++i)
    {
        phase += step;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            holdL = std::round (juce::jlimit (-1.0f, 1.0f, l[i]) * levels) / levels;
            holdR = std::round (juce::jlimit (-1.0f, 1.0f, r[i]) * levels) / levels;
        }
        l[i] = holdL;
        r[i] = holdR;
    }
}

float gloomHz (float gloom) noexcept { return 18000.0f * std::pow (700.0f / 18000.0f, juce::jlimit (0.0f, 1.0f, gloom)); }

void Space::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::Reverb::Parameters p;
    p.roomSize = 0.88f; p.damping = 0.62f; p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f;
    reverb.setParameters (p);
    reverb.setSampleRate (sr);
    dl.assign ((size_t) (sr * 1.6) + 4, 0.0f);
    dr.assign ((size_t) (sr * 1.6) + 4, 0.0f);
    wl.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    wr.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    for (auto* s : { &gloomS, &roomS, &echoS, &driveS }) s->reset (sr, 0.05);
    reset();
}

void Space::reset()
{
    reverb.reset();
    std::fill (dl.begin(), dl.end(), 0.0f);
    std::fill (dr.begin(), dr.end(), 0.0f);
    pos = 0;
    lpL = lpR = lp2L = lp2R = fbL = fbR = 0.0f;
}

void Space::process (float* l, float* r, int n, float gloom, float drive, float room, float echo, double bpm) noexcept
{
    gloomS.setTargetValue (gloom);
    roomS.setTargetValue (room);
    echoS.setTargetValue (echo);
    driveS.setTargetValue (drive);
    const int len = (int) dl.size();
    const int delay = juce::jlimit (1, len - 2, (int) (60.0 / juce::jlimit (40.0, 300.0, bpm) * 0.75 * sr));   // a dotted 1/8
    for (int start = 0; start < n; start += (int) wl.size())
    {
        const int m = juce::jmin ((int) wl.size(), n - start);
        float* L = l + start;
        float* R = r + start;
        // drive and gloom first: the dry sound gets dark, the room hears it dark too
        for (int i = 0; i < m; ++i)
        {
            const float d = driveS.getNextValue();
            if (d > 0.001f)
            {
                const float g = 1.0f + 9.0f * d;
                L[i] = std::tanh (L[i] * g) / std::tanh (g) ;
                R[i] = std::tanh (R[i] * g) / std::tanh (g);
            }
            const float c = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * gloomHz (gloomS.getNextValue()) / (float) sr);
            lpL += (L[i] - lpL) * c; lp2L += (lpL - lp2L) * c;
            lpR += (R[i] - lpR) * c; lp2R += (lpR - lp2R) * c;
            L[i] = lp2L; R[i] = lp2R;
        }
        std::copy (L, L + m, wl.data());
        std::copy (R, R + m, wr.data());
        reverb.processStereo (wl.data(), wr.data(), m);
        for (int i = 0; i < m; ++i)
        {
            const float rm = roomS.getNextValue(), ec = echoS.getNextValue();
            const int rd = (pos - delay + len) % len;
            const float el = dl[(size_t) rd], er = dr[(size_t) rd];
            // the echo gets darker every time it comes back (cross-fed: it bounces from wall to wall)
            fbL += ((L[i] + er * 0.5f) - fbL) * 0.35f;
            fbR += ((R[i] + el * 0.5f) - fbR) * 0.35f;
            dl[(size_t) pos] = fbL;
            dr[(size_t) pos] = fbR;
            pos = (pos + 1) % len;
            L[i] = L[i] * (1.0f - 0.3f * rm) + wl[(size_t) i] * 0.6f * rm + el * 0.55f * ec;
            R[i] = R[i] * (1.0f - 0.3f * rm) + wr[(size_t) i] * 0.6f * rm + er * 0.55f * ec;
        }
    }
}
} // namespace tomb
