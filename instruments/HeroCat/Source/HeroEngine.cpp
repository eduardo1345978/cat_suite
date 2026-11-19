#include "HeroEngine.h"

using namespace juce;

namespace hero
{
StringArray sourceNames()    { return { "Saw Lead", "Square", "Supersaw", "Chip Pulse", "Guitar", "Strings", "Flute", "Choir", "Bell" }; }
StringArray chordModeNames() { return { "Off (one note)", "Scale: triads", "Scale: 7ths", "Scale: anime add9", "Major", "Minor", "Sus2", "Power", "Maj7", "Min7" }; }
StringArray scaleNames()     { return { "Major", "Minor", "Dorian", "Mixolydian", "Harmonic minor", "Pentatonic", "Japanese (In)" }; }
StringArray arpModeNames()   { return { "Off", "Up", "Down", "Up-Down", "Random", "Waterfall", "Pattern" }; }
StringArray rateNames()      { return { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }; }
StringArray textureNames()   { return { "Off", "Rain", "Birds", "Room", "Chimes", "Wind" }; }
StringArray lfoShapeNames()  { return { "Sine", "Triangle", "Saw", "Square", "Random", "Bounce" }; }
StringArray lfoTargetNames() { return { "Cutoff", "Pitch", "Pan", "A/B Mix", "Crush", "Volume" }; }
StringArray keyNames()       { return { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }; }

namespace
{
const std::vector<std::vector<int>>& scales()
{
    static const std::vector<std::vector<int>> s {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
        { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 4, 7, 9 }, { 0, 1, 5, 7, 8 } };
    return s;
}

struct Kind { std::vector<int> set; const char* name; };
const std::vector<Kind>& kinds()
{
    static const std::vector<Kind> k {
        { { 0, 4, 7 }, "" }, { { 0, 3, 7 }, "m" }, { { 0, 3, 6 }, "dim" }, { { 0, 4, 8 }, "aug" }, { { 0, 2, 7 }, "sus2" },
        { { 0, 5, 7 }, "sus4" }, { { 0, 7 }, "5" }, { { 0, 4, 7, 11 }, "maj7" }, { { 0, 3, 7, 10 }, "m7" }, { { 0, 4, 7, 10 }, "7" },
        { { 0, 3, 6, 10 }, "m7b5" }, { { 0, 2, 4, 7 }, "add9" }, { { 0, 2, 3, 7 }, "m(add9)" }, { { 0, 3, 7, 11 }, "mMaj7" },
        { { 0, 1, 5 }, "sus b2" }, { { 0, 1, 5, 8 }, "" } };
    return k;
}

constexpr double twoPi = MathConstants<double>::twoPi;

inline float blep (double t, double dt)
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}
inline float saw (double t, double dt) { return (float) (2.0 * t - 1.0) - blep (t, dt); }
inline void adv (double& ph, double inc) { ph += inc; if (ph >= 1.0) ph -= 1.0; }
}

String chordKindName (int k) { return isPositiveAndBelow (k, (int) kinds().size()) ? String (kinds()[(size_t) k].name) : String(); }

//==============================================================================
Engine::Engine()
{
    for (auto& a : noteLog) a = 0;
    const float def[16] = { 1, 0, 0.6f, 0.6f, 1, 0, 0.6f, 0, 1, 0, 0.6f, 0.6f, 1, 0.6f, 0, 0.8f };
    for (int i = 0; i < 16; ++i) pattern[(size_t) i] = def[i];
}

void Engine::randomisePattern (Random& r)
{
    for (int i = 0; i < 16; ++i)
    {
        const bool beat = i % 4 == 0;
        const float x = r.nextFloat();
        pattern[(size_t) i] = beat ? (x < 0.85f ? 1.0f : 0.6f) : (x < 0.4f ? 0.0f : x < 0.75f ? 0.55f : 0.85f);
    }
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    chorusBuf.assign (2 * 4096, 0.0f);
    delayL.assign ((size_t) (sr * 2.0) + 4, 0.0f);
    delayR.assign ((size_t) (sr * 2.0) + 4, 0.0f);
    verb.setSampleRate (sr);
    outS.reset (sr, 0.05);
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) { v.active = false; v.releasing = false; }
    for (auto& p : pending) p.on = false;
    keysDown.clear(); keyChords.clear(); pool.clear(); lastChord.clear();
    latched = false; clockOn = false; lastPpq = -1.0;
    std::fill (chorusBuf.begin(), chorusBuf.end(), 0.0f);
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    for (auto& p : pings) p.env = 0.0f;
    verb.reset();
    chordRoot = -1; arpStep = -1;
}

