#include "CatSounds.h"
#include <cmath>

namespace hub
{
namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float mtof (float note) noexcept { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
    inline float lpCoef (float hz, double sr) noexcept { return 1.0f - std::exp (-twoPi * juce::jlimit (10.0f, 20000.0f, hz) / (float) sr); }
    inline float wrap (float p) noexcept { return p - std::floor (p); }
    inline void panGains (float pan, float& gl, float& gr) noexcept
    {
        gl = std::cos (pan * juce::MathConstants<float>::halfPi);
        gr = std::sin (pan * juce::MathConstants<float>::halfPi);
    }

    struct KeysPatch { float a, d, s, r, gain; };
    const KeysPatch keysPatches[KeysSynth::numSounds] = {
        { 0.25f, 1.00f, 0.70f, 0.90f, 0.15f },   // Soft Pad
        { 0.003f, 1.50f, 0.12f, 0.35f, 0.30f },  // E-Piano
        { 0.002f, 0.35f, 0.00f, 0.20f, 0.24f },  // Pluck
        { 0.35f, 2.00f, 0.85f, 1.20f, 0.12f },   // Strings
        { 0.002f, 0.25f, 0.55f, 0.06f, 0.09f },  // Chip (square wave, like an old game console)
        { 0.002f, 1.60f, 0.00f, 1.40f, 0.20f },  // Glass Bell (Frutiger Aero chimes)
        { 0.001f, 8.00f, 1.00f, 0.30f, 0.22f },  // Piano (the strings decay by themselves; release = the damper)
        { 0.001f, 8.00f, 1.00f, 0.18f, 0.34f },  // Nylon Guitar (Karplus-Strong string)
    };
}

//==============================================================================
void KeysSynth::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    for (auto& b : ksBuf) b.assign ((size_t) (sr / 25.0) + 8, 0.0f);   // down to ~25 Hz
    allOff();
}

void KeysSynth::strikePiano (Voice& v)
{
    const float f0 = v.freq;
    const float B = 0.00012f + 0.0000006f * f0;                                     // string stiffness
    const float tau = juce::jlimit (0.6f, 9.0f, 7.0f * std::sqrt (130.0f / f0));    // low notes ring longer
    const float bright = 0.55f + 0.9f * v.vel;
    const float dt = 1.0f / (float) sr;
    int n = 0;
    for (int k = 1; k <= pianoPartials + 3 && n < pianoPartials + 3; ++k)
    {
        const bool second = k > pianoPartials;                  // partials 13..15 = the second string of 1..3
        const int kk = second ? k - pianoPartials : k;
        float fk = f0 * (float) kk * std::sqrt (1.0f + B * (float) (kk * kk));
        if (second) fk *= 1.0012f;
        if (fk > 0.45f * (float) sr) continue;
        const float tk = tau / (1.0f + 0.45f * (float) (kk - 1));
        float amp = std::pow ((float) kk, -1.15f) * std::exp (-(float) (kk - 1) / (2.2f * bright));
        if (second) amp *= 0.6f;
        v.pa1[n] = 0.6f * amp; v.pa2[n] = 0.4f * amp;          // double decay: a quick drop after the strike, then the long ring
        v.pm1[n] = std::exp (-dt / (0.18f * tk)); v.pm2[n] = std::exp (-dt / tk);
        const float w = twoPi * fk * dt;
        v.pc[n] = std::cos (w); v.ps[n] = std::sin (w);
        v.pre[n] = 1.0f; v.pim[n] = 0.0f;
        ++n;
    }
    v.partials = n;
    v.pianoInit = true;
}

void KeysSynth::pluckString (Voice& v, std::vector<float>& buf)
{
    // fill the string with a burst of noise, softer (low-passed) for quiet notes, with a pluck-position comb
    const int len = juce::jlimit (2, (int) buf.size() - 2, (int) v.ksLen + 1);
    float lp = 0.0f;
    const float bright = 0.25f + 0.7f * v.vel;
    for (int i = 0; i < len; ++i)
    {
        lp += bright * (v.rnd.next() - lp);
        buf[(size_t) i] = lp;
    }
    const int comb = juce::jmax (1, len / 7);   // plucked a seventh of the way along: warmer
    for (int i = len - 1; i >= comb; --i) buf[(size_t) i] -= 0.6f * buf[(size_t) (i - comb)];
    float mean = 0.0f;
    for (int i = 0; i < len; ++i) mean += buf[(size_t) i];
    mean /= (float) len;
    for (int i = 0; i < len; ++i) buf[(size_t) i] -= mean;
    for (size_t i = (size_t) len; i < buf.size(); ++i) buf[i] = 0.0f;
    v.ksW = len;          // the first read (ksW - length) starts at the plucked noise
    v.ksPrev = 0.0f;
    v.ksInit = true;
}

