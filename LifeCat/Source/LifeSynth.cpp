#include "LifeSynth.h"

namespace lifecat
{
juce::StringArray themeNames() { return { "Garden", "Ocean", "Space", "Cells" }; }
juce::StringArray soundTypeNames() { return { "Natural", "Gene Mix", "Pad", "Bell", "Pluck", "Percussion", "Drone", "Texture" }; }

SoundType naturalSound (Role r) noexcept
{
    switch (r)
    {
        case Role::Pollinator: return SoundType::Pluck;
        case Role::Scavenger:  return SoundType::Percussion;
        case Role::Giant:      return SoundType::Drone;
        case Role::Swarm:      return SoundType::Pluck;
        case Role::Drummer:    return SoundType::Percussion;
        case Role::Builder:    return SoundType::Bell;
        default:               return SoundType::Genes;      // singers: their genes (with glide + vibrato)
    }
}

namespace
{
    constexpr float twoPi = 6.28318530718f;

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    inline float fastSin (float x) noexcept { return std::sin (x); }

    float lerpTable (const float* table, int n, float u) noexcept
    {
        const float pos = juce::jlimit (0.0f, 1.0f, u) * (float) (n - 1);
        const int i = std::min ((int) pos, n - 2);
        const float f = pos - (float) i;
        return table[i] + f * (table[i + 1] - table[i]);
    }
}

void Synth::prepare (double sr, int maxBlock)
{
    limiter.prepare (sr);
    sampleRate = sr;
    for (auto& v : voices) v.active = false;
    delayL.assign ((size_t) (sr * 4.0) + 2, 0.0f);
    delayR.assign ((size_t) (sr * 4.0) + 2, 0.0f);
    delayPos = 0;
    delaySamples = 0.0f;
    reverb.setSampleRate (sr);
    reverb.reset();
    lastSize = -1.0f;
    wetL.assign ((size_t) juce::jmax (512, maxBlock), 0.0f);
    wetR.assign ((size_t) juce::jmax (512, maxBlock), 0.0f);
    echoL.assign ((size_t) juce::jmax (512, maxBlock), 0.0f);
    echoR.assign ((size_t) juce::jmax (512, maxBlock), 0.0f);
}

int Synth::activeVoices() const noexcept
{
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Synth::allOff()
{
    for (auto& v : voices) if (v.active) v.releasing = true;
}

void Synth::setupVoice (Voice& v, const NoteRequest& r)
{
    const auto& g = r.genes;
    v.req = r;
    v.freq = 440.0f * std::pow (2.0f, (r.midiNote - 69.0f) / 12.0f);
    v.t = 0.0;
    v.env = 0.0f;
    v.releasing = false;
    v.age = ++ageCounter;
    v.noise = 0x1234567u + v.age * 2654435761u;

    // theme colour + timbre gene -> mix of pad / bell / texture
    static const float base[4][3] = { { 0.25f, 0.75f, 0.00f },     // Garden: bells, kalimba
                                      { 0.75f, 0.15f, 0.10f },     // Ocean: watery pads
                                      { 0.35f, 0.15f, 0.50f },     // Space: shimmering textures
                                      { 0.10f, 0.60f, 0.30f } };   // Cells: glassy clicks
    const int th = juce::jlimit (0, 3, (int) settings.theme);
    const float t = g[gTimbre];
    float wp = base[th][0] + (0.5f - t) * 0.9f;
    float wb = base[th][1] + 0.35f - std::abs (t - 0.5f) * 0.7f;
    float wt = base[th][2] + (t - 0.5f) * 0.9f;
    wp = juce::jmax (0.0f, wp); wb = juce::jmax (0.0f, wb); wt = juce::jmax (0.0f, wt);
    float wperc = 0.0f, wsub = 0.0f;
    switch (r.sound)
    {
        case SoundType::Pad:        wp = 1.0f; wb = wt = 0.0f; break;
        case SoundType::Bell:       wb = 1.0f; wp = wt = 0.0f; break;
        case SoundType::Pluck:      wb = 1.0f; wp = 0.0f; wt = 0.0f; wperc = 0.25f; break;
        case SoundType::Percussion: wperc = 1.0f; wp = wb = 0.0f; wt = 0.15f; break;
        case SoundType::Drone:      wp = 0.8f; wb = 0.0f; wt = 0.2f; wsub = 0.7f; break;
        case SoundType::Texture:    wt = 1.0f; wp = wb = 0.0f; break;
        case SoundType::Genes:
        default: break;
    }
    const float sum = juce::jmax (0.001f, wp + wb + wt);
    v.wPad = wp / sum; v.wBell = wb / sum; v.wTex = wt / sum;
    v.wPerc = wperc; v.wSub = wsub;

    const float atkScale = std::pow (4.0f, settings.attack), relScale = std::pow (4.0f, settings.release);
    v.attackSec = juce::jlimit (0.001f, 4.0f, 0.002f * std::pow (1000.0f, g[gAttack]) * atkScale * (th == 3 ? 0.3f : 1.0f));
    v.releaseSec = juce::jlimit (0.03f, 10.0f, 0.08f * std::pow (60.0f, g[gRelease]) * relScale * (th == 1 || th == 2 ? 1.6f : 1.0f) * (th == 3 ? 0.5f : 1.0f));
    v.bellDecay = (0.15f + 2.5f * g[gRelease]) * (th == 3 ? 0.35f : (th == 0 ? 0.8f : 1.2f)) * relScale;
    if (r.sound == SoundType::Pluck) v.bellDecay = juce::jmin (v.bellDecay, 0.12f + 0.5f * g[gRelease]);
    if (r.sound == SoundType::Drone) v.attackSec = juce::jmax (v.attackSec, 0.25f);
    v.bellAmp = v.idxAmp = 1.0f;
    v.bellMul = std::exp (-1.0f / (v.bellDecay * (float) sampleRate));
    v.idxMul = std::exp (-1.0f / ((v.bellDecay * 0.35f + 0.02f) * (float) sampleRate));

    static const float ratios[] = { 1.0f, 2.0f, 2.76f, 3.0f, 3.5f, 4.2f, 5.4f, 7.1f };
    v.fmRatio = lerpTable (ratios, 8, g[gHarm]) + (th == 3 ? 1.5f : 0.0f);
    const float bright = juce::jlimit (0.0f, 1.0f, 0.1f + 0.8f * g[gBright] + 0.4f * settings.brightness);
    v.fmIndex = 0.6f + 4.0f * bright;
    if (r.sound == SoundType::Pluck) { v.fmRatio = g[gHarm] < 0.5f ? 1.0f : 2.0f; v.fmIndex += 1.5f; }

    // percussion: a tuned noise burst with a falling "body" tone
    const float percDecay = 0.035f + 0.22f * g[gRelease] * relScale;
    v.percAmp = 1.0f;
    v.percMul = std::exp (-1.0f / (percDecay * (float) sampleRate));
    v.percBodyPh = 0.0f;
    v.percBodyMul = 1.0f + 1.5f * g[gHarm];
    v.subPh = 0.0f;

    // glide: slide up into the note from below
    const float semis = g[gGlide] > 0.15f ? 1.0f + 6.0f * g[gGlide] : 0.0f;
    v.glideMul = std::pow (2.0f, -semis / 12.0f);
    v.glideCoef = std::exp (-1.0f / ((0.02f + 0.22f * g[gGlide]) * (float) sampleRate));
    v.echo = g[gEcho] * g[gEcho];
    const float spread = 0.002f + 0.012f * g[gHarm];
    v.detune[0] = 1.0f; v.detune[1] = 1.0f + spread; v.detune[2] = 1.0f - spread * 0.9f;
    for (int i = 0; i < 3; ++i) v.ph[i] = (float) ((v.age * (i + 1) * 7919u) % 1000u) / 1000.0f;
    v.fmCar = v.fmMod = v.shimPh = v.vibPh = v.wobPh = 0.0f;
    v.panPh = g[gPan] * twoPi;
    v.lpIc1 = v.lpIc2 = v.bp1a = v.bp1b = v.bp2a = v.bp2b = 0.0f;

    // keep the level even across the register (high notes are perceived louder)
    const float regComp = std::pow (220.0f / juce::jlimit (55.0f, 3000.0f, v.freq), 0.18f);
    static const float themeGain[] = { 1.0f, 1.0f, 0.62f, 0.95f };
    v.gain = r.velocity * 0.18f * regComp * themeGain[th] * juce::jmax (0.0f, r.gain) * (r.sound == SoundType::Pluck ? 0.75f : 1.0f);
    const float pan = juce::jlimit (-1.0f, 1.0f, r.pan * settings.width);
    v.panL = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * 1.41f;
    v.panR = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * 1.41f;
}

void Synth::noteOn (const NoteRequest& r)
{
    // polyphony counts only voices that are not fading out
    int sounding = 0;
    for (const auto& v : voices) if (v.active && ! v.fading) ++sounding;
    if (sounding >= maxVoices)
    {
        // steal the quietest releasing voice, else the oldest - and fade it out instead of cutting it
        Voice* victim = nullptr;
        float best = 1.0e9f;
        for (auto& v : voices)
            if (v.active && ! v.fading && v.releasing && v.env * v.gain < best) { best = v.env * v.gain; victim = &v; }
        if (victim == nullptr)
            for (auto& v : voices)
                if (v.active && ! v.fading && (victim == nullptr || v.age < victim->age)) victim = &v;
        if (victim != nullptr)
        {
            victim->fading = true;
            victim->fadeGain = 1.0f;
            victim->fadeStep = 1.0f / (0.008f * (float) sampleRate);
        }
    }

    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr)   // every slot busy (many steals at once): reuse the most faded one
        for (auto& v : voices)
            if (v.fading && (target == nullptr || v.fadeGain < target->fadeGain)) target = &v;
    if (target == nullptr) return;
    setupVoice (*target, r);
    target->fading = false;
    target->fadeGain = 1.0f;
    target->active = true;
}

void Synth::render (float* left, float* right, int n)
{
    std::fill (left, left + n, 0.0f);
    std::fill (right, right + n, 0.0f);
    const int echoN = juce::jmin (n, (int) echoL.size());
    std::fill (echoL.begin(), echoL.begin() + echoN, 0.0f);
    std::fill (echoR.begin(), echoR.begin() + echoN, 0.0f);
    const float sr = (float) sampleRate;
    const float dtSec = 1.0f / sr;
    const int th = juce::jlimit (0, 3, (int) settings.theme);

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const auto& g = v.req.genes;
        const float attCoef = 1.0f - std::exp (-1.0f / (v.attackSec * sr * 0.25f));
        const float relCoef = std::exp (-6.9f / (v.releaseSec * sr));
        const float vibDepth = (2.0f + 28.0f * g[gVibrato] + (th == 1 ? 10.0f : 0.0f)) / 1200.0f;
        const float vibRate = 3.5f + 3.0f * g[gVibrato];
        const float wobDepth = 0.15f + 0.85f * g[gWobble], wobRate = 0.08f + 1.2f * g[gWobble];
        const float shim = g[gShimmer] * (th == 2 ? 0.45f : 0.28f);
        const float bright = juce::jlimit (0.0f, 1.0f, 0.1f + 0.8f * g[gBright] + 0.4f * settings.brightness);
        const float q = 15.0f + 70.0f * g[gHarm];
        float a1 = 0, a2 = 0, a3 = 0, k = 1.4f;
        float rc1 = 0, rr1 = 0, rc2 = 0, rr2 = 0;

        for (int i = 0; i < n; ++i)
        {
            // every 16 samples: filters follow vibrato / wobble
            if ((i & 15) == 0)
            {
                const float f = v.freq;
                const float cutoff = juce::jlimit (60.0f, sr * 0.45f, f * (1.5f + 20.0f * bright) * (1.0f + 0.5f * wobDepth * std::sin (v.wobPh)));
                const float gg = std::tan (juce::MathConstants<float>::pi * cutoff / sr);
                k = 1.6f - 0.9f * g[gHarm];
                a1 = 1.0f / (1.0f + gg * (gg + k));
                a2 = gg * a1;
                a3 = gg * a2;
                const float w1 = twoPi * juce::jmin (f, sr * 0.45f) / sr, w2 = twoPi * juce::jmin (f * 2.0f, sr * 0.45f) / sr;
                rr1 = std::exp (-juce::MathConstants<float>::pi * f / (q * sr));
                rr2 = std::exp (-juce::MathConstants<float>::pi * f * 2.0f / (q * sr));
                rc1 = 2.0f * rr1 * std::cos (w1);
                rc2 = 2.0f * rr2 * std::cos (w2);
            }

            // envelope
            if (! v.releasing)
            {
                v.env += (1.0f - v.env) * attCoef;
                if (v.t >= v.req.lengthSec) v.releasing = true;
            }
            else
                v.env *= relCoef;

            v.vibPh += twoPi * vibRate * dtSec;
            v.wobPh += twoPi * wobRate * dtSec;
            v.panPh += twoPi * 0.07f * dtSec;
            if (v.vibPh > twoPi) v.vibPh -= twoPi;
            if (v.wobPh > twoPi) v.wobPh -= twoPi;
            const float vibFade = juce::jmin (1.0f, (float) v.t / 0.4f);
            v.glideMul = 1.0f + (v.glideMul - 1.0f) * v.glideCoef;
            const float f = v.freq * v.glideMul * (1.0f + vibDepth * vibFade * std::sin (v.vibPh) * 0.69f);
            const float inc = f / sr;

            float out = 0.0f;
            // pad: three detuned saws -> low-pass
            if (v.wPad > 0.001f)
            {
                float saw = 0.0f;
                for (int s = 0; s < 3; ++s)
                {
                    const float di = inc * v.detune[s];
                    v.ph[s] += di;
                    if (v.ph[s] >= 1.0f) v.ph[s] -= 1.0f;
                    saw += 2.0f * v.ph[s] - 1.0f - polyBlep (v.ph[s], di);
                }
                saw *= 0.33f;
                const float v3 = saw - v.lpIc2;
                const float v1 = a1 * v.lpIc1 + a2 * v3;
                const float v2 = v.lpIc2 + a2 * v.lpIc1 + a3 * v3;
                v.lpIc1 = 2.0f * v1 - v.lpIc1;
                v.lpIc2 = 2.0f * v2 - v.lpIc2;
                out += v.wPad * v2 * 0.9f * v.env;
            }
            // bell: 2-operator FM with a decaying index
            if (v.wBell > 0.001f)
            {
                const float tt = (float) v.t;
                const float bellEnv = v.bellAmp * juce::jmin (1.0f, tt / 0.002f) * (v.releasing ? v.env : 1.0f);
                const float idx = v.fmIndex * v.idxAmp;
                v.bellAmp *= v.bellMul;
                v.idxAmp *= v.idxMul;
                v.fmMod += twoPi * inc * v.fmRatio;
                v.fmCar += twoPi * inc;
                if (v.fmMod > twoPi) v.fmMod -= twoPi;
                if (v.fmCar > twoPi) v.fmCar -= twoPi;
                out += v.wBell * std::sin (v.fmCar + idx * std::sin (v.fmMod)) * bellEnv * 0.8f;
            }
            // texture: noise through two resonators (fundamental + octave)
            if (v.wTex > 0.001f)
            {
                v.noise ^= v.noise << 13; v.noise ^= v.noise >> 17; v.noise ^= v.noise << 5;
                const float white = (float) (int32_t) v.noise * (1.0f / 2147483648.0f);
                const float y1 = (1.0f - rr1) * white * 6.0f + rc1 * v.bp1a - rr1 * rr1 * v.bp1b;
                v.bp1b = v.bp1a; v.bp1a = y1;
                const float y2 = (1.0f - rr2) * white * 4.0f + rc2 * v.bp2a - rr2 * rr2 * v.bp2b;
                v.bp2b = v.bp2a; v.bp2a = y2;
                out += v.wTex * (y1 + 0.5f * y2 * bright) * v.env;
            }
            // shimmer: an octave above, gently trembling
            if (shim > 0.001f)
            {
                v.shimPh += twoPi * inc * 2.0f;
                if (v.shimPh > twoPi) v.shimPh -= twoPi;
                out += shim * std::sin (v.shimPh) * (0.6f + 0.4f * std::sin (v.vibPh * 1.7f)) * v.env * 0.6f;
            }

            // percussion: noise burst through the resonator + a falling body tone
            if (v.wPerc > 0.001f)
            {
                v.noise ^= v.noise << 13; v.noise ^= v.noise >> 17; v.noise ^= v.noise << 5;
                const float white = (float) (int32_t) v.noise * (1.0f / 2147483648.0f);
                const float y1 = (1.0f - rr1) * white * 8.0f + rc1 * v.bp1a - rr1 * rr1 * v.bp1b;
                v.bp1b = v.bp1a; v.bp1a = y1;
                v.percBodyMul = 1.0f + (v.percBodyMul - 1.0f) * 0.9993f;
                v.percBodyPh += twoPi * inc * v.percBodyMul;
                if (v.percBodyPh > twoPi) v.percBodyPh -= twoPi;
                out += v.wPerc * v.percAmp * (0.33f * y1 + 0.38f * std::sin (v.percBodyPh) + 0.15f * white * v.percAmp);
                v.percAmp *= v.percMul;
            }
            // drone: a sub-octave sine under the pad
            if (v.wSub > 0.001f)
            {
                v.subPh += twoPi * inc * 0.5f;
                if (v.subPh > twoPi) v.subPh -= twoPi;
                out += v.wSub * std::sin (v.subPh) * v.env * 0.8f;
            }

            out *= v.gain;
            if (v.fading)
            {
                out *= v.fadeGain;
                v.fadeGain -= v.fadeStep;
                if (v.fadeGain <= 0.0f) { v.active = false; break; }
            }
            const float drift = 0.25f * g[gPan] * std::sin (v.panPh);
            const float outL = out * juce::jlimit (0.0f, 1.5f, v.panL - drift), outR = out * juce::jlimit (0.0f, 1.5f, v.panR + drift);
            left[i] += outL;
            right[i] += outR;
            if (v.echo > 0.001f && i < echoN) { echoL[(size_t) i] += outL * v.echo; echoR[(size_t) i] += outR * v.echo; }
            v.t += dtSec;
        }

        if (v.releasing && v.env < 1.0e-4f && (v.wBell < 0.001f || v.bellAmp < 1.0e-3f || v.env < 1.0e-5f) && (v.wPerc < 0.001f || v.percAmp < 1.0e-3f))
            v.active = false;
        if (v.wPerc > 0.5f && v.wPad < 0.001f && v.wBell < 0.001f && v.percAmp < 1.0e-3f && v.t > 0.05) v.active = false;
        if (v.t > v.req.lengthSec + v.releaseSec * 3.0f + 0.5f && v.env < 1.0e-3f) v.active = false;
    }