int Engine::voicesSounding() const
{
    int n = 0;
    for (auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

//==============================================================================
std::vector<int> Engine::chordFor (int key, const Settings& s, const std::vector<int>& previous) const
{
    std::vector<int> notes;
    if (s.chordMode == 0) return { key };
    if (s.chordMode <= 3)
    {
        const auto& sc = scales()[(size_t) jlimit (0, (int) scales().size() - 1, s.scale)];
        const int n = (int) sc.size();
        const int rel = key - s.key;
        const int oct = (int) std::floor (rel / 12.0);
        const int pc = rel - oct * 12;
        int idx = 0;                                   // the scale note at or just below the key (it snaps into the scale)
        for (int i = 0; i < n; ++i) if (sc[(size_t) i] <= pc) idx = i;
        auto deg = [&] (int j) { return s.key + 12 * oct + sc[(size_t) (j % n)] + 12 * (j / n); };
        notes = { deg (idx), deg (idx + 2), deg (idx + 4) };
        if (s.chordMode == 2) notes.push_back (deg (idx + 6));
        if (s.chordMode == 3) notes.push_back (deg (idx + 7 + (n == 7 ? 1 : 0)));   // the 9th (an octave + 2 steps up)
    }
    else
    {
        static const std::vector<int> shapes[] = { { 0, 4, 7 }, { 0, 3, 7 }, { 0, 2, 7 }, { 0, 7, 12 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 } };
        for (int i : shapes[jlimit (0, 5, s.chordMode - 4)]) notes.push_back (key + i);
    }

    // voice leading: the bass stays, every upper note goes to the octave nearest the last chord
    if (s.voiceLead && previous.size() > 1 && notes.size() > 1)
    {
        const int bass = notes[0];
        for (size_t i = 1; i < notes.size(); ++i)
        {
            int best = notes[i], bestD = 1000;
            for (int k : { -12, 0, 12 })
            {
                const int c = notes[i] + k;
                if (c <= bass) continue;
                int d = 1000;
                for (int p : previous) d = jmin (d, std::abs (c - p));
                if (d < bestD) { bestD = d; best = c; }
            }
            notes[i] = best;
        }
        std::sort (notes.begin() + 1, notes.end());
        notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
    }
    for (auto& x : notes) x = jlimit (0, 127, x);
    return notes;
}

void Engine::nameChord (const std::vector<int>& notes)
{
    if (notes.empty()) return;
    const int root = notes[0] % 12;
    std::vector<int> set;
    for (int n : notes) set.push_back (((n - notes[0]) % 12 + 12) % 12);
    std::sort (set.begin(), set.end());
    set.erase (std::unique (set.begin(), set.end()), set.end());
    int kind = -1;
    for (size_t i = 0; i < kinds().size(); ++i) if (kinds()[i].set == set) kind = (int) i;
    chordKind = kind;
    chordRoot = root;
}

void Engine::logNote (int note, float vel)
{
    const int p = logPos.load();
    noteLog[(size_t) (p & 63)] = note | (jlimit (1, 127, roundToInt (vel * 127.0f)) << 8);
    logPos = p + 1;
}

//==============================================================================
void Engine::keyDown (int key, float vel, const Settings& s, MidiBuffer* out, int pos)
{
    auto notes = chordFor (key, s, lastChord);
    lastChord = notes;
    nameChord (notes);
    keysDown.push_back (key);
    keyChords.push_back ({ key, notes });

    if (s.arpMode != ArpOff)
    {
        arpVel = vel;
        if (latched) { pool.clear(); latched = false; }
        const bool wasEmpty = pool.empty();
        for (int n : notes) pool.push_back (n);
        std::sort (pool.begin(), pool.end());
        pool.erase (std::unique (pool.begin(), pool.end()), pool.end());
        if (wasEmpty) { poolCounter = 0; waterfallPos = 0; upDownDir = 1; if (! s.playing) { beats = 0.0; stepIndex = 0; nextStep = 0.0; } }
        return;
    }
    const int strumStep = roundToInt (s.strum * s.strum * 0.12 * sr);       // up to 120 ms between notes
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const float v = vel * (i == 0 ? 1.0f : 0.85f);
        if (i == 0 || strumStep == 0) start (notes[i], v, key, -1, 1.0f, s, out, pos);
        else schedule ((int) i * strumStep, notes[i], v, key, -1, 1.0f);
    }
}

void Engine::keyUp (int key, MidiBuffer* out, int pos)
{
    keysDown.erase (std::remove (keysDown.begin(), keysDown.end(), key), keysDown.end());
    for (size_t i = 0; i < keyChords.size(); ++i)
        if (keyChords[i].first == key) { keyChords.erase (keyChords.begin() + (long) i); break; }
    for (auto& v : voices) if (v.active && ! v.releasing && v.owner == key) release (v, out, pos);
    for (auto& p : pending) if (p.on && p.owner == key) p.on = false;

    if (keysDown.empty() && latchNow) { latched = ! pool.empty(); return; }
    pool.clear();
    for (auto& [k, ns] : keyChords) for (int n : ns) pool.push_back (n);
    std::sort (pool.begin(), pool.end());
    pool.erase (std::unique (pool.begin(), pool.end()), pool.end());
}

void Engine::schedule (int wait, int note, float vel, int owner, int gate, float longRelease)
{
    for (auto& p : pending)
        if (! p.on) { p = { true, wait, note, owner, gate, vel, longRelease }; return; }
}

void Engine::start (int note, float vel, int owner, int gate, float longRelease, const Settings& s, MidiBuffer* out, int pos)
{
    // the same note still held by the same owner: let it go first
    for (auto& v : voices) if (v.active && ! v.releasing && v.note == note && v.owner == owner) release (v, out, pos);

    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (pick == nullptr)
        for (auto& v : voices)
            if (v.releasing && (pick == nullptr || v.env < pick->env)) pick = &v;
    if (pick == nullptr)
        for (auto& v : voices)
            if (pick == nullptr || v.age > pick->age) pick = &v;
    if (pick->active && ! pick->releasing && out != nullptr) out->addEvent (MidiMessage::noteOff (1, pick->note), pos);

    auto& v = *pick;
    const double hz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    v.active = true; v.releasing = false;
    v.note = note; v.owner = owner; v.gateLeft = gate; v.vel = vel; v.longRelease = longRelease;
    v.stage = 0; v.env = 0.0f; v.age = 0.0; v.s1 = v.s2 = 0.0f;
    v.targetHz = hz;
    v.hz = (s.glide > 0.001f && lastHz > 0.0) ? lastHz : hz;
    lastHz = hz;
    v.pan = jlimit (0.15f, 0.85f, 0.5f + (float) (note - 64) / 60.0f + (rng.nextFloat() - 0.5f) * 0.12f);
    startSlot (v.a, s.srcA, hz, vel);
    startSlot (v.b, s.srcB, hz * std::pow (2.0, s.octB), vel);
    logNote (note, vel);
    if (out != nullptr) out->addEvent (MidiMessage::noteOn (1, note, vel), pos);
}

void Engine::release (Voice& v, MidiBuffer* out, int pos)
{
    if (v.releasing) return;
    v.releasing = true;
    v.stage = 2;
    v.relCoef = (float) std::exp (-6.9 / (relSeconds * v.longRelease * sr));   // -60 dB in the release time
    if (out != nullptr) out->addEvent (MidiMessage::noteOff (1, v.note), pos);
}

void Engine::startSlot (Slot& sl, int src, double hz, float vel)
{
    sl.src = src;
    if (src < 0) return;
    for (auto& p : sl.ph) p = rng.nextDouble();
    sl.mph = 0.0;
    sl.b1 = sl.b2 = sl.b3 = sl.b4 = sl.f1 = sl.f2 = sl.last = sl.noiseLp = 0.0f;
    if (src == Guitar || src == Strings)
    {
        std::fill (sl.ks.begin(), sl.ks.end(), 0.0f);
        sl.w = 0;
        sl.excite = jmax (2, roundToInt (sr / hz));                  // one period of noise = the pluck
        sl.f2 = 0.25f + 0.65f * vel;                                  // how bright the pluck is
    }
    if (src == Flute) sl.excite = roundToInt (sr * 0.06);             // the "chiff" when you start to blow
    if (src == Bell) { sl.idx = 1.2f + 2.4f * vel; sl.f1 = 1.0f; }
}

float Engine::renderSlot (Slot& sl, Voice& v, double hz, const Settings& s)
{
    const double dt = hz / sr;
    switch (sl.src)
    {
        case Saw:
        {
            const double d = s.detune * 0.012;
            const float o = saw (sl.ph[0], dt * (1.0 - d)) + saw (sl.ph[1], dt * (1.0 + d));
            adv (sl.ph[0], dt * (1.0 - d)); adv (sl.ph[1], dt * (1.0 + d));
            return o * 0.5f;
        }
        case Square:
        {
            const double t = sl.ph[0], t2 = std::fmod (t + 0.5, 1.0);
            const float o = (t < 0.5 ? 1.0f : -1.0f) + blep (t, dt) - blep (t2, dt);
            adv (sl.ph[0], dt);
            return o * 0.55f;
        }
        case Supersaw:
        {
            static const double spread[5] = { -1.0, -0.45, 0.0, 0.5, 1.0 };
            float o = 0.0f;
            for (int i = 0; i < 5; ++i)
            {
                const double inc = dt * (1.0 + spread[i] * (0.004 + s.detune * 0.03));
                o += saw (sl.ph[i], inc) * (i == 2 ? 1.0f : 0.75f);
                adv (sl.ph[i], inc);
            }
            return o * 0.3f;
        }
        case Chip:
        {
            float o = sl.ph[0] < 0.25 ? 1.0f : -1.0f;
            adv (sl.ph[0], dt);
            return std::round (o * 7.0f) / 7.0f * 0.5f;
        }
        case Guitar:
        case Strings:
        {
            // a string: a delay one period long, the sound goes round and round and gets softer and darker (Karplus-Strong)
            const double d = jlimit (2.0, (double) ksSize - 4.0, sr / hz - 0.5);
            double rp = sl.w - d; if (rp < 0) rp += ksSize;
            const int i0 = (int) rp; const float fr = (float) (rp - i0);
            const float y = sl.ks[(size_t) i0] + fr * (sl.ks[(size_t) ((i0 + 1) % ksSize)] - sl.ks[(size_t) i0]);
            const float avg = 0.5f * (y + sl.last);
            sl.last = y;
            float in = 0.0f;
            const float white = rng.nextFloat() * 2.0f - 1.0f;
            sl.noiseLp += sl.f2 * (white - sl.noiseLp);
            if (sl.excite > 0) { --sl.excite; in = sl.noiseLp * 0.9f; }
            if (sl.src == Strings && ! v.releasing) in += sl.noiseLp * 0.035f;     // the bow keeps rubbing
            const float loss = sl.src == Guitar ? 0.9975f : 0.9993f;
            sl.ks[(size_t) sl.w] = avg * loss + in;
            sl.w = (sl.w + 1) % ksSize;
            return y * (sl.src == Guitar ? 0.9f : 1.6f);
        }
        case Flute:
        {
            const double t = sl.ph[0];
            float o = (float) (std::sin (twoPi * t) + 0.22 * std::sin (2.0 * twoPi * t) + 0.08 * std::sin (3.0 * twoPi * t));
            adv (sl.ph[0], dt);
            const float white = rng.nextFloat() * 2.0f - 1.0f;
            sl.noiseLp += 0.3f * (white - sl.noiseLp);
            const float breath = (white - sl.noiseLp) * 0.08f * (0.4f + (float) std::abs (o));
            float chiff = 0.0f;
            if (sl.excite > 0) { --sl.excite; chiff = sl.noiseLp * 0.5f * (float) sl.excite / (float) (sr * 0.06); }
            return o * 0.45f + breath + chiff;
        }
        case Choir:
        {
            float o = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                const double inc = dt * (1.0 + (i - 1) * 0.0045);
                o += saw (sl.ph[i], inc);
                adv (sl.ph[i], inc);
            }
            // two "Ah" resonances of the mouth (formants)
            const float fA = 2.0f * (float) std::sin (MathConstants<double>::pi * 750.0 / sr);
            const float fB = 2.0f * (float) std::sin (MathConstants<double>::pi * 1150.0 / sr);
            const float hA = o - sl.b1 - 0.12f * sl.b2; sl.b2 += fA * hA; sl.b1 += fA * sl.b2;
            const float hB = o - sl.b3 - 0.15f * sl.b4; sl.b4 += fB * hB; sl.b3 += fB * sl.b4;
            return (sl.b2 * 0.09f + sl.b4 * 0.06f + o * 0.02f);
        }
        case Bell:
        {
            const float o = (float) std::sin (twoPi * sl.ph[0] + sl.idx * std::sin (twoPi * sl.mph));
            adv (sl.ph[0], dt); adv (sl.mph, dt * 3.5);
            sl.idx *= 0.99993f; sl.f1 *= 0.99996f;
            return o * 0.5f * sl.f1;
        }
        default: return 0.0f;
    }
}

