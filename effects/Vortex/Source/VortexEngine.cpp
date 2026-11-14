#include "VortexEngine.h"
#include <cmath>

using namespace juce;

namespace vortex
{
StringArray modeNames() { return { "Pitch", "Filter", "Both" }; }
StringArray syncNames() { return { "Free", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }; }

double syncBars (int index) noexcept
{
    static constexpr double bars[] { 0.0, 1.0, 2.0, 4.0, 8.0, 16.0 };
    return bars[jlimit (0, 5, index)];
}

double spinRate (float spin) noexcept
{
    const float a = std::abs (spin);
    if (a < 0.02f) return 0.0;
    const double r = 0.02 * std::pow (60.0, (double) jmin (1.0f, a));   // 0.02 .. 1.2 octaves a second
    return spin > 0.0f ? r : -r;
}

static inline double wrap (double x, double span) noexcept
{
    x = std::fmod (x, span);
    return x < 0.0 ? x + span : x;
}

float window (double t, float power) noexcept
{
    if (t <= 0.0 || t >= 1.0) return 0.0f;
    const double s = std::sin (MathConstants<double>::pi * t);
    return (float) std::pow (s * s, (double) power);
}

float pitchPower (float focus) noexcept  { return 0.6f + 5.0f * focus * focus; }
float filterPower (float focus) noexcept { return 0.5f + 3.5f * focus; }

double layerOctave (int i, double position) noexcept
{
    return pitchLow + wrap ((double) i * (pitchSpan / pitchLayers) + position, pitchSpan);
}

double bandHz (int j, double position) noexcept
{
    return filterLowHz * std::exp2 (wrap ((double) j + position, (double) filterBands));
}

//==============================================================================
void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    winSamples = 0.05 * sr;
    minDelay = 8.0;
    const int need = (int) (minDelay + 1.4 * winSamples) + 16;
    int size = 1;
    while (size < need) size <<= 1;
    mask = size - 1;
    for (auto& r : ring) r.assign ((size_t) size, 0.0f);

    int fbSize = 1;
    while (fbSize < (int) (0.7 * sr) + 8) fbSize <<= 1;
    fbMask = fbSize - 1;
    for (auto& r : fbRing) r.assign ((size_t) fbSize, 0.0f);

    for (auto* s : { &mixS, &outS, &genS, &fbS, &widthS, &focusS }) s->reset (sr, 0.05);
    reset();
}

void Engine::reset()
{
    for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
    for (auto& r : fbRing) std::fill (r.begin(), r.end(), 0.0f);
    writePos = fbPos = 0;
    for (int i = 0; i < pitchLayers; ++i) phase[(size_t) i] = (double) i / pitchLayers;
    for (auto& ch : svf) for (auto& f : ch) { f.ic1 = f.ic2 = 0.0f; }
    genPhase.fill (0.0);
    fbHp.fill (0.0f); lp.fill (0.0f); hpState.fill (0.0f);
    inPow = wetPow = 0.0;
    wetGain = 1.0f;
    filterCountdown = 0;
    first = true;
}

