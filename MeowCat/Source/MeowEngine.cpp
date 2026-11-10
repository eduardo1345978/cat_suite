#include "MeowEngine.h"

namespace meow
{
namespace
{
constexpr float pi = juce::MathConstants<float>::pi;

//                       resonances (Hz)                 loudness of each            width (Hz)              loud  open
const Vowel vM { { 270.0f, 1000.0f, 2300.0f, 3300.0f }, { 1.0f, 0.06f, 0.03f, 0.02f }, { 70.0f, 200.0f, 300.0f, 400.0f }, 0.22f, 0.0f };
const Vowel vI { { 290.0f, 2250.0f, 2950.0f, 3700.0f }, { 1.0f, 0.55f, 0.40f, 0.18f }, { 60.0f,  90.0f, 140.0f, 220.0f }, 0.80f, 0.3f };
const Vowel vE { { 480.0f, 1850.0f, 2550.0f, 3500.0f }, { 1.0f, 0.55f, 0.32f, 0.15f }, { 70.0f, 100.0f, 150.0f, 230.0f }, 0.90f, 0.5f };
const Vowel vA { { 820.0f, 1300.0f, 2600.0f, 3500.0f }, { 1.0f, 0.75f, 0.30f, 0.15f }, { 90.0f, 110.0f, 170.0f, 250.0f }, 1.00f, 1.0f };
const Vowel vO { { 520.0f,  880.0f, 2500.0f, 3400.0f }, { 1.0f, 0.50f, 0.12f, 0.07f }, { 80.0f, 100.0f, 160.0f, 250.0f }, 0.90f, 0.65f };
const Vowel vU { { 330.0f,  720.0f, 2350.0f, 3300.0f }, { 1.0f, 0.28f, 0.06f, 0.04f }, { 70.0f,  90.0f, 150.0f, 250.0f }, 0.70f, 0.25f };
// a hiss: no voice, only air through the open mouth, high up
const Vowel vHiss { { 2800.0f, 4200.0f, 5600.0f, 7200.0f }, { 0.6f, 1.0f, 0.9f, 0.6f }, { 900.0f, 1200.0f, 1500.0f, 2000.0f }, 1.0f, 1.0f };

constexpr float catGain = 0.55f;
constexpr int controlRate = 16;          // the mouth moves every 16 samples

float clamp01 (float x) noexcept { return juce::jlimit (0.0f, 1.0f, x); }
float smooth (float x) noexcept { x = clamp01 (x); return x * x * (3.0f - 2.0f * x); }
float angerOf (float mood) noexcept { return clamp01 ((mood - 0.5f) * 2.0f); }
float sweetOf (float mood) noexcept { return clamp01 ((0.5f - mood) * 2.0f); }
float lionOf (float size) noexcept { return clamp01 ((size - 0.7f) / 0.3f); }

float polyBlep (float t, float dt) noexcept
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

/** Where the mouth is at t (0..1) of a meow, for this mood. */
Vowel meowVowel (float t, float mood, bool chatter) noexcept
{
    if (chatter)    // "ek": e - a - e, quick
    {
        if (t < 0.25f) return mix (vE, vA, smooth (t / 0.25f));
        return mix (vA, vE, smooth ((t - 0.25f) / 0.75f));
    }
    const float angry = angerOf (mood), sweet = sweetOf (mood);
    const float k1 = 0.10f + 0.08f * angry, k2 = 0.38f + 0.05f * angry, k3 = 0.72f, k4 = 0.90f;
    const Vowel open = mix (vE, vA, clamp01 (mood * 2.0f));
    const Vowel round = mix (vO, vU, sweet * 0.7f);
    if (t < k1) return mix (vM, vI, smooth (t / k1));
    if (t < k2) return mix (vI, open, smooth ((t - k1) / (k2 - k1)));
    if (t < k3) return mix (open, round, smooth ((t - k2) / (k3 - k2)));
    if (t < k4) return mix (round, vU, smooth ((t - k3) / (k4 - k3)));
    return mix (vU, vM, smooth ((t - k4) / (1.0f - k4)));
}

float holdPoint (float mood) noexcept { return 0.38f + 0.05f * angerOf (mood); }

struct Contour { float start, peak, peakAt, end; };

Contour contourOf (float mood) noexcept
{
    const Contour sweet { -1.0f, 3.0f, 0.65f, 2.0f }, normal { -2.0f, 2.5f, 0.35f, -5.0f }, angry { -4.0f, 1.0f, 0.30f, -8.0f };
    auto lerp = [] (const Contour& a, const Contour& b, float x)
    {
        return Contour { a.start + (b.start - a.start) * x, a.peak + (b.peak - a.peak) * x, a.peakAt + (b.peakAt - a.peakAt) * x, a.end + (b.end - a.end) * x };
    };
    return mood < 0.5f ? lerp (sweet, normal, mood * 2.0f) : lerp (normal, angry, (mood - 0.5f) * 2.0f);
}
} // namespace

Vowel mix (const Vowel& x, const Vowel& y, float t) noexcept
{
    Vowel v;
    for (int i = 0; i < formants; ++i)
    {
        v.f[i] = x.f[i] * std::pow (y.f[i] / x.f[i], t);      // move in pitch, not in Hz
        v.a[i] = x.a[i] + (y.a[i] - x.a[i]) * t;
        v.bw[i] = x.bw[i] + (y.bw[i] - x.bw[i]) * t;
    }
    v.loud = x.loud + (y.loud - x.loud) * t;
    v.open = x.open + (y.open - x.open) * t;
    return v;
}

float formantScale (float size) noexcept { return 1.55f * std::pow (0.7f / 1.55f, clamp01 (size)); }

float meowSeconds (float length, float mood) noexcept
{
    const float base = 0.25f + std::pow (clamp01 (length), 1.5f) * 2.5f;
    return base * (0.7f + 0.8f * clamp01 (mood));
}

//==============================================================================
void Engine::Svf::set (float hz, float q, float rate) noexcept
{
    g = std::tan (pi * juce::jlimit (20.0f, rate * 0.45f, hz) / rate);
    k = 1.0f / juce::jmax (0.5f, q);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

float Engine::Svf::band (float x) noexcept
{
    const float v3 = x - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    return v1 * k;
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.78f; rp.damping = 0.45f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
    reverb.setParameters (rp);
    reverb.setSampleRate (sr);
    echoL.assign ((size_t) (sr * 0.5) + 8, 0.0f);
    echoR.assign ((size_t) (sr * 0.5) + 8, 0.0f);
    alleySmooth.reset (sr, 0.05);
    outSmooth.reset (sr, 0.05);
    juce::ignoreUnused (maxBlock);
    reset();
}

void Engine::reset()
{
    for (auto& n : notes) n = Note();
    reverb.reset();
    std::fill (echoL.begin(), echoL.end(), 0.0f);
    std::fill (echoR.begin(), echoR.end(), 0.0f);
    echoPos = 0;
    pedal = false;
    alleySmooth.setCurrentAndTargetValue (settings.alley);
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (settings.outDb));
    mouthNow = hissNow = purrNow = 0.0f;
}

bool Engine::noteBusy (const Note& n) const noexcept
{
    if (! n.active) return false;
    if (n.held || n.sustained || n.purrLevel > 1.0e-4f) return true;
    for (auto& c : n.cats) if (c.active) return true;
    return false;
}

int Engine::activeNotes() const noexcept
{
    int k = 0;
    for (auto& n : notes) if (noteBusy (n)) ++k;
    return k;
}

int Engine::activeCats() const noexcept
{
    int k = 0;
    for (auto& n : notes) for (auto& c : n.cats) if (c.active) ++k;
    return k;
}

void Engine::startCats (Note& n, bool chatter)
{
    const int choir = juce::jlimit (1, maxCats, settings.choir);
    const float chaos = clamp01 (settings.chaos);
    const bool hiss = ! chatter && settings.hiss > 0.001f && n.velocity >= 1.0f - 0.35f * settings.hiss;
    for (int k = 0; k < maxCats; ++k)
    {
        auto& c = n.cats[(size_t) k];
        if (k >= choir) { if (! chatter) c.active = false; continue; }
        const uint32_t keep = c.seed;
        c = Cat();
        c.seed = keep * 747796405u + 2891336453u + (uint32_t) (n.note * 131 + k * 7919) + clock;
        c.active = true;
        c.hiss = hiss;
        const bool lead = k == 0;
        auto r = [this] { return random01() - 0.5f; };   // -0.5 .. 0.5
        c.sizeShift = lead ? r() * 0.3f * chaos : r() * 0.5f;
        c.detune = lead ? r() * 30.0f * chaos : r() * (30.0f + 80.0f * chaos);
        c.delay = lead ? 0.0f : (chatter ? 0.012f : 0.03f + 0.12f * chaos) * random01();
        c.pan = lead ? r() * 0.3f * chaos : juce::jlimit (-0.9f, 0.9f, ((k % 2 == 0) ? 1.0f : -1.0f) * (0.25f + 0.6f * random01()));
        c.gain = 1.0f / std::sqrt ((float) choir);
        const Contour ct = contourOf (settings.mood);
        c.peak = ct.peak + r() * 4.0f * chaos;
        if (chatter)
            c.seconds = 0.055f + 0.035f * random01();
        else if (hiss)
            c.seconds = 0.35f + clamp01 (settings.length) * 0.9f;
        else
            c.seconds = meowSeconds (settings.length, settings.mood) * (1.0f + r() * 0.8f * chaos) * (lead ? 1.0f : 0.85f + 0.3f * random01());
        if (! chatter && ! hiss && random01() < chaos * 0.4f) c.crackAt = 0.25f + 0.5f * random01();
        c.jitter = 0.0f;
        c.phase = random01();
        c.counter = 0;
        for (auto& f : c.f) f.clear();
        // 'chatter' is remembered through the cat's seconds (they are tiny); the shape is picked by the note's mode
    }
    if (! chatter) ++meows;
    lastNote = n.note;
}

void Engine::noteOn (int note, float velocity)
{
    ++clock;
    Note* slot = nullptr;
    for (auto& n : notes) if (n.active && n.note == note) { slot = &n; break; }
    if (slot == nullptr) for (auto& n : notes) if (! noteBusy (n)) { slot = &n; break; }
    if (slot == nullptr)
    {
        slot = &notes[0];
        for (auto& n : notes) if (n.age < slot->age) slot = &n;    // the oldest cat makes room
    }
    auto& n = *slot;
    const bool same = n.active && n.note == note;
    n.active = true;
    n.held = true;
    n.sustained = false;
    n.note = note;
    n.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    n.age = clock;
    if (! same) { n.purrLevel = 0.0f; n.purrPhase = 0.0f; n.purrBreath = 0.0f; n.purrBurst = 0.0f; n.purrLp1 = n.purrLp2 = 0.0f; for (auto& f : n.purrF) f.clear(); }
    n.chatterNext = 0.0;
    n.chatterCount = 0;
    if (settings.mode == Mode::Meow || settings.mode == Mode::Sing) startCats (n, false);
    else if (settings.mode == Mode::Chatter) { for (auto& c : n.cats) c.active = false; ++meows; lastNote = note; }
    else { for (auto& c : n.cats) c.active = false; lastNote = note; }
}

void Engine::noteOff (int note)
{
    for (auto& n : notes)
        if (n.active && n.note == note && n.held)
        {
            n.held = false;
            n.sustained = pedal;
        }
}

void Engine::sustain (bool down)
{
    pedal = down;
    if (! down) for (auto& n : notes) n.sustained = false;
}

void Engine::allNotesOff()
{
    for (auto& n : notes) { n.held = false; n.sustained = false; }
    pedal = false;
}

float Engine::renderCat (Cat& c, const Note& n, float& outL, float& outR) noexcept
{
    if (c.delay > 0.0f) { c.delay -= (float) (1.0 / sr); return 0.0f; }

    const bool chatter = settings.mode == Mode::Chatter;
    const bool sing = settings.mode == Mode::Sing && ! c.hiss;
    const float angry = angerOf (settings.mood);
    const float size = clamp01 (settings.size + c.sizeShift);
    const float lion = lionOf (size);
    const float hold = holdPoint (settings.mood);
    const bool holding = sing && (n.held || n.sustained);

    if (c.counter <= 0)
    {
        c.counter = controlRate;
        const float scale = formantScale (size);
        const Vowel v = c.hiss ? vHiss : meowVowel (c.t, settings.mood, chatter);
        const float fs = c.hiss ? std::sqrt (scale) : scale;
        for (int i = 0; i < formants; ++i)
        {
            c.f[i].set (v.f[i] * fs, v.f[i] / (v.bw[i] * std::sqrt (fs)), (float) sr);
            c.fa[i] = v.a[i];
        }
        c.loud = v.loud;
        c.open = v.open;

        // the pitch, in semitones from the note
        float st = 0.0f;
        const Contour ct = contourOf (settings.mood);
        if (chatter)
            st = 4.0f - 7.0f * c.t;
        else if (sing)
            st = c.t < hold ? ct.start * (1.0f - smooth (c.t / hold)) : ct.end * smooth ((c.t - hold) / (1.0f - hold)) * 0.6f;
        else
            st = c.t < ct.peakAt ? ct.start + (c.peak - ct.start) * smooth (c.t / ct.peakAt)
                                 : c.peak + (ct.end - c.peak) * smooth ((c.t - ct.peakAt) / (1.0f - ct.peakAt));
        if (c.crackAt > 0.0f && std::abs (c.t - c.crackAt) * c.seconds < 0.04f) st += 7.0f;     // the voice cracks
        const float elapsed = c.t * c.seconds;
        const float vibDepth = 0.12f + 1.1f * std::pow (angry, 1.5f) + (holding ? 0.25f * clamp01 (elapsed - 0.3f) : 0.0f);
        const float vibRate = 5.2f + 1.5f * angry;
        st += vibDepth * clamp01 (c.t * 3.0f) * std::sin (2.0f * pi * vibRate * elapsed + (float) c.seed * 1.0e-6f);
        const float semis = (float) (n.note + 12 * settings.octave - 69) + st + c.detune * 0.01f + bend * 2.0f;
        const float hz = juce::jlimit (20.0f, (float) sr * 0.4f, 440.0f * std::exp2 (semis / 12.0f));
        c.tilt = hz / (float) sr;   // the phase step

        // how loud now
        float edge = clamp01 (elapsed / (chatter ? 0.004f : (c.hiss ? 0.006f : 0.025f)));
        if (! holding || c.t < hold) edge *= c.t > 0.92f ? clamp01 ((1.0f - c.t) / 0.08f) : 1.0f;
        if (c.hiss) edge *= (c.t < 0.06f ? 1.4f : 1.0f) * std::pow (1.0f - c.t, 0.7f);
        c.amp = edge;

        // move through the meow
        const float step = (float) controlRate / ((float) sr * juce::jmax (0.02f, c.seconds));
        if (holding && c.t + step >= hold) c.t = juce::jmax (c.t, hold);
        else c.t += step;
        if (c.t >= 1.0f) { c.active = false; return 0.0f; }
    }
    --c.counter;

    // the voice
    float src;
    if (c.hiss)
        src = noise (c.seed) * 0.8f;
    else
    {
        const float rough = juce::jmin (0.85f, angry * 0.7f + lion * 0.5f);
        const float dt = c.tilt * (1.0f + c.jitter);
        c.phase += dt;
        if (c.phase >= 1.0f)
        {
            c.phase -= 1.0f;
            ++c.period;
            c.jitter = noise (c.seed) * 0.03f * rough;
        }
        const float saw = 2.0f * c.phase - 1.0f - polyBlep (c.phase, dt);
        const float sub = (c.period & 1) ? 1.0f - 0.6f * rough : 1.0f;     // every other period weaker: a growl
        src = saw * sub + noise (c.seed) * (0.05f + 0.1f * angry + 0.15f * lion);
    }

    float y = 0.0f;
    for (int i = 0; i < formants; ++i) y += c.fa[i] * c.f[i].band (src);
    y *= c.amp * c.loud * c.gain * catGain * (0.25f + 0.75f * n.velocity);

    const float a = (c.pan + 1.0f) * pi * 0.25f;
    outL += y * std::cos (a) * 1.4142f;
    outR += y * std::sin (a) * 1.4142f;
    return c.amp * c.open;
}

float Engine::renderPurr (Note& n, float amount) noexcept
{
    const bool on = (n.held || n.sustained) && amount > 0.001f;
    const float target = on ? amount : 0.0f;
    const float speed = (float) (1.0 / (sr * (on ? 0.15 : 0.3)));
    n.purrLevel += juce::jlimit (-speed, speed, target - n.purrLevel);
    if (n.purrLevel < 1.0e-4f && ! on) { n.purrLevel = 0.0f; return 0.0f; }

    const float size = clamp01 (settings.size);
    n.purrBreath += (float) (1.0 / (2.4 * sr));
    if (n.purrBreath >= 1.0f) n.purrBreath -= 1.0f;
    const float inhale = n.purrBreath < 0.42f ? 1.0f : 0.0f;
    const float edge = smooth (std::min (n.purrBreath, std::abs (n.purrBreath - 0.42f)) / 0.06f);      // soft turns
    const float fp = (26.0f - 8.0f * size) * (1.0f + (float) (n.note - 60) / 120.0f) * (inhale > 0.5f ? 1.12f : 1.0f);
    n.purrPhase += fp / (float) sr;
    if (n.purrPhase >= 1.0f) { n.purrPhase -= 1.0f; n.purrBurst = 1.0f; }
    n.purrBurst *= std::exp (-1.0f / (0.006f * (float) sr));
    uint32_t s = rng;
    const float exc = (noise (s) * 0.8f + 0.6f) * n.purrBurst;
    rng = s;
    float y = n.purrF[0].band (exc) + 0.5f * n.purrF[1].band (exc);
    const float lp = 1.0f - std::exp (-2.0f * pi * 1100.0f / (float) sr);     // a purr is low: no clicks up high
    n.purrLp1 += (y - n.purrLp1) * lp;
    n.purrLp2 += (n.purrLp1 - n.purrLp2) * lp;
    y = n.purrLp2 * 1.6f;
    const float breathGain = (inhale > 0.5f ? 0.55f : 1.0f) * (0.35f + 0.65f * edge);
    return y * breathGain * n.purrLevel * 0.9f * (0.4f + 0.6f * n.velocity);
}

void Engine::render (float* l, float* r, int numSamples)
{
    const float purrAmount = settings.mode == Mode::Purr ? juce::jmax (0.85f, settings.purr, wheel) : juce::jmax (settings.purr, wheel);
    const double chatterStep = (settings.playing && settings.bpm > 20.0) ? 60.0 / settings.bpm / 4.0 * sr
                                                                        : sr / (7.0 + 6.0 * (1.0 - clamp01 (settings.length)));
    float mouthPeak = 0.0f, hissPeak = 0.0f, purrPeak = 0.0f;
    for (auto& n : notes)
    {
        if (! n.active) continue;
        {
            const float scale = formantScale (clamp01 (settings.size));
            n.purrF[0].set (220.0f * scale, 1.2f, (float) sr);
            n.purrF[1].set (650.0f * scale, 2.0f, (float) sr);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            if (settings.mode == Mode::Chatter && n.held)
            {
                n.chatterNext -= 1.0;
                if (n.chatterNext <= 0.0)
                {
                    const float chaos = clamp01 (settings.chaos);
                    n.chatterNext += chatterStep * (1.0 + (random01() - 0.5) * 0.3 * chaos);
                    if (random01() >= 0.15f * chaos || n.chatterCount == 0) startCats (n, true);
                    ++n.chatterCount;
                }
            }
            float sl = 0.0f, sr2 = 0.0f;
            for (auto& c : n.cats)
                if (c.active)
                {
                    const float open = renderCat (c, n, sl, sr2);
                    mouthPeak = juce::jmax (mouthPeak, open * n.velocity);
                    if (c.hiss) hissPeak = juce::jmax (hissPeak, c.amp);
                }
            if (n.purrLevel > 0.0f || ((n.held || n.sustained) && purrAmount > 0.001f))
            {
                const float p = renderPurr (n, purrAmount);
                sl += p; sr2 += p;
                purrPeak = juce::jmax (purrPeak, n.purrLevel);
            }
            l[i] += sl;
            r[i] += sr2;
        }
        if (! noteBusy (n)) n.active = false;
    }
    ++clock;
    const float k = 0.35f;
    mouthNow += (mouthPeak - mouthNow) * k;
    hissNow += (hissPeak - hissNow) * k;
    purrNow += (purrPeak - purrNow) * k;
    mouth = mouthNow;
    hissing = hissNow;
    purring = purrNow;
}

void Engine::finish (float* l, float* r, int numSamples)
{
    alleySmooth.setTargetValue (clamp01 (settings.alley));
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (settings.outDb));
    const int dl = (int) (0.23 * sr), dr = (int) (0.31 * sr);
    const int len = (int) echoL.size();