//==============================================================================
std::vector<int> Engine::arpSequence (const Settings& s) const
{
    std::vector<int> seq;
    for (int o = 0; o < jlimit (1, 4, s.octaves); ++o)
        for (int n : pool) if (n + 12 * o <= 127) seq.push_back (n + 12 * o);
    return seq;
}

void Engine::arpTick (const Settings& s, MidiBuffer* out, int pos, int stepSamples)
{
    const int step = (int) (stepIndex % 16);
    arpStep = step;
    if (pool.empty()) return;
    float vel = arpVel;
    if (s.arpMode == ArpPattern)
    {
        const float pv = pattern[(size_t) step].load();
        if (pv < 0.02f) return;
        vel *= pv;
    }
    const auto seq = arpSequence (s);
    const int n = (int) seq.size();
    if (n == 0) return;

    int note = seq[0];
    int gate = jmax (64, roundToInt (stepSamples * jlimit (0.05f, 1.0f, s.gate)));
    float longRel = 1.0f;
    switch (s.arpMode)
    {
        case ArpUp:   note = seq[(size_t) (poolCounter % n)]; break;
        case ArpDown: note = seq[(size_t) (n - 1 - poolCounter % n)]; break;
        case ArpUpDown:
        {
            const int period = jmax (1, 2 * n - 2);
            const int p = poolCounter % period;
            note = seq[(size_t) (p < n ? p : period - p)];
            break;
        }
        case ArpRandom: note = seq[(size_t) rng.nextInt (n)]; break;
        case ArpWaterfall:
        {
            // falls from the top to the bottom, each note keeps ringing under the next ones
            note = seq[(size_t) (n - 1 - waterfallPos % n)];
            ++waterfallPos;
            vel *= 1.0f - 0.18f * (float) ((waterfallPos - 1) % 3);
            gate = roundToInt (stepSamples * (2.0f + s.gate * 6.0f));
            longRel = 3.0f;
            break;
        }
        default: note = seq[(size_t) (poolCounter % n)]; break;
    }
    ++poolCounter;

    // CHAOS: it improvises a little
    const float c = s.chaos;
    if (c > 0.0f)
    {
        if (stepIndex % 4 != 0 && rng.nextFloat() < c * 0.18f) return;               // a rest
        if (rng.nextFloat() < c * 0.25f) note = seq[(size_t) rng.nextInt (n)];        // another chord note
        if (rng.nextFloat() < c * 0.2f && note + 12 <= 127) note += 12;               // an octave jump
        vel *= 1.0f - c * 0.4f * rng.nextFloat();
        if (rng.nextFloat() < c * 0.15f) schedule (stepSamples / 2, note, vel * 0.8f, -2, gate / 2, longRel);   // a double hit
    }
    start (note, vel, -2, gate, longRel, s, out, pos);
}

