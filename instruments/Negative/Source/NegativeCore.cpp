#include "NegativeCore.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>

namespace negative
{
juce::StringArray modeNames()   { return { "Exact", "Musical", "Texture" }; }
juce::StringArray lengthNames() { return { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }; }
double lengthBars (int i)       { static const double b[] = { 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 4, i)]; }
juce::StringArray keyNames()    { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
juce::StringArray scaleNames()  { return { "Major", "Minor", "Pentatonic", "Dorian", "Chromatic" }; }

namespace
{
const std::vector<int>& scaleSet (int s)
{
    static const std::vector<std::vector<int>> sets { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 3, 5, 7, 10 },
                                                      { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } };
    return sets[(size_t) juce::jlimit (0, 4, s)];
}

void boxBlur (Gray& g, int r)
{
    if (r <= 0 || g.empty()) return;
    std::vector<float> tmp (g.v.size());
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int y = 0; y < g.h; ++y)                 // across
        {
            double acc = 0.0; int n = 0;
            for (int x = -r; x < g.w + r; ++x)
            {
                if (x + r < g.w) { acc += g.v[(size_t) (y * g.w + x + r)]; ++n; }
                if (x - r - 1 >= 0) { acc -= g.v[(size_t) (y * g.w + x - r - 1)]; --n; }
                if (x >= 0 && x < g.w) tmp[(size_t) (y * g.w + x)] = (float) (acc / juce::jmax (1, n));
            }
        }
        for (int x = 0; x < g.w; ++x)                 // down
        {
            double acc = 0.0; int n = 0;
            for (int y = -r; y < g.h + r; ++y)
            {
                if (y + r < g.h) { acc += tmp[(size_t) ((y + r) * g.w + x)]; ++n; }
                if (y - r - 1 >= 0) { acc -= tmp[(size_t) ((y - r - 1) * g.w + x)]; --n; }
                if (y >= 0 && y < g.h) g.v[(size_t) (y * g.w + x)] = (float) (acc / juce::jmax (1, n));
            }
        }
    }
}

/** A sine that turns by a fixed step every sample (no sin() per sample). */
struct Rotor
{
    double c = 1.0, s = 0.0, dc = 1.0, ds = 0.0;
    void set (double hz, double sr, double phase) { c = std::cos (phase); s = std::sin (phase); dc = std::cos (juce::MathConstants<double>::twoPi * hz / sr); ds = std::sin (juce::MathConstants<double>::twoPi * hz / sr); }
    float next() noexcept { const double nc = c * dc - s * ds; s = s * dc + c * ds; c = nc; return (float) s; }
    void fix() noexcept { const double m = 1.0 / std::sqrt (c * c + s * s); c *= m; s *= m; }
};

void normalise (juce::AudioBuffer<float>& b)
{
    const float peak = juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
    if (peak > 1.0e-9f) b.applyGain (0.891f / peak);       // -1 dBFS
    else b.clear();
}

double yForHz (double hz, double low, double high, int h)  // row (0 = top = high)
{
    return (1.0 - std::log (hz / low) / std::log (high / low)) * (double) (h - 1);
}

/** Average brightness of rows y0..y1 at column x. */
float bandAt (const Gray& g, float x, double y0, double y1)
{
    if (y1 < y0) std::swap (y0, y1);
    const int a = juce::jlimit (0, g.h - 1, (int) std::floor (y0)), b = juce::jlimit (0, g.h - 1, (int) std::ceil (y1));
    float s = 0.0f; int n = 0;
    for (int y = a; y <= b; ++y) { s += g.at (x, (float) y); ++n; }
    return n > 0 ? s / (float) n : 0.0f;
}
} // namespace