void KeysSynth::allOff()
{
    for (auto& v : voices) v = Voice();
}

void KeysSynth::handle (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) noteOn (m.getChannel(), m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) noteOff (m.getChannel(), m.getNoteNumber());
    else if (m.isPitchWheel())
        chanBend[(size_t) juce::jlimit (1, 16, m.getChannel())] = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * bendRange;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) if (v.active) v.released = true;
        chanBend.fill (0.0f);
    }
}

void KeysSynth::noteOn (int ch, int note, float vel)
{
    // reuse the voice already on this note, else a free one, else the oldest
    Voice* pick = nullptr;
    for (auto& v : voices) if (v.active && v.note == note && v.ch == ch) { pick = &v; break; }
    if (pick == nullptr) for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (pick == nullptr)
    {
        pick = &voices[0];
        for (auto& v : voices) if (v.age < pick->age) pick = &v;
    }
    const float oldEnv = pick->active ? pick->env : 0.0f;
    *pick = Voice();
    pick->active = true;
    pick->note = note;
    pick->ch = juce::jlimit (1, 16, ch);
    pick->freq = mtof ((float) note);
    pick->vel = juce::jlimit (0.05f, 1.0f, vel);
    pick->pan = 0.5f + 0.18f * std::sin ((float) note * 1.7f);
    pick->env = oldEnv;                       // no click when a voice is re-used
    pick->ph[1] = 0.33f; pick->ph[2] = 0.66f;
    pick->age = ++ageCounter;
    pick->rnd.s = 0x9e3779b9u ^ (uint32_t) (note * 2654435761u) ^ pick->age;
    // the guitar string: delay length for this pitch (the averaging filter adds half a sample)
    pick->ksLen = juce::jlimit (2.0f, (float) (ksBuf[0].size() - 4), (float) sr / pick->freq - 0.5f);
    // low strings ring longer, loud plucks a little shorter
    pick->ksDamp = juce::jlimit (0.990f, 0.9995f, 1.0f - (0.0012f + 0.0000035f * pick->freq) * (48000.0f / (float) sr));
    if (sound == 7) pluckString (*pick, ksBuf[(size_t) (pick - voices.data())]);
    if (sound == 6) strikePiano (*pick);
}

void KeysSynth::noteOff (int ch, int note)
{
    for (auto& v : voices) if (v.active && v.note == note && v.ch == ch && ! v.released) v.released = true;
}