//==============================================================================
float Engine::texture (const Settings& s, float& r)
{
    float l = 0.0f; r = 0.0f;
    const float white = rng.nextFloat() * 2.0f - 1.0f;
    auto ping = [this] (double hz, float amp, float decaySeconds)
    {
        for (auto& p : pings)
            if (p.env < 0.001f)
            {
                p.env = amp; p.hz = hz; p.ph = 0.0; p.pan = 0.2f + 0.6f * rng.nextFloat();
                p.decay = (float) std::exp (-1.0 / (decaySeconds * sr));
                return;
            }
    };
    switch (s.texture)
    {
        case 1:   // rain: a soft hiss and drops
            texLp += 0.25f * (white - texLp);
            texLp2 += 0.02f * (texLp - texLp2);
            l = (texLp - texLp2) * 0.35f;
            r = l * 0.8f + (rng.nextFloat() * 2.0f - 1.0f) * 0.02f;
            if (rng.nextFloat() < 0.0007f) ping (1800.0 + rng.nextFloat() * 3500.0, 0.08f + rng.nextFloat() * 0.08f, 0.012f);
            break;
        case 2:   // birds: short songs of 2-5 chirps, now and then
            if (chirpT < 0.0f && rng.nextFloat() < 0.00005f) { chirpsLeft = 2 + rng.nextInt (4); chirpT = 0.0f; chirpHz = 2600.0f + rng.nextFloat() * 2200.0f; chirpPan = 0.15f + 0.7f * rng.nextFloat(); }
            if (chirpT >= 0.0f)
            {
                const float len = 0.09f;
                chirpT += (float) (1.0 / sr);
                if (chirpT < len)
                {
                    const float e = std::sin (MathConstants<float>::pi * chirpT / len);
                    const double hz = chirpHz * (1.0 + 0.35 * chirpT / len) * (1.0 + 0.04 * std::sin (twoPi * 40.0 * chirpT));
                    chirpPh += hz / sr; if (chirpPh > 1.0) chirpPh -= 1.0;
                    const float o = (float) std::sin (twoPi * chirpPh) * e * e * 0.12f;
                    l = o * (1.0f - chirpPan); r = o * chirpPan;
                }
                else if (chirpT > len + 0.06f) { if (--chirpsLeft > 0) { chirpT = 0.0f; chirpHz *= 0.9f + 0.25f * rng.nextFloat(); } else chirpT = -1.0f; }
            }
            texLp += 0.05f * (white - texLp);
            l += texLp * 0.015f; r += texLp * 0.015f;
            break;
        case 3:   // a quiet room
            brown = 0.998f * brown + white * 0.02f;
            texLp += 0.1f * (brown - texLp);
            l = texLp * 0.3f; r = l * 0.92f + white * 0.003f;
            break;
        case 4:   // wind chimes on the chord's notes
            if (rng.nextFloat() < 0.00013f)
            {
                const auto& src = pool.empty() ? lastChord : pool;
                int note = src.empty() ? 84 : src[(size_t) rng.nextInt ((int) src.size())];
                while (note < 84) note += 12;
                ping (440.0 * std::pow (2.0, (note - 69) / 12.0), 0.07f + rng.nextFloat() * 0.05f, 2.5f);
            }
            break;
        case 5:   // wind: noise through a slowly moving resonance
        {
            windPh += 0.07 / sr;
            const float fc = 380.0f + 320.0f * (float) std::sin (twoPi * windPh) + 120.0f * (float) std::sin (twoPi * windPh * 2.7);
            const float f = 2.0f * std::sin (MathConstants<float>::pi * fc / (float) sr);
            const float h = white - windL - 0.35f * windB; windB += f * h; windL += f * windB;
            l = windB * 0.12f; r = windB * 0.1f + white * 0.002f;
            break;
        }
        default: break;
    }
    for (auto& p : pings)
        if (p.env > 0.001f)
        {
            const float o = (float) std::sin (twoPi * p.ph) * p.env;
            p.ph += p.hz / sr; if (p.ph > 1.0) p.ph -= 1.0;
            p.env *= p.decay;
            l += o * (1.0f - p.pan); r += o * p.pan;
        }
    return l;
}