    // ---- delay (ping-pong, tempo synced)
    const int dsize = (int) delayL.size();
    const float target = juce::jlimit (1.0f, (float) dsize - 2.0f, (float) (settings.delayBeats * 60.0 / juce::jmax (30.0, settings.bpm) * sampleRate));
    if (delaySamples < 1.0f) delaySamples = target;
    for (int i = 0; i < n; ++i)
    {
        delaySamples += (target - delaySamples) * 0.0005f;
        float rp = (float) delayPos - delaySamples;
        while (rp < 0.0f) rp += (float) dsize;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const float dl = delayL[(size_t) i0] * (1.0f - fr) + delayL[(size_t) ((i0 + 1) % dsize)] * fr;
        const float dr = delayR[(size_t) i0] * (1.0f - fr) + delayR[(size_t) ((i0 + 1) % dsize)] * fr;
        const float sendL = i < echoN ? echoL[(size_t) i] : 0.0f, sendR = i < echoN ? echoR[(size_t) i] : 0.0f;
        delayL[(size_t) delayPos] = (0.5f * (left[i] + right[i]) * 0.8f * settings.delayMix + 0.6f * (sendL + sendR)) + dr * settings.delayFeedback;
        delayR[(size_t) delayPos] = dl * settings.delayFeedback;
        delayPos = (delayPos + 1) % dsize;
        left[i] += dl;
        right[i] += dr;
    }

