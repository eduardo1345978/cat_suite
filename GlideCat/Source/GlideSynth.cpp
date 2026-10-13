#include "GlideSynth.h"
#include <numeric>

namespace glidecat
{
namespace
{
    constexpr float twoPi = 6.28318530718f;
    constexpr int controlPeriod = 16;

    inline float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)        { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline float smoothstep (float x) noexcept
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    struct SvfCoeffs { float a1, a2, a3, k; };

    inline SvfCoeffs svf (float fc, float q, float sr) noexcept
    {
        fc = juce::jlimit (16.0f, sr * 0.45f, fc);
        const float g = std::tan (juce::MathConstants<float>::pi * fc / sr);
        const float k = 1.0f / juce::jmax (0.5f, q);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        return { a1, g * a1, g * g * a1, k };
    }

    // Scales as semitone masks (relative to the root)
    const std::array<std::vector<int>, 10>& scaleTable()
    {
        static const std::array<std::vector<int>, 10> t { {
            { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },   // Chromatic
            { 0, 2, 4, 5, 7, 9, 11 },                   // Major
            { 0, 2, 3, 5, 7, 8, 10 },                   // Minor
            { 0, 2, 3, 5, 7, 9, 10 },                   // Dorian
            { 0, 2, 4, 5, 7, 9, 10 },                   // Mixolydian
            { 0, 2, 4, 6, 7, 9, 11 },                   // Lydian
            { 0, 2, 4, 7, 9 },                          // Major pentatonic
            { 0, 3, 5, 7, 10 },                         // Minor pentatonic
            { 0, 7 },                                   // Fifths
            { 0 },                                      // Octaves
        } };
        return t;
    }
}

juce::StringArray curveNames()   { return { "Linear", "Exponential", "S-Curve", "Elastic", "Semitone Stairs" }; }
juce::StringArray pathNames()    { return { "Direct", "Arc Up", "Arc Down", "Zigzag", "Swirl" }; }
juce::StringArray startNames()   { return { "From Below", "From Above", "Random", "From Centre", "From Outside", "Alternating" }; }
juce::StringArray staggerNames() { return { "Low to High", "High to Low", "Outside In", "Random", "Shortest First" }; }
juce::StringArray pilotNames()
{
    return { "Off", "Octave Leaps", "Scale Walk", "Glide Arp", "Bounce", "Weave", "Breathe", "Wander", "Mirror", "Sequence", "Random Loop" };
}
juce::StringArray scaleNames()   { return { "Chromatic", "Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Major Penta", "Minor Penta", "Fifths", "Octaves" }; }
juce::StringArray feetNames()    { return { "16'", "8'", "4'", "2'" }; }
juce::StringArray modTypeNames() { return { "Sine LFO", "Triangle LFO", "Saw LFO", "Square LFO", "Envelope", "8 Steps", "Random" }; }
juce::StringArray modModeNames() { return { "Poly", "Mono", "Free" }; }
juce::StringArray modSourceNames()
{
    return { "Off", "M1", "M2", "M3", "M4", "Velocity", "Key", "Pressure", "Timbre", "Glide", "Voice #", "Random", "Mod Wheel" };
}
juce::StringArray modDestNames()
{
    return { "Off", "Pitch", "Cutoff I", "Cutoff II", "Cutoff Both", "Resonance", "Pulse Width", "Level", "Pan", "Detune", "Noise", "Ring Mod", "Vibrato" };
}
juce::StringArray syncNames()    { return { "Free (Hz)", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/16T" }; }
double syncToPpq (int i)
{
    static const double v[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return v[juce::jlimit (0, 8, i)];
}

//==============================================================================
Synth::Synth()
{
    held.reserve (64);
    sustained.reserve (64);
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& v = voices[(size_t) i];
        v.rnd.setSeed ((juce::int64) (i * 7919 + 13));
        v.pan = (i % 2 == 0 ? -1.0f : 1.0f) * (0.3f + 0.7f * (float) (i / 2) / (float) (maxVoices / 2));
        v.vibRateMul = 0.92f + 0.16f * v.rnd.nextFloat();
        for (auto& s : v.loop) s = 0;
        for (auto& h : views[(size_t) i].history) h = -1.0f;
    }
}

void Synth::prepare (double sr, int)
{
    sampleRate = sr;
    for (auto& v : voices)
    {
        v.active = v.gate = false;
        v.note = -1;
        for (auto& l : v.layers) l = Layer();
    }
    held.clear();
}

int Synth::numSounding() const noexcept
{
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

//==============================================================================
float Synth::shapeCurve (float u) const
{
    u = juce::jlimit (0.0f, 1.0f, u);
    switch (settings.curve)
    {
        case Curve::Linear:      return u;
        case Curve::Exponential: return (1.0f - std::exp (-5.0f * u)) / (1.0f - std::exp (-5.0f));
        case Curve::Elastic:     return u >= 1.0f ? 1.0f : 1.0f - std::exp (-6.0f * u) * std::cos (3.0f * juce::MathConstants<float>::pi * u);
        case Curve::Stairs:      return u;   // quantised to semitones in the pitch calculation
        case Curve::SCurve:
        default:                 return smoothstep (u);
    }
}

float Synth::pathOffset (const Voice& v, float u) const
{
    if (u <= 0.0f || u >= 1.0f) return 0.0f;
    const float d = settings.pathDepth;
    switch (settings.path)
    {
        case Path::ArcUp:   return d * std::sin (juce::MathConstants<float>::pi * u);
        case Path::ArcDown: return -d * std::sin (juce::MathConstants<float>::pi * u);
        case Path::Zigzag:  { const float tri = 1.0f - std::abs (std::fmod (u * 4.0f, 2.0f) - 1.0f); return d * (tri * 2.0f - 1.0f) * (1.0f - u); }
        case Path::Swirl:   return v.pathSign * d * std::sin (twoPi * u) * (1.0f - 0.5f * u);
        case Path::Direct:
        default:            return 0.0f;
    }
}

float Synth::glideDuration (Voice& v, float distance)
{
    float t = settings.rateMode ? settings.rateMs * 0.001f * std::abs (distance) : settings.glideMs * 0.001f;
    t *= 1.0f + settings.glideSpread * (v.rnd.nextFloat() * 2.0f - 1.0f) * 0.8f;
    return juce::jmax (0.002f, t);
}

void Synth::startGlide (Voice& v, float target, float delaySeconds)
{
    v.from = v.pitch;
    v.to = target;
    v.glideT = 0.0f;
    v.glideDur = glideDuration (v, target - v.pitch);
    v.delay = juce::jmax (0.0f, delaySeconds);
    v.pathSign = v.rnd.nextBool() ? 1.0f : -1.0f;
    if (std::abs (target - v.from) < 1.0e-4f) v.glideT = 1.0f;
}

void Synth::gateOn (Voice& v, int channel, int note, float velocity, bool retrigger)
{
    v.channel = channel;
    v.note = note;
    v.velocity = velocity;
    v.age = ++counter;
    v.merging = false;
    v.silentFor = 0.0f;
    v.pilotOffset = 0.0f;
    v.pilotIndex = 0;
    if (retrigger || ! v.active)
    {
        v.noteTime = 0.0f;
        v.vibPhase = v.rnd.nextFloat() * twoPi;
        for (int m = 0; m < 4; ++m)
            if (settings.modMode[(size_t) m] == 0) v.modPhase[(size_t) m] = 0.0f;
    }
    if (! v.active)
        for (auto& l : v.layers) l = Layer();
    for (auto& l : v.layers)
    {
        l.ampStage = 1;     // attack from wherever the level is now: a continuing line swells, it does not restart
        l.filtStage = 1;
    }
    v.active = v.gate = true;
}

void Synth::releaseVoice (Voice& v)
{
    if (! v.gate) return;
    v.gate = false;
    v.releaseTime = 0.0f;
    for (auto& l : v.layers)
    {
        l.ampStage = 4;
        l.filtStage = 4;
    }
    if (settings.pilotReturn && std::abs (v.pilotOffset) > 1.0e-3f && v.note >= 0)
    {
        v.pilotOffset = 0.0f;
        startGlide (v, (float) v.note, 0.0f);
    }
}

Synth::Voice* Synth::freeVoice()
{
    const int n = juce::jlimit (1, maxVoices, settings.voices);
    Voice* best = nullptr;
    auto score = [this] (const Voice& v) -> double
    {
        if (! v.active && (v.note < 0 || v.silentFor > settings.lineMemory)) return 0.0;      // forgotten
        if (! v.active) return 1.0e3 - v.silentFor;                                            // silent, remembered
        if (! v.gate) return 1.0e6 + (double) v.age;                                           // releasing
        return 1.0e12 + (double) v.age;                                                         // held: steal the oldest
    };
    for (int i = 0; i < n; ++i)
        if (best == nullptr || score (voices[(size_t) i]) < score (*best))
            best = &voices[(size_t) i];
    return best;
}

//==============================================================================
void Synth::notesOn (const std::vector<std::pair<int, int>>& notes, const std::vector<float>& velocities)
{
    if (notes.empty()) return;
    for (const auto& n : notes)
    {
        held.erase (std::remove (held.begin(), held.end(), n), held.end());
        held.push_back (n);
    }

    const int limit = juce::jlimit (1, maxVoices, settings.voices);

    // Pool: the lines that can continue into this chord
    std::vector<Voice*> pool;
    for (int i = 0; i < limit; ++i)
    {
        auto& v = voices[(size_t) i];
        if (v.note < 0) continue;
        const bool held = v.gate;
        const bool remembered = ! v.gate && (v.active || v.silentFor < settings.lineMemory);
        if (settings.releasedLines ? remembered : held)
            pool.push_back (&v);
    }
    // The new notes, low to high
    std::vector<int> order (notes.size());
    std::iota (order.begin(), order.end(), 0);
    std::sort (order.begin(), order.end(), [&] (int a, int b) { return notes[(size_t) a].second < notes[(size_t) b].second; });
    std::sort (pool.begin(), pool.end(), [] (const Voice* a, const Voice* b) { return a->pitch < b->pitch; });

    const int k = (int) order.size(), m = (int) pool.size();
    std::vector<int> match ((size_t) k, -1);   // note (sorted index) -> pool index

    if (m > 0)
    {
        if (settings.smartLeading)
        {
            // Smart: non-crossing voice leading with the least total movement (order-preserving DP)
            const float inf = 1.0e9f;
            std::vector<std::vector<float>> dp ((size_t) k + 1, std::vector<float> ((size_t) m + 1, inf));
            dp[0][0] = 0.0f;
            for (int i = 0; i <= k; ++i)
                for (int j = 0; j <= m; ++j)
                {
                    if (dp[(size_t) i][(size_t) j] >= inf) continue;
                    const float c = dp[(size_t) i][(size_t) j];
                    if (i < k && j < m)
                    {
                        const float d = std::abs ((float) notes[(size_t) order[(size_t) i]].second - pool[(size_t) j]->pitch);
                        dp[(size_t) i + 1][(size_t) j + 1] = juce::jmin (dp[(size_t) i + 1][(size_t) j + 1], c + d);
                    }
                    if (k > m && i < k) dp[(size_t) i + 1][(size_t) j] = juce::jmin (dp[(size_t) i + 1][(size_t) j], c);   // note without a line
                    if (m > k && j < m) dp[(size_t) i][(size_t) j + 1] = juce::jmin (dp[(size_t) i][(size_t) j + 1], c);   // line not needed
                }
            // Backtrack
            int i = k, j = m;
            while (i > 0 || j > 0)
            {
                if (i > 0 && j > 0)
                {
                    const float d = std::abs ((float) notes[(size_t) order[(size_t) i - 1]].second - pool[(size_t) j - 1]->pitch);
                    if (std::abs (dp[(size_t) i][(size_t) j] - (dp[(size_t) i - 1][(size_t) j - 1] + d)) < 1.0e-3f)
                    {
                        match[(size_t) i - 1] = j - 1;
                        --i; --j;
                        continue;
                    }
                }
                if (k > m && i > 0 && std::abs (dp[(size_t) i][(size_t) j] - dp[(size_t) i - 1][(size_t) j]) < 1.0e-3f) { --i; continue; }
                if (j > 0) { --j; continue; }
                --i;
            }
        }
        else
        {
            // Classic: closest pairs first (lines may cross)
            std::vector<bool> usedNote ((size_t) k, false), usedVoice ((size_t) m, false);
            for (int pairs = 0; pairs < juce::jmin (k, m); ++pairs)
            {
                float best = 1.0e9f; int bi = -1, bj = -1;
                for (int i = 0; i < k; ++i)
                    for (int j = 0; j < m; ++j)
                        if (! usedNote[(size_t) i] && ! usedVoice[(size_t) j])
                        {
                            const float d = std::abs ((float) notes[(size_t) order[(size_t) i]].second - pool[(size_t) j]->pitch);
                            if (d < best) { best = d; bi = i; bj = j; }
                        }
                if (bi < 0) break;
                usedNote[(size_t) bi] = usedVoice[(size_t) bj] = true;
                match[(size_t) bi] = bj;
            }
        }
    }

    // Who starts when (stagger), by target pitch
    struct Plan { Voice* voice; int noteIndex; float start; bool fresh; };
    std::vector<Plan> plans;
    std::vector<bool> poolUsed ((size_t) m, false);
    float centre = 0.0f;
    for (const auto& n : notes) centre += (float) n.second;
    centre /= (float) notes.size();

    for (int i = 0; i < k; ++i)
    {
        const int ni = order[(size_t) i];
        const float target = (float) notes[(size_t) ni].second;
        if (match[(size_t) i] >= 0)
        {
            poolUsed[(size_t) match[(size_t) i]] = true;
            plans.push_back ({ pool[(size_t) match[(size_t) i]], ni, pool[(size_t) match[(size_t) i]]->pitch, false });
            continue;
        }
        // A note without a line: branch off the nearest line, or start fresh
        float start = target;
        if (settings.branch && m > 0)
        {
            float best = 1.0e9f;
            for (auto* p : pool)
                if (std::abs (p->pitch - target) < best) { best = std::abs (p->pitch - target); start = p->pitch; }
        }
        else if (settings.startDepth > 0.0f)
        {
            const float d = settings.startDepth;
            switch (settings.startFrom)
            {
                case StartFrom::Below:     start = target - d; break;
                case StartFrom::Above:     start = target + d; break;
                case StartFrom::Random:    start = target + (rng.nextBool() ? d : -d); break;
                case StartFrom::Centre:    start = centre; break;
                case StartFrom::Outside:   start = target + (target >= centre ? d : -d); break;
                case StartFrom::Alternate: start = target + (i % 2 == 0 ? -d : d); break;
            }
        }
        plans.push_back ({ nullptr, ni, start, true });
    }

    // Stagger order
    std::vector<int> idx (plans.size());
    std::iota (idx.begin(), idx.end(), 0);
    auto tgt = [&] (int p) { return (float) notes[(size_t) plans[(size_t) p].noteIndex].second; };
    switch (settings.staggerOrder)
    {
        case Stagger::HighLow:       std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return tgt (a) > tgt (b); }); break;
        case Stagger::OutsideIn:     std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return std::abs (tgt (a) - centre) > std::abs (tgt (b) - centre); }); break;
        case Stagger::Random:        for (int i = (int) idx.size() - 1; i > 0; --i) std::swap (idx[(size_t) i], idx[(size_t) rng.nextInt (i + 1)]); break;
        case Stagger::ShortestFirst: std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return std::abs (tgt (a) - plans[(size_t) a].start) < std::abs (tgt (b) - plans[(size_t) b].start); }); break;
        case Stagger::LowHigh:
        default:                     std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return tgt (a) < tgt (b); }); break;
    }

    for (int rank = 0; rank < (int) idx.size(); ++rank)
    {
        auto& plan = plans[(size_t) idx[(size_t) rank]];
        const auto [ch, note] = notes[(size_t) plan.noteIndex];
        const float delay = settings.staggerMs * 0.001f * (float) rank;
        Voice* v = plan.voice;
        if (v == nullptr)
        {
            v = freeVoice();
            if (v == nullptr) continue;
            const bool wasSounding = v->active;
            gateOn (*v, ch, note, velocities[(size_t) plan.noteIndex], true);
            if (! wasSounding) for (auto& l : v->layers) l.amp = 0.0f;
            v->pitch = plan.start;
        }
        else
            gateOn (*v, ch, note, velocities[(size_t) plan.noteIndex], false);
        startGlide (*v, (float) note, delay);
    }

    // Lines not needed any more: merge into a neighbour while fading, or just fade
    for (int j = 0; j < m; ++j)
    {
        if (poolUsed[(size_t) j]) continue;
        auto& v = *pool[(size_t) j];
        if (v.gate) releaseVoice (v);
        if (settings.merge && v.active)
        {
            float best = 1.0e9f, target = v.pitch;
            for (const auto& n : notes)
                if (std::abs ((float) n.second - v.pitch) < best) { best = std::abs ((float) n.second - v.pitch); target = (float) n.second; }
            v.merging = true;
            startGlide (v, target, 0.0f);
        }
    }
}