//==============================================================================
void Engine::process (AudioBuffer<float>& buffer, const MidiBuffer& midiIn, MidiBuffer* out, const Settings& s)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    latchNow = s.latch;

    // the arp loses its notes when it is switched off
    if (s.arpMode == ArpOff && ! pool.empty())
    {
        if (latched) pool.clear();
        latched = false;
    }
    if (s.arpMode != ArpOff && ! s.latch && latched) { pool.clear(); latched = false; }

    const float attackS = 0.001f + s.attack * s.attack * 3.0f;
    const float decayS = 0.01f + s.decay * s.decay * 3.0f;
    relSeconds = 0.01f + s.release * s.release * 5.0f;
    const float attInc = 1.0f / (attackS * (float) sr);
    const float decCoef = (float) std::exp (-4.0 / (decayS * sr));
    const float glideCoef = s.glide > 0.001f ? (float) std::exp (-1.0 / ((0.005 + s.glide * s.glide * 0.6) * sr)) : 0.0f;
    static const double rates[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    const double stepBeats = rates[jlimit (0, 5, s.rate)];
    const double beatInc = s.bpm / 60.0 / sr;
    const int stepSamples = jmax (1, (int) (stepBeats / beatInc));
    auto stepTime = [&] (long long k) { return (double) k * stepBeats + ((k & 1) ? s.swing * stepBeats * 0.5 : 0.0); };
    const float baseCut = 30.0f * std::pow (2.0f, s.cutoff * 9.3f);
    const float k = 2.0f - 1.9f * s.reso;
    outS.setTargetValue (Decibels::decibelsToGain (s.outDb));

    // the delay time: a dotted 1/8 note
    const int delaySamples = jlimit (1, (int) delayL.size() - 2, (int) (0.75 * 60.0 / s.bpm * sr));
    const float fb = 0.3f + 0.4f * s.delay;

    auto it = midiIn.cbegin();
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        // 1. the keys
        for (; it != midiIn.cend() && (*it).samplePosition <= i; ++it)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn()) keyDown (m.getNoteNumber(), m.getFloatVelocity(), s, out, i);
            else if (m.isNoteOff()) keyUp (m.getNoteNumber(), out, i);
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                for (auto& v : voices) if (v.active) release (v, out, i);
                for (auto& p : pending) p.on = false;
                keysDown.clear(); keyChords.clear(); pool.clear(); latched = false;
            }
        }
        // 2. strummed notes and double hits that are due
        for (auto& p : pending)
            if (p.on && --p.wait <= 0) { p.on = false; start (p.note, p.vel, p.owner, p.gate, p.longRelease, s, out, i); }

        // 3. the arp's clock (the DAW's when it plays, its own when it does not)
        if (s.arpMode != ArpOff)
        {
            if (s.playing)
            {
                const double host = s.ppq + i * beatInc;
                if (lastPpq < 0.0 || std::abs (host - beats - beatInc) > 0.02)
                {
                    stepIndex = (long long) std::ceil (host / stepBeats - 1e-6);
                    nextStep = stepTime (stepIndex);
                }
                beats = host; lastPpq = host;
            }
            else { beats += beatInc; lastPpq = -1.0; }
            if (beats + 1e-9 >= nextStep)
            {
                arpTick (s, out, i, stepSamples);
                ++stepIndex;
                nextStep = stepTime (stepIndex);
                if (nextStep < beats) { stepIndex = (long long) std::ceil (beats / stepBeats); nextStep = stepTime (stepIndex); }
            }
        }

        // 4. the LFO (and CHAOS's slow wander)
        lfoPh += s.lfoRate / sr;
        if (lfoPh >= 1.0) { lfoPh -= 1.0; lfoHold = rng.nextFloat() * 2.0f - 1.0f; }
        float lfo = 0.0f;
        switch (s.lfoShape)
        {
            case 0: lfo = (float) std::sin (twoPi * lfoPh); break;
            case 1: lfo = 1.0f - 4.0f * std::abs ((float) lfoPh - 0.5f); break;
            case 2: lfo = 1.0f - 2.0f * (float) lfoPh; break;
            case 3: lfo = lfoPh < 0.5 ? 1.0f : -1.0f; break;
            case 4: lfo = lfoHold; break;
            default: lfo = 2.0f * (float) std::abs (std::sin (MathConstants<double>::pi * lfoPh)) - 1.0f; break;
        }
        const float m = lfo * s.lfoDepth;
        chaosWalk = jlimit (-1.0f, 1.0f, chaosWalk * 0.99999f + (rng.nextFloat() - 0.5f) * 0.004f * s.chaos);
        const float cutMul = std::pow (2.0f, (s.lfoTarget == 0 ? m * 3.0f : 0.0f) + chaosWalk * s.chaos * 1.5f);
        const double pitchMul = s.lfoTarget == 1 ? std::pow (2.0, m / 12.0) : 1.0;
        const float mix = jlimit (0.0f, 1.0f, s.mix + (s.lfoTarget == 3 ? m * 0.5f : 0.0f));

        // 5. the voices
        float l = 0.0f, r = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            v.age += 1.0 / sr;
            if (v.gateLeft > 0 && --v.gateLeft == 0) release (v, out, i);
            // the envelope
            if (v.stage == 0) { v.env += attInc; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; } }
            else if (v.stage == 1) v.env = s.sustain + (v.env - s.sustain) * decCoef;
            else
            {
                v.env *= v.relCoef;
                if (v.env < 0.0002f) { v.active = false; continue; }
            }
            // the pitch: glide, the delayed vibrato (the anime lead's), the LFO
            if (glideCoef > 0.0f) v.hz = v.targetHz + (v.hz - v.targetHz) * glideCoef; else v.hz = v.targetHz;
            const float vib = s.vibrato * jlimit (0.0f, 1.0f, (float) (v.age - 0.25) / 0.4f);
            const double hz = v.hz * pitchMul * std::pow (2.0, vib * 0.35 * std::sin (twoPi * 5.5 * v.age) / 12.0);

            float o = renderSlot (v.a, v, hz, s);
            if (v.b.src >= 0) o = o * (1.0f - mix) + renderSlot (v.b, v, hz * std::pow (2.0, s.octB), s) * mix;

            // the filter (low-pass, opens with the envelope)
            const float fc = jlimit (20.0f, (float) sr * 0.45f, baseCut * cutMul * std::pow (2.0f, s.envAmt * v.env * 5.0f) * (0.55f + 0.45f * v.vel));
            const float g = std::tan (MathConstants<float>::pi * fc / (float) sr);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float v1 = a1 * (v.s1 + g * (o - v.s2));
            const float v2 = v.s2 + g * v1;
            v.s1 = 2.0f * v1 - v.s1; v.s2 = 2.0f * v2 - v.s2;
            const float y = v2 * v.env * (0.35f + 0.65f * v.vel) * 0.32f;
            l += y * std::sqrt (1.0f - v.pan); r += y * std::sqrt (v.pan);
        }
        if (s.lfoTarget == 2) { const float p = 0.5f + 0.5f * m; l *= std::sqrt (1.0f - p) * 1.414f; r *= std::sqrt (p) * 1.414f; }
        if (s.lfoTarget == 5) { const float g = 1.0f - s.lfoDepth * (0.5f - 0.5f * lfo); l *= g; r *= g; }

        // 6. the texture under it
        if (s.texture > 0)
        {
            float tr = 0.0f;
            const float tl = texture (s, tr);
            l += tl * s.texLevel; r += tr * s.texLevel;
        }

        // 7. the effects: crush, drive, chorus, delay (the reverb and width come after, for the whole block)
        const float crush = jlimit (0.0f, 1.0f, s.crush + (s.lfoTarget == 4 ? jmax (0.0f, m) : 0.0f));
        if (crush > 0.001f)
        {
            crushCount += 1.0f;
            if (crushCount >= 1.0f + crush * 14.0f)
            {
                crushCount = 0.0f;
                const float steps = std::pow (2.0f, 15.0f - crush * 11.0f);
                crushHoldL = std::round (l * steps) / steps; crushHoldR = std::round (r * steps) / steps;
            }
            l = crushHoldL; r = crushHoldR;
        }
        if (s.drive > 0.001f)
        {
            const float kd = 1.0f + s.drive * 8.0f, n = 1.0f / std::sqrt (kd);
            l = std::tanh (l * kd) * n; r = std::tanh (r * kd) * n;
        }
        if (s.chorus > 0.001f)
        {
            chorusPh += 0.6 / sr; if (chorusPh > 1.0) chorusPh -= 1.0;
            chorusBuf[(size_t) chorusW] = l; chorusBuf[(size_t) (4096 + chorusW)] = r;
            auto rd = [&] (int base, double ms)
            {
                double rp = chorusW - ms * 0.001 * sr; while (rp < 0) rp += 4096;
                const int i0 = (int) rp; const float f = (float) (rp - i0);
                return chorusBuf[(size_t) (base + i0)] + f * (chorusBuf[(size_t) (base + (i0 + 1) % 4096)] - chorusBuf[(size_t) (base + i0)]);
            };
            const float wl = rd (0, 8.0 + 3.0 * std::sin (twoPi * chorusPh)), wr = rd (4096, 8.0 + 3.0 * std::cos (twoPi * chorusPh));
            chorusW = (chorusW + 1) % 4096;
            l = l * (1.0f - 0.3f * s.chorus) + wl * 0.6f * s.chorus;
            r = r * (1.0f - 0.3f * s.chorus) + wr * 0.6f * s.chorus;
        }
        if (s.delay > 0.001f)
        {
            const int n = (int) delayL.size();
            int rp = delayW - delaySamples; if (rp < 0) rp += n;
            const float dl = delayL[(size_t) rp], dr = delayR[(size_t) rp];
            delayLpL += 0.35f * (dr - delayLpL); delayLpR += 0.35f * (dl - delayLpR);
            delayL[(size_t) delayW] = (l + r) * 0.5f + delayLpL * fb;     // ping-pong: left feeds right, right feeds left
            delayR[(size_t) delayW] = delayLpR * fb;
            delayW = (delayW + 1) % n;
            l += dl * s.delay * 0.6f; r += dr * s.delay * 0.6f;
        }
        L[i] = l;
        if (R != nullptr) R[i] = r; else L[i] = 0.5f * (l + r);
    }

    // the reverb and the width
    if (s.reverb > 0.001f && R != nullptr)
    {
        Reverb::Parameters rp;
        rp.roomSize = 0.55f + 0.44f * s.reverb; rp.damping = 0.4f; rp.wetLevel = s.reverb * 0.4f; rp.dryLevel = 1.0f; rp.width = 1.0f;
        verb.setParameters (rp);
        verb.processStereo (L, R, numSamples);
    }
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outS.getNextValue();
        if (R != nullptr)
        {
            const float mid = 0.5f * (L[i] + R[i]), side = 0.5f * (L[i] - R[i]) * (1.0f + s.width * 1.2f);
            L[i] = (mid + side) * g; R[i] = (mid - side) * g;
            peak = jmax (peak, std::abs (L[i]), std::abs (R[i]));
        }
        else { L[i] *= g; peak = jmax (peak, std::abs (L[i])); }
    }
    level = peak;
    lfoNow = (float) std::sin (twoPi * lfoPh) * s.lfoDepth;
    if (keysDown.empty() && pool.empty() && voicesSounding() == 0) chordRoot = -1;
}
} // namespace hero