    // ---- weather you can hear: rain hiss + drops, wind gusts, snow hush
    const float amb = settings.ambience;
    if (amb > 0.001f && (settings.rain > 0.01f || settings.wind > 0.01f || settings.snow > 0.01f))
    {
        const float rainLvl = amb * settings.rain * 0.05f, windLvl = amb * settings.wind * 0.09f, snowLvl = amb * settings.snow * 0.03f;
        const float dropChance = settings.rain * 9.0f / sr;
        for (int i = 0; i < n; ++i)
        {
            ambNoise ^= ambNoise << 13; ambNoise ^= ambNoise >> 17; ambNoise ^= ambNoise << 5;
            const float w = (float) (int32_t) ambNoise * (1.0f / 2147483648.0f);
            // rain: bright hiss
            rainLp += 0.35f * (w - rainLp);
            const float hiss = w - rainLp;
            // drops: little resonant plinks
            if ((float) (ambNoise & 0xffffff) * (1.0f / 16777216.0f) < dropChance) { dropEnv = 1.0f; dropFreq = 1500.0f + 3500.0f * (float) ((ambNoise >> 8) & 0xff) / 255.0f; }
            dropPh += twoPi * dropFreq / sr;
            if (dropPh > twoPi) dropPh -= twoPi;
            dropEnv *= 0.9985f;
            const float drop = std::sin (dropPh) * dropEnv * dropEnv;
            // wind: dark noise swelling slowly
            windPhase += twoPi * 0.11f / sr;
            if (windPhase > twoPi) windPhase -= twoPi;
            const float gustCoef = 0.004f + 0.012f * (0.5f + 0.5f * std::sin (windPhase));
            windLp1 += gustCoef * (w - windLp1);
            windLp2 += gustCoef * (windLp1 - windLp2);
            const float windSig = windLp2 * (6.0f + 4.0f * std::sin (windPhase * 2.3f));
            // snow: very soft airy hush
            snowLp += 0.08f * (w - snowLp);
            const float a = hiss * rainLvl + drop * rainLvl * 1.6f, b = windSig * windLvl + snowLp * snowLvl;
            left[i] += a * (1.0f - 0.3f * std::sin (windPhase)) + b;
            right[i] += a * (1.0f + 0.3f * std::sin (windPhase)) + b * 0.9f;
        }
    }

