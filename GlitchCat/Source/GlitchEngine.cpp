#include "GlitchEngine.h"

namespace glitch
{
juce::StringArray voiceTypeNames() { return { "Whisper", "Vowels", "Choir", "Mouth", "My voice" }; }
juce::StringArray beatModeNames()  { return { "Off", "Bug beat", "Low keys", "Bugs + keys" }; }
juce::StringArray beatStyleNames() { return { "Drill", "Broken", "Dust", "Rave" }; }

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr int chunk = 16;

inline float polyBlep (double t, double dt) noexcept
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

inline float smoothstep (float a, float b, float x) noexcept
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

// vowels a e i o u: the first three formants (Hz)
constexpr float formants[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 290, 2250, 3000 }, { 570, 840, 2410 }, { 320, 870, 2240 } };

struct Svf
{
    float a1 = 0, a2 = 0, a3 = 0;
    void set (float f, float q, double sr) noexcept
    {
        const float g = std::tan (pi * juce::jmin (f, (float) sr * 0.45f) / (float) sr);
        const float k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline float bp (float x, float& ic1, float& ic2) const noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v1;
    }
};

// the swarm's beats: 16 steps per bar, five lanes. x = accent, o = normal, g = ghost
struct Lane { const char* pattern; int note; int oddLength; int flash; };
struct Style { Lane lanes[5]; };
const Style styles[NumBeatStyles] = {
    // Drill: busy breaks, snares everywhere
    { { { "x..x......x..x..", 36, 15, 0 }, { "....x..g.x..x.gx", 38, 13, 1 }, { "oooooooooooooooo", 42, 7, 2 },
        { "..............o.", 46, 5, 2 }, { "...g.....g......", 37, 9, 3 } } },
    // Broken: a chopped breakbeat
    { { { "x.x.......x.x...", 36, 14, 0 }, { "....x.....g.x..g", 38, 12, 1 }, { "o.o.o.o.o.o.o.oo", 42, 7, 2 },
        { "......o.........", 46, 5, 2 }, { "........g.......", 39, 11, 3 } } },
    // Dust: sparse and soft
    { { { "x.........x.....", 36, 13, 0 }, { "........o.......", 37, 11, 1 }, { "o.g.o.g.o.g.o.g.", 69, 7, 2 },
        { "................", 46, 5, 2 }, { "...........g....", 45, 5, 3 } } },
    // Rave: four on the floor
    { { { "x...x...x...x...", 36, 15, 0 }, { "....x.......x...", 39, 13, 1 }, { "g.g.g.g.g.g.g.g.", 42, 7, 2 },
        { "..o...o...o...o.", 46, 5, 2 }, { "......g.......g.", 56, 9, 3 } } },
};

const float bugRatios[Engine::bugsPerNote] = { 1.0f, 0.75f, 1.25f, 2.0f / 3.0f, 0.875f, 1.5f, 0.8f, 1.2f };
const int drumKeyMap[12] = { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 };

int flashLaneFor (int note) noexcept
{
    if (note == 35 || note == 36) return 0;
    if (note == 38 || note == 40 || note == 39) return 1;
    if (note == 42 || note == 44 || note == 46 || note == 69) return 2;
    return 3;
}
}

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    drums.prepare (sr);
    kitNow = -1;
    drumBuffer.setSize (2, juce::jmax (1, maxBlock), false, false, true);
    drumMidi.ensureSize (1024);
    int size = 1;
    while (size < (int) (sr * 4.0)) size <<= 1;
    ringL.assign ((size_t) size, 0.0f);
    ringR.assign ((size_t) size, 0.0f);
    ringMask = size - 1;
    reverb.setSampleRate (sr);
    lastSpace = -1.0f;
    reset();
}

void Engine::reset()
{
    for (auto& n : notes) n = Note();
    for (auto& k : displayKey) k = -1;
    for (auto& d : display) d.bright = 0.0f;
    numEvents = 0;
    clockRunning = false;
    lastHostPpq = -1.0;
    beat = 0.0;
    nextStep = 0.0;
    takeoverUntil = -1.0;
    notesStoppedAt = -1.0;
    glitchType = GlitchNone;
    glitchMix = 0.0f;
    glitchNow = GlitchNone;
    lastGlitchStep = -1.0;
    std::fill (ringL.begin(), ringL.end(), 0.0f);
    std::fill (ringR.begin(), ringR.end(), 0.0f);
    writePos = 0;          // the ring may have got smaller (a lower sample rate)
    glitchAnchor = 0.0;
    drums.allOff();
    reverb.reset();
    tapeLpL = tapeLpR = 0.0f;
    bend = 0.0f;
    modWheel = 0.0f;
}