void Synth::noteOff (int channel, int note)
{
    const auto key = std::make_pair (channel, note);
    held.erase (std::remove (held.begin(), held.end(), key), held.end());
    if (sustainDown)
    {
        sustained.push_back (key);
        return;
    }
    for (auto& v : voices)
        if (v.gate && v.channel == channel && v.note == note)
            releaseVoice (v);
}

void Synth::pitchBend (int channel, float v) { channels[(size_t) juce::jlimit (0, 16, channel)].bend = v; }
void Synth::pressure (int channel, float v)  { channels[(size_t) juce::jlimit (0, 16, channel)].pressure = v; }
void Synth::timbre (int channel, float v)    { channels[(size_t) juce::jlimit (0, 16, channel)].timbre = v; }

void Synth::sustain (bool down)
{
    sustainDown = down;
    if (! down)
    {
        for (const auto& [ch, n] : sustained)
            if (std::find (held.begin(), held.end(), std::make_pair (ch, n)) == held.end())
                for (auto& v : voices)
                    if (v.gate && v.channel == ch && v.note == n) releaseVoice (v);
        sustained.clear();
    }
}

void Synth::allNotesOff()
{
    held.clear();
    sustained.clear();
    sustainDown = false;
    for (auto& v : voices) releaseVoice (v);
}

