#include "HaloEngine.h"

namespace halo
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
enum Kind { Major, Minor, Maj7, Min7, Sus2, Dom7 };
struct Chord { int offset; Kind kind; };

const std::vector<std::vector<Chord>>& progressions()
{
    static const std::vector<std::vector<Chord>> p {
        { { 0, Major }, { 7, Major }, { 9, Minor }, { 5, Major } },          // Pop: I V vi IV
        { { 0, Maj7 }, { 4, Min7 }, { 5, Maj7 }, { 5, Minor } },            // Dreamy: I iii IV iv
        { { 9, Minor }, { 5, Major }, { 0, Major }, { 7, Major } },          // Epic: vi IV I V
        { { 0, Minor }, { 8, Major }, { 3, Major }, { 10, Major } },         // Minor Night: i VI III VII
        { { 0, Minor }, { 10, Major }, { 8, Major }, { 7, Major } },         // Andalusian: i VII VI V
        { { 0, Sus2 }, { 5, Maj7 } },                                        // Floating: Isus2 IVmaj7
        { { 2, Min7 }, { 7, Dom7 }, { 0, Maj7 }, { 9, Min7 } },              // Jazz: ii V I vi
        { { 0, Major } },                                                    // Hold: just the key's chord
    };
    return p;
}

std::vector<int> intervals (Kind k)
{
    switch (k)
    {
        case Major: return { 0, 4, 7, 12 };
        case Minor: return { 0, 3, 7, 12 };
        case Maj7:  return { 0, 4, 7, 11 };
        case Min7:  return { 0, 3, 7, 10 };
        case Sus2:  return { 0, 2, 7, 12 };
        case Dom7:  return { 0, 4, 7, 10 };
    }
    return { 0, 4, 7, 12 };
}

const double orbitBeats[5][4] = {
    { 1.0, 1.5, 2.0, 3.0 },        // Waltzing
    { 2.0, 3.0, 4.0, 6.0 },        // Calm
    { 0.5, 0.75, 1.0, 1.5 },       // Busy
    { 0.75, 1.0, 4.0 / 3.0, 2.0 }, // Polyrhythm 3:4
    { 4.0, 6.0, 8.0, 12.0 },       // Sparse
};

inline double mtof (double n) { return 440.0 * std::exp2 ((n - 69.0) / 12.0); }
inline float saw (double ph) { return (float) (2.0 * ph - 1.0); }

/** one step of a band-pass (TPT state-variable filter) */
inline float bandpass (float x, float g, float k, float* s) noexcept
{
    const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float v3 = x - s[1];
    const float v1 = a1 * s[0] + a2 * v3, v2 = s[1] + a2 * s[0] + a3 * v3;
    s[0] = 2.0f * v1 - s[0]; s[1] = 2.0f * v2 - s[1];
    return v1;
}
}

juce::StringArray progressionNames()
{
    return { "Pop (I V vi IV)", "Dreamy (I iii IV iv)", "Epic (vi IV I V)", "Minor Night (i VI III VII)",
             "Andalusian (i VII VI V)", "Floating (Isus2 IVmaj7)", "Jazz (ii V I vi)", "Hold one chord" };
}
juce::StringArray orbitNames() { return { "Waltzing", "Calm", "Busy", "Polyrhythm 3:4", "Sparse" }; }
juce::StringArray vowelNames() { return { "Ah", "Oh", "Oo", "Ee" }; }

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    hall.setSampleRate (sampleRate);
    outS.reset (sampleRate, 0.05);
    reset();
}

void Engine::reset()
{
    held.clear();
    chord.fill (-1);
    chordSize = 0; step = -1;
    for (auto& v : choir) v = ChoirVoice();
    for (auto& b : bellsPool) b.on = false;
    phase.fill (0.0);
    ownBeats = lastBeats = 0.0;
    wasPlaying = false;
    hall.reset();
    lpL = lpR = 0.0f;
    chordRoot = -1;
}

void Engine::noteOn (int note, float)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);
    step = -1;                                      // a new chord / key: start the progression again
}

void Engine::noteOff (int note)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    step = -1;
}

void Engine::allOff() { held.clear(); step = -1; }

std::vector<int> Engine::currentChord() const
{
    std::vector<int> c;
    for (int i = 0; i < chordSize; ++i) c.push_back (chord[(size_t) i]);
    return c;
}