float Engine::madness() const noexcept
{
    return juce::jlimit (0.0f, 1.0f, s.madness + modWheel * (1.0f - s.madness));
}

//==============================================================================
void Engine::noteOn (int key, float vel)
{
    Note* slot = nullptr;
    for (auto& n : notes) if (n.active && n.key == key) { slot = &n; break; }            // the same key again
    if (slot == nullptr) for (auto& n : notes) if (! n.active) { slot = &n; break; }
    if (slot == nullptr)                                                                    // steal: the oldest released, else the oldest
    {
        for (auto& n : notes) if (! n.held && (slot == nullptr || n.order < slot->order)) slot = &n;
        if (slot == nullptr) for (auto& n : notes) if (slot == nullptr || n.order < slot->order) slot = &n;
    }
    const bool fresh = ! slot->active;
    slot->active = true;
    slot->held = true;
    slot->key = key;
    slot->vel = vel;
    slot->order = ++orderCounter;
    if (fresh) slot->env = 0.0f;
    for (int i = 0; i < bugsPerNote; ++i)
    {
        auto& b = slot->bugs[(size_t) i];
        if (fresh) b = Bug();
        b.ratio = bugRatios[i];
        startBug (*slot, b, i, true);
    }
}

void Engine::noteOff (int key)
{
    for (auto& n : notes)
        if (n.active && n.held && n.key == key) n.held = false;
    bool any = false;
    for (auto& n : notes) any |= n.active && n.held;
    if (! any) notesStoppedAt = beat;
}

void Engine::startBug (Note& note, Bug& b, int index, bool first)
{
    const float m = madness();
    const double spb = samplesPerBeatNow;

    // pitch: a small spread (the warm chorus), wider and with jumps when mad
    const float spreadCents = 3.0f + 9.0f * s.swarm;
    float off = (rnd2() + rnd2()) * 0.5f * spreadCents / 100.0f * (s.detune ? 1.0f + m * 4.0f : 1.0f);
    if (index == 0 && m < 0.5f) off *= 0.3f;                       // one bug stays near the note: the swarm keeps its pitch
    if (s.detune && rnd() < std::pow (m, 1.5f) * 0.7f)
    {
        static const float jumps[] = { 12.0f, -12.0f, 0.5f, -0.5f, 7.0f, -5.0f, 5.0f, 19.0f, -24.0f, 3.0f, -1.0f, 24.0f };
        const int range = m > 0.75f ? 12 : (m > 0.45f ? 8 : 4);
        off += jumps[juce::jlimit (0, range - 1, (int) (rnd() * (float) range))];
    }
    b.offsetTarget = off;
    if (first) b.offset = off;
    const float glideSeconds = 0.25f * std::pow (0.016f, m);
    b.glide = 1.0f - std::exp (-(float) chunk / (glideSeconds * (float) sr));

    // where it flies to
    b.panTarget = 0.5f + rnd2() * 0.48f * s.flight;
    if (first) b.pan = 0.5f + rnd2() * 0.2f * s.flight;

    // how long this segment lasts. On the grid, every bug of a note makes the same choice at the same moment,
    // so the rolls are tight ratchets instead of a smear (odd rhythm then pulls them apart on purpose)
    const float freedom = juce::jmax (0.0f, (m - 0.55f) / 0.45f);
    // (off the grid the shared choices follow the segment count instead, so the note's bugs still roll together)
    if (first) b.segCount = 0;
    juce::Random shared ((juce::int64) note.order * 7919 + (freedom > 0.0f ? 100003 * (juce::int64) ++b.segCount : (juce::int64) std::llround (renderBeat * 32.0)));
    auto pick = [&] { return shared.nextFloat(); };
    double d;
    if (s.rolls)
    {
        const int maxK = 1 + (int) (m * 5.0f);
        const int k = juce::jmin (maxK, (int) (std::pow (pick(), 1.0f / (1.0f + 2.5f * m)) * (float) (maxK + 1)));   // madder: shorter more often
        d = 2.0 * std::pow (2.0, -k);
        if (m > 0.6f && pick() < 0.3f) d *= 2.0 / 3.0;               // triplets
    }
    else d = 1.0 + rnd() * 2.0;
    if (s.odd && m > 0.1f) d *= b.ratio;                           // every bug its own length: they drift apart and meet again
    if (freedom > 0.0f) d *= 1.0 + freedom * (pick() * 2.0f - 1.0f) * 0.6;
    b.segLen = juce::jmax (32.0, d * spb);
    if (first)
    {
        if (s.rolls && freedom <= 0.0f)
        {
            b.segLeft = (d - std::fmod (beat, d)) * spb;          // the first segment ends on the grid
            if (b.segLeft < 32.0) b.segLeft += b.segLen;
        }
        else b.segLeft = b.segLen * (0.3 + 0.7 * pick());
    }
    else b.segLeft += b.segLen;

    // rolls: the bug starts again (a ratchet) instead of just gliding on
    b.retrig = s.rolls && pick() < smoothstep (0.12f, 0.6f, m);
    if (b.retrig && ! first) ++retriggers;
    if (b.retrig || first)
    {
        b.seg = 0.0f;
        b.decayEnv = 1.0f;
        const double t = b.segLen / sr * (0.15 + 0.6 * (1.0 - m));
        b.segDecay = b.retrig ? (float) std::exp (-1.0 / juce::jmax (0.002, t) / sr) : 1.0f;
    }
    else b.segDecay = 1.0f;

    b.vowelTarget = rnd() * 4.0f;
    if (first) b.vowel = b.vowelTarget;
    b.wingRate = 12.0f + rnd() * 30.0f;

    // my voice: where in the recording this bug reads
    if (const auto* smp = sample.load(); smp != nullptr && ! smp->data.empty())
    {
        const double len = (double) smp->data.size();
        b.reverse = s.stutter && rnd() < m * 0.5f;
        if (first || rnd() < 0.2f + m) b.samplePos = rnd() * len * (m < 0.3f ? 0.3 : 1.0);
    }
    juce::ignoreUnused (note);
}