void KeysSynth::render (float* l, float* r, int n)
{
    const auto& p = keysPatches[sound];
    const float dt = 1.0f / (float) sr;
    const float rel = std::exp (-1.0f / (p.r * (float) sr));
    for (size_t vi = 0; vi < voices.size(); ++vi)
    {
        auto& v = voices[vi];
        if (! v.active) continue;
        float gl, gr;
        panGains (v.pan, gl, gr);
        if (sound == 7 && ! v.ksInit) pluckString (v, ksBuf[vi]);
        if (sound == 6 && ! v.pianoInit) strikePiano (v);
        const float baseFreq = v.freq;
        v.freq = baseFreq * std::pow (2.0f, chanBend[(size_t) v.ch] / 12.0f);   // pitch bend (Bend-Cat)
        for (int i = 0; i < n; ++i)
        {
            // envelope
            if (v.released) v.env *= rel;
            else
            {
                const float target = v.t < p.a ? v.t / p.a : p.s + (1.0f - p.s) * std::exp (-(v.t - p.a) / p.d);
                v.env += (target - v.env) * (v.t < p.a ? 1.0f : 0.02f);
            }
            v.t += dt;
            float x = 0.0f;
            switch (sound)
            {
                case 0:   // soft pad: 3 detuned saws, gentle low-pass
                case 3:   // strings: wider detune, brighter, vibrato
                {
                    const float vib = sound == 3 ? 1.0f + 0.004f * std::sin (twoPi * 5.2f * v.t) * juce::jmin (1.0f, v.t * 2.0f) : 1.0f;
                    const float det = sound == 3 ? 0.007f : 0.004f;
                    const float mul[3] = { 1.0f, 1.0f + det, 1.0f - det };
                    for (int k = 0; k < 3; ++k)
                    {
                        const float inc = v.freq * mul[k] * vib * dt;
                        x += polyBlepSaw (v.ph[k], inc);
                        v.ph[k] = wrap (v.ph[k] + inc);
                    }
                    const float cut = sound == 3 ? 2600.0f + 1800.0f * v.vel : 900.0f + 1600.0f * v.vel;
                    x = v.f2.lp (v.f1.lp (x, lpCoef (cut, sr)), lpCoef (cut * 1.5f, sr));
                    break;
                }
                case 1:   // e-piano: 2-op FM with a decaying index + a little tine
                {
                    const float index = (0.4f + 1.8f * v.vel) * std::exp (-v.t * 4.0f);
                    const float mod = std::sin (twoPi * v.mph);
                    x = std::sin (twoPi * v.ph[0] + index * mod) + 0.12f * std::exp (-v.t * 14.0f) * std::sin (twoPi * v.ph[1] * 14.0f);
                    v.ph[0] = wrap (v.ph[0] + v.freq * dt);
                    v.ph[1] = wrap (v.ph[1] + v.freq * dt);
                    v.mph = wrap (v.mph + v.freq * dt);
                    break;
                }
                case 4:   // chip: a square wave, a touch of low-pass so it isn't harsh
                {
                    const float inc = v.freq * dt;
                    x = (v.ph[0] < 0.5f ? 1.0f : -1.0f) * 0.8f;
                    v.ph[0] = wrap (v.ph[0] + inc);
                    x = v.f1.lp (x, lpCoef (7000.0f, sr));
                    break;
                }
                case 5:   // glass bell: FM at 3.5 with a fading index, plus a soft octave partial
                {
                    const float index = (1.2f + 1.5f * v.vel) * std::exp (-v.t * 2.5f);
                    x = std::sin (twoPi * v.ph[0] + index * std::sin (twoPi * v.mph)) + 0.25f * std::sin (twoPi * v.ph[1]) * std::exp (-v.t * 1.5f);
                    v.ph[0] = wrap (v.ph[0] + v.freq * dt);
                    v.ph[1] = wrap (v.ph[1] + v.freq * 2.0f * dt);
                    v.mph = wrap (v.mph + v.freq * 3.5f * dt);
                    break;
                }
                case 6:   // piano: inharmonic partials (stiff strings), each dying at its own speed, the lowest three
                {         //        doubled by a second string a touch sharp (beating), a felt hammer thump at the start
                    for (int k = 0; k < v.partials; ++k)
                    {
                        x += (v.pa1[k] + v.pa2[k]) * v.pim[k];
                        const float re = v.pre[k] * v.pc[k] - v.pim[k] * v.ps[k];
                        v.pim[k] = v.pre[k] * v.ps[k] + v.pim[k] * v.pc[k];
                        v.pre[k] = re;
                        v.pa1[k] *= v.pm1[k];
                        v.pa2[k] *= v.pm2[k];
                    }
                    if ((++v.renorm & 1023) == 0)   // keep the phasors on the unit circle
                        for (int k = 0; k < v.partials; ++k)
                        {
                            const float m = 1.0f / juce::jmax (1.0e-6f, std::sqrt (v.pre[k] * v.pre[k] + v.pim[k] * v.pim[k]));
                            v.pre[k] *= m; v.pim[k] *= m;
                        }
                    x *= 0.62f;
                    if (v.t < 0.02f)
                    {
                        v.hammerLp += 0.25f * (v.rnd.next() - v.hammerLp);
                        x += v.hammerLp * 0.35f * v.vel * std::exp (-v.t * 260.0f);
                    }
                    break;
                }
                case 7:   // nylon guitar: a Karplus-Strong string (a delay line with a gentle low-pass in the loop)
                {
                    auto& buf = ksBuf[vi];
                    const int size = (int) buf.size();
                    const float len = juce::jlimit (2.0f, (float) (size - 4), v.ksLen / std::pow (2.0f, -chanBend[(size_t) v.ch] / 12.0f));
                    float rp = (float) v.ksW - len;
                    while (rp < 0.0f) rp += (float) size;
                    const int i0 = (int) rp, i1 = (i0 + 1) % size;
                    const float fr = rp - (float) i0;
                    const float y = buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
                    const float damp = v.released ? 0.97f : v.ksDamp;
                    const float nv = 0.5f * (y + v.ksPrev) * damp;
                    v.ksPrev = y;
                    buf[(size_t) v.ksW] = nv;
                    v.ksW = (v.ksW + 1) % size;
                    x = v.f1.lp (y, lpCoef (2500.0f + 4000.0f * v.vel, sr)) * 1.6f;   // the body takes the edge off
                    break;
                }
                default:  // pluck: saw through a closing filter
                {
                    const float inc = v.freq * dt;
                    x = polyBlepSaw (v.ph[0], inc);
                    v.ph[0] = wrap (v.ph[0] + inc);
                    const float cut = 250.0f + 5500.0f * v.vel * std::exp (-v.t * 9.0f);
                    x = v.f2.lp (v.f1.lp (x, lpCoef (cut, sr)), lpCoef (cut * 1.3f, sr));
                    break;
                }
            }
            x *= v.env * v.vel * p.gain;
            l[i] += x * gl;
            r[i] += x * gr;
        }
        v.freq = baseFreq;
        if ((v.released || p.s <= 0.0f) && v.t > p.a + 0.01f && v.env < 1.0e-4f) v.active = false;
    }
}

