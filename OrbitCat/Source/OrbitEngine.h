#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace orbitcat
{
constexpr int numSlots = 8;      // per-note parameter sets (lowest held note = slot 1)
constexpr int maxBodies = 16;
constexpr double twoPi = 6.283185307179586;

struct BodyParams
{
    float mass = 50.0f;          // 1..100 kg: pull on the others, loudness, planet size
    float radius = 8.0f;         // 1..32 AU: wider = slower (Kepler)
    float eccentricity = 0.0f;   // 0..0.9: fast near the sun, slow far out
    float speed = 1.0f;          // 0.25..4 x
    int octave = 0;              // -2..+2
    float damp = 0.5f;           // 0..1: how quickly pushes from the others die away
    float gate = 0.2f;           // seconds
    float boost = 0.0f;          // 0..1: louder when moving fast
};

struct GlobalParams
{
    float gravity = 0.5f;        // 0..1: central pull (overall tempo of the system) and mutual pull
    float drag = 0.0f;           // 0..1: orbits slowly spiral inwards -> accelerando while held
    float turbulence = 0.0f;     // 0..1: random wobble
    double alignRadians = 0.0;   // trigger line angle
    int spokes = 1;              // 1..6 trigger lines (2+ make eccentric orbits pulse unevenly)
};

struct Body
{
    bool active = false;
    int note = 60;
    int channel = 1;
    int slot = 0;
    double phase = 0.0;          // mean anomaly in cycles
    double deviation = 0.0;      // extra speed from mutual pull / turbulence (cycles per beat)
    double age = 0.0;            // beats since the key went down
    double omega = 0.0;          // orientation of the orbit (periapsis angle)
    double theta = 0.0;          // current angle around the sun
    double radiusNow = 8.0;      // current distance from the sun (AU)
    double semiMajor = 8.0;      // current orbit size (shrinks with drag)
    float noiseA = 0.0f, noiseB = 0.0f;
};

/**
    Orbital arpeggiator model.

    Every held note is a body orbiting a central sun. Mean motion follows Kepler's third law
    (period grows with radius^1.5), eccentric orbits are solved with Kepler's equation so they
    speed up near the sun, and every body pulls on the others (scaled by mass and distance) -
    those pushes are damped per note and disturbed by turbulence. A note is triggered each time
    its body crosses one of the alignment spokes. All time is in beats, so the system follows
    the host tempo.
*/
class OrbitEngine
{
public:
    std::array<Body, maxBodies> bodies;

    void reset() { for (auto& b : bodies) b = {}; }
    int numActive() const { int n = 0; for (auto& b : bodies) n += b.active ? 1 : 0; return n; }

    /** Adds a body that sits exactly on the first alignment spoke, so it sounds right away. */
    int addNote (int note, int channel, const GlobalParams& g, const std::array<BodyParams, numSlots>& params, juce::Random& rng);
    void removeNote (int note, int channel);

    /** Re-assigns parameter slots by pitch (lowest note -> slot 1). */
    void assignSlots();

    /**
        Advances the system by dtBeats. For every spoke crossing calls
        onTrigger (bodyIndex, fractionOfStep, speedRatio) where speedRatio is the body's
        angular speed relative to its average (above 1 near the sun).
    */
    template <typename Fn>
    void step (double dtBeats, const GlobalParams& g, const std::array<BodyParams, numSlots>& params, Fn&& onTrigger)
    {
        if (dtBeats <= 0.0)
            return;

        // Mutual pull, computed from the positions at the start of the step
        std::array<double, maxBodies> force {};
        const double coupling = 0.35 * g.gravity;
        for (int i = 0; i < maxBodies; ++i)
        {
            if (! bodies[(size_t) i].active) continue;
            const auto& bi = bodies[(size_t) i];
            for (int j = 0; j < maxBodies; ++j)
            {
                if (i == j || ! bodies[(size_t) j].active) continue;
                const auto& bj = bodies[(size_t) j];
                const double dx = bj.radiusNow * std::cos (bj.theta) - bi.radiusNow * std::cos (bi.theta);
                const double dy = bj.radiusNow * std::sin (bj.theta) - bi.radiusNow * std::sin (bi.theta);
                const double dist2 = (dx * dx + dy * dy) / 64.0;
                const double mass = params[(size_t) bj.slot].mass / 50.0;
                force[(size_t) i] += coupling * mass * std::sin (bj.theta - bi.theta) / (1.0 + dist2);
            }
        }

        for (int i = 0; i < maxBodies; ++i)
        {
            auto& b = bodies[(size_t) i];
            if (! b.active) continue;
            const auto& p = params[(size_t) b.slot];

            // Drag: the orbit slowly spirals in over ~8 beats (up to half its size)
            b.age += dtBeats;
            const double shrink = 1.0 - 0.5 * g.drag * (1.0 - std::exp (-b.age / 8.0));
            b.semiMajor = juce::jmax (0.5, (double) p.radius * shrink);

            const double period = 2.0 * std::pow (b.semiMajor / 8.0, 1.5) / juce::jmax (0.05, (double) p.speed)
                                / std::sqrt (0.25 + 1.5 * g.gravity);
            const double meanMotion = 1.0 / period;   // cycles per beat

            // Pushes from the others decay with damping, turbulence keeps stirring
            const double damping = 0.4 + p.damp * 8.0;
            const double wind = g.turbulence * meanMotion * 0.9
                              * (std::sin (b.age * 1.7 + b.noiseA) * 0.6 + std::sin (b.age * 0.53 + b.noiseB) * 0.4);
            b.deviation += (force[(size_t) i] * meanMotion + wind - damping * b.deviation) * dtBeats;
            b.deviation = juce::jlimit (-0.85 * meanMotion, 3.0 * meanMotion, b.deviation);

            const double previousTheta = b.theta;
            b.phase += (meanMotion + b.deviation) * dtBeats;
            const double e = juce::jlimit (0.0, 0.9, (double) p.eccentricity);
            const double meanAnomaly = twoPi * (b.phase - std::floor (b.phase));
            double E = 0.0, nu = 0.0;
            solveKepler (meanAnomaly, e, E, nu);
            b.theta = b.omega + nu;
            b.radiusNow = b.semiMajor * (1.0 - e * std::cos (E));

            // Spoke crossings during this step
            double moved = std::fmod (b.theta - previousTheta, twoPi);
            if (moved < 0.0) moved += twoPi;
            if (moved <= 0.0 || moved > juce::MathConstants<double>::pi)
                continue;

            const double speedRatio = moved / (twoPi * meanMotion * dtBeats);
            const int spokes = juce::jlimit (1, 6, g.spokes);
            for (int k = 0; k < spokes; ++k)
            {
                const double spoke = g.alignRadians + twoPi * k / spokes;
                double toSpoke = std::fmod (spoke - previousTheta, twoPi);
                if (toSpoke < 0.0) toSpoke += twoPi;
                if (toSpoke > 0.0 && toSpoke <= moved)
                    onTrigger (i, toSpoke / moved, speedRatio);
            }
        }
    }

    static void solveKepler (double meanAnomaly, double e, double& eccentricAnomaly, double& trueAnomaly)
    {
        double E = e < 0.8 ? meanAnomaly : juce::MathConstants<double>::pi;
        for (int it = 0; it < 8; ++it)
            E -= (E - e * std::sin (E) - meanAnomaly) / (1.0 - e * std::cos (E));
        eccentricAnomaly = E;
        trueAnomaly = 2.0 * std::atan2 (std::sqrt (1.0 + e) * std::sin (E * 0.5), std::sqrt (1.0 - e) * std::cos (E * 0.5));
    }
};

/** Factory presets (my own orbital "states"). */
struct OrbitPreset
{
    juce::String name;
    GlobalParams global;
    float alignDegrees, tilt, glow;
    int quantize;
    std::array<BodyParams, numSlots> bodies;
};

const std::vector<OrbitPreset>& getOrbitPresets();
juce::StringArray getQuantizeNames();     // Off, 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T
double quantizeToPpq (int index);         // 0 = off

} // namespace orbitcat
