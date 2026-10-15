#include "OrbitEngine.h"
#include <algorithm>

namespace orbitcat
{
int OrbitEngine::addNote (int note, int channel, const GlobalParams& g, const std::array<BodyParams, numSlots>& params, juce::Random& rng)
{
    removeNote (note, channel);

    int index = -1;
    for (int i = 0; i < maxBodies; ++i)
        if (! bodies[(size_t) i].active) { index = i; break; }
    if (index < 0)
        return -1;

    auto& b = bodies[(size_t) index];
    b = {};
    b.active = true;
    b.note = note;
    b.channel = channel;
    b.noiseA = rng.nextFloat() * 6.28f;
    b.noiseB = rng.nextFloat() * 6.28f;
    assignSlots();

    // Start just before the first spoke (so the note sounds now), with a random orbit orientation
    const auto& p = params[(size_t) b.slot];
    b.phase = rng.nextFloat();
    double E = 0.0, nu = 0.0;
    solveKepler (twoPi * b.phase, juce::jlimit (0.0, 0.9, (double) p.eccentricity), E, nu);
    b.omega = g.alignRadians - nu - 1.0e-7;
    b.theta = g.alignRadians - 1.0e-7;
    b.semiMajor = p.radius;
    b.radiusNow = p.radius * (1.0 - p.eccentricity * std::cos (E));
    return index;
}

void OrbitEngine::removeNote (int note, int channel)
{
    for (auto& b : bodies)
        if (b.active && b.note == note && b.channel == channel)
            b.active = false;
    assignSlots();
}

void OrbitEngine::assignSlots()
{
    std::array<int, maxBodies> order {};
    int n = 0;
    for (int i = 0; i < maxBodies; ++i)
        if (bodies[(size_t) i].active) order[(size_t) n++] = i;
    std::sort (order.begin(), order.begin() + n, [this] (int a, int b) { return bodies[(size_t) a].note < bodies[(size_t) b].note; });
    for (int k = 0; k < n; ++k)
        bodies[(size_t) order[(size_t) k]].slot = k % numSlots;
}

juce::StringArray getQuantizeNames() { return { "Off", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }; }

double quantizeToPpq (int index)
{
    static const double values[] = { 0.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    return values[juce::jlimit (0, 6, index)];
}

//==============================================================================
namespace
{
    using Fill = std::function<BodyParams (int slot)>;

    OrbitPreset make (const char* name, float gravity, float drag, float turbulence, float align, int spokes,
                      float tilt, float glow, int quantize, Fill fill)
    {
        OrbitPreset p;
        p.name = name;
        p.global.gravity = gravity;
        p.global.drag = drag;
        p.global.turbulence = turbulence;
        p.global.spokes = spokes;
        p.alignDegrees = align;
        p.tilt = tilt;
        p.glow = glow;
        p.quantize = quantize;
        for (int i = 0; i < numSlots; ++i)
            p.bodies[(size_t) i] = fill (i);
        return p;
    }

    BodyParams body (float mass, float radius, float ecc, float speed, int octave, float damp, float gate, float boost)
    {
        return { mass, radius, ecc, speed, octave, damp, gate, boost };
    }
}

const std::vector<OrbitPreset>& getOrbitPresets()
{
    static const std::vector<OrbitPreset> presets = {
        // Evenly spaced orbits, tight grid: a clean, ordered arpeggio
        make ("Clockwork", 0.5f, 0.0f, 0.0f, 90.0f, 1, 30.0f, 0.6f, 3,
              [] (int i) { return body (50, 4.0f + 2.0f * (float) i, 0.0f, 1.0f, 0, 0.9f, 0.15f, 0.1f); }),
        // Two bodies at a time phase-lock and drift apart again
        make ("Binary Dance", 0.75f, 0.0f, 0.05f, 0.0f, 2, 40.0f, 0.7f, 3,
              [] (int i) { return body (i % 2 == 0 ? 80.0f : 35.0f, 6.0f + (float) (i / 2) * 3.0f, 0.15f, 1.0f, 0, 0.25f, 0.18f, 0.2f); }),
        // Highly eccentric: bursts near the sun, long gaps far out
        make ("Comet Tails", 0.55f, 0.0f, 0.0f, 200.0f, 2, 25.0f, 0.8f, 0,
              [] (int i) { return body (30, 7.0f + 1.5f * (float) i, 0.75f, 1.0f, i % 3 == 2 ? 1 : 0, 0.6f, 0.12f, 0.6f); }),
        // Slow wide orbits, long gates: ambient chord tones
        make ("Lazy Moons", 0.3f, 0.0f, 0.1f, 270.0f, 1, 50.0f, 0.5f, 0,
              [] (int i) { return body (70, 14.0f + 3.0f * (float) i, 0.1f, 0.6f, 0, 0.8f, 1.2f, 0.0f); }),
        // Many fast small bodies, turbulent: generative sparkles
        make ("Swarm", 0.6f, 0.0f, 0.45f, 45.0f, 3, 20.0f, 0.9f, 4,
              [] (int i) { return body (15, 2.0f + 0.7f * (float) i, 0.3f, 1.5f, i % 2, 0.4f, 0.06f, 0.35f); }),
        // Strong mutual pull, low damping: orbits push and pull each other
        make ("Tidal Pull", 0.95f, 0.0f, 0.0f, 120.0f, 1, 35.0f, 0.75f, 3,
              [] (int i) { return body (60 + 10.0f * (float) (i % 3), 5.0f + 2.5f * (float) i, 0.2f, 1.0f, 0, 0.05f, 0.2f, 0.25f); }),
        // Drag spirals the system inwards: a slow accelerando while you hold
        make ("Slingshot", 0.5f, 0.85f, 0.0f, 0.0f, 1, 30.0f, 0.8f, 0,
              [] (int i) { return body (45, 12.0f + 2.0f * (float) i, 0.35f, 1.0f, 0, 0.7f, 0.12f, 0.5f); }),
        // Bass body far out, melody bodies close in
        make ("Gas Giant", 0.55f, 0.0f, 0.05f, 180.0f, 2, 45.0f, 0.65f, 3,
              [] (int i) { return i == 0 ? body (100, 20.0f, 0.2f, 1.0f, -1, 0.8f, 0.6f, 0.0f)
                                         : body (25, 3.0f + 1.2f * (float) i, 0.4f, 1.0f, i > 3 ? 1 : 0, 0.5f, 0.1f, 0.4f); }),
        // Ratios 1 : 2 : 3 : 4 -> polyrhythm on a 16th grid
        make ("Resonance", 0.5f, 0.0f, 0.0f, 90.0f, 1, 15.0f, 0.55f, 3,
              [] (int i) { return body (50, 8.0f * std::pow (1.0f + (float) (i % 4), 2.0f / 3.0f), 0.0f, 1.0f, i / 4, 1.0f, 0.15f, 0.0f); }),
        // Triplet spokes, mid eccentricity: swinging, rolling figures
        make ("Kepler Waltz", 0.5f, 0.0f, 0.1f, 30.0f, 3, 35.0f, 0.7f, 6,
              [] (int i) { return body (50, 6.0f + 1.8f * (float) i, 0.45f, 1.0f, 0, 0.5f, 0.14f, 0.3f); }),
        // Noisy, free timing: controlled rhythmic accidents
        make ("Asteroid Belt", 0.45f, 0.1f, 0.7f, 315.0f, 2, 60.0f, 0.85f, 0,
              [] (int i) { return body (10 + 8.0f * (float) (i % 4), 4.0f + 0.9f * (float) i, 0.5f, 1.2f, i % 3 - 1, 0.3f, 0.08f, 0.5f); }),
        // Bass-heavy: everything an octave down, slow and heavy
        make ("Heavy Core", 0.7f, 0.0f, 0.0f, 0.0f, 1, 20.0f, 0.5f, 2,
              [] (int i) { return body (90, 9.0f + 3.0f * (float) i, 0.25f, 0.8f, -1, 0.85f, 0.35f, 0.15f); }),
        // Up an octave, quick and light
        make ("Starlight", 0.4f, 0.0f, 0.2f, 60.0f, 2, 70.0f, 1.0f, 4,
              [] (int i) { return body (20, 3.0f + 1.0f * (float) i, 0.2f, 1.3f, 1, 0.6f, 0.07f, 0.4f); }),
        // Same orbit, different speeds: phasing (Reich-style)
        make ("Phase Drift", 0.5f, 0.0f, 0.0f, 90.0f, 1, 10.0f, 0.6f, 0,
              [] (int i) { return body (50, 8.0f, 0.0f, 1.0f + 0.03f * (float) i, 0, 1.0f, 0.16f, 0.0f); }),
        // Gentle everything: a calm starting point
        make ("Quiet Orbit", 0.4f, 0.0f, 0.05f, 90.0f, 1, 36.0f, 0.4f, 3,
              [] (int i) { return body (50, 8.0f + 2.0f * (float) i, 0.1f, 1.0f, 0, 0.7f, 0.3f, 0.1f); }),
        // Maximum chaos (still musical with the 1/16 grid)
        make ("Supernova", 1.0f, 0.4f, 0.9f, 0.0f, 4, 55.0f, 1.0f, 3,
              [] (int i) { return body (30 + 9.0f * (float) i, 2.5f + 1.5f * (float) i, 0.6f, 1.5f, (i % 3) - 1, 0.15f, 0.09f, 0.7f); }),
        // ---- more systems
        // Two groups on very different orbits: a slow bass pulse under a fast arpeggio
        make ("Twin Suns", 0.6f, 0.0f, 0.05f, 90.0f, 2, 35.0f, 0.7f, 3,
              [] (int i) { return i % 2 == 0 ? body (85, 16.0f + (float) i, 0.1f, 1.0f, -1, 0.8f, 0.4f, 0.0f)
                                             : body (25, 3.0f + 0.8f * (float) i, 0.2f, 1.0f, 0, 0.6f, 0.08f, 0.3f); }),
        // Ratios 2 : 3 : 4 : 5 on eighth notes
        make ("Harmonic Ladder", 0.5f, 0.0f, 0.0f, 0.0f, 1, 25.0f, 0.6f, 2,
              [] (int i) { return body (50, 6.0f * std::pow (2.0f + (float) (i % 4), 2.0f / 3.0f), 0.0f, 1.0f, i / 4, 1.0f, 0.2f, 0.0f); }),
        // Very slow, very long notes: evolving pad chords
        make ("Glacial", 0.2f, 0.0f, 0.05f, 270.0f, 1, 60.0f, 0.4f, 0,
              [] (int i) { return body (60, 20.0f + 2.0f * (float) i, 0.05f, 0.4f, 0, 0.9f, 1.8f, 0.0f); }),
        // Fast tight orbits on 1/32: glittering ratchets
        make ("Pulsar", 0.8f, 0.0f, 0.0f, 45.0f, 2, 20.0f, 1.0f, 4,
              [] (int i) { return body (20, 1.5f + 0.4f * (float) i, 0.1f, 2.0f, 1, 0.9f, 0.04f, 0.2f); }),
        // Eccentric orbits with 4 spokes: lopsided funk accents
        make ("Funk Orbit", 0.55f, 0.0f, 0.1f, 20.0f, 4, 30.0f, 0.8f, 3,
              [] (int i) { return body (40 + 8.0f * (float) (i % 3), 5.0f + 1.6f * (float) i, 0.55f, 1.0f, 0, 0.5f, 0.1f, 0.6f); }),
        // Triplet grid, mid orbits: rolling 12/8 arpeggio
        make ("Triplet Planets", 0.5f, 0.0f, 0.05f, 60.0f, 1, 30.0f, 0.65f, 6,
              [] (int i) { return body (50, 5.0f + 1.5f * (float) i, 0.15f, 1.0f, 0, 0.7f, 0.12f, 0.2f); }),
        // Drag plus turbulence: a wobbly accelerando that never repeats
        make ("Black Hole", 0.7f, 0.6f, 0.4f, 180.0f, 2, 50.0f, 0.95f, 3,
              [] (int i) { return body (60, 14.0f + 1.5f * (float) i, 0.3f, 1.0f, (i % 2) - 1, 0.4f, 0.1f, 0.6f); }),
        // Low register only: heavy sub planets for bass lines
        make ("Sub Orbit", 0.5f, 0.0f, 0.0f, 0.0f, 1, 15.0f, 0.5f, 2,
              [] (int i) { return body (90, 7.0f + 3.0f * (float) i, 0.2f, 1.0f, -2 + (i % 2), 0.9f, 0.25f, 0.1f); }),
        // High and fast, random octaves: sparkling bells
        make ("Ice Moons", 0.45f, 0.0f, 0.25f, 300.0f, 3, 65.0f, 1.0f, 4,
              [] (int i) { return body (15, 2.5f + 0.9f * (float) i, 0.35f, 1.4f, 1 + (i % 2), 0.5f, 0.05f, 0.4f); }),
        // Every body at the same radius, slightly different speeds: slow phasing chords
        make ("Choir Phase", 0.4f, 0.0f, 0.0f, 90.0f, 1, 10.0f, 0.5f, 0,
              [] (int i) { return body (50, 10.0f, 0.0f, 1.0f + 0.015f * (float) i, 0, 1.0f, 0.6f, 0.0f); }),
        // 3 + 2 spokes feel on the 16th grid: hip-hop bounce
        make ("Orbit Bounce", 0.55f, 0.0f, 0.05f, 10.0f, 2, 35.0f, 0.75f, 3,
              [] (int i) { return body (55, 6.0f + 2.2f * (float) i, 0.4f, 1.0f, i == 0 ? -1 : 0, 0.6f, 0.14f, 0.5f); }),
        // Mutual pull dominates: chaotic but tied to the grid
        make ("Three Body", 1.0f, 0.0f, 0.1f, 135.0f, 1, 40.0f, 0.85f, 3,
              [] (int i) { return body (90 - 10.0f * (float) (i % 3), 4.0f + 1.0f * (float) i, 0.25f, 1.0f, 0, 0.02f, 0.15f, 0.3f); }),
        // Gentle eighth-note arpeggio, no surprises: a good backing part
        make ("Steady Arp", 0.5f, 0.0f, 0.0f, 90.0f, 1, 30.0f, 0.5f, 2,
              [] (int i) { return body (50, 6.0f + 1.0f * (float) i, 0.0f, 1.0f, 0, 1.0f, 0.18f, 0.0f); }),
        // Comets with 2 spokes and octave jumps: dramatic swoops
        make ("Halley", 0.6f, 0.0f, 0.0f, 250.0f, 2, 45.0f, 0.9f, 3,
              [] (int i) { return body (35, 9.0f + 2.0f * (float) i, 0.85f, 1.0f, (i % 3) - 1, 0.5f, 0.1f, 0.8f); }),
        // Free timing (no grid), slow and loose: ambient generative
        make ("Nebula", 0.35f, 0.0f, 0.5f, 200.0f, 1, 70.0f, 0.6f, 0,
              [] (int i) { return body (30, 8.0f + 2.5f * (float) i, 0.4f, 0.7f, (i % 2), 0.4f, 0.9f, 0.2f); }),
        // Speeds 1, 1.5, 2: interlocking hocket on 16ths
        make ("Hocket", 0.5f, 0.0f, 0.0f, 90.0f, 1, 25.0f, 0.6f, 3,
              [] (int i) { return body (50, 8.0f, 0.0f, 1.0f + 0.5f * (float) (i % 3), i / 3, 1.0f, 0.1f, 0.0f); }),
        // ---- v3
        // A ring of small moons around one heavy planet: bass pulse plus a halo of notes
        make ("Saturn Rings", 0.55f, 0.0f, 0.05f, 90.0f, 3, 15.0f, 0.8f, 3,
              [] (int i) { return i == 0 ? body (100, 18.0f, 0.05f, 0.8f, -1, 0.9f, 0.5f, 0.0f)
                                         : body (12, 4.0f + 0.35f * (float) i, 0.05f, 1.3f, 1, 0.8f, 0.06f, 0.2f); }),
        // Slow wide ellipses on the quarter grid: big, simple chord tones
        make ("Grand Tour", 0.4f, 0.0f, 0.0f, 0.0f, 1, 30.0f, 0.55f, 1,
              [] (int i) { return body (60, 10.0f + 2.5f * (float) i, 0.5f, 0.8f, 0, 0.8f, 0.6f, 0.2f); }),
        // Fast orbits on the 16th-triplet grid: rolling trap hats feel
        make ("Rolling Moons", 0.6f, 0.0f, 0.15f, 30.0f, 2, 40.0f, 0.9f, 6,
              [] (int i) { return body (20, 2.0f + 0.6f * (float) i, 0.2f, 1.7f, 1, 0.6f, 0.05f, 0.3f); }),
        // Equal masses, slight eccentricity, eighth notes: a gentle music box
        make ("Music Box", 0.45f, 0.0f, 0.0f, 90.0f, 1, 20.0f, 0.6f, 2,
              [] (int i) { return body (40, 5.0f + 1.3f * (float) i, 0.15f, 1.0f, 1, 0.9f, 0.12f, 0.1f); }),
        // Lots of drag: the system spirals in and speeds up (a riser)
        make ("Event Horizon", 0.8f, 1.0f, 0.1f, 180.0f, 2, 60.0f, 1.0f, 3,
              [] (int i) { return body (50, 18.0f + 1.5f * (float) i, 0.25f, 1.0f, (i % 2), 0.5f, 0.1f, 0.5f); }),
        // Two bodies only matter: a call and response
        make ("Call and Response", 0.5f, 0.0f, 0.05f, 0.0f, 2, 30.0f, 0.7f, 2,
              [] (int i) { return body (i < 2 ? 70.0f : 10.0f, i < 2 ? 7.0f + 4.0f * (float) i : 25.0f + (float) i, 0.4f, 1.0f, i == 1 ? 1 : 0, 0.7f, 0.2f, 0.4f); }),
        // Turbulent light bodies, free timing: wind chimes
        make ("Wind Chimes", 0.3f, 0.0f, 0.8f, 270.0f, 1, 70.0f, 0.9f, 0,
              [] (int i) { return body (10, 3.0f + 1.1f * (float) i, 0.3f, 1.1f, 1 + (i % 2), 0.3f, 0.4f, 0.3f); }),
        // Bodies in 3 : 4 : 5 resonance on 16ths: a clockwork polyrhythm
        make ("Gear Train", 0.5f, 0.0f, 0.0f, 90.0f, 1, 20.0f, 0.6f, 3,
              [] (int i) { return body (50, 7.0f * std::pow (3.0f + (float) (i % 3), 2.0f / 3.0f), 0.0f, 1.0f, i / 3 - 1, 1.0f, 0.12f, 0.0f); }),
    };
    return presets;
}

} // namespace orbitcat