void Engine::buildChord (const Settings& s)
{
    std::array<int, maxChord> notes;
    notes.fill (-1);
    int n = 0;
    if (held.size() >= 2)
    {
        // the keys you hold are the chord
        auto keys = held;
        std::sort (keys.begin(), keys.end());
        for (int k : keys) if (n < maxChord) notes[(size_t) n++] = k;
        chordRoot = keys.front() % 12;
        chordKind = -1;
    }
    else if (held.size() == 1)
    {
        // one key: the progression in that key, this bar's chord
        const auto& prog = progressions()[(size_t) juce::jlimit (0, (int) progressions().size() - 1, s.progression)];
        const auto c = prog[(size_t) (juce::jmax (0, step) % (int) prog.size())];
        int root = held.front() + c.offset;
        if (c.offset > 6) root -= 12;
        notes[(size_t) n++] = root - 12;                       // the bass
        for (int iv : intervals (c.kind)) if (n < maxChord) notes[(size_t) n++] = root + iv;
        chordRoot = ((root % 12) + 12) % 12;
        chordKind = (int) c.kind;
    }
    else
    {
        chordRoot = -1;
    }

    // voice leading: the lowest voice goes to the lowest new note and so on, each one gliding (sorted pairs move least)
    std::sort (notes.begin(), notes.begin() + n);
    for (int i = 0; i < maxChord; ++i)
    {
        auto& v = choir[(size_t) i];
        if (i < n)
        {
            v.target = mtof (notes[(size_t) i]);
            if (v.hz <= 0.0 || v.amp < 0.001f) v.hz = v.target;   // a silent voice starts on its note
            v.ampTarget = 1.0f;
        }
        else v.ampTarget = 0.0f;
    }
    chord = notes;
    chordSize = n;
}