    constexpr int chunk = 256;
    float wl[chunk], wr[chunk];
    float peak = 0.0f;
    for (int start = 0; start < numSamples; start += chunk)
    {
        const int m = juce::jmin (chunk, numSamples - start);
        std::copy (l + start, l + start + m, wl);
        std::copy (r + start, r + start + m, wr);
        reverb.processStereo (wl, wr, m);
        for (int i = 0; i < m; ++i)
        {
            const float a = alleySmooth.getNextValue();
            const float g = outSmooth.getNextValue();
            const int j = start + i;
            const float el = echoL[(size_t) ((echoPos - dl + len) % len)];
            const float er = echoR[(size_t) ((echoPos - dr + len) % len)];
            echoL[(size_t) echoPos] = (l[j] + er * 0.38f) * 0.9f;    // crossed: the echo bounces between the walls
            echoR[(size_t) echoPos] = (r[j] + el * 0.38f) * 0.9f;
            echoPos = (echoPos + 1) % len;
            const float dry = 1.0f - 0.35f * a;
            l[j] = (l[j] * dry + wl[i] * 0.55f * a + el * 0.45f * a) * g;
            r[j] = (r[j] * dry + wr[i] * 0.55f * a + er * 0.45f * a) * g;
            peak = juce::jmax (peak, std::abs (l[j]), std::abs (r[j]));
        }
    }
    level = level.load() * 0.8f + peak * 0.2f;
}
} // namespace meow