float Gray::at (float x, float y) const noexcept
{
    if (empty()) return 0.0f;
    x = juce::jlimit (0.0f, (float) (w - 1), x); y = juce::jlimit (0.0f, (float) (h - 1), y);
    const int x0 = (int) x, y0 = (int) y, x1 = juce::jmin (w - 1, x0 + 1), y1 = juce::jmin (h - 1, y0 + 1);
    const float fx = x - (float) x0, fy = y - (float) y0;
    const float a = v[(size_t) (y0 * w + x0)], b = v[(size_t) (y0 * w + x1)], c = v[(size_t) (y1 * w + x0)], d = v[(size_t) (y1 * w + x1)];
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

Gray prepare (const juce::Image& photo, float contrast, float soft, bool invert, int maxW, int maxH)
{
    Gray g;
    if (! photo.isValid()) return g;
    auto img = photo;
    if (img.getWidth() > maxW || img.getHeight() > maxH)
        img = img.rescaled (juce::jmin (maxW, img.getWidth()), juce::jmin (maxH, img.getHeight()), juce::Graphics::highResamplingQuality);
    g.w = img.getWidth(); g.h = img.getHeight();
    g.v.resize ((size_t) (g.w * g.h));
    const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
    const float k = std::pow (4.0f, juce::jlimit (0.0f, 1.0f, contrast) * 2.0f - 1.0f);
    for (int y = 0; y < g.h; ++y)
        for (int x = 0; x < g.w; ++x)
        {
            const auto c = bd.getPixelColour (x, y);
            float l = (0.2126f * c.getFloatRed() + 0.7152f * c.getFloatGreen() + 0.0722f * c.getFloatBlue()) * c.getFloatAlpha();
            if (invert) l = 1.0f - l;
            g.v[(size_t) (y * g.w + x)] = juce::jlimit (0.0f, 1.0f, (l - 0.5f) * k + 0.5f);
        }
    boxBlur (g, juce::roundToInt (juce::jlimit (0.0f, 1.0f, soft) * 8.0f));
    return g;
}

juce::AudioBuffer<float> render (const Gray& g, const RenderSettings& s, const std::atomic<bool>* cancel)
{
    const double sr = s.sampleRate > 1000.0 ? s.sampleRate : 48000.0;
    const int total = juce::jlimit (1, (int) (sr * 120.0), (int) (s.seconds * sr));
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    if (g.empty()) return out;
    const double low = juce::jlimit (10.0, sr * 0.45, (double) juce::jmin (s.lowHz, s.highHz - 1.0f));
    const double high = juce::jlimit (low * 1.05, sr * 0.48, (double) s.highHz);
    auto cancelled = [cancel] { return cancel != nullptr && cancel->load(); };
    juce::Random rng (0x6e67);

    if (s.mode == 0)
    {
        // EXACT: shaped noise through an inverse FFT, frame by frame
        int order = 11;
        while ((1 << order) < (int) (sr * 0.04)) ++order;
        const int n = 1 << order, hop = n / 8;
        juce::dsp::FFT fft (order);
        std::vector<std::complex<float>> spec ((size_t) n), time ((size_t) n);
        std::vector<float> win ((size_t) n), mag ((size_t) (n / 2 + 1));
        for (int i = 0; i < n; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* o = out.getWritePointer (ch);
            for (int start = -n / 2; start < total; start += hop)
            {
                if (cancelled()) return {};
                const float x = (float) (start + n / 2) / (float) total * (float) (g.w - 1);
                for (int k = 0; k <= n / 2; ++k)
                {
                    const double hz = (double) k * sr / (double) n;
                    float m = 0.0f;
                    if (hz >= low && hz <= high) m = g.at (x, (float) yForHz (hz, low, high, g.h));
                    mag[(size_t) k] = m * m;                              // brightness as power, so the picture keeps its contrast
                }
                for (int k = 0; k <= n / 2; ++k)
                {
                    const float ph = rng.nextFloat() * juce::MathConstants<float>::twoPi;
                    spec[(size_t) k] = std::polar (mag[(size_t) k], ph);
                    if (k > 0 && k < n / 2) spec[(size_t) (n - k)] = std::conj (spec[(size_t) k]);
                }
                spec[0] = { spec[0].real(), 0.0f };
                fft.perform (spec.data(), time.data(), true);
                for (int i = 0; i < n; ++i)
                {
                    const int t = start + i;
                    if (t >= 0 && t < total) o[t] += time[(size_t) i].real() * win[(size_t) i];
                }
            }
        }
    }
    else if (s.mode == 1)
    {
        // MUSICAL: the picture's rows are the notes of the key; brightness = how loud each note is
        std::vector<double> hz;
        for (int m = 0; m < 128; ++m)
        {
            const double f = 440.0 * std::pow (2.0, (m - 69) / 12.0);
            const int deg = ((m - s.key) % 12 + 12) % 12;
            const auto& set = scaleSet (s.scale);
            if (f >= low && f <= high && std::find (set.begin(), set.end(), deg) != set.end()) hz.push_back (f);
        }
        if (hz.empty()) return out;
        const int frameRate = 200, fh = juce::jmax (1, (int) (sr / frameRate));
        const int frames = total / fh + 2;
        std::vector<float> amp ((size_t) frames);
        for (size_t i = 0; i < hz.size(); ++i)
        {
            if (cancelled()) return {};
            const double lo = i > 0 ? std::sqrt (hz[i] * hz[i - 1]) : hz[i] / std::pow (2.0, 1.0 / 24.0);
            const double hi = i + 1 < hz.size() ? std::sqrt (hz[i] * hz[i + 1]) : hz[i] * std::pow (2.0, 1.0 / 24.0);
            const double y0 = yForHz (juce::jmin (hi, high), low, high, g.h), y1 = yForHz (juce::jmax (lo, low), low, high, g.h);
            bool any = false;
            for (int f = 0; f < frames; ++f)
            {
                const float x = (float) f / (float) (frames - 1) * (float) (g.w - 1);
                float v = juce::jlimit (0.0f, 1.0f, (bandAt (g, x, y0, y1) - 0.25f) / 0.75f);
                v = v * v;
                amp[(size_t) f] = v;
                any |= v > 0.001f;
            }
            if (! any) continue;
            // a soft tone: the note + a bit of its octave and twelfth; each note leans a little left or right
            Rotor r1, r2, r3;
            r1.set (hz[i], sr, rng.nextDouble() * 6.28); r2.set (hz[i] * 2.0, sr, rng.nextDouble() * 6.28); r3.set (hz[i] * 3.0, sr, rng.nextDouble() * 6.28);
            const bool h2 = hz[i] * 2.0 < sr * 0.45, h3 = hz[i] * 3.0 < sr * 0.45;
            const float pan = (i % 2 == 0 ? -0.35f : 0.35f) * (0.4f + 0.6f * rng.nextFloat());
            const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
            auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
            for (int t = 0; t < total; ++t)
            {
                const int f = t / fh;
                const float a = amp[(size_t) f] + (amp[(size_t) juce::jmin (frames - 1, f + 1)] - amp[(size_t) f]) * (float) (t - f * fh) / (float) fh;
                const float v = r1.next() + (h2 ? 0.25f * r2.next() : 0.0f) + (h3 ? 0.1f * r3.next() : 0.0f);
                if ((t & 1023) == 0) { r1.fix(); r2.fix(); r3.fix(); }
                L[t] += v * a * gl; R[t] += v * a * gr;
            }
        }
    }
    else
    {
        // TEXTURE: a soft pad of slowly moving, slightly detuned partials
        const int partials = 64;
        const double top = juce::jmin (high, 12000.0);
        const int frameRate = 100, fh = juce::jmax (1, (int) (sr / frameRate));
        const int frames = total / fh + 2;
        std::vector<float> amp ((size_t) frames);
        auto* L = out.getWritePointer (0); auto* R = out.getWritePointer (1);
        for (int p = 0; p < partials; ++p)
        {
            if (cancelled()) return {};
            const double f = low * std::pow (top / low, (p + 0.5) / partials);
            const double fl = low * std::pow (top / low, (double) p / partials), fu = low * std::pow (top / low, (p + 1.0) / partials);
            const double y0 = yForHz (fu, low, high, g.h), y1 = yForHz (fl, low, high, g.h);
            float smooth = 0.0f;
            const float k = 1.0f - std::exp (-1.0f / (0.25f * (float) frameRate));
            bool any = false;
            for (int i = 0; i < frames; ++i)
            {
                const float x = (float) i / (float) (frames - 1) * (float) (g.w - 1);
                smooth += (bandAt (g, x, y0, y1) - smooth) * k;
                amp[(size_t) i] = smooth * smooth / (float) std::sqrt (f / low + 1.0);   // a pad is darker at the top
                any |= amp[(size_t) i] > 0.0005f;
            }
            if (! any) continue;
            Rotor a, b;
            a.set (f * std::pow (2.0, 6.0 / 1200.0), sr, rng.nextDouble() * 6.28);
            b.set (f * std::pow (2.0, -6.0 / 1200.0), sr, rng.nextDouble() * 6.28);
            const float pan = rng.nextFloat() * 1.6f - 0.8f;
            const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
            for (int t = 0; t < total; ++t)
            {
                const int i = t / fh;
                const float av = amp[(size_t) i] + (amp[(size_t) juce::jmin (frames - 1, i + 1)] - amp[(size_t) i]) * (float) (t - i * fh) / (float) fh;
                const float va = a.next(), vb = b.next();
                if ((t & 1023) == 0) { a.fix(); b.fix(); }
                L[t] += (va * 0.7f + vb * 0.3f) * av * gl;
                R[t] += (va * 0.3f + vb * 0.7f) * av * gr;
            }
        }
    }
    // short fades at both ends (no click when it loops)
    const int fade = juce::jmin (total / 4, (int) (sr * 0.005));
    for (int ch = 0; ch < 2; ++ch)
    {
        out.applyGainRamp (ch, 0, fade, 0.0f, 1.0f);
        out.applyGainRamp (ch, total - fade, fade, 1.0f, 0.0f);
    }
    normalise (out);
    return out;
}

juce::Image analyse (const juce::AudioBuffer<float>& b, double sr, float lowHz, float highHz, int width, int height)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, width), juce::jmax (1, height), true);
    const int total = b.getNumSamples();
    if (total <= 0 || b.getNumChannels() <= 0) return img;
    sr = sr > 1000.0 ? sr : 48000.0;
    const int order = sr > 60000.0 ? 13 : 12, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) (2 * n)), win ((size_t) n);
    for (int i = 0; i < n; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
    const double low = juce::jmax (10.0, (double) lowHz), high = juce::jlimit (low * 1.05, sr * 0.5, (double) highHz);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    const float norm = 2.0f / ((float) n * 0.5f);
    for (int x = 0; x < width; ++x)
    {
        const int centre = (int) ((double) (x + 0.5) / width * total);
        std::fill (data.begin(), data.end(), 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const int t = centre - n / 2 + i;
            if (t < 0 || t >= total) continue;
            float v = 0.0f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch) v += b.getSample (ch, t);
            data[(size_t) i] = v / (float) b.getNumChannels() * win[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (data.data(), true);
        for (int y = 0; y < height; ++y)
        {
            const double hz = low * std::pow (high / low, 1.0 - (double) y / juce::jmax (1, height - 1));
            const double bin = hz * n / sr;
            const int k = juce::jlimit (0, n / 2 - 1, (int) bin);
            const float fr = (float) (bin - k);
            const float m = (data[(size_t) k] + (data[(size_t) k + 1] - data[(size_t) k]) * fr) * norm;
            const float db = juce::Decibels::gainToDecibels (m, -120.0f);
            const auto v = (juce::uint8) juce::jlimit (0, 255, (int) ((db + 90.0f) / 90.0f * 255.0f));
            bd.setPixelColour (x, y, juce::Colour (v, v, v));
        }
    }
    return img;
}

juce::String toBase64Png (const juce::Image& img)
{
    if (! img.isValid()) return {};
    juce::MemoryOutputStream m;
    juce::PNGImageFormat png;
    if (! png.writeImageToStream (img, m)) return {};
    return m.getMemoryBlock().toBase64Encoding();
}

juce::Image fromBase64Png (const juce::String& s)
{
    if (s.isEmpty()) return {};
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (s)) return {};
    return juce::ImageFileFormat::loadFrom (mb.getData(), mb.getSize());
}
} // namespace negative