//==============================================================================
float Synth::quantiseToScale (float semis, int root) const
{
    const auto& sc = scaleTable()[(size_t) juce::jlimit (0, 9, settings.pilotScale)];
    const int s = juce::roundToInt (semis);
    int best = s, bestD = 1000;
    for (int o = -5; o <= 5; ++o)
        for (int deg : sc)
        {
            const int cand = root + deg + 12 * o;
            if (std::abs (cand - s) < bestD) { bestD = std::abs (cand - s); best = cand; }
        }
    return (float) best;
}

void Synth::pilotStep (double stepIndex)
{
    const int limit = juce::jlimit (1, maxVoices, settings.voices);
    std::vector<Voice*> gated;
    for (int i = 0; i < limit; ++i)
        if (voices[(size_t) i].gate && voices[(size_t) i].note >= 0) gated.push_back (&voices[(size_t) i]);
    if (gated.empty()) return;
    std::sort (gated.begin(), gated.end(), [] (const Voice* a, const Voice* b) { return a->note < b->note; });

    std::vector<int> chord;
    for (auto* v : gated) chord.push_back (v->note);
    float centre = 0.0f;
    for (int n : chord) centre += (float) n;
    centre /= (float) chord.size();
    const int root = chord.front();
    const float range = settings.pilotRange;
    const int step = (int) stepIndex;

    double stepSeconds = settings.pilotRateHz > 0.0f ? 1.0 / settings.pilotRateHz : 1.0;
    if (settings.pilotSync > 0 && hostRunning)
        stepSeconds = syncToPpq (settings.pilotSync) * 60.0 / juce::jmax (20.0, hostBpm);

    for (int i = 0; i < (int) gated.size(); ++i)
    {
        auto& v = *gated[(size_t) i];
        if (v.rnd.nextFloat() >= settings.pilotDensity) continue;
        const float note = (float) v.note;
        float off = v.pilotOffset;
        switch (settings.pilot)
        {
            case Pilot::OctaveLeaps:
            {
                const int maxOct = juce::jmax (1, (int) (range / 12.0f));
                off = 12.0f * (float) (v.rnd.nextInt (2 * maxOct + 1) - maxOct);
                break;
            }
            case Pilot::ScaleWalk:
                off = quantiseToScale (note + off + (v.rnd.nextBool() ? 2.0f : -2.0f), root) - note;
                break;
            case Pilot::GlideArp:
                off = (float) chord[(size_t) ((v.pilotIndex++ + i) % (int) chord.size())] - note + (v.pilotIndex % 2 == 0 ? 0.0f : 12.0f);
                break;
            case Pilot::Bounce:
                off = (v.pilotIndex++ % 2 == 0) ? quantiseToScale (note + range * 0.5f, root) - note : 0.0f;
                break;
            case Pilot::Weave:
            {
                const int nb = i + ((step + i) % 2 == 0 ? 1 : -1);
                off = (nb >= 0 && nb < (int) chord.size() && step % 2 == 1) ? (float) chord[(size_t) nb] - note : 0.0f;
                break;
            }
            case Pilot::Breathe:
                off = step % 2 == 1 ? quantiseToScale (note + (centre - note) * 0.8f, root) - note : 0.0f;
                break;
            case Pilot::Wander:
                off = quantiseToScale (note + (v.rnd.nextFloat() * 2.0f - 1.0f) * range, root) - note;
                break;
            case Pilot::Mirror:
                off = step % 2 == 1 ? quantiseToScale (2.0f * centre - note, root) - note : 0.0f;
                break;
            case Pilot::Sequence:
                off = (float) settings.pilotSteps[(size_t) (step % 8)];
                break;
            case Pilot::RandomLoop:
                if (step % 8 == 0)   // evolve: one step of the loop changes every time round
                    v.loop[(size_t) v.rnd.nextInt (8)] = (int) (quantiseToScale (note + (v.rnd.nextFloat() * 2.0f - 1.0f) * range, root) - note);
                off = (float) v.loop[(size_t) (step % 8)];
                break;
            case Pilot::Off:
            default:
                break;
        }
        off = juce::jlimit (-range, range, off);
        v.pilotOffset = off;
        const float delay = (float) stepSeconds * (settings.pilotRandom * v.rnd.nextFloat() * 0.5f + settings.pilotSpread * 0.25f * (float) i / (float) gated.size());
        v.from = v.pitch;
        v.to = note + off;
        v.glideT = std::abs (v.to - v.from) < 1.0e-4f ? 1.0f : 0.0f;
        v.glideDur = juce::jmax (0.01f, settings.pilotSmooth * (float) stepSeconds);
        v.delay = delay;
        v.pathSign = v.rnd.nextBool() ? 1.0f : -1.0f;
    }
}

