#include "SwarmEngine.h"

namespace swarm
{
juce::StringArray syncNames()  { return { "1/32", "1/16", "1/8", "1/4", "1/16T", "1/8T" }; }
double syncBeats (int i)
{
    static const double b[] = { 0.125, 0.25, 0.5, 1.0, 1.0 / 6.0, 1.0 / 3.0 };
    return b[juce::jlimit (0, 5, i)];
}
juce::StringArray scaleNames() { return { "Octaves", "Major", "Minor", "Pentatonic", "Dorian", "Dreamy (lydian)", "Dark (phrygian)" }; }

namespace
{
// the steps a grain may jump by, in semitones from the note it was cut from
const std::vector<int>& scaleSteps (int scale)
{
    static const std::vector<std::vector<int>> s {
        { 12, 7, -5, -12 },
        { 2, 4, 5, 7, 9, 11 },
        { 2, 3, 5, 7, 8, 10 },
        { 2, 4, 7, 9 },
        { 2, 3, 5, 7, 9, 10 },
        { 2, 4, 6, 7, 9, 11 },
        { 1, 3, 5, 7, 8, 10 },
    };
    return s[(size_t) juce::jlimit (0, (int) s.size() - 1, scale)];
}
constexpr float pi = juce::MathConstants<float>::pi;

// half a raised cosine (0 -> 1), from a table: the grains' fades without a cos() per grain per sample
float fade (float u) noexcept
{
    static const auto table = []
    {
        std::array<float, 1026> t {};
        for (size_t i = 0; i < t.size(); ++i) t[i] = 0.5f - 0.5f * std::cos (pi * juce::jmin (1.0f, (float) i / 1024.0f));
        return t;
    }();
    const float x = juce::jlimit (0.0f, 1.0f, u) * 1024.0f;
    const auto i = (size_t) x;
    return table[i] + (table[i + 1] - table[i]) * (x - (float) i);
}
}

Engine::Character Engine::blend (float x, float y) noexcept
{
    //                    size  dens   up    down  fifth scale rev   scat   rhy  chaos space  tone   dirt  echo  spread soft
    static const Character dream { 1.7f, 1.25f, 0.35f, 0.0f, 0.12f, 0.10f, 0.30f, 0.20f, 0.0f, 0.10f, 0.35f, 0.12f, 0.0f, 0.15f, 0.9f, 1.0f };
    static const Character pulse { 0.45f, 1.0f, 0.10f, 0.05f, 0.05f, 0.15f, 0.10f, -0.25f, 1.0f, 0.10f, -0.2f, 0.05f, 0.05f, 0.45f, 0.6f, 0.15f };
    static const Character dark  { 1.9f, 0.85f, 0.0f, 0.45f, 0.10f, 0.10f, 0.40f, 0.25f, 0.0f, 0.15f, 0.30f, -0.35f, 0.25f, 0.15f, 0.7f, 1.0f };
    static const Character wild  { 0.9f, 1.4f, 0.25f, 0.25f, 0.20f, 0.50f, 0.50f, 0.45f, 0.30f, 1.0f, 0.10f, 0.0f, 0.30f, 0.30f, 1.0f, 0.55f };
    x = juce::jlimit (0.0f, 1.0f, x); y = juce::jlimit (0.0f, 1.0f, y);
    const float w[4] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };
    const Character* c[4] = { &dream, &pulse, &dark, &wild };
    Character o {};
    auto* out = reinterpret_cast<float*> (&o);
    for (int k = 0; k < 4; ++k)
    {
        auto* in = reinterpret_cast<const float*> (c[k]);
        for (size_t f = 0; f < sizeof (Character) / sizeof (float); ++f) out[f] += w[k] * in[f];
    }
    return o;
}

Engine::Biquad Engine::lowPass (double sr, double f, double q)
{
    const double w = juce::MathConstants<double>::twoPi * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
    Biquad b;
    b.b0 = (float) ((1.0 - cs) * 0.5 / a0); b.b1 = (float) ((1.0 - cs) / a0); b.b2 = b.b0;
    b.a1 = (float) (-2.0 * cs / a0); b.a2 = (float) ((1.0 - al) / a0);
    return b;
}