void Engine::renderNotes (float* outL, float* outR, int start, int n)
{
    const float m = madness();
    const int nb = 2 + juce::roundToInt (s.swarm * 6.0f);
    const float bugGain = 0.6f / std::sqrt ((float) nb);
    const float atk = 1.0f - std::exp (-1.0f / (juce::jmax (0.001f, s.attack) * (float) sr));
    const float rel = 1.0f - std::exp (-1.0f / (juce::jmax (0.01f, s.release) * (float) sr));
    const float panCoef = 1.0f - std::exp (-(float) chunk / ((1.6f - s.flight * 1.4f) * (float) sr));
    const float lpBase = 200.0f * std::pow (2.0f, s.tone * 6.5f);
    const float wingDepth = 0.12f * s.swarm * (0.3f + m);
    const auto* smp = s.voiceType == MyVoice ? sample.load() : nullptr;
    const bool useSample = smp != nullptr && ! smp->data.empty();
    const int voiceType = (s.voiceType == MyVoice && ! useSample) ? Vowels : s.voiceType;
    const float voiceGain = s.voiceLevel * (voiceType == Whisper ? 4.0f : 2.2f);
    const float vowelSpeed = voiceType == Mouth ? 0.25f : 0.02f;
    hub::Noise& nz = hiss;

    for (int c0 = 0; c0 < n; c0 += chunk)
    {
        const int len = juce::jmin (chunk, n - c0);
        renderBeat = beat + (start + c0) / samplesPerBeatNow;
        float* l = outL + start + c0;
        float* r = outR + start + c0;
        const float wow = s.tape * (0.10f * std::sin ((float) wowPh) + 0.035f * std::sin ((float) flutterPh));
        wowPh += twoPi * 0.45 * len / sr;
        flutterPh += twoPi * 5.7 * len / sr;
        if (wowPh > twoPi) wowPh -= twoPi;
        if (flutterPh > twoPi) flutterPh -= twoPi;

        for (auto& note : notes)
        {
            if (! note.active) continue;
            const float target = note.held ? 1.0f : 0.0f;
            const float coef = note.held ? atk : rel;
            float envBuf[chunk];
            {
                float e = note.env;
                for (int i = 0; i < len; ++i) { e += (target - e) * coef; envBuf[i] = e; }
                note.env = e;
            }

            for (int j = 0; j < nb; ++j)
            {
                auto& b = note.bugs[(size_t) j];
                b.segLeft -= len;
                if (b.segLeft <= 0.0) startBug (note, b, j, false);
                b.offset += (b.offsetTarget - b.offset) * b.glide;
                b.pan += (b.panTarget - b.pan) * panCoef;
                b.vowel += (b.vowelTarget - b.vowel) * vowelSpeed;
                const float semis = (float) note.key + b.offset + wow + bend;
                const double freq = 440.0 * std::pow (2.0, (semis - 69.0) / 12.0);
                const double inc = freq / sr;
                if (inc > 0.45 || inc <= 0.0) continue;
                const float lpCut = juce::jmin (18000.0f, lpBase * (1.0f + 0.6f * b.seg));
                const float a = 1.0f - std::exp (-twoPi * lpCut / (float) sr);
                const float gl = std::cos (b.pan * pi * 0.5f), gr = std::sin (b.pan * pi * 0.5f);

                Svf f1, f2, f3;
                const bool formant = s.voiceLevel > 0.0f && ! useSample;
                if (formant)
                {
                    const int v0 = juce::jlimit (0, 4, (int) b.vowel), v1 = juce::jmin (4, v0 + 1);
                    const float fr = b.vowel - (float) v0;
                    const float shift = voiceType == Choir ? 0.95f : 1.0f;
                    f1.set (shift * juce::jmap (fr, formants[v0][0], formants[v1][0]), 7.0f, sr);
                    f2.set (shift * juce::jmap (fr, formants[v0][1], formants[v1][1]), 9.0f, sr);
                    f3.set (shift * juce::jmap (fr, formants[v0][2], formants[v1][2]), 11.0f, sr);
                }
                const double sampleStep = useSample ? (freq / 261.6256) * (smp->rate / sr) : 0.0;
                const double sampleLen = useSample ? (double) smp->data.size() : 1.0;

                for (int i = 0; i < len; ++i)
                {
                    const float e = envBuf[i];
                    // the segment's envelope
                    if (b.seg < 1.0f) b.seg = juce::jmin (1.0f, b.seg + 1.0f / (0.0015f * (float) sr));
                    b.decayEnv = b.retrig ? b.decayEnv * b.segDecay : b.decayEnv + (1.0f - b.decayEnv) * 0.002f;
                    const float segLevel = b.seg * b.decayEnv;

                    // the synth: saw -> square -> glass
                    const double ph = b.ph;
                    const float saw = (float) (2.0 * ph - 1.0) - polyBlep (ph, inc);
                    float synth;
                    if (s.shape < 0.5f)
                    {
                        double ph2 = ph + 0.5; if (ph2 >= 1.0) ph2 -= 1.0;
                        const float sq = (ph < 0.5 ? -1.0f : 1.0f) - polyBlep (ph, inc) + polyBlep (ph2, inc);   // in step with the saw, so they add
                        synth = juce::jmap (s.shape * 2.0f, saw, sq * 0.8f);
                    }
                    else
                    {
                        double ph2 = ph + 0.5; if (ph2 >= 1.0) ph2 -= 1.0;
                        const float sq = (ph < 0.5 ? -1.0f : 1.0f) - polyBlep (ph, inc) + polyBlep (ph2, inc);   // in step with the saw, so they add
                        const float glass = std::sin (twoPi * (float) ph + 1.3f * std::sin (twoPi * (float) b.ph2));
                        synth = juce::jmap ((s.shape - 0.5f) * 2.0f, sq * 0.8f, glass);
                    }
                    b.ph += inc; if (b.ph >= 1.0) b.ph -= 1.0;
                    b.ph2 += inc * 2.0; if (b.ph2 >= 1.0) b.ph2 -= 1.0;
                    b.lp1 += a * (synth - b.lp1);
                    b.lp2 += a * (b.lp1 - b.lp2);
                    float out = b.lp2 * s.synthLevel;

                    // the voice
                    if (s.voiceLevel > 0.0f)
                    {
                        float v = 0.0f;
                        if (useSample)
                        {
                            // the voice may have been swapped for a shorter one while this bug was reading the old one
                            if (! (b.samplePos >= 0.0 && b.samplePos < sampleLen))
                                b.samplePos = std::isfinite (b.samplePos) ? std::fmod (std::abs (b.samplePos), sampleLen) : 0.0;
                            const int i0 = juce::jlimit (0, (int) smp->data.size() - 1, (int) b.samplePos);
                            const float fr = (float) (b.samplePos - i0);
                            const auto& d = smp->data;
                            const float x0 = d[(size_t) i0], x1 = d[(size_t) ((i0 + 1) % (int) d.size())];
                            v = (x0 + (x1 - x0) * fr);
                            b.samplePos += b.reverse ? -sampleStep : sampleStep;
                            while (b.samplePos >= sampleLen) b.samplePos -= sampleLen;
                            while (b.samplePos < 0.0) b.samplePos += sampleLen;
                        }
                        else
                        {
                            const float src = voiceType == Whisper ? nz.next() : (voiceType == Choir ? saw * 0.8f + nz.next() * 0.08f : saw);
                            v = f1.bp (src, b.f1a, b.f1b) + 0.7f * f2.bp (src, b.f2a, b.f2b) + 0.35f * f3.bp (src, b.f3a, b.f3b);
                        }
                        out += v * voiceGain;
                    }

                    // wings
                    b.wingPh += twoPi * b.wingRate / (float) sr;
                    if (b.wingPh > twoPi) b.wingPh -= twoPi;
                    const float wing = 1.0f - wingDepth * (0.5f + 0.5f * std::sin (b.wingPh));
                    const float x = out * bugGain * note.vel * e * segLevel * wing;
                    l[i] += x * gl;
                    r[i] += x * gr;
                }
            }
            if (! note.held && note.env < 1.0e-4f) note.active = false;
        }
    }
}