float Synth::modSource (const Voice& v, int source, int index) const
{
    switch (source)
    {
        case 1: case 2: case 3: case 4: return v.modValue[(size_t) (source - 1)];
        case 5:  return v.velocity * 2.0f - 1.0f;
        case 6:  return ((float) juce::jmax (0, v.note) - 60.0f) / 24.0f;
        case 7:  return channels[(size_t) juce::jlimit (0, 16, v.channel)].pressure;
        case 8:  return channels[(size_t) juce::jlimit (0, 16, v.channel)].timbre;
        case 9:  return v.glideT < 1.0f ? 1.0f - v.glideT : 0.0f;
        case 10: return settings.voices > 1 ? (float) index / (float) (settings.voices - 1) * 2.0f - 1.0f : 0.0f;
        case 11: return (float) ((v.age * 2654435761u) % 1000u) / 500.0f - 1.0f;
        case 12: return modWheelValue;
        default: return 0.0f;
    }
}

//==============================================================================
void Synth::render (float* left, float* right, int numSamples)
{
    // Autopilot clock
    if (settings.pilot != Pilot::Off)
    {
        int64_t step;
        if (settings.pilotSync > 0 && hostRunning)
            step = (int64_t) std::floor (hostPpq / syncToPpq (settings.pilotSync));
        else
        {
            pilotClock += (double) numSamples / sampleRate * settings.pilotRateHz;
            step = (int64_t) std::floor (pilotClock);
        }
        if (step != lastPilotStep)
        {
            lastPilotStep = step;
            pilotStep ((double) step);
        }
    }

    const int limit = juce::jlimit (1, maxVoices, settings.voices);
    int inUse = 0;
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& v = voices[(size_t) i];
        auto& view = views[(size_t) i];
        if (! v.active)
        {
            v.silentFor += (float) numSamples / (float) sampleRate;
            view.active = false;
            v.historyCounter += numSamples;
            if (v.historyCounter >= (int) (sampleRate / 64.0))
            {
                v.historyCounter = 0;
                const int w = view.write.load();
                view.history[(size_t) (w % historyLength)] = -1.0f;
                view.write = w + 1;
            }
            continue;
        }
        if (i >= limit && v.gate) releaseVoice (v);
        ++inUse;
        renderVoice (v, i, left, right, numSamples);
    }
    uiVoicesInUse = inUse;
}