//==============================================================================
void BassSynth::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    allOff();
}

void BassSynth::allOff()
{
    stackSize = 0;
    gate = false;
    env = 0.0f;
}

void BassSynth::handle (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        for (int i = 0; i < stackSize; ++i)
            if (stack[(size_t) i] == note) { for (int k = i; k + 1 < stackSize; ++k) stack[(size_t) k] = stack[(size_t) k + 1]; --stackSize; break; }
        if (stackSize < (int) stack.size()) stack[(size_t) stackSize++] = note;
        lastChannel = juce::jlimit (1, 16, m.getChannel());
        target = mtof ((float) note);
        if (! gate || sound == 3) { freq = target; fenv = 1.0f; }
        vel = juce::jlimit (0.1f, 1.0f, m.getFloatVelocity());
        gate = true;
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        for (int i = 0; i < stackSize; ++i)
            if (stack[(size_t) i] == note) { for (int k = i; k + 1 < stackSize; ++k) stack[(size_t) k] = stack[(size_t) k + 1]; --stackSize; break; }
        if (stackSize > 0) target = mtof ((float) stack[(size_t) stackSize - 1]);
        else gate = false;
    }
    else if (m.isPitchWheel())
        chanBend[(size_t) juce::jlimit (1, 16, m.getChannel())] = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * bendRange;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        stackSize = 0;
        gate = false;
        chanBend.fill (0.0f);
    }
}

void BassSynth::render (float* l, float* r, int n)
{
    if (! gate && env < 1.0e-5f) return;
    const float dt = 1.0f / (float) sr;
    const float glide = 1.0f - std::exp (-1.0f / (0.03f * (float) sr));
    const float att = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
    const float rel = std::exp (-1.0f / (0.08f * (float) sr));
    const float fdec = std::exp (-1.0f / (0.15f * (float) sr));
    const float pluckDec = std::exp (-1.0f / (0.35f * (float) sr));
    const float dcA = lpCoef (20.0f, sr);
    const float bendMul = std::pow (2.0f, chanBend[(size_t) lastChannel] / 12.0f);
    for (int i = 0; i < n; ++i)
    {
        freq += (target * bendMul - freq) * glide;
        if (gate) env += (1.0f - env) * att;
        else env *= rel;
        fenv *= sound == 3 ? pluckDec : fdec;   // the pluck's level follows its own (slower) decay
        float x = 0.0f;
        switch (sound)
        {
            case 0:  // sub
                x = std::sin (twoPi * ph[0]) + 0.25f * std::sin (twoPi * 2.0f * ph[0]);
                x *= 0.55f;
                break;
            case 1:  // saw
            {
                x = polyBlepSaw (ph[0], freq * dt);
                const float cut = 160.0f + 1900.0f * fenv * vel;
                x = f2.lp (f1.lp (x, lpCoef (cut, sr)), lpCoef (cut * 1.4f, sr)) * 0.5f;
                break;
            }
            case 2:  // reese: two detuned saws
            {
                x = polyBlepSaw (ph[1], freq * 1.006f * dt) + polyBlepSaw (ph[2], freq * 0.994f * dt);
                x = f2.lp (f1.lp (x, lpCoef (850.0f, sr)), lpCoef (1100.0f, sr)) * 0.3f;
                break;
            }
            default: // pluck
            {
                x = polyBlepSaw (ph[0], freq * dt);
                const float cut = 120.0f + 2600.0f * fenv * vel;
                x = f2.lp (f1.lp (x, lpCoef (cut, sr)), lpCoef (cut * 1.3f, sr)) * 0.55f * (0.25f + 0.75f * fenv);
                break;
            }
        }
        ph[0] = wrap (ph[0] + freq * dt);
        ph[1] = wrap (ph[1] + freq * 1.006f * dt);
        ph[2] = wrap (ph[2] + freq * 0.994f * dt);
        x = dc.hp (x, dcA) * env * vel * 0.5f;
        l[i] += x;
        r[i] += x;
    }
}