//==============================================================================
void Engine::addEvent (double atBeat, int note, float vel)
{
    if (numEvents >= (int) events.size()) return;
    events[(size_t) numEvents++] = { atBeat, note, juce::jlimit (0.05f, 1.0f, vel) };
}

void Engine::playDrumKey (int key, float vel, double atBeat)
{
    const int note = drumKeyMap[((key % 12) + 12) % 12];
    addEvent (atBeat, note, vel);
    const float m = madness();
    if (s.rolls && rnd() < m * 0.6f)                // the swarm grabs the hit too: a roll after it
    {
        const int k = 2 + (int) (rnd() * (2.0f + m * 4.0f));
        const double step = (m > 0.6f ? 0.0625 : 0.125);
        for (int i = 1; i < k; ++i) addEvent (atBeat + step * i, note, vel * (1.0f - 0.6f * (float) i / (float) k));
    }
    if (s.beatMode == BeatBoth) takeoverUntil = (std::floor (atBeat / 4.0) + 2.0) * 4.0;   // the keys have the drums until the next bar ends
}

void Engine::scheduleBeat (double fromBeat, double toBeat)
{
    juce::ignoreUnused (fromBeat);
    const float m = madness();
    const float freedom = juce::jmax (0.0f, (m - 0.55f) / 0.45f);
    bool anyHeld = false;
    for (auto& n : notes) anyHeld |= n.active && n.held;
    const double stopBar = notesStoppedAt >= 0.0 ? std::ceil ((notesStoppedAt + 1.0e-6) / 4.0) * 4.0 : -1.0;
    const auto& st = styles[juce::jlimit (0, NumBeatStyles - 1, s.beatStyle)];

    while (nextStep / 4.0 < toBeat)
    {
        const double stepBeat = nextStep / 4.0;
        const auto step = (long long) nextStep;
        nextStep += 1.0;
        const bool auto_ = (s.beatMode == BeatAuto || s.beatMode == BeatBoth)
                        && (anyHeld || stepBeat < stopBar) && stepBeat >= takeoverUntil;
        if (! auto_) continue;

        for (int ln = 0; ln < 5; ++ln)
        {
            const auto& lane = st.lanes[ln];
            int length = 16;
            if (s.odd && m > 0.25f) length = lane.oddLength;     // polymeters: lanes of 15, 13, 7, 5 ... drift against the bar
            const char c = lane.pattern[(int) (step % length) % 16];
            float vel = c == 'x' ? 1.0f : c == 'o' ? 0.75f : c == 'g' ? 0.35f : 0.0f;
            if (vel > 0.0f && ! (ln == 0 && step % 16 == 0) && rnd() < m * 0.15f) vel = 0.0f;           // drops
            if (vel <= 0.0f && (ln == 1 || ln == 2) && rnd() < m * 0.22f) vel = 0.3f + rnd() * 0.3f;     // ghosts
            if (vel <= 0.0f) continue;
            const double jitter = freedom * rnd2() * 0.12;
            const double t = stepBeat + jitter;
            if (s.rolls && ln >= 1 && rnd() < m * 0.55f)
            {
                static const int counts[] = { 2, 3, 4, 6, 8 };
                const int k = counts[juce::jlimit (0, 4, (int) (rnd() * (1.0f + m * 4.0f)))];
                const bool up = rnd() < 0.5f;
                for (int i = 0; i < k; ++i)
                {
                    const float ramp = up ? 0.35f + 0.65f * (float) (i + 1) / (float) k : 1.0f - 0.6f * (float) i / (float) k;
                    addEvent (t + 0.25 * i / k, lane.note, vel * ramp);
                }
            }
            else addEvent (t, lane.note, vel);
            if (ln == 0 && m > 0.4f && rnd() < m * 0.2f) addEvent (t + 0.125, lane.note, vel * 0.7f);
        }
        // machine-gun hats
        if (s.rolls && m > 0.7f && rnd() < (m - 0.7f) * 0.5f)
            for (int i = 0; i < 16; ++i) addEvent (stepBeat + i / 32.0, 42, 0.4f + 0.5f * (float) i / 16.0f);
    }
}

