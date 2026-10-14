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
    }

    void reset() noexcept { gain = 1.0f; }

    inline void processSample (float& l, float& r) noexcept
    {
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

private:
    static constexpr float threshold = 0.85f, knee = 0.9f;
    float attack = 0.02f, release = 0.0001f, gain = 1.0f;
};

} // namespace cat