//==============================================================================
void DrumKit::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    allOff();
}

void DrumKit::allOff()
{
    for (auto& h : hits) h.active = false;
}

void DrumKit::handle (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) trigger (m.getNoteNumber(), m.getFloatVelocity());
}

void DrumKit::trigger (int note, float vel)
{
    Type type = Rim;
    float pitch = 1.0f, pan = 0.5f;
    switch (note)
    {
        case 35: case 36:                    type = Kick; break;
        case 38: case 40:                    type = Snare; break;
        case 39:                             type = Clap; break;
        case 37:                             type = Rim; break;
        case 42: case 44:                    type = ClosedHat; pan = 0.62f; break;
        case 46:                             type = OpenHat; pan = 0.62f; break;
        case 49: case 52: case 55: case 57:  type = Crash; pan = note == 57 ? 0.68f : 0.34f; break;
        case 51: case 53: case 59:           type = Ride; pan = 0.4f; break;
        case 41: case 43: case 45: case 47: case 48: case 50:
            type = Tom;
            pitch = 90.0f * std::pow (2.0f, (float) (note - 41) / 12.0f * 1.2f);
            pan = juce::jmap ((float) (note - 41), 0.0f, 9.0f, 0.3f, 0.7f);
            break;
        case 56:                             type = Cowbell; pan = 0.55f; break;
        case 69: case 70: case 82:           type = Shaker; pan = 0.6f; break;
        default:                             type = Rim; break;
    }
    if (type == ClosedHat)   // closed hat chokes the open one
        for (auto& h : hits) if (h.active && h.type == OpenHat) h.active = false;

    Hit* pick = nullptr;
    for (auto& h : hits) if (! h.active) { pick = &h; break; }
    if (pick == nullptr)
    {
        pick = &hits[0];
        for (auto& h : hits) if (h.t > pick->t) pick = &h;
    }
    *pick = Hit();
    pick->active = true;
    pick->type = type;
    pick->vel = std::pow (juce::jlimit (0.05f, 1.0f, vel), 1.2f);
    pick->pitch = pitch;
    pick->pan = pan;
}