void Engine::fireEvents (double fromBeat, double toBeat, double samplesPerBeat, int blockStart, int blockLen, juce::MidiBuffer& out)
{
    for (int i = 0; i < numEvents;)
    {
        auto& e = events[(size_t) i];
        if (e.beat < toBeat)
        {
            if (e.beat > fromBeat - 1.0)
            {
                const int at = blockStart + juce::jlimit (0, blockLen - 1, (int) ((e.beat - fromBeat) * samplesPerBeat));
                out.addEvent (juce::MidiMessage::noteOn (10, e.note, e.vel), at);
                auto& f = drumFlash[(size_t) flashLaneFor (e.note)];
                f = juce::jmax (f.load(), e.vel);
            }
            e = events[(size_t) --numEvents];
        }
        else ++i;
    }
}

//==============================================================================
void Engine::maybeStartGlitch (double spb)
{
    const float m = madness();
    if (! s.stutter || m <= 0.02f || glitchType != GlitchNone) return;
    if (rnd() > 0.55f * std::pow (m, 1.6f)) return;
    const float freedom = juce::jmax (0.0f, (m - 0.55f) / 0.45f);

    float w[5] = { 0.35f, 0.25f, 0.15f, m > 0.4f ? 0.12f : 0.0f, m > 0.5f ? 0.15f : 0.0f };
    float sum = 0.0f;
    for (float x : w) sum += x;
    float pick = rnd() * sum;
    int type = GlitchRepeat;
    for (int i = 0; i < 5; ++i) { if (pick < w[i]) { type = GlitchRepeat + i; break; } pick -= w[i]; }

    static const double lengths[] = { 0.25, 0.5, 1.0 };
    double durBeats = lengths[juce::jmin (2, (int) (rnd() * (1.0f + m * 2.0f)))];
    if (freedom > 0.0f) durBeats *= 1.0 + freedom * rnd2() * 0.5;
    glitchLen = glitchLeft = juce::jmax (64.0, durBeats * spb);
    static const double slices[] = { 0.125, 0.0625, 0.03125 };
    glitchSlice = juce::jmax (32.0, slices[juce::jmin (2, (int) (rnd() * (1.0f + m * 2.0f)))] * spb);
    glitchRead = 0.0;
    glitchAnchor = (double) writePos;
    glitchSpeed = type == GlitchPitch ? (rnd() < 0.6f ? 2.0 : 0.5) : 1.0;
    if (type == GlitchTapeStop) glitchRead = 0.0;
    glitchType = type;
    glitchNow = type;
    ++glitchEvents;
}

