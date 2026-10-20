#include "VoiceModel.h"
#include <algorithm>

namespace singcat
{
namespace
{
    constexpr float pi = 3.14159265358979f;
}

const std::array<VowelAnchor, 8>& vowelAnchors()
{
    // Area functions (cm^2, larynx -> lips) fitted to adult male formants for a 17 cm tract.
    static const std::array<VowelAnchor, 8> anchors { {
        { "i",  0.06f, 0.06f, { 1.30f, 2.87f, 9.05f, 7.09f, 2.20f, 0.62f, 0.88f, 2.01f }, 0.18f, 1.00f },
        { "e",  0.16f, 0.40f, { 1.25f, 2.20f, 3.96f, 2.49f, 0.91f, 0.83f, 1.99f, 3.69f }, 0.42f, 0.90f },
        { "ae", 0.24f, 0.82f, { 1.26f, 1.27f, 1.28f, 1.38f, 1.24f, 2.50f, 5.36f, 6.71f }, 0.78f, 0.85f },
        { "a",  0.55f, 0.95f, { 1.13f, 1.74f, 1.24f, 1.62f, 3.66f, 13.51f, 12.26f, 2.64f }, 1.00f, 0.66f },
        { "aw", 0.90f, 0.70f, { 1.32f, 3.05f, 0.53f, 0.83f, 7.50f, 6.98f, 7.66f, 1.22f }, 0.72f, 0.45f },
        { "o",  0.93f, 0.40f, { 1.17f, 4.01f, 1.01f, 0.93f, 1.88f, 17.45f, 1.74f, 1.18f }, 0.50f, 0.30f },
        { "u",  0.95f, 0.06f, { 1.44f, 5.18f, 0.64f, 3.55f, 8.32f, 14.05f, 2.00f, 0.42f }, 0.22f, 0.18f },
        { "uh", 0.55f, 0.50f, { 1.72f, 1.33f, 0.78f, 1.24f, 1.61f, 2.96f, 1.32f, 0.87f }, 0.46f, 0.60f },
    } };
    return anchors;
}

VowelShape shapeAt (float x, float y)
{
    static const auto logAnchors = []
    {
        std::array<std::array<float, numCtrl>, 8> l {};
        for (size_t a = 0; a < 8; ++a)
            for (size_t i = 0; i < (size_t) numCtrl; ++i)
                l[a][i] = std::log (vowelAnchors()[a].area[i]);
        return l;
    }();

    const auto& anchors = vowelAnchors();
    float w[8], total = 0.0f;
    for (size_t a = 0; a < 8; ++a)
    {
        const float dx = x - anchors[a].x, dy = y - anchors[a].y;
        const float d2 = dx * dx + dy * dy + 1.0e-4f;
        w[a] = 1.0f / (d2 * std::sqrt (d2));
        total += w[a];
    }

    VowelShape s;
    s.mouthOpen = s.mouthWidth = 0.0f;
    for (size_t a = 0; a < 8; ++a)
    {
        const float wa = w[a] / total;
        for (size_t i = 0; i < (size_t) numCtrl; ++i)
            s.logArea[i] += wa * logAnchors[a][i];
        s.mouthOpen += wa * anchors[a].mouthOpen;
        s.mouthWidth += wa * anchors[a].mouthWidth;
    }
    return s;
}

//==============================================================================
void Glottis::reset (uint32_t seed) noexcept
{
    noise.seed (seed);
    phase = noise.uni();
    periodScale = ampScale = 1.0f;
    hp = lp = lastNoise = 0.0f;
    newPeriod();
}

void Glottis::newPeriod() noexcept
{
    oddPeriod = ! oddPeriod;
    periodScale = 1.0f + jitter * noise.next();
    ampScale = 1.0f + shimmer * noise.next();
    if (rasp > 0.0f)
    {
        // Period doubling: every other pulse is weaker and a little late (growl / rasp)
        periodScale *= 1.0f + (oddPeriod ? 1.0f : -1.0f) * rasp * 0.05f;
        ampScale *= oddPeriod ? 1.0f - 0.6f * rasp : 1.0f;
    }
    openQ = 0.85f + (0.42f - 0.85f) * std::clamp (tenseness, 0.0f, 1.0f);
}

float Glottis::process (float f0, float sampleRate) noexcept
{
    phase += f0 / (periodScale * sampleRate);
    if (phase >= 1.0f)
    {
        phase -= std::floor (phase);
        newPeriod();
    }

    const float tp = openQ * 0.7f, tn = openQ * 0.3f;
    float d = 0.0f, flow = 0.0f;
    if (phase < tp)
    {
        d = 0.5f * pi / tp * std::sin (pi * phase / tp);
        flow = 0.5f - 0.5f * std::cos (pi * phase / tp);
    }
    else if (phase < tp + tn)
    {
        const float u = (phase - tp) / tn;
        d = -0.5f * pi / tn * std::sin (0.5f * pi * u);
        flow = std::cos (0.5f * pi * u);
    }

    // The closing phase ends with a sudden jump of the flow derivative (from its most negative
    // value back to 0). A jump like that aliases into a buzzy, metallic hiss on high notes, so it
    // is smoothed with a polyBLEP over the samples around it (a band-limited step).
    {
        const float dt = f0 / (periodScale * sampleRate);
        const float tc = tp + tn;
        if (dt > 0.0f && dt < 0.5f && tn > 2.0f * dt)
        {
            const float jump = 0.5f * pi / tn;           // the derivative's step up at closure
            float t = (phase - tc) / dt;
            if (t < -1.0f && phase + 1.0f - tc < dt) t = (phase + 1.0f - tc) / dt;   // closure just before a wrap
            if (t >= 0.0f && t < 1.0f)       d += 0.5f * jump * (2.0f * t - t * t - 1.0f);
            else if (t > -1.0f && t < 0.0f)  d += 0.5f * jump * (t * t + 2.0f * t + 1.0f);
        }
    }

    // Aspiration: band-limited noise, pulsed by the glottal flow while voiced
    const float white = noise.next();
    hp = 0.92f * (hp + white - lastNoise);
    lastNoise = white;
    lp += 0.55f * (hp - lp);
    const float v = std::min (1.0f, voicing);
    const float mod = (1.0f - v) + v * (0.3f + 0.7f * flow);

    return 0.12f * d * ampScale * voicing + lp * mod * aspiration;
}

//==============================================================================
void Tract::prepare (int numSections) noexcept
{
    n = std::clamp (numSections, 6, maxSections);
    nn = std::clamp ((int) std::lround (n * 0.62f), 4, maxNasal);
    velumIndex = std::clamp ((int) std::lround (n * 0.42f), 2, n - 2);

    for (int j = 0; j < nn; ++j)
    {
        // Nasal cavity: narrow at the velum, widest in the middle, narrower at the nostrils
        const float x = ((float) j + 0.5f) / (float) nn;
        nArea[(size_t) j] = 0.35f + 2.4f * std::pow (std::sin (pi * std::min (x * 1.15f, 1.0f) * 0.92f), 0.7f);
    }
    for (int i = 0; i < n; ++i)
        area[(size_t) i] = target[(size_t) i] = 2.0f;
    clear();
    updateReflections();
}

int Tract::sectionAt (float position) const noexcept
{
    return std::clamp ((int) std::lround (position * (float) n), 1, n - 1);
}

void Tract::clear() noexcept
{
    R.fill (0.0f); L.fill (0.0f); jR.fill (0.0f); jL.fill (0.0f);
    nR.fill (0.0f); nL.fill (0.0f); njR.fill (0.0f); njL.fill (0.0f);
}

void Tract::setTarget (const std::array<float, numCtrl>& logArea, float velumArea) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const float pos = ((float) i + 0.5f) / (float) n * (float) (numCtrl - 1);
        const int a = std::min ((int) pos, numCtrl - 2);
        const float f = pos - (float) a;
        target[(size_t) i] = std::exp (logArea[(size_t) a] + f * (logArea[(size_t) a + 1] - logArea[(size_t) a]));
    }
    velumTarget = std::max (0.0f, velumArea);
}

