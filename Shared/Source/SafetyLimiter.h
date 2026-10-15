#pragma once

#include <cmath>
#include <algorithm>

namespace cat
{
/**
    Output safety for the instruments. When many notes pile up, the level is turned down
    smoothly (stereo-linked gain riding, ~1 ms down / ~250 ms back up) and anything that
    still pokes above 0.9 is rounded off with a continuous soft knee, so the output never
    passes 1.0 and never jumps (the old "if > 0.8 -> tanh" safety jumped from 0.8 to 0.66,
    which crackled whenever lots of voices played together).
*/
class SafetyLimiter
{
public:
    void prepare (double sampleRate) noexcept
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        attack = (float) (1.0 - std::exp (-1.0 / (0.001 * sr)));
        release = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));
        gain = 1.0f;
        // DC / sub-sonic filter (2 x one-pole high-pass at ~18 Hz): nothing below hearing ever
        // reaches the speakers, even if something inside a Cat produces DC or a runaway rumble
        hpR = (float) (1.0 - 2.0 * 3.14159265358979 * 18.0 / sr);
        resetFilter();
    }

    void reset() noexcept { gain = 1.0f; resetFilter(); }

    inline void processSample (float& l, float& r) noexcept
    {
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        l = hp (l, hl[0]); l = hp (l, hl[1]);
        r = hp (r, hr[0]); r = hp (r, hr[1]);
        const float peak = std::max (std::abs (l), std::abs (r));
        const float target = peak > threshold ? threshold / peak : 1.0f;
        gain += (target - gain) * (target < gain ? attack : release);
        l = softClip (l * gain);
        r = softClip (r * gain);
    }

    void process (float* l, float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i) processSample (l[i], r[i]);
    }

    /** Continuous, smooth above 0.9, never above 1.0. */
    static inline float softClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= knee) return x;
        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return x < 0.0f ? -y : y;
    }

    float currentGain() const noexcept { return gain; }

    /** True while something is far too loud (the limiter is turning it down by more than 20 dB):
        a sign that a feedback loop ran away; the Cat can then reset its effects. */
    bool isOverloaded() const noexcept { return gain < 0.1f; }

private:
    struct HpState { float x1 = 0.0f, y1 = 0.0f; };
    inline float hp (float x, HpState& st) const noexcept
    {
        const float y = x - st.x1 + hpR * st.y1;
        st.x1 = x;
        st.y1 = y;
        return y;
    }
    void resetFilter() noexcept { hl[0] = hl[1] = hr[0] = hr[1] = HpState(); }

    HpState hl[2], hr[2];
    float hpR = 0.9976f;
    static constexpr float threshold = 0.85f, knee = 0.9f;
    float attack = 0.02f, release = 0.0001f, gain = 1.0f;
};

} // namespace cat