float Engine::readRing (int ch, double delay) const noexcept
{
    const auto& r = ring[(size_t) ch];
    const double rp = (double) writePos - delay;
    const double fl = std::floor (rp);
    const float t = (float) (rp - fl);
    const int i = (int) fl;
    const float y0 = r[(size_t) ((i - 1) & mask)], y1 = r[(size_t) (i & mask)], y2 = r[(size_t) ((i + 1) & mask)], y3 = r[(size_t) ((i + 2) & mask)];
    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void Engine::updateFilters (int ch, double position, float k) noexcept
{
    const float pw = filterPower (focusS.getCurrentValue());
    float sum = 0.0f;
    auto& bank = svf[(size_t) ch];
    for (int j = 0; j < filterBands; ++j)
    {
        const double t = wrap ((double) j + position, (double) filterBands) / filterBands;
        const double hz = jmin (filterLowHz * std::exp2 (t * filterBands), 0.45 * sr);
        const float w = window (t, pw);
        const float g = (float) std::tan (MathConstants<double>::pi * hz / sr);
        auto& f = bank[(size_t) j];
        f.a1 = 1.0f / (1.0f + g * (g + k));
        f.a2 = g * f.a1;
        f.a3 = g * f.a2;
        f.w = w;
        sum += w * w;
    }
    const float norm = 1.0f / std::sqrt (jmax (1.0e-6f, sum));
    for (auto& f : bank) f.w *= jmin (norm, 4.0f);
}

void Engine::process (AudioBuffer<float>& buffer, const Settings& s)
{
    const int n = buffer.getNumSamples(), chans = jmin (2, buffer.getNumChannels());
    if (n <= 0 || chans <= 0 || ring[0].empty()) return;

    mixS.setTargetValue (jlimit (0.0f, 1.0f, s.mix));
    outS.setTargetValue (Decibels::decibelsToGain (s.outDb));
    genS.setTargetValue (jlimit (0.0f, 1.0f, s.gen));
    fbS.setTargetValue (jlimit (0.0f, 0.95f, s.feedback));
    widthS.setTargetValue (chans > 1 ? jlimit (0.0f, 1.0f, s.width) : 0.0f);
    focusS.setTargetValue (jlimit (0.0f, 1.0f, s.focus));

    // ---- where the spiral is ----
    const double bars = syncBars (s.sync);
    double target = spinRate (s.spin);
    if (bars > 0.0)
    {
        const double beatsPerOctave = bars * jmax (1.0, s.barBeats);
        const double dir = s.spin < 0.0f ? -1.0 : 1.0;
        target = dir * jmax (20.0, s.bpm) / 60.0 / beatsPerOctave;
        if (s.playing)
        {
            double diff = dir * s.ppq / beatsPerOctave - pos;
            diff -= std::round (diff);                                  // everything repeats every octave
            pos += first ? diff : diff * jmin (1.0, (double) n / (0.1 * sr));
        }
    }
    first = false;
    rate += (target - rate) * (1.0 - std::exp (-(double) n / (0.08 * sr)));
    const double step = rate / sr;

    const auto mode = s.mode;
    const bool doPitch = mode != Mode::filter, doFilter = mode != Mode::pitch;
    const float k = 1.0f / (0.7f + 8.0f * jlimit (0.0f, 1.0f, s.focus));
    const double fbDelay = jlimit (0.04 * sr, 0.6 * sr, 30.0 / jmax (20.0, s.bpm) * sr);   // an 1/8 note
    const float fbHpA = 1.0f - std::exp (-MathConstants<float>::twoPi * 60.0f / (float) sr);
    const float powA = 1.0f - std::exp (-1.0f / (0.3f * (float) sr));
    const float gainA = 1.0f - std::exp (-1.0f / (0.05f * (float) sr));

    // tone: below 0 a low-pass closes (20 kHz .. 600 Hz), above 0 a high-pass opens (20 .. 640 Hz)
    const float tone = jlimit (-1.0f, 1.0f, s.tone);
    const float lpHz = tone < 0.0f ? 20000.0f * std::exp2 (tone * 5.0f) : 20000.0f;
    const float hpHz = tone > 0.0f ? 20.0f * std::exp2 (tone * 5.0f) : 0.0f;
    const float lpA = 1.0f - std::exp (-MathConstants<float>::twoPi * jmin (lpHz, 0.45f * (float) sr) / (float) sr);
    const float hpA = 1.0f - std::exp (-MathConstants<float>::twoPi * hpHz / (float) sr);
    const bool useLp = tone < -0.001f, useHp = tone > 0.001f;

    float peakIn = 0.0f, peakOut = 0.0f;
    std::array<float, pitchLayers> w {};
    std::array<double, pitchLayers> ratio {};

    for (int i = 0; i < n; ++i)
    {
        pos += step;
        const float mix = mixS.getNextValue(), outG = outS.getNextValue(), gen = genS.getNextValue(),
                    fb = fbS.getNextValue() * 0.85f, width = widthS.getNextValue();
        focusS.getNextValue();

        float x[2], src[2], wet[2] { 0.0f, 0.0f };
        for (int ch = 0; ch < chans; ++ch)
        {
            x[ch] = buffer.getSample (ch, i);
            peakIn = jmax (peakIn, std::abs (x[ch]));
            const float back = fbRing[(size_t) ch][(size_t) ((fbPos - (int) fbDelay) & fbMask)];
            src[ch] = x[ch] + (fb > 1.0e-4f ? std::tanh (back * fb) : 0.0f);
        }

        if (doPitch)
        {
            for (int ch = 0; ch < chans; ++ch) ring[(size_t) ch][(size_t) writePos] = src[ch];
            const float pw = pitchPower (focusS.getCurrentValue());
            float sum = 0.0f;
            for (int l = 0; l < pitchLayers; ++l)
            {
                const double o = layerOctave (l, pos);
                w[(size_t) l] = window ((o - pitchLow) / pitchSpan, pw);
                ratio[(size_t) l] = std::exp2 (o);
                sum += w[(size_t) l] * w[(size_t) l];
            }
            const float norm = jmin (2.0f, 1.0f / std::sqrt (jmax (1.0e-6f, sum)));
            const double extra = width * 0.37 * winSamples;            // the right side reads a bit later: wide
            for (int l = 0; l < pitchLayers; ++l)
            {
                double& ph = phase[(size_t) l];
                ph += (1.0 - ratio[(size_t) l]) / winSamples;
                ph -= std::floor (ph);
                if (w[(size_t) l] < 1.0e-5f) continue;
                const double ph2 = ph + 0.5 - std::floor (ph + 0.5);
                const float sn = (float) std::sin (MathConstants<double>::pi * ph);
                const float a = sn * sn;
                const double d1 = minDelay + ph * winSamples, d2 = minDelay + ph2 * winSamples;
                for (int ch = 0; ch < chans; ++ch)
                {
                    const double e = ch == 1 ? extra : 0.0;
                    const float y = a * readRing (ch, d1 + e) + (1.0f - a) * readRing (ch, d2 + e);
                    wet[ch] += w[(size_t) l] * norm * y;
                }
            }
            writePos = (writePos + 1) & mask;
        }
        else
        {
            for (int ch = 0; ch < chans; ++ch) wet[ch] = src[ch];
        }

        if (doFilter)
        {
            if (--filterCountdown <= 0)
            {
                filterCountdown = 8;
                for (int ch = 0; ch < chans; ++ch) updateFilters (ch, pos + (ch == 1 ? width * 0.25 : 0.0), k);
            }
            for (int ch = 0; ch < chans; ++ch)
            {
                float out = 0.0f;
                for (auto& f : svf[(size_t) ch])
                {
                    const float v3 = wet[ch] - f.ic2;
                    const float v1 = f.a1 * f.ic1 + f.a2 * v3;
                    const float v2 = f.ic2 + f.a2 * f.ic1 + f.a3 * v3;
                    f.ic1 = 2.0f * v1 - f.ic1;
                    f.ic2 = 2.0f * v2 - f.ic2;
                    out += f.w * k * v1;
                }
                wet[ch] = out;
            }
        }

        // feedback: what came out goes round again an 1/8 note later (low end removed)
        for (int ch = 0; ch < chans; ++ch)
        {
            fbHp[(size_t) ch] += fbHpA * (wet[ch] - fbHp[(size_t) ch]);
            fbRing[(size_t) ch][(size_t) fbPos] = wet[ch] - fbHp[(size_t) ch];
        }
        fbPos = (fbPos + 1) & fbMask;

        // auto gain: the spiral is as loud as what came in
        float inP = 0.0f, wetP = 0.0f;
        for (int ch = 0; ch < chans; ++ch) { inP += x[ch] * x[ch]; wetP += wet[ch] * wet[ch]; }
        inPow += powA * (inP / chans - inPow);
        wetPow += powA * (wetP / chans - wetPow);
        if (inPow > 1.0e-7)
        {
            const float want = jlimit (0.25f, 10.0f, (float) std::sqrt (inPow / jmax (1.0e-9, wetPow)));
            wetGain += gainA * (want - wetGain);
        }

        // the Shepard tone of its own
        float g[2] { 0.0f, 0.0f };
        if (gen > 1.0e-4f)
        {
            for (int m = 0; m < genPartials; ++m)
            {
                const double t = wrap ((double) m + pos, (double) genPartials) / genPartials;
                const float wg = window (t, 1.5f);
                double& ph = genPhase[(size_t) m];
                ph += MathConstants<double>::twoPi * genLowHz * std::exp2 (t * genPartials) / sr;
                if (ph > MathConstants<double>::twoPi) ph -= MathConstants<double>::twoPi;
                if (wg < 1.0e-5f) continue;
                const float v = wg * (float) std::sin (ph);
                const float side = (m & 1) ? width * 0.6f : -width * 0.6f;
                g[0] += v * (1.0f - side);
                g[1] += v * (1.0f + side);
            }
            g[0] *= 0.16f * gen;
            g[1] *= 0.16f * gen;
        }

        for (int ch = 0; ch < chans; ++ch)
        {
            float y = wet[ch] * wetGain + g[chans == 1 ? 0 : ch];
            if (useLp) { lp[(size_t) ch] += lpA * (y - lp[(size_t) ch]); y = lp[(size_t) ch]; }
            if (useHp) { hpState[(size_t) ch] += hpA * (y - hpState[(size_t) ch]); y -= hpState[(size_t) ch]; }
            const float out = (x[ch] * (1.0f - mix) + y * mix) * outG;
            peakOut = jmax (peakOut, std::abs (out));
            buffer.setSample (ch, i, out);
        }
    }

    if (std::abs (pos) > 1.0e6) pos -= std::floor (pos);
    screenPos = (float) (pos - std::floor (pos));
    rateNow = (float) rate;
    inLevel = jmax (peakIn, inLevel.load() * 0.85f);
    outLevel = jmax (peakOut, outLevel.load() * 0.85f);
    autoGainDb = Decibels::gainToDecibels (wetGain);
}
} // namespace vortex