void DrumKit::render (float* l, float* r, int n)
{
    const float dt = 1.0f / (float) sr;
    const bool k808 = kit == 1, lofi = kit == 2, live = kit == 3;
    for (auto& h : hits)
    {
        if (! h.active) continue;
        float gl, gr;
        panGains (h.pan, gl, gr);
        for (int i = 0; i < n; ++i)
        {
            const float t = h.t;
            float x = 0.0f, len = 1.0f;
            switch (h.type)
            {
                case Kick:
                {
                    const float f0 = k808 ? 46.0f : (live ? 62.0f : 52.0f), f1 = k808 ? 120.0f : 165.0f;
                    const float sweep = k808 ? 0.05f : 0.032f, decay = k808 ? 0.9f : (live ? 0.2f : 0.3f);
                    const float f = f0 + (f1 - f0) * std::exp (-t / sweep);
                    h.ph = wrap (h.ph + f * dt);
                    x = std::sin (twoPi * h.ph) * std::exp (-t / decay);
                    if (k808) x = std::tanh (x * 1.6f) * 0.8f;
                    x += (live ? 0.35f : 0.2f) * noise.next() * std::exp (-t / 0.004f);
                    x *= 0.95f;
                    len = decay * 6.0f;
                    break;
                }
                case Snare:
                {
                    const float nd = k808 ? 0.22f : (live ? 0.24f : 0.17f);
                    h.ph = wrap (h.ph + 185.0f * dt);
                    h.ph2 = wrap (h.ph2 + 330.0f * dt);
                    const float tone = (std::sin (twoPi * h.ph) + 0.5f * std::sin (twoPi * h.ph2)) * std::exp (-t / 0.07f);
                    const float nz = h.f2.lp (h.f1.hp (noise.next(), lpCoef (1200.0f, sr)), lpCoef (k808 ? 7000.0f : 9000.0f, sr)) * std::exp (-t / nd);
                    x = 0.45f * tone + 0.85f * nz;
                    len = nd * 6.0f;
                    break;
                }
                case Clap:
                {
                    float e = 0.0f;
                    for (float s : { 0.0f, 0.011f, 0.022f }) if (t >= s) e += std::exp (-(t - s) / 0.006f);
                    if (t > 0.03f) e += 0.7f * std::exp (-(t - 0.03f) / 0.16f);
                    x = h.f2.lp (h.f1.hp (noise.next(), lpCoef (900.0f, sr)), lpCoef (5000.0f, sr)) * e * 0.9f;
                    len = 1.0f;
                    break;
                }
                case Rim:
                    h.ph = wrap (h.ph + 1700.0f * dt);
                    h.ph2 = wrap (h.ph2 + 520.0f * dt);
                    x = (0.6f * std::sin (twoPi * h.ph) + 0.4f * std::sin (twoPi * h.ph2)) * std::exp (-t / 0.018f) * 0.6f;
                    len = 0.12f;
                    break;
                case ClosedHat:
                case OpenHat:
                case Shaker:
                {
                    const float d = h.type == OpenHat ? 0.33f : (h.type == Shaker ? 0.06f : (k808 ? 0.06f : 0.042f));
                    const float att = h.type == Shaker ? juce::jmin (1.0f, t / 0.012f) : 1.0f;
                    x = h.f1.hp (noise.next(), lpCoef (h.type == Shaker ? 5000.0f : 7500.0f, sr)) * std::exp (-t / d) * att;
                    x *= h.type == Shaker ? 0.35f : 0.42f;
                    len = d * 6.0f;
                    break;
                }
                case Crash:
                    x = h.f1.hp (noise.next(), lpCoef (4500.0f, sr)) * std::exp (-t / (live ? 1.8f : 1.3f)) * 0.36f;
                    len = 8.0f;
                    break;
                case Ride:
                {
                    float m = 0.0f;
                    const float ratios[4] = { 1.0f, 1.47f, 2.09f, 2.56f };
                    for (int k = 0; k < 4; ++k) m += std::sin (twoPi * wrap (t * 420.0f * ratios[k]));
                    x = (0.18f * m * std::exp (-t / 0.9f) + 0.3f * h.f1.hp (noise.next(), lpCoef (6000.0f, sr)) * std::exp (-t / 0.25f)) * 0.5f;
                    len = 5.0f;
                    break;
                }
                case Tom:
                {
                    const float f = h.pitch * (1.0f + 0.5f * std::exp (-t / 0.05f));
                    h.ph = wrap (h.ph + f * dt);
                    x = std::sin (twoPi * h.ph) * std::exp (-t / 0.35f) * 0.75f + 0.1f * noise.next() * std::exp (-t / 0.01f);
                    len = 2.0f;
                    break;
                }
                case Cowbell:
                {
                    h.ph = wrap (h.ph + 540.0f * dt);
                    h.ph2 = wrap (h.ph2 + 800.0f * dt);
                    const float sq = (h.ph < 0.5f ? 1.0f : -1.0f) + (h.ph2 < 0.5f ? 1.0f : -1.0f);
                    x = h.f2.lp (h.f1.hp (sq, lpCoef (400.0f, sr)), lpCoef (3000.0f, sr)) * std::exp (-t / 0.22f) * 0.3f;
                    len = 1.4f;
                    break;
                }
            }
            x *= h.vel;
            l[i] += x * gl;
            r[i] += x * gr;
            h.t += dt;
            if (h.t > len) { h.active = false; break; }
        }
    }

    if (lofi)
    {
        // crunchy: lower sample rate + fewer bits
        for (int i = 0; i < n; ++i)
        {
            if (crushCount-- <= 0) { crushCount = 3; crushHold = std::round ((l[i] + r[i]) * 0.5f * 24.0f) / 24.0f; }
            l[i] = r[i] = crushHold * 0.9f;
        }
    }
}
} // namespace hub