    // ---- reverb
    if (std::abs (settings.reverbSize - lastSize) > 0.005f)
    {
        lastSize = settings.reverbSize;
        juce::Reverb::Parameters p;
        p.roomSize = juce::jlimit (0.0f, 1.0f, 0.4f + 0.58f * settings.reverbSize);
        p.damping = 0.35f;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);
    }
    int done = 0;
    while (done < n)
    {
        const int m = juce::jmin (n - done, (int) wetL.size());
        for (int i = 0; i < m; ++i) { wetL[(size_t) i] = left[done + i]; wetR[(size_t) i] = right[done + i]; }
        reverb.processStereo (wetL.data(), wetR.data(), m);
        for (int i = 0; i < m; ++i)
        {
            float l = (left[done + i] * (1.0f - 0.4f * settings.reverbMix) + wetL[(size_t) i] * settings.reverbMix * 0.7f) * settings.volume;
            float r = (right[done + i] * (1.0f - 0.4f * settings.reverbMix) + wetR[(size_t) i] * settings.reverbMix * 0.7f) * settings.volume;
            limiter.processSample (l, r);
            left[done + i] = l;
            right[done + i] = r;
        }
        done += m;
    }
    float peak = 0.0f;
    for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));
    uiLevel = juce::jmax (peak, uiLevel.load() * 0.92f);
}

} // namespace lifecat