void Engine::glitchProcess (float* l, float* r, int n, double spb)
{
    const float m = madness();
    const float freedom = juce::jmax (0.0f, (m - 0.55f) / 0.45f);
    const float fade = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
    const double beatsPerSample = 1.0 / spb;
    auto read = [this] (const std::vector<float>& ring, double pos) noexcept
    {
        const auto i0 = (int) std::floor (pos);
        const float fr = (float) (pos - i0);
        const float a = ring[(size_t) (i0 & ringMask)], b = ring[(size_t) ((i0 + 1) & ringMask)];
        return a + (b - a) * fr;
    };

    for (int i = 0; i < n; ++i)
    {
        ringL[(size_t) writePos] = l[i];
        ringR[(size_t) writePos] = r[i];

        // a new glitch? on the 1/16 grid (gentle) or whenever (insane)
        if ((i & 15) == 0 && s.stutter)
        {
            const double b = beat + i * beatsPerSample;
            const double step = std::floor (b * 4.0);
            if (step != lastGlitchStep) { lastGlitchStep = step; if (freedom < 1.0f) maybeStartGlitch (spb); }
            if (freedom > 0.0f)
            {
                nextGlitchCheck -= 16.0;
                if (nextGlitchCheck <= 0.0)
                {
                    nextGlitchCheck = spb * 0.25 * (0.3 + 1.4 * rnd());
                    maybeStartGlitch (spb);
                }
            }
        }

        float wl = l[i], wr = r[i];
        if (glitchType != GlitchNone)
        {
            const double t = glitchLen - glitchLeft;
            switch (glitchType)
            {
                case GlitchRepeat:
                case GlitchPitch:
                {
                    const double p = glitchAnchor - glitchSlice + std::fmod (t * glitchSpeed, glitchSlice);
                    wl = read (ringL, p); wr = read (ringR, p);
                    // tiny fades at every loop point
                    const double inSlice = std::fmod (t * glitchSpeed, glitchSlice);
                    const float edge = (float) juce::jmin (1.0, juce::jmin (inSlice, glitchSlice - inSlice) / 48.0);
                    wl *= edge; wr *= edge;
                    break;
                }
                case GlitchReverse:
                {
                    const double p = glitchAnchor - 1.0 - t;
                    wl = read (ringL, p); wr = read (ringR, p);
                    break;
                }
                case GlitchTapeStop:
                {
                    const double speed = juce::jmax (0.0, 1.0 - t / glitchLen);
                    glitchRead += speed;
                    const double p = glitchAnchor + glitchRead - 1.0;
                    wl = read (ringL, p) * (float) juce::jmin (1.0, speed * 4.0);
                    wr = read (ringR, p) * (float) juce::jmin (1.0, speed * 4.0);
                    break;
                }
                case GlitchChop:
                {
                    const double slot = std::floor (t / (spb / 8.0));
                    const bool on = ((long long) slot % 2) == 0;
                    if (! on) { wl = 0.0f; wr = 0.0f; }
                    break;
                }
                default: break;
            }
            glitchLeft -= 1.0;
            if (glitchLeft <= 0.0) { glitchType = GlitchNone; glitchNow = GlitchNone; }
        }
        const float targetMix = glitchType != GlitchNone ? 1.0f : 0.0f;
        glitchMix += (targetMix - glitchMix) * fade;
        if (glitchMix > 1.0e-4f)
        {
            l[i] += (wl - l[i]) * glitchMix;
            r[i] += (wr - r[i]) * glitchMix;
        }
        writePos = (writePos + 1) & ringMask;
    }
}