namespace hub
{
//==============================================================================
void TrackFx::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    chL.assign ((size_t) (sr * 0.05) + 8, 0.0f);
    chR.assign (chL.size(), 0.0f);
    dlL.assign ((size_t) (sr * 2.1) + 8, 0.0f);
    dlR.assign (dlL.size(), 0.0f);
    reset();
}

void TrackFx::reset()
{
    std::fill (chL.begin(), chL.end(), 0.0f);
    std::fill (chR.begin(), chR.end(), 0.0f);
    std::fill (dlL.begin(), dlL.end(), 0.0f);
    std::fill (dlR.begin(), dlR.end(), 0.0f);
    fl1 = fl2 = fr1 = fr2 = 0.0f;
    cutSmooth = 20000.0f;
    chW = dlW = 0;
    chPh = dlLp = dlLpR = dlSmooth = 0.0f;
    holdL = holdR = 0.0f;
    holdCount = 0;
    apL.fill (0.0f); apR.fill (0.0f);
    phPh = phFbL = phFbR = trPh = 0.0f;
}

void TrackFx::process (float* l, float* r, int n, const TrackFxSettings& s)
{
    const float srf = (float) sr;

    // ---- filter: two one-pole low-passes (12 dB), cutoff glides
    if (s.filter > 0.002f || cutSmooth < 19000.0f)
    {
        const float target = 20000.0f * std::pow (0.0075f, juce::jlimit (0.0f, 1.0f, s.filter));   // 20 kHz .. 150 Hz
        for (int i = 0; i < n; ++i)
        {
            cutSmooth += (target - cutSmooth) * 0.002f;
            const float a = lpCoef (cutSmooth, sr);
            fl1 += a * (l[i] - fl1); fl2 += a * (fl1 - fl2);
            fr1 += a * (r[i] - fr1); fr2 += a * (fr1 - fr2);
            l[i] = fl2; r[i] = fr2;
        }
    }

    // ---- drive: soft saturation, level kept about the same
    if (s.drive > 0.002f)
    {
        const float k = 1.0f + 12.0f * s.drive * s.drive;
        const float comp = 1.0f / std::tanh (k * 0.4f) * 0.4f;
        for (int i = 0; i < n; ++i)
        {
            l[i] = std::tanh (k * l[i]) * comp;
            r[i] = std::tanh (k * r[i]) * comp;
        }
    }

    // ---- chorus: two slowly moving short delays, one per side
    if (s.chorus > 0.002f)
    {
        const int size = (int) chL.size();
        for (int i = 0; i < n; ++i)
        {
            chL[(size_t) chW] = l[i];
            chR[(size_t) chW] = r[i];
            chPh += 0.6f / srf;
            if (chPh >= 1.0f) chPh -= 1.0f;
            auto tap = [&] (const std::vector<float>& b, float ph)
            {
                const float d = (0.012f + 0.004f * (0.5f + 0.5f * std::sin (twoPi * ph))) * srf;
                float rp = (float) chW - d;
                while (rp < 0.0f) rp += (float) size;
                const int i0 = ((int) rp) % size;
                const float fr = rp - std::floor (rp);
                return b[(size_t) i0] * (1.0f - fr) + b[(size_t) ((i0 + 1) % size)] * fr;
            };
            const float wl = tap (chL, chPh), wr = tap (chR, chPh + 0.25f);
            chW = (chW + 1) % size;
            l[i] = l[i] * (1.0f - 0.5f * s.chorus) + wl * s.chorus * 0.8f;
            r[i] = r[i] * (1.0f - 0.5f * s.chorus) + wr * s.chorus * 0.8f;
        }
    }

    // ---- echo: dotted eighth, ping-pong, darker each time, never runs away
    if (s.delay > 0.002f)
    {
        const int size = (int) dlL.size();
        const float target = juce::jlimit (16.0f, (float) size - 8.0f, (float) (0.75 * 60.0 / juce::jmax (40.0, s.bpm)) * srf);
        if (dlSmooth < 1.0f) dlSmooth = target;
        const float a = lpCoef (3500.0f, sr);
        const float fb = 0.25f + 0.3f * s.delay;   // < 0.6
        for (int i = 0; i < n; ++i)
        {
            dlSmooth += (target - dlSmooth) * 0.0005f;
            float rp = (float) dlW - dlSmooth;
            while (rp < 0.0f) rp += (float) size;
            const int i0 = ((int) rp) % size;
            const float fr = rp - std::floor (rp);
            const float dl = dlL[(size_t) i0] * (1.0f - fr) + dlL[(size_t) ((i0 + 1) % size)] * fr;
            const float dr = dlR[(size_t) i0] * (1.0f - fr) + dlR[(size_t) ((i0 + 1) % size)] * fr;
            dlLp += a * (dl - dlLp);
            dlLpR += a * (dr - dlLpR);
            dlL[(size_t) dlW] = 0.5f * (l[i] + r[i]) + dlLpR * fb;
            dlR[(size_t) dlW] = dlLp * fb;
            dlW = (dlW + 1) % size;
            l[i] += dl * s.delay * 0.7f;
            r[i] += dr * s.delay * 0.7f;
        }
    }

    // ---- phaser: six all-pass stages swept by a slow LFO, a little feedback, mixed with the dry sound
    if (s.phaser > 0.002f)
    {
        const float depth = juce::jlimit (0.0f, 1.0f, s.phaser);
        const float rate = 0.15f + 0.35f * depth;               // Hz
        const float fb = 0.2f + 0.45f * depth;
        for (int i = 0; i < n; ++i)
        {
            phPh += rate / srf;
            if (phPh >= 1.0f) phPh -= 1.0f;
            auto coefAt = [&] (float ph)
            {
                const float lfo = 0.5f + 0.5f * std::sin (twoPi * ph);
                const float fc = 250.0f * std::pow (12.0f, lfo);  // 250 Hz .. 3 kHz
                const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (fc, srf * 0.45f) / srf);
                return (t - 1.0f) / (t + 1.0f);
            };
            const float cl = coefAt (phPh), cr = coefAt (phPh + 0.2f);
            auto run = [] (std::array<float, 6>& z, float c, float x)
            {
                for (auto& st : z)
                {
                    const float y = c * x + st;
                    st = x - c * y;
                    x = y;
                }
                return x;
            };
            const float wl = run (apL, cl, l[i] + phFbL * fb);
            const float wr = run (apR, cr, r[i] + phFbR * fb);
            phFbL = std::isfinite (wl) ? wl : 0.0f;
            phFbR = std::isfinite (wr) ? wr : 0.0f;
            if (! std::isfinite (wl) || ! std::isfinite (wr)) { apL.fill (0.0f); apR.fill (0.0f); continue; }
            const float mix = 0.5f * depth;
            l[i] = l[i] * (1.0f - mix) + wl * mix;
            r[i] = r[i] * (1.0f - mix) + wr * mix;
        }
    }

    // ---- tremolo: the volume pulses in 1/8 notes (smooth, the right side a little behind: it moves in the stereo field)
    if (s.tremolo > 0.002f)
    {
        const float depth = juce::jlimit (0.0f, 1.0f, s.tremolo) * 0.85f;
        const float rate = (float) (juce::jmax (40.0, s.bpm) / 60.0 * 2.0);   // 1/8 notes
        for (int i = 0; i < n; ++i)
        {
            trPh += rate / srf;
            if (trPh >= 1.0f) trPh -= 1.0f;
            const float gl = 1.0f - depth * (0.5f - 0.5f * std::cos (twoPi * trPh));
            const float gr = 1.0f - depth * (0.5f - 0.5f * std::cos (twoPi * (trPh + 0.18f)));
            l[i] *= gl;
            r[i] *= gr;
        }
    }

    // ---- crush: fewer samples and fewer bits
    if (s.crush > 0.002f)
    {
        const int hold = 1 + (int) (s.crush * 7.0f);
        const float steps = 4.0f + 60.0f * (1.0f - s.crush) * (1.0f - s.crush);
        for (int i = 0; i < n; ++i)
        {
            if (holdCount-- <= 0)
            {
                holdCount = hold - 1;
                holdL = std::round (l[i] * steps) / steps;
                holdR = std::round (r[i] * steps) / steps;
            }
            l[i] = holdL;
            r[i] = holdR;
        }
    }
}
} // namespace hub