Engine::Biquad Engine::highPass (double sr, double f, double q)
{
    const double w = juce::MathConstants<double>::twoPi * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
    Biquad b;
    b.b0 = (float) ((1.0 + cs) * 0.5 / a0); b.b1 = (float) (-(1.0 + cs) / a0); b.b2 = b.b0;
    b.a1 = (float) (-2.0 * cs / a0); b.a2 = (float) ((1.0 - al) / a0);
    return b;
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    int size = 1;
    while (size < (int) (sr * 8.0)) size <<= 1;
    ringL.assign ((size_t) size, 0.0f);
    ringR.assign ((size_t) size, 0.0f);
    ringMask = size - 1;
    echoL.assign ((size_t) (sr * 2.6) + 4, 0.0f);
    echoR.assign (echoL.size(), 0.0f);
    static const double ms[numLines] = { 29.7, 37.1, 41.1, 43.7, 53.3, 61.7, 71.9, 83.3 };
    for (int i = 0; i < numLines; ++i)
    {
        lineLen[(size_t) i] = (int) (ms[i] * 1.7 * sr / 1000.0);
        lines[(size_t) i].assign ((size_t) lineLen[(size_t) i] + 64, 0.0f);
    }
    preDelayL.assign ((size_t) (sr * 0.03) + 1, 0.0f);
    preDelayR.assign (preDelayL.size(), 0.0f);
    const int n = juce::jmax (1, maxBlock);
    wetL.assign ((size_t) n, 0.0f); wetR.assign ((size_t) n, 0.0f);
    dryL.assign ((size_t) n, 0.0f); dryR.assign ((size_t) n, 0.0f);
    for (auto* s : { &sMix, &sOut, &sSpace, &sDirt, &sTone, &sEcho }) s->reset (sr, 0.05);
    for (int k = 0; k < 4; ++k)
    {
        wetHp[(size_t) k] = highPass (sr, 120.0, 0.70710678);
        dryLp[(size_t) k] = lowPass (sr, 120.0, 0.70710678);
        inLp[(size_t) k] = highPass (sr, 120.0, 0.70710678);
    }
    reset();
}

void Engine::reset()
{
    std::fill (ringL.begin(), ringL.end(), 0.0f);
    std::fill (ringR.begin(), ringR.end(), 0.0f);
    writePos = 0;                    // the ring may have got smaller (a new sample rate)
    frozenAt = -1; frozenStart = frozenEnd = 0.0;
    for (auto& g : grains) g = Grain();
    untilNext = 0.0; freeClock = 0.0; lastStep = -1.0; freezeCursor = 0.0f;
    lx = 0.1; ly = 0.0; lz = 20.0;
    env = 0.0f; lifeDens = lifeSize = 0.0f;
    for (auto& b : wetHp) b.z1 = b.z2 = 0.0f;
    for (auto& b : dryLp) b.z1 = b.z2 = 0.0f;
    for (auto& b : inLp) b.z1 = b.z2 = 0.0f;
    toneL = toneR = toneHpL = toneHpR = 0.0f;
    std::fill (echoL.begin(), echoL.end(), 0.0f);
    std::fill (echoR.begin(), echoR.end(), 0.0f);
    echoPos = 0; echoLpL = echoLpR = 0.0f;
    for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
    linePos.fill (0); lineLp.fill (0.0f);
    std::fill (preDelayL.begin(), preDelayL.end(), 0.0f);
    std::fill (preDelayR.begin(), preDelayR.end(), 0.0f);
    preDelayPos = 0;
    modPhase = 0.0;
    inPow = inPowAll = outPow = 0.0;
    agGain = 1.0f;
    activeGrains = 0;
    frozen = false;
    eventFifo.reset();
}