void Tract::smooth (float amount) noexcept
{
    for (int i = 0; i < n; ++i)
        area[(size_t) i] += (target[(size_t) i] - area[(size_t) i]) * amount;
    velum += (velumTarget - velum) * amount;
    updateReflections();
}

void Tract::updateReflections() noexcept
{
    for (int i = 1; i < n; ++i)
    {
        const float a = std::max (area[(size_t) i - 1], 0.01f), b = std::max (area[(size_t) i], 0.01f);
        k[(size_t) i] = (a - b) / (a + b);
    }
    // Nasal tube: its first section is the velum opening (closed velum = no coupling)
    nArea[0] = std::max (velum, 0.0005f);
    for (int j = 1; j < nn; ++j)
        nk[(size_t) j] = (nArea[(size_t) j - 1] - nArea[(size_t) j]) / (nArea[(size_t) j - 1] + nArea[(size_t) j]);
}

float Tract::step (float glottalIn, float noise, int noiseIndex) noexcept
{
    jR[0] = L[0] * glottalReflection + glottalIn;
    jL[(size_t) n] = R[(size_t) n - 1] * lipReflection;

    for (int i = 1; i < n; ++i)
    {
        if (i == velumIndex)
        {
            // 3-way junction (pressure waves): oral back, oral front, nasal
            const float aa = area[(size_t) i - 1], ab = area[(size_t) i], an = nArea[0];
            const float pj = 2.0f * (aa * R[(size_t) i - 1] + ab * L[(size_t) i] + an * nL[0]) / (aa + ab + an);
            jL[(size_t) i] = pj - R[(size_t) i - 1];
            jR[(size_t) i] = pj - L[(size_t) i];
            njR[0] = pj - nL[0];
        }
        else
        {
            const float w = k[(size_t) i] * (R[(size_t) i - 1] - L[(size_t) i]);
            jR[(size_t) i] = R[(size_t) i - 1] + w;
            jL[(size_t) i] = L[(size_t) i] + w;
        }
    }

    // Turbulence (fricatives, plosive bursts): noise injected just in front of the constriction
    if (noiseIndex >= 1 && noiseIndex < n)
    {
        jR[(size_t) noiseIndex] += 0.5f * noise;
        jL[(size_t) noiseIndex] += 0.5f * noise;
    }

    for (int j = 1; j < nn; ++j)
    {
        const float w = nk[(size_t) j] * (nR[(size_t) j - 1] - nL[(size_t) j]);
        njR[(size_t) j] = nR[(size_t) j - 1] + w;
        njL[(size_t) j] = nL[(size_t) j] + w;
    }
    njL[(size_t) nn] = nR[(size_t) nn - 1] * lipReflection;

    for (int i = 0; i < n; ++i)
    {
        R[(size_t) i] = jR[(size_t) i] * loss;
        L[(size_t) i] = jL[(size_t) i + 1] * loss;
    }
    for (int j = 0; j < nn; ++j)
    {
        nR[(size_t) j] = njR[(size_t) j] * loss;
        nL[(size_t) j] = njL[(size_t) j + 1] * loss;
    }

    // Radiated sound follows the volume velocity through the lips / nostrils (pressure x area)
    float out = 0.4f * (R[(size_t) n - 1] * area[(size_t) n - 1] + nR[(size_t) nn - 1] * nArea[(size_t) nn - 1]);
    if (noiseIndex >= n - 1)          // f / v / p bursts: the turbulence is right at the lips
        out += 0.25f * noise;
    return out;
}

} // namespace singcat
