#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace catbox
{
/** A big detuned-saw synth for the Arp (8 voices x 5 saws, a soft low-pass, an amp envelope). */
class SuperSaw
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? (float) sampleRate : 48000.0f;
        for (auto& v : voices) v = {};
    }

    void noteOn (int note, int velocity) noexcept
    {
        Voice* pick = nullptr;
        for (auto& v : voices) if (v.note == note && v.stage != Voice::Off) { pick = &v; break; }
        if (pick == nullptr) for (auto& v : voices) if (v.stage == Voice::Off) { pick = &v; break; }
        if (pick == nullptr)
        {
            pick = &voices[0];
            for (auto& v : voices) if (v.age < pick->age) pick = &v;   // steal the oldest
        }
        auto& v = *pick;
        v.note = note;
        v.freq = 440.0f * std::pow (2.0f, (float) (note - 69) / 12.0f);
        v.vel = 0.35f + 0.65f * (float) velocity / 127.0f;
        v.stage = Voice::Attack;
        v.age = ++counter;
        for (int k = 0; k < 5; ++k) v.ph[k] = (float) ((counter * 7 + k * 13) % 97) / 97.0f;   // free-running feel
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices) if (v.note == note && v.stage != Voice::Off) v.stage = Voice::Release;
    }

    void allOff() noexcept { for (auto& v : voices) if (v.stage != Voice::Off) v.stage = Voice::Release; }

    void render (float* l, float* r, int n) noexcept
    {
        const float dt = 1.0f / sr;
        const float att = 1.0f - std::exp (-dt / 0.006f), dec = std::exp (-dt / 0.35f), rel = std::exp (-dt / 0.22f);
        const float a = 1.0f - std::exp (-2.0f * 3.14159265f * 6500.0f * dt);
        static const float detune[5] = { -0.019f, -0.008f, 0.0f, 0.009f, 0.021f };
        static const float pan[5] = { 0.15f, 0.35f, 0.5f, 0.65f, 0.85f };
        for (auto& v : voices)
        {
            if (v.stage == Voice::Off) continue;
            for (int i = 0; i < n; ++i)
            {
                if (v.stage == Voice::Attack) { v.env += (1.0f - v.env) * att; if (v.env > 0.98f) v.stage = Voice::Decay; }
                else if (v.stage == Voice::Decay) v.env = 0.7f + (v.env - 0.7f) * dec;
                else if (v.stage == Voice::Release) { v.env *= rel; if (v.env < 1.0e-4f) { v.stage = Voice::Off; v.env = 0.0f; break; } }
                float sl = 0.0f, srr = 0.0f;
                for (int k = 0; k < 5; ++k)
                {
                    const float inc = v.freq * (1.0f + detune[k]) * dt;
                    v.ph[k] += inc;
                    if (v.ph[k] >= 1.0f) v.ph[k] -= 1.0f;
                    float s = 2.0f * v.ph[k] - 1.0f;   // PolyBLEP saw
                    if (v.ph[k] < inc)             { const float t = v.ph[k] / inc;          s -= t + t - t * t - 1.0f; }
                    else if (v.ph[k] > 1.0f - inc) { const float t = (v.ph[k] - 1.0f) / inc; s -= t * t + t + t + 1.0f; }
                    sl += s * (1.0f - pan[k]);
                    srr += s * pan[k];
                }
                v.lpL += a * (sl - v.lpL);
                v.lpR += a * (srr - v.lpR);
                const float g = v.env * v.vel * 0.11f;
                l[i] += v.lpL * g;
                r[i] += v.lpR * g;
            }
        }
    }

private:
    struct Voice
    {
        enum Stage { Off, Attack, Decay, Release } stage = Off;
        int note = -1;
        unsigned age = 0;
        float freq = 440.0f, vel = 1.0f, env = 0.0f, lpL = 0.0f, lpR = 0.0f;
        float ph[5] {};
    };
    std::array<Voice, 8> voices;
    float sr = 48000.0f;
    unsigned counter = 0;
};
} // namespace catbox