void Engine::ring (double hz, float amp, float pan, float decay)
{
    Bell* slot = nullptr;
    for (auto& b : bellsPool) if (! b.on) { slot = &b; break; }
    if (slot == nullptr)
    {
        slot = &bellsPool[0];
        for (auto& b : bellsPool) if (b.env < slot->env) slot = &b;
    }
    *slot = Bell();
    slot->on = true; slot->hz = hz; slot->amp = amp; slot->pan = pan; slot->decay = decay;
    slot->env = amp; slot->envMul = std::exp (-1.0 / (decay * sr)); slot->idx = 1.0;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (juce::jmin (1, buffer.getNumChannels() - 1));

    const double bpm = juce::jlimit (30.0, 300.0, s.bpm);
    const double beatsPerSample = bpm / 60.0 / sr;
    if (s.playing != wasPlaying) { ownBeats = 0.0; step = -1; }
    wasPlaying = s.playing;
    double beats = s.playing ? s.ppq : ownBeats;

    // the vowel: two resonances between Ah, Oh, Oo, Ee
    static const float f1[4] = { 800.0f, 450.0f, 325.0f, 280.0f }, f2[4] = { 1150.0f, 800.0f, 700.0f, 2250.0f };
    const float vw = juce::jlimit (0.0f, 3.0f, s.vowel);
    const int vi = juce::jmin (2, (int) vw);
    const float vf = vw - (float) vi;
    const float F1 = f1[vi] + (f1[vi + 1] - f1[vi]) * vf, F2 = f2[vi] + (f2[vi + 1] - f2[vi]) * vf;
    const float g1 = (float) std::tan (juce::MathConstants<double>::pi * F1 / sr), g2 = (float) std::tan (juce::MathConstants<double>::pi * F2 / sr);
    const double glideSec = 0.02 + std::pow (juce::jlimit (0.0f, 1.0f, s.glide), 2.0f) * 1.5;
    const double glideK = 1.0 - std::exp (-1.0 / (glideSec * sr));
    const float attackK = 1.0f - std::exp (-1.0f / (0.35f * (float) sr)), releaseK = 1.0f - std::exp (-1.0f / (1.4f * (float) sr));
    const float dryK = 1.0f - std::exp ((float) (-twoPi * (600.0 + 5000.0 * s.brightness) / sr));
    const double detune[3] = { 1.0, std::exp2 (7.0 / 1200.0), std::exp2 (-8.0 / 1200.0) };
    const float choirGain = s.choir * 0.22f, bellGain = s.bells * 0.35f;
    const float dustRate = s.dust * 10.0f / (float) sr;
    const double barBeats = 4.0 * juce::jmax (0.25, (double) s.change);
    const auto& per = orbitBeats[juce::jlimit (0, 4, s.orbits)];
    outS.setTargetValue (juce::Decibels::decibelsToGain (s.outDb));

    const double strikeMul = std::exp (-6.0 / sr);
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // the chord: a new one at every CHANGE bars (or when the keys change)
        const int nowStep = (int) std::floor (juce::jmax (0.0, beats) / barBeats);
        if (nowStep != step) { step = nowStep; buildChord (s); }

        // the planets: a bell each time one passes the top
        for (int k = 0; k < 4; ++k)
        {
            const double ph = std::fmod (juce::jmax (0.0, beats) * s.spin / per[k], 1.0);
            if (ph < phase[(size_t) k] && chordSize > 0)
            {
                const int tones = chordSize > 1 ? chordSize - (held.size() == 1 ? 1 : 0) : 1;
                const int pick = held.size() == 1 ? 1 + (k % juce::jmax (1, tones)) : k % chordSize;
                const int note = chord[(size_t) juce::jmin (pick, chordSize - 1)] + (k < 2 ? 12 : 24);
                ring (mtof (note), 0.6f + 0.1f * (float) k, 0.2f + 0.2f * (float) k, 1.2f + 0.5f * (float) (3 - k));
                planetFlash[(size_t) k] = 1.0f;
            }
            phase[(size_t) k] = ph;
        }

        // star dust
        if (chordSize > 0 && rng.nextFloat() < dustRate)
        {
            const int note = chord[(size_t) rng.nextInt (chordSize)] + 36 + 12 * rng.nextInt (2);
            ring (mtof (juce::jmin (note, 115)), 0.12f + 0.12f * rng.nextFloat(), rng.nextFloat(), 0.25f + 0.3f * rng.nextFloat());
        }

        float outL = 0.0f, outR = 0.0f;
        // the choir
        for (int v = 0; v < maxChord; ++v)
        {
            auto& c = choir[(size_t) v];
            c.amp += (c.ampTarget - c.amp) * (c.ampTarget > c.amp ? attackK : releaseK);
            if (c.amp < 1.0e-5f && c.ampTarget == 0.0f) continue;
            c.hz += (c.target - c.hz) * glideK;
            float x = 0.0f;
            for (int d = 0; d < 3; ++d)
            {
                c.ph[d] += c.hz * detune[d] / sr;
                c.ph[d] -= std::floor (c.ph[d]);
                x += saw (c.ph[d]);
            }
            x *= 0.33f;
            const float y = 0.9f * bandpass (x, g1, 0.25f, c.b1) + 0.55f * bandpass (x, g2, 0.3f, c.b2);
            const float sig = y * c.amp * choirGain;
            const float pan = 0.5f + 0.35f * ((float) v / (float) (maxChord - 1) - 0.5f);
            outL += sig * (1.0f - pan) * 1.4f;
            outR += sig * pan * 1.4f;
        }
        // the bells (2-operator FM: a bright strike, a pure ring)
        for (auto& b : bellsPool)
        {
            if (! b.on) continue;
            b.env *= b.envMul;
            b.idx *= strikeMul;
            const float env = (float) b.env * (float) juce::jmin (1.0, b.t * 400.0);
            if (b.t > 0.05 && env < 1.0e-4f) { b.on = false; continue; }
            const double index = (0.6 + 2.4 * s.brightness) * b.idx;
            b.mph += b.hz * 3.5 / sr; b.mph -= std::floor (b.mph);
            b.ph += b.hz / sr; b.ph -= std::floor (b.ph);
            const float x = (float) std::sin (twoPi * b.ph + index * std::sin (twoPi * b.mph)) * env * bellGain;
            outL += x * (1.0f - b.pan);
            outR += x * b.pan;
            b.t += 1.0 / sr;
        }
        // a gentle tone control
        lpL += (outL - lpL) * dryK; lpR += (outR - lpR) * dryK;
        const float o = outS.getNextValue();
        l[i] = lpL * o; r[i] = lpR * o;
        beats += beatsPerSample;
    }
    if (! s.playing) ownBeats = beats;
    lastBeats = beats;

    // the shimmering hall
    juce::Reverb::Parameters hp;
    hp.roomSize = 0.92f; hp.damping = 0.35f; hp.width = 1.0f;
    hp.wetLevel = s.shimmer * 0.5f; hp.dryLevel = 1.0f - s.shimmer * 0.25f;
    hall.setParameters (hp);
    if (r != l) hall.processStereo (l, r, n); else hall.processMono (l, n);
    for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
    level = peak;
    for (int k = 0; k < 4; ++k)
    {
        planetPhase[(size_t) k] = (float) phase[(size_t) k];
        planetFlash[(size_t) k] = planetFlash[(size_t) k].load() * 0.8f;
    }
}
} // namespace halo