int Engine::popGrainEvents (GrainEvent* dest, int max)
{
    const auto scope = eventFifo.read (juce::jmin (max, eventFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = events[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = events[(size_t) (scope.startIndex2 + i)];
    return n;
}

float Engine::readRing (const std::vector<float>& r, double pos) const noexcept
{
    // cubic (Hermite) interpolation: grains pitched up or down stay clean
    const auto i = (int) std::floor (pos);
    const float t = (float) (pos - i);
    const float y0 = r[(size_t) ((i - 1) & ringMask)], y1 = r[(size_t) (i & ringMask)],
                y2 = r[(size_t) ((i + 1) & ringMask)], y3 = r[(size_t) ((i + 2) & ringMask)];
    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void Engine::captureFreeze()
{
    // hold the last stretch that has sound in it (up to 3 s), even if FREEZE comes a moment after it stopped
    const int chunk = juce::jmax (16, (int) (sr * 0.01));
    const int maxBack = juce::jmin (ringMask - chunk, (int) (sr * 6.0));
    auto energyAt = [this, chunk] (int endPos)
    {
        double e = 0.0;
        for (int k = 1; k <= chunk; ++k)
        {
            const auto i = (size_t) ((endPos - k) & ringMask);
            e += (double) ringL[i] * ringL[i] + (double) ringR[i] * ringR[i];
        }
        return e / chunk;
    };
    double loudest = 0.0;
    for (int back = 0; back < maxBack; back += chunk) loudest = juce::jmax (loudest, energyAt (frozenAt - back));
    const double floorE = juce::jmax (1.0e-10, loudest * 1.0e-4);          // -40 dB under the loudest bit
    int end = 0;
    while (end < maxBack && energyAt (frozenAt - end) < floorE) end += chunk;
    if (end >= maxBack) end = 0;
    int start = end, quiet = 0;
    const int maxLen = (int) (sr * 3.0), gapMax = (int) (sr * 0.5);
    while (start - end < maxLen && start < maxBack)
    {
        if (energyAt (frozenAt - start) < floorE) { quiet += chunk; if (quiet > gapMax) break; }
        else quiet = 0;
        start += chunk;
    }
    start -= juce::jmin (quiet, start - end);
    if (start - end < (int) (sr * 0.05)) start = end + (int) (sr * 0.05);
    frozenEnd = (double) frozenAt - end;
    frozenStart = (double) frozenAt - start;
    freezeCursor = 0.5f;
}

void Engine::stepLife (const Settings& s, int n)
{
    const float life = juce::jlimit (0.0f, 1.0f, s.life);
    if (life < 1.0e-3f)
    {
        wx = s.x; wy = s.y; lifeDens = lifeSize = 0.0f;
        return;
    }
    // the attractor runs faster when the sound is loud
    const double rate = (0.12 + 1.5 * (double) env) * (0.35 + 0.65 * (double) life);
    double t = (double) n / sr * rate * 0.6;
    while (t > 0.0)
    {
        const double h = juce::jmin (0.005, t);
        const double dx = 10.0 * (ly - lx), dy = lx * (28.0 - lz) - ly, dz = lx * ly - (8.0 / 3.0) * lz;
        lx += dx * h; ly += dy * h; lz += dz * h;
        t -= h;
    }
    if (! (std::isfinite (lx) && std::isfinite (ly) && std::isfinite (lz))) { lx = 0.1; ly = 0.0; lz = 20.0; }
    const float nx = (float) juce::jlimit (-1.0, 1.0, lx / 20.0), ny = (float) juce::jlimit (-1.0, 1.0, ly / 27.0),
                nz = (float) juce::jlimit (-1.0, 1.0, (lz - 25.0) / 18.0);
    const float tx = juce::jlimit (0.0f, 1.0f, s.x + 0.32f * life * nx), ty = juce::jlimit (0.0f, 1.0f, s.y + 0.32f * life * ny);
    const float k = 1.0f - std::exp (-(float) n / (0.08f * (float) sr));
    wx += (tx - wx) * k; wy += (ty - wy) * k;
    lifeDens = 0.5f * life * nz;
    lifeSize = 0.4f * life * (0.6f * nx - 0.4f * ny);
}

void Engine::startGrain (const Settings& s, const Character& c, double aligned)
{
    Grain* g = nullptr;
    for (auto& gg : grains) if (! gg.on) { g = &gg; break; }
    if (g == nullptr) return;

    auto rnd = [this] { return rng.nextFloat(); };
    const float life = juce::jlimit (0.0f, 1.0f, s.life);
    const float scat = juce::jlimit (0.0f, 1.0f, s.scatter + c.scatter + 0.2f * life * (s.density - 0.5f));

    // its note
    const float pAmt = juce::jlimit (0.0f, 1.0f, s.pitch) * 2.0f;
    float semis = 0.0f;
    {
        const float up = c.up * pAmt, down = c.down * pAmt, fifth = c.fifth * pAmt, jump = c.scaleJump * pAmt;
        const float r = rnd() * juce::jmax (1.0f, up + down + fifth + jump);
        const auto& steps = scaleSteps (s.scale);
        if (r < up) semis = (c.chaos > 0.5f && rnd() < 0.3f) ? 24.0f : 12.0f;
        else if (r < up + down) semis = -12.0f;
        else if (r < up + down + fifth) semis = rnd() < 0.7f ? 7.0f : -5.0f;
        else if (r < up + down + fifth + jump)
        {
            semis = (float) steps[(size_t) rng.nextInt ((int) steps.size())];
            if (semis > 0.0f && rnd() < 0.35f) semis -= 12.0f;
        }
    }
    const double ratio = std::pow (2.0, (double) semis / 12.0);

    // its length
    float ms = grainMs (s.grain) * c.size * (1.0f + lifeSize) * (1.0f - 0.3f * life * env);
    if (c.chaos > 0.05f) ms *= std::pow (2.0f, c.chaos * (rnd() * 3.2f - 1.6f));
    int len = (int) (juce::jlimit (8.0f, 1500.0f, ms) * 0.001f * (float) sr);
    if (aligned >= 0.0) len = juce::jmin (len, (int) (aligned * 0.92));        // a chop stays inside its step
    len = juce::jmax (64, len);
    const double span = len * ratio;
    const bool reverse = rnd() < c.rev;

    // where in the sound it comes from
    double start;
    const bool isFrozen = frozenAt >= 0;
    if (isFrozen)
    {
        const double room = juce::jmax (0.0, frozenEnd - frozenStart - span - 8.0);
        double centre = frozenStart + 4.0 + freezeCursor * room;
        centre += (rnd() * 2.0f - 1.0f) * scat * room * 0.5;
        start = juce::jlimit (frozenStart + 4.0, juce::jmax (frozenStart + 4.0, frozenEnd - span - 4.0), centre);
    }
    else
    {
        const double minDelay = 64.0 + (ratio > 1.0 ? span - len : 0.0) + (reverse ? span : 0.0);
        double delay = minDelay + (double) rnd() * sr * (0.02 + 1.2 * scat) * (aligned >= 0.0 ? 0.25 : 1.0);
        if (rnd() < 0.12f * life) delay += sr * (1.0 + rnd() * 5.0);          // it remembers something from before
        delay = juce::jmin (delay, (double) ringMask - span - 16.0);
        start = (double) writePos - delay;
    }
    g->on = true;
    g->len = len;
    g->age = 0;
    g->step = reverse ? -ratio : ratio;
    g->pos = reverse ? start + span : start;
    // the shorter the fade, the more it chops; never under 3 ms, so it never clicks
    const float minFade = juce::jmin (1.0f, 2.0f * 0.003f * (float) sr / (float) len);
    g->tukey = juce::jmax (minFade, aligned >= 0.0 ? 0.12f : c.soft);

    const double rateNow = (double) juce::jmax (1.0f, 3.0f * std::pow (60.0f, s.density) * c.dens);
    const float overlap = (float) (rateNow * len / sr);
    const float amp = 0.9f / std::sqrt (1.0f + overlap * 0.5f);
    const float pan = (rnd() * 2.0f - 1.0f) * c.spread * (0.35f + 0.65f * scat);
    const float th = (pan + 1.0f) * pi * 0.25f;
    g->gl = std::cos (th) * amp * 1.41421356f;
    g->gr = std::sin (th) * amp * 1.41421356f;
    ++grainsStarted;

    const auto scope = eventFifo.write (1);
    if (scope.blockSize1 > 0)
        events[(size_t) scope.startIndex1] = { juce::jlimit (0.0f, 1.0f, wx + pan * 0.13f + (rnd() - 0.5f) * 0.05f),
                                              juce::jlimit (0.0f, 1.0f, wy - semis / 60.0f + (rnd() - 0.5f) * 0.05f),
                                              semis, pan, (float) len / (float) sr, amp, reverse };
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int n = juce::jmin (buffer.getNumSamples(), (int) wetL.size());
    if (n <= 0 || ringL.empty()) return;
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;

    stepLife (s, n);
    wanderX = wx; wanderY = wy;
    const auto c = blend (wx, wy);
    const float life = juce::jlimit (0.0f, 1.0f, s.life);

    // freeze: stop writing, keep the last few seconds
    if (s.freeze && frozenAt < 0) { frozenAt = writePos; captureFreeze(); }
    if (! s.freeze && frozenAt >= 0) frozenAt = -1;
    frozen = frozenAt >= 0;
    if (frozenAt >= 0) freezeCursor = juce::jlimit (0.0f, 1.0f, freezeCursor + (float) n / (float) sr * 0.03f * (0.3f + life) * (rng.nextFloat() - 0.5f) * 4.0f);

    sMix.setTargetValue (juce::jlimit (0.0f, 1.0f, s.mix));
    sOut.setTargetValue (juce::Decibels::decibelsToGain (s.outDb));
    sSpace.setTargetValue (juce::jlimit (0.0f, 1.0f, s.space + c.space * (0.4f + s.space)));
    sDirt.setTargetValue (juce::jlimit (0.0f, 1.0f, s.dirt + c.dirt));
    sTone.setTargetValue (juce::jlimit (0.0f, 1.0f, s.tone + c.tone));
    sEcho.setTargetValue (juce::jlimit (0.0f, 1.0f, c.echo * (0.3f + s.space)));

    const double spb = sr * 60.0 / juce::jlimit (30.0, 300.0, s.bpm);
    const double grid = syncBeats (s.sync);
    const double rateFree = 3.0 * std::pow (60.0, (double) s.density) * c.dens * (1.0 + lifeDens) * (1.0 - c.rhythm);
    const double stepChance = juce::jlimit (0.0, 1.0, 0.3 + 0.9 * (double) s.density) * (double) c.rhythm;
    const double beat0 = s.ppq >= 0.0 ? s.ppq : freeClock;
    const float envA = 1.0f - std::exp (-1.0f / (0.01f * (float) sr)), envR = 1.0f - std::exp (-1.0f / (0.3f * (float) sr));
    const float pc = 1.0f - std::exp (-1.0f / (0.4f * (float) sr));
    const float agc = 1.0f - std::exp (-1.0f / (0.6f * (float) sr));
    const double echoBeats = 0.75;
    const int echoD = juce::jlimit (1, (int) echoL.size() - 2, (int) (echoBeats * spb));
    std::array<float, numLines> decay {};
    {
        const float sp = sSpace.getTargetValue(), rt60 = 1.2f + 10.0f * sp * sp;
        for (int k = 0; k < numLines; ++k) decay[(size_t) k] = std::pow (10.0f, -3.0f * (float) lineLen[(size_t) k] / (rt60 * (float) sr));
    }

    int active = 0;
    for (int i = 0; i < n; ++i)
    {
        const float xl = L[i], xr = R[i];
        dryL[(size_t) i] = xl; dryR[(size_t) i] = xr;
        const float a = 0.5f * (std::abs (xl) + std::abs (xr));
        env += (a - env) * (a > env ? envA : envR);
        const float hl = inLp[1].run (inLp[0].run (xl)), hr = inLp[3].run (inLp[2].run (xr));
        inPow += ((double) (hl * hl + hr * hr) * 0.5 - inPow) * pc;
        inPowAll += ((double) (xl * xl + xr * xr) * 0.5 - inPowAll) * pc;

        if (frozenAt < 0)
        {
            ringL[(size_t) writePos] = xl;
            ringR[(size_t) writePos] = xr;
            writePos = (writePos + 1) & ringMask;
        }

        // new grains: on the grid (PULSE) and / or free
        const double b = beat0 + (double) i / spb;
        const double step = std::floor (b / grid);
        if (step != lastStep)
        {
            if (lastStep >= 0.0 && stepChance > 0.0 && rng.nextDouble() < stepChance)
            {
                startGrain (s, c, grid * spb);
                if (s.density > 0.6f && rng.nextFloat() < 0.5f) startGrain (s, c, grid * spb);
            }
            lastStep = step;
        }
        if (rateFree > 0.3)
        {
            untilNext -= 1.0;
            if (untilNext <= 0.0)
            {
                startGrain (s, c, -1.0);
                const float scat = juce::jlimit (0.0f, 1.0f, s.scatter + c.scatter);
                untilNext = sr / rateFree * (1.0 + scat * 0.9 * (rng.nextDouble() * 2.0 - 1.0));
            }
        }

        // play the grains
        float gl = 0.0f, gr = 0.0f;
        for (auto& g : grains)
        {
            if (! g.on) continue;
            const float t = (float) g.age / (float) g.len;
            const float h = g.tukey * 0.5f;
            float w = 1.0f;
            if (t < h) w = fade (t / h);
            else if (t > 1.0f - h) w = fade ((1.0f - t) / h);
            gl += readRing (ringL, g.pos) * w * g.gl;
            gr += readRing (ringR, g.pos) * w * g.gr;
            g.pos += g.step;
            if (++g.age >= g.len) g.on = false;
        }
        wetL[(size_t) i] = gl; wetR[(size_t) i] = gr;
    }
    freeClock = beat0 + (double) n / spb;
    if (s.ppq < 0.0 && freeClock > 1.0e6) freeClock = 0.0;
    for (auto& g : grains) active += g.on ? 1 : 0;
    activeGrains = active;
    energy = juce::jlimit (0.0f, 1.0f, std::sqrt (env) * 1.5f);

    // after the grains: clean lows, dirt, tone, echo, reverb
    for (int i = 0; i < n; ++i)
    {
        float l = wetL[(size_t) i], r = wetR[(size_t) i];
        l = wetHp[1].run (wetHp[0].run (l));
        r = wetHp[3].run (wetHp[2].run (r));

        const float dirt = sDirt.getNextValue();
        if (dirt > 0.001f)
        {
            const float k = 1.0f + 14.0f * std::pow (dirt, 1.5f);
            const float m = juce::jmin (1.0f, dirt * 3.0f), norm = 1.0f / std::sqrt (k);
            l += (std::tanh (k * l) * norm - l) * m;
            r += (std::tanh (k * r) * norm - r) * m;
        }

        const float tone = sTone.getNextValue();
        if (tone < 0.5f)
        {
            const float fc = 20000.0f * std::pow (0.03f, (0.5f - tone) * 2.0f);
            const float k = 1.0f - std::exp (-2.0f * pi * fc / (float) sr);
            toneL += (l - toneL) * k; toneR += (r - toneR) * k;
            l = toneL; r = toneR;
        }
        else
        {
            const float fc = 20.0f * std::pow (40.0f, (tone - 0.5f) * 2.0f);
            const float k = 1.0f - std::exp (-2.0f * pi * fc / (float) sr);
            toneHpL += (l - toneHpL) * k; toneHpR += (r - toneHpR) * k;
            l -= toneHpL; r -= toneHpR;
        }

        // echo: dotted 1/8 ping-pong, each repeat darker
        const float echo = sEcho.getNextValue();
        {
            int rp = echoPos - echoD; if (rp < 0) rp += (int) echoL.size();
            const float el = echoL[(size_t) rp], er = echoR[(size_t) rp];
            echoLpL += (el - echoLpL) * 0.35f; echoLpR += (er - echoLpR) * 0.35f;
            echoL[(size_t) echoPos] = l * echo + echoLpR * 0.45f;
            echoR[(size_t) echoPos] = r * echo + echoLpL * 0.45f;
            if (++echoPos >= (int) echoL.size()) echoPos = 0;
            l += el * 0.8f; r += er * 0.8f;
        }

        // reverb
        const float space = sSpace.getNextValue();
        const float pdL = preDelayL[(size_t) preDelayPos], pdR = preDelayR[(size_t) preDelayPos];
        preDelayL[(size_t) preDelayPos] = l; preDelayR[(size_t) preDelayPos] = r;
        if (++preDelayPos >= (int) preDelayL.size()) preDelayPos = 0;
        std::array<float, numLines> o {};
        float sum = 0.0f;
        modPhase += 0.6 / sr;
        if (modPhase > 1.0) modPhase -= 1.0;
        for (int k = 0; k < numLines; ++k)
        {
            const auto ku = (size_t) k;
            const float mod = 6.0f * std::sin (2.0f * pi * ((float) modPhase + (float) k * 0.125f));
            double rpos = (double) linePos[ku] - (double) lineLen[ku] + 8.0 + mod;
            const int sz = (int) lines[ku].size();
            while (rpos < 0.0) rpos += sz;
            const int i0 = (int) rpos % sz, i1 = (i0 + 1) % sz;
            const float fr = (float) (rpos - std::floor (rpos));
            o[ku] = lines[ku][(size_t) i0] + (lines[ku][(size_t) i1] - lines[ku][(size_t) i0]) * fr;
            sum += o[ku];
        }
        const float hh = 2.0f / (float) numLines * sum;
        float rvL = 0.0f, rvR = 0.0f;
        for (int k = 0; k < numLines; ++k)
        {
            const auto ku = (size_t) k;
            const float fb = (o[ku] - hh) * decay[ku];
            lineLp[ku] += (fb - lineLp[ku]) * 0.55f;                  // the air takes the top off
            const float in = (k & 1) ? pdR : pdL;
            lines[ku][(size_t) linePos[ku]] = lineLp[ku] + in * 0.35f;
            if (++linePos[ku] >= (int) lines[ku].size()) linePos[ku] = 0;
            if (k & 1) rvR += o[ku]; else rvL += o[ku];
        }
        l = l * (1.0f - 0.35f * space) + rvL * 0.32f * space;
        r = r * (1.0f - 0.35f * space) + rvR * 0.32f * space;
        wetL[(size_t) i] = l; wetR[(size_t) i] = r;
        outPow += ((double) (l * l + r * r) * 0.5 - outPow) * pc;
    }

    // auto gain: the swarm as loud as what came in (held while frozen or quiet)
    if (frozenAt < 0 && inPow > 1.0e-7 && inPow > inPowAll * 0.01 && outPow > 1.0e-9)
    {
        const float target = (float) juce::jlimit (0.1, 4.0, std::sqrt (inPow / outPow));
        agGain += (target - agGain) * (1.0f - std::pow (1.0f - agc, (float) n));
    }
    autoGainDb = juce::Decibels::gainToDecibels (agGain);

    for (int i = 0; i < n; ++i)
    {
        const float m = sMix.getNextValue(), og = sOut.getNextValue();
        const float lowL = dryLp[1].run (dryLp[0].run (dryL[(size_t) i]));
        const float lowR = dryLp[3].run (dryLp[2].run (dryR[(size_t) i]));
        // the lows stay dry and solid; MIX only swaps what's above them for the swarm
        const float ol = (1.0f - m) * dryL[(size_t) i] + m * (lowL + wetL[(size_t) i] * agGain);
        const float orr = (1.0f - m) * dryR[(size_t) i] + m * (lowR + wetR[(size_t) i] * agGain);
        L[i] = ol * og;
        if (R != L) R[i] = orr * og;
    }
}
} // namespace swarm