void Synth::renderVoice (Voice& v, int index, float* left, float* right, int numSamples)
{
    const float sr = (float) sampleRate;
    const auto& ch = channels[(size_t) juce::jlimit (0, 16, v.channel)];
    auto& view = views[(size_t) index];

    for (int start = 0; start < numSamples; start += controlPeriod)
    {
        const int n = juce::jmin (controlPeriod, numSamples - start);
        const float dt = (float) n / sr;

        // ---- glide
        if (v.delay > 0.0f) v.delay -= dt;
        else if (v.glideT < 1.0f)
        {
            v.glideT = juce::jmin (1.0f, v.glideT + dt / v.glideDur);
        }
        float base = v.from + (v.to - v.from) * shapeCurve (v.glideT);
        if (settings.curve == Curve::Stairs && v.glideT < 1.0f)
            base = v.from + std::round ((v.to - v.from) * v.glideT);
        v.pitch = base;
        float pitch = base + pathOffset (v, v.glideT);

        // ---- expression
        v.noteTime += dt;
        const float swellAmt = 1.0f - std::exp (-v.noteTime / juce::jmax (0.05f, settings.swellTime));
        v.driftTimer -= dt;
        if (v.driftTimer <= 0.0f)
        {
            v.driftTarget = (v.rnd.nextFloat() * 2.0f - 1.0f) * settings.drift * 0.25f;
            v.wanderTarget = (v.rnd.nextFloat() * 2.0f - 1.0f) * settings.wander;
            v.driftTimer = 0.5f + 1.5f * v.rnd.nextFloat();
        }
        v.driftValue += (v.driftTarget - v.driftValue) * juce::jmin (1.0f, dt * 1.5f);
        v.wanderValue += (v.wanderTarget - v.wanderValue) * juce::jmin (1.0f, dt * 0.8f);

        // ---- modulators
        float mPitch = 0.0f, mCut[2] = { 0.0f, 0.0f }, mRes = 0.0f, mPw = 0.0f, mLevel = 0.0f, mPan = 0.0f, mDetune = 0.0f, mNoise = 0.0f, mRing = 0.0f, mVib = 0.0f;
        for (int m = 0; m < 4; ++m)
        {
            auto& ph = settings.modMode[(size_t) m] == 1 ? monoModPhase[(size_t) m] : v.modPhase[(size_t) m];
            const float rate = settings.modRate[(size_t) m];
            if (settings.modMode[(size_t) m] != 1 || index == 0)
                ph += rate * dt;
            const int type = settings.modType[(size_t) m];
            float val = 0.0f;
            const float p = ph - std::floor (ph);
            switch (type)
            {
                case 0: val = std::sin (twoPi * p); break;
                case 1: val = 1.0f - 4.0f * std::abs (p - 0.5f); break;
                case 2: val = 2.0f * p - 1.0f; break;
                case 3: val = p < 0.5f ? 1.0f : -1.0f; break;
                case 4: val = ph < 0.2f ? ph / 0.2f : juce::jmax (0.0f, 1.0f - (ph - 0.2f)); break;   // attack / decay, not looping
                case 5: case 6:
                {
                    const int cyc = (int) std::floor (ph * (type == 5 ? 8.0f : 1.0f));
                    const float r = (float) (((uint32_t) cyc * 2654435761u + (uint32_t) (m * 977 + index * 131)) % 2000u) / 1000.0f - 1.0f;
                    if (type == 5) val = r;
                    else { v.modTarget[(size_t) m] = r; val = v.modValue[(size_t) m] + (v.modTarget[(size_t) m] - v.modValue[(size_t) m]) * juce::jmin (1.0f, dt * rate * 4.0f); }
                    break;
                }
                default: break;
            }
            v.modValue[(size_t) m] = val;
        }
        for (const auto& slot : settings.matrix)
        {
            if (slot.source == 0 || slot.dest == 0 || std::abs (slot.amount) < 1.0e-4f) continue;
            const float s = modSource (v, slot.source, index) * slot.amount;
            switch (slot.dest)
            {
                case 1:  mPitch += 12.0f * s; break;
                case 2:  mCut[0] += 4.0f * s; break;
                case 3:  mCut[1] += 4.0f * s; break;
                case 4:  mCut[0] += 4.0f * s; mCut[1] += 4.0f * s; break;
                case 5:  mRes += 2.0f * s; break;
                case 6:  mPw += 0.45f * s; break;
                case 7:  mLevel += s; break;
                case 8:  mPan += s; break;
                case 9:  mDetune += 50.0f * s; break;
                case 10: mNoise += s; break;
                case 11: mRing += s; break;
                case 12: mVib += s; break;
                default: break;
            }
        }

        const float vibFade = juce::jlimit (0.0f, 1.0f, (v.noteTime - settings.vibDelay) / 1.0f);
        v.vibPhase += twoPi * settings.vibRate * v.vibRateMul * dt;
        if (v.vibPhase > twoPi) v.vibPhase -= twoPi;
        pitch += (settings.vibDepth + mVib) * vibFade * std::sin (v.vibPhase);
        pitch += v.driftValue + mPitch;
        pitch += ch.bend * settings.bendRange;
        if (! v.gate)
            pitch += settings.releaseBend * juce::jlimit (0.0f, 1.0f, v.releaseTime / juce::jmax (0.05f, settings.layers[0].release));

        const float pressure = ch.pressure;
        const float brightOct = settings.brighten * swellAmt + v.wanderValue + ch.timbre * 2.0f + pressure * 1.0f;
        const float level = (1.0f - settings.swell * 0.6f * (1.0f - swellAmt)) * (0.75f + 0.5f * pressure) * juce::jmax (0.0f, 1.0f + mLevel);
        const float ring = juce::jlimit (0.0f, 1.0f, settings.ring + mRing);
        const float pan = juce::jlimit (-1.0f, 1.0f, v.pan * settings.panSpread + mPan);
        const float gl = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * level;
        const float gr = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi) * level;

        // ---- layers: coefficients for this control block
        struct LayerBlock { double sawInc[3]; double pulseInc, sineInc; float pw; SvfCoeffs hp, lp; float gain, ampTarget; };
        LayerBlock lb[2] {};
        bool anyActive = false;
        for (int li = 0; li < 2; ++li)
        {
            const auto& ls = settings.layers[(size_t) li];
            auto& l = v.layers[(size_t) li];
            if (! ls.on) { l.amp = 0.0f; continue; }
            static const float feet[] = { -12.0f, 0.0f, 12.0f, 24.0f };
            const float lp = pitch + feet[juce::jlimit (0, 3, ls.feet)] + ls.fine * 0.01f;
            const double f0 = 440.0 * std::pow (2.0, (lp - 69.0) / 12.0);
            const float det = (ls.detune + mDetune) * 0.01f;
            const int uni = juce::jlimit (1, 3, ls.unison);
            for (int u = 0; u < 3; ++u)
            {
                const float off = uni == 1 ? 0.0f : (uni == 2 ? (u == 0 ? -0.5f : 0.5f) * det : (float) (u - 1) * det);
                lb[li].sawInc[u] = f0 * std::pow (2.0, off / 12.0) / sampleRate;
            }
            lb[li].pulseInc = lb[li].sineInc = f0 / sampleRate;
            l.pwmPhase += ls.pwmRate * dt;
            lb[li].pw = juce::jlimit (0.05f, 0.95f, ls.pw + mPw + ls.pwm * 0.4f * std::sin (twoPi * (float) (l.pwmPhase - std::floor (l.pwmPhase))));

            // filter envelope: initial level -> attack level -> 0, release back to the initial level
            const float fdt = dt;
            switch (l.filtStage)
            {
                case 1: l.filt += (ls.egAL - ls.egIL) * fdt / juce::jmax (0.002f, ls.egAtk);
                        if ((ls.egAL >= ls.egIL && l.filt >= ls.egAL) || (ls.egAL < ls.egIL && l.filt <= ls.egAL)) { l.filt = ls.egAL; l.filtStage = 2; }
                        break;
                case 2: l.filt += (0.0f - l.filt) * juce::jmin (1.0f, fdt * 4.6f / juce::jmax (0.01f, ls.egDec)); break;
                case 4: l.filt += (ls.egIL - l.filt) * juce::jmin (1.0f, fdt * 4.6f / juce::jmax (0.01f, ls.egRel)); break;
                default: break;
            }
            const float keyOct = ls.key * (pitch - 60.0f) / 12.0f;
            const float velOct = ls.brill * (v.velocity - 0.5f) * 3.0f;
            const float cutoff = ls.lpf * std::pow (2.0f, ls.egDepth * l.filt + keyOct + velOct + brightOct + mCut[li]);
            lb[li].lp = svf (cutoff, ls.lpRes + mRes, sr);
            lb[li].hp = svf (ls.hpf, ls.hpRes, sr);
            lb[li].gain = ls.level * (0.7f + 0.3f * v.velocity);
            anyActive = anyActive || l.ampStage != 0;
        }
        juce::ignoreUnused (anyActive);

        // ---- audio
        float peak = 0.0f;
        for (int s = 0; s < n; ++s)
        {
            float out[2] = { 0.0f, 0.0f };
            for (int li = 0; li < 2; ++li)
            {
                const auto& ls = settings.layers[(size_t) li];
                auto& l = v.layers[(size_t) li];
                if (! ls.on) continue;

                // amp envelope
                switch (l.ampStage)
                {
                    case 1: l.amp += 1.0f / (juce::jmax (0.001f, ls.attack) * sr); if (l.amp >= 1.0f) { l.amp = 1.0f; l.ampStage = 2; } break;
                    case 2: l.amp += (ls.sustain - l.amp) * (4.6f / (juce::jmax (0.01f, ls.decay) * sr)); break;
                    case 4: l.amp *= std::exp (-6.9f / (juce::jmax (0.01f, ls.release) * sr)); if (l.amp < 1.0e-5f) { l.amp = 0.0f; l.ampStage = 0; } break;
                    default: break;
                }

                const auto& b = lb[li];
                float x = 0.0f;
                const int uni = juce::jlimit (1, 3, ls.unison);
                if (ls.saw > 0.0f)
                {
                    float saw = 0.0f;
                    for (int u = 0; u < uni; ++u)
                    {
                        auto& ph = l.sawPhase[(size_t) u];
                        saw += (float) (2.0 * ph - 1.0) - polyBlep (ph, b.sawInc[u]);
                        ph += b.sawInc[u];
                        if (ph >= 1.0) ph -= 1.0;
                    }
                    x += ls.saw * saw / std::sqrt ((float) uni);
                }
                if (ls.pulse > 0.0f)
                {
                    const double ph = l.pulsePhase;
                    float p = ph < b.pw ? 1.0f : -1.0f;
                    p += polyBlep (ph, b.pulseInc);
                    p -= polyBlep (std::fmod (ph + 1.0 - b.pw, 1.0), b.pulseInc);
                    x += ls.pulse * p;
                }
                l.pulsePhase += b.pulseInc;
                if (l.pulsePhase >= 1.0) l.pulsePhase -= 1.0;
                if (ls.sine > 0.0f)
                    x += ls.sine * std::sin ((float) (l.sinePhase * 6.283185307179586));
                l.sinePhase += b.sineInc;
                if (l.sinePhase >= 1.0) l.sinePhase -= 1.0;
                const float noiseAmt = juce::jlimit (0.0f, 1.0f, ls.noise + mNoise);
                if (noiseAmt > 0.0f)
                    x += noiseAmt * (v.rnd.nextFloat() * 2.0f - 1.0f);

                // 12 dB high-pass, then resonant 12 dB low-pass (TPT state variable)
                {
                    const auto& c = b.hp;
                    const float v3 = x - l.hp2;
                    const float v1 = c.a1 * l.hp1 + c.a2 * v3;
                    const float v2 = l.hp2 + c.a2 * l.hp1 + c.a3 * v3;
                    l.hp1 = 2.0f * v1 - l.hp1;
                    l.hp2 = 2.0f * v2 - l.hp2;
                    x = x - c.k * v1 - v2;
                }
                {
                    const auto& c = b.lp;
                    const float v3 = x - l.lp2;
                    const float v1 = c.a1 * l.lp1 + c.a2 * v3;
                    const float v2 = l.lp2 + c.a2 * l.lp1 + c.a3 * v3;
                    l.lp1 = 2.0f * v1 - l.lp1;
                    l.lp2 = 2.0f * v2 - l.lp2;
                    x = v2;
                }
                out[li] = x * l.amp * b.gain;
            }
            float y = (out[0] + out[1]) * (1.0f - ring) + ring * 2.0f * out[0] * out[1];
            y *= 0.18f;
            left[start + s] += y * gl;
            right[start + s] += y * gr;
            peak = juce::jmax (peak, std::abs (y));
        }

        if (! v.gate) v.releaseTime += dt;
        const bool silent = v.layers[0].ampStage == 0 && v.layers[1].ampStage == 0;
        if (silent && ! v.gate)
        {
            v.active = false;
            v.silentFor = 0.0f;
            for (auto& l : v.layers) { l.hp1 = l.hp2 = l.lp1 = l.lp2 = 0.0f; }
        }

        // ---- view (64 points per second)
        v.historyCounter += n;
        if (v.historyCounter >= (int) (sampleRate / 64.0))
        {
            v.historyCounter = 0;
            const int w = view.write.load();
            view.history[(size_t) (w % historyLength)] = v.active ? pitch : -1.0f;
            view.write = w + 1;
        }
        view.active = v.active;
        view.pitch = pitch;
        view.target = v.to;
        view.level = juce::jmax (peak * 6.0f, view.level.load() * 0.9f);
        view.width = (settings.layers[0].detune + mDetune) * (settings.layers[0].unison > 1 ? 1.0f : 0.0f);
        view.pressure = pressure;
        if (! v.active) break;
    }
}

} // namespace glidecat