//==============================================================================
void Engine::process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& settings, const Transport& tp)
{
    s = settings;
    const int n = out.getNumSamples();
    out.clear();
    if (n <= 0 || out.getNumChannels() == 0) return;
    float* l = out.getWritePointer (0);
    float* r = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    if (r == nullptr) r = l;

    const double bpm = tp.bpm > 20.0 ? tp.bpm : s.fallbackBpm;
    const double spb = sr * 60.0 / bpm;
    samplesPerBeatNow = spb;
    const bool drumKeys = s.beatMode == BeatKeys || s.beatMode == BeatBoth;

    // the clock: Cubase's when it plays, our own (from the first note) when it doesn't
    bool anyActive = false;
    for (auto& nt : notes) anyActive |= nt.active;
    bool anyNoteOn = false;
    for (const auto meta : midi) anyNoteOn |= meta.getMessage().isNoteOn();
    if (tp.playing)
    {
        if (! clockRunning || std::abs (tp.ppq - lastHostPpq) > 0.05)
        {
            numEvents = 0;
            nextStep = std::ceil (tp.ppq * 4.0 - 1.0e-9);
        }
        beat = tp.ppq;
        clockRunning = true;
    }
    else
    {
        if (clockRunning && lastHostPpq >= 0.0) { clockRunning = false; numEvents = 0; }   // Cubase stopped
        if (clockRunning && ! anyActive && numEvents == 0 && (notesStoppedAt < 0.0 || beat > std::ceil (notesStoppedAt / 4.0) * 4.0 + 0.01))
            clockRunning = false;
        if (! clockRunning && anyNoteOn)
        {
            beat = 0.0; nextStep = 0.0; numEvents = 0; takeoverUntil = -1.0; notesStoppedAt = -1.0;
            clockRunning = true;
        }
        lastHostPpq = -1.0;
    }
    const double blockBeats = n / spb;
    if (tp.playing) lastHostPpq = tp.ppq + blockBeats;

    // the low keys -> drums (they need the clock)
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && drumKeys && msg.getNoteNumber() < drumKeysBelow)
            playDrumKey (msg.getNoteNumber(), msg.getFloatVelocity(), beat + meta.samplePosition / spb);
    }

    // the drums
    drumMidi.clear();
    if (clockRunning)
    {
        scheduleBeat (beat, beat + blockBeats + 0.25);
        fireEvents (beat, beat + blockBeats, spb, 0, n, drumMidi);
    }
    if (s.kit != kitNow) { kitNow = s.kit; drums.setSound (s.kit); }
    drumBuffer.setSize (2, n, false, false, true);
    drumBuffer.clear();
    {
        float* dl = drumBuffer.getWritePointer (0);
        float* dr = drumBuffer.getWritePointer (1);
        int pos = 0;
        for (const auto meta : drumMidi)
        {
            const int at = juce::jlimit (pos, n, meta.samplePosition);
            if (at > pos) { drums.render (dl + pos, dr + pos, at - pos); pos = at; }
            drums.handle (meta.getMessage());
        }
        if (pos < n) drums.render (dl + pos, dr + pos, n - pos);
    }

    // the swarm, with the notes coming in where they are in the block
    {
        int pos = 0;
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            const int at = juce::jlimit (pos, n, meta.samplePosition);
            if (at > pos) { renderNotes (l, r, pos, at - pos); pos = at; }
            if (msg.isNoteOn())
            {
                if (! (drumKeys && msg.getNoteNumber() < drumKeysBelow)) noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
            }
            else if (msg.isNoteOff())
            {
                if (! (drumKeys && msg.getNoteNumber() < drumKeysBelow)) noteOff (msg.getNoteNumber());
            }
            else if (msg.isPitchWheel()) bend = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
            else if (msg.isController() && msg.getControllerNumber() == 1) modWheel = (float) msg.getControllerValue() / 127.0f;
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                for (auto& nt : notes) nt.held = false;
        }
        if (pos < n) renderNotes (l, r, pos, n - pos);
    }

    // drums into the mix (before the glitch buffer, so they get chopped too)
    for (int i = 0; i < n; ++i)
    {
        l[i] += drumBuffer.getSample (0, i) * s.drumLevel * 0.9f;
        if (r != l) r[i] += drumBuffer.getSample (1, i) * s.drumLevel * 0.9f;
    }

    // stutter / reverse / tape stop / chop
    if (r != l) glitchProcess (l, r, n, spb);

    // tape: warmth, a softer top, a little hiss
    if (s.tape > 0.001f)
    {
        const float drive = 1.0f + s.tape * 1.5f;
        const float cut = 20000.0f - s.tape * 12000.0f;
        const float a = 1.0f - std::exp (-twoPi * cut / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            const float h = hiss.next() * s.tape * 0.0012f;
            float xl = std::tanh (l[i] * drive) / drive * (1.0f + 0.25f * s.tape) + h;
            tapeLpL += a * (xl - tapeLpL);
            l[i] = tapeLpL;
            if (r != l)
            {
                float xr = std::tanh (r[i] * drive) / drive * (1.0f + 0.25f * s.tape) + h;
                tapeLpR += a * (xr - tapeLpR);
                r[i] = tapeLpR;
            }
        }
    }

    // space
    if (s.space > 0.001f)
    {
        if (std::abs (s.space - lastSpace) > 0.002f)
        {
            lastSpace = s.space;
            juce::Reverb::Parameters p;
            p.roomSize = 0.5f + 0.45f * s.space;
            p.damping = 0.4f;
            p.wetLevel = 0.45f * s.space;
            p.dryLevel = 1.0f - 0.25f * s.space;
            p.width = 1.0f;
            reverb.setParameters (p);
        }
        if (r != l) reverb.processStereo (l, r, n); else reverb.processMono (l, n);
    }

    const float g = juce::Decibels::decibelsToGain (s.volumeDb);
    for (int i = 0; i < n; ++i) { l[i] *= g; if (r != l) r[i] *= g; }

    // the clock moves on
    if (clockRunning) beat += blockBeats;
    beatNow = beat;

    // the screen
    int bugs = 0;
    const int nb = 2 + juce::roundToInt (s.swarm * 6.0f);
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& nt = notes[i];
        displayKey[i] = nt.active ? nt.key : -1;
        for (int j = 0; j < bugsPerNote; ++j)
        {
            auto& d = display[i * bugsPerNote + (size_t) j];
            const auto& b = nt.bugs[(size_t) j];
            const bool on = nt.active && j < nb;
            d.x = b.pan;
            d.pitch = (float) nt.key + b.offset;
            d.bright = on ? nt.env * b.seg * b.decayEnv : 0.0f;
            bugs += on ? 1 : 0;
        }
    }
    activeBugs = bugs;
    for (auto& f : drumFlash) f = f.load() * 0.85f;
}
} // namespace glitch
