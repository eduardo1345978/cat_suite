#include "ChoirEngine.h"

namespace singcat
{
namespace
{
    constexpr float twoPi = 6.28318530718f;
    constexpr int controlPeriod = 32;

    float smoothstep (float x) noexcept
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    juce::Point<float> lerp (juce::Point<float> a, juce::Point<float> b, float t) noexcept
    {
        return a + (b - a) * t;
    }

    uint32_t hash (uint32_t x) noexcept
    {
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }
}

//==============================================================================
const TypeInfo& typeInfo (SingerType t)
{
    //                 name          cm    note tense breath jitter shimmer vibR  vibD  nasal  cat    fem    child  elder
    static const TypeInfo types[] = {
        { "Bass",        18.0f, 48, 0.58f, 0.03f, 0.004f, 0.03f, 0.95f, 1.0f, 0.00f, false, false, false, false },
        { "Tenor",       17.0f, 57, 0.62f, 0.03f, 0.004f, 0.03f, 1.00f, 1.0f, 0.00f, false, false, false, false },
        { "Alto",        15.2f, 64, 0.52f, 0.06f, 0.004f, 0.03f, 1.00f, 1.0f, 0.00f, false, true,  false, false },
        { "Soprano",     14.2f, 72, 0.56f, 0.05f, 0.004f, 0.03f, 1.05f, 1.0f, 0.00f, false, true,  false, false },
        { "Child",       11.5f, 72, 0.50f, 0.08f, 0.006f, 0.04f, 1.05f, 0.4f, 0.02f, false, false, true,  false },
        { "Old Man",     17.6f, 50, 0.45f, 0.12f, 0.016f, 0.09f, 0.80f, 1.7f, 0.03f, false, false, false, true  },
        { "Old Woman",   14.8f, 64, 0.45f, 0.12f, 0.016f, 0.09f, 0.80f, 1.7f, 0.03f, false, true,  false, true  },
        { "Kitten",       5.5f, 81, 0.55f, 0.10f, 0.010f, 0.05f, 1.20f, 0.4f, 0.06f, true,  false, true,  false },
        { "Cat",          7.0f, 74, 0.60f, 0.08f, 0.010f, 0.05f, 1.10f, 0.5f, 0.06f, true,  true,  false, false },
        { "Tomcat",       9.0f, 64, 0.70f, 0.06f, 0.012f, 0.06f, 0.90f, 0.6f, 0.08f, true,  false, false, false },
    };
    return types[(size_t) t];
}

juce::StringArray castNames()
{
    return { "Mixed Choir", "Women", "Men", "Children", "Elders", "Kittens", "Tomcats", "Alley Cats", "Cats & Humans",
             "Opera Stars", "Low Voices", "High Voices", "Family", "Cat Family" };
}

SingerType castMember (int cast, int i)
{
    using T = SingerType;
    switch (cast)
    {
        case 1:  { static const T c[] = { T::Soprano, T::Alto };                  return c[i % 2]; }
        case 2:  { static const T c[] = { T::Tenor, T::Bass };                    return c[i % 2]; }
        case 3:  return T::Child;
        case 4:  { static const T c[] = { T::ElderWoman, T::ElderMan };           return c[i % 2]; }
        case 5:  return T::Kitten;
        case 6:  return T::Tomcat;
        case 7:  { static const T c[] = { T::Cat, T::Kitten, T::Tomcat };         return c[i % 3]; }
        case 8:  { static const T c[] = { T::Soprano, T::Cat, T::Tenor, T::Kitten, T::Alto, T::Tomcat, T::Bass, T::Cat }; return c[i % 8]; }
        case 9:  { static const T c[] = { T::Soprano, T::Tenor };                 return c[i % 2]; }
        case 10: { static const T c[] = { T::Bass, T::Alto, T::Tenor, T::Bass };  return c[i % 4]; }
        case 11: { static const T c[] = { T::Soprano, T::Child, T::Alto, T::Soprano }; return c[i % 4]; }
        case 12: { static const T c[] = { T::Bass, T::Alto, T::Child, T::ElderWoman, T::Tenor, T::Child }; return c[i % 6]; }
        case 13: { static const T c[] = { T::Tomcat, T::Cat, T::Kitten, T::Kitten };      return c[i % 4]; }
        default: { static const T c[] = { T::Soprano, T::Alto, T::Tenor, T::Bass }; return c[i % 4]; }
    }
}

const StyleInfo& styleInfo (int style)
{
    //                name        tense breath vibD  vibR  delay  scoop scoopT rasp  nasal larynx drift
    static const StyleInfo styles[] = {
        { "Classical", 0.66f, 0.04f, 48.0f, 5.6f, 0.30f,  0.0f, 0.06f, 0.00f, 0.00f, 0.60f, 0.5f },
        { "Gospel",    0.74f, 0.08f, 34.0f, 6.0f, 0.22f, -1.6f, 0.08f, 0.18f, 0.04f, 0.30f, 0.8f },
        { "Pop",       0.55f, 0.18f, 16.0f, 5.2f, 0.45f, -0.7f, 0.05f, 0.04f, 0.04f, 0.10f, 1.0f },
        { "Folk",      0.56f, 0.12f,  8.0f, 5.0f, 0.60f, -0.3f, 0.05f, 0.05f, 0.18f, 0.05f, 1.0f },
        { "Jazz",      0.50f, 0.22f, 24.0f, 5.0f, 0.55f, -1.0f, 0.10f, 0.05f, 0.06f, 0.15f, 1.0f },
        { "Breathy",   0.22f, 0.60f, 12.0f, 5.0f, 0.50f, -0.3f, 0.06f, 0.00f, 0.04f, 0.00f, 1.0f },
        { "Pansori",   0.86f, 0.14f, 70.0f, 4.2f, 0.18f,  1.3f, 0.13f, 0.60f, 0.28f, 0.20f, 0.9f },
        { "Chant",     0.60f, 0.06f,  0.0f, 5.0f, 1.00f,  0.0f, 0.06f, 0.00f, 0.25f, 0.25f, 0.6f },
        { "Opera",     0.78f, 0.03f, 85.0f, 5.8f, 0.15f,  0.4f, 0.07f, 0.00f, 0.00f, 0.80f, 0.4f },
        { "Bulgarian", 0.90f, 0.05f,  6.0f, 6.2f, 0.80f, -0.2f, 0.04f, 0.10f, 0.40f, 0.10f, 0.5f },
        { "Soul",      0.68f, 0.12f, 30.0f, 5.4f, 0.35f, -2.0f, 0.12f, 0.25f, 0.05f, 0.25f, 1.0f },
        { "Robot",     0.62f, 0.00f,  0.0f, 5.0f, 1.00f,  0.0f, 0.00f, 0.00f, 0.10f, 0.00f, 0.0f },
        { "Lullaby",   0.30f, 0.35f, 10.0f, 4.6f, 0.60f, -0.4f, 0.10f, 0.00f, 0.02f, 0.05f, 0.8f },
    };
    return styles[(size_t) juce::jlimit (0, (int) (sizeof (styles) / sizeof (styles[0])) - 1, style)];
}

juce::StringArray styleNames()  { return { "Classical", "Gospel", "Pop", "Folk", "Jazz", "Breathy", "Pansori", "Chant", "Opera", "Bulgarian", "Soul", "Robot", "Lullaby" }; }
juce::StringArray modeNames()   { return { "Solo", "Choir", "Tutti" }; }
juce::StringArray moveNames()   { return { "Fixed", "Wave", "Envelope", "Steps", "Path", "Random" }; }
juce::StringArray onsetNames()  { return { "Auto", "Vowel", "Hum (m)", "N", "Meow" }; }
juce::StringArray roomNames()   { return { "Studio", "Room", "Hall", "Cathedral", "Plate", "Cave", "Forest" }; }

//==============================================================================
Engine::Engine()
{
    held.reserve (256);
    scratch.reserve (maxSingers);
    lyrics.reserve (600);
    lyricsAudio.reserve (600);
    path.reserve (maxPathPoints);
    pathAudio.reserve (maxPathPoints);
    for (auto& v : stageX) v = 0.0f;
    for (auto& v : stageY) v = 0.3f;
    arrangeStage (maxSingers, 0);
    makeTraits();
}

void Engine::prepare (double sr, int maxBlock)
{
    limiter.prepare (sr);
    sampleRate = sr;
    stepsPerSample = settings.highQuality && sr < 70000.0 ? 2 : 1;
    stepRate = (float) (sr * stepsPerSample);

    reverb.setSampleRate (sr);
    reverb.reset();
    lastRoom = -1;
    preDelay.assign ((size_t) (sr * 0.06) + 2, 0.0f);
    preDelayPos = 0;
    const int chunk = juce::jmax (maxBlock, 512);
    sendBuffer.assign ((size_t) chunk, 0.0f);
    wetL.assign ((size_t) chunk, 0.0f);
    wetR.assign ((size_t) chunk, 0.0f);

    for (int i = 0; i < maxSingers; ++i)
    {
        auto& s = singers[(size_t) i];
        s.active = s.gate = false;
        s.env = 0.0f;
        prepareTract (s);
        views[(size_t) i].singing = false;
        views[(size_t) i].level = 0.0f;
    }
    held.clear();
    globalTime = 0.0;
}

void Engine::setSettings (const Settings& newSettings)
{
    const auto old = settings;
    settings = newSettings;
    settings.singers = juce::jlimit (1, maxSingers, settings.singers);

    if (settings.seed != traitsSeed || settings.cast != traitsCast)
        makeTraits();

    const int newSteps = settings.highQuality && sampleRate < 70000.0 ? 2 : 1;
    if (newSteps != stepsPerSample)
    {
        stepsPerSample = newSteps;
        stepRate = (float) (sampleRate * stepsPerSample);
        for (auto& s : singers)
            prepareTract (s);   // tract length is counted in steps
    }

    // Singers beyond the count leave the stage
    for (int i = settings.singers; i < maxSingers; ++i)
        if (singers[(size_t) i].gate)
            releaseSinger (singers[(size_t) i]);

    if (old.mode != settings.mode)
    {
        for (auto& s : singers)
            if (s.gate) releaseSinger (s);
        if (settings.mode == Mode::Tutti)
            retuneTutti();
        else if (settings.mode == Mode::Solo && ! held.empty())
        {
            const auto& top = *std::max_element (held.begin(), held.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });
            startSinger (singers[0], 0, top.channel, top.note, 0, top.velocity, false, 0.0f);
        }
    }
    else if (settings.mode == Mode::Tutti && old.singers != settings.singers)
        retuneTutti();
}

void Engine::makeTraits()
{
    traitsSeed = settings.seed;
    traitsCast = settings.cast;
    for (int i = 0; i < maxSingers; ++i)
    {
        auto& s = singers[(size_t) i];
        s.type = castMember (settings.cast, i);
        Noise r;
        r.seed (hash (settings.seed * 7919u + (uint32_t) i * 104729u + 17u));
        s.tractScale = 1.0f + 0.045f * r.next();
        s.detune = 7.0f * r.next();
        s.vibRateMul = 1.0f + 0.1f * r.next();
        s.vibDepthMul = 1.0f + 0.25f * r.next();
        s.onsetDelay = r.uni();
        s.phase = r.uni();
        s.purrRate = 25.0f + 3.0f * r.next();
        s.wobble = r.uni();
        s.rnd.seed (hash ((uint32_t) i * 2654435761u + settings.seed));
        if (! s.active)
            prepareTract (s);
    }
}

void Engine::prepareTract (Singer& s)
{
    const float cm = typeInfo (s.type).tractCm * s.tractScale * (1.0f + 0.15f * settings.neck);
    s.tract.prepare ((int) std::lround (cm * stepRate / speedOfSound));
    s.glottis.reset (s.rnd.s);
    s.lpState = s.dcX = s.dcY = 0.0f;
}

int Engine::registerOf (int index) const
{
    return typeInfo (singers[(size_t) index].type).centreNote;
}

int Engine::numActiveSingers() const noexcept
{
    int n = 0;
    for (const auto& s : singers) n += s.active ? 1 : 0;
    return n;
}

//==============================================================================
void Engine::startSinger (Singer& s, int index, int channel, int note, int shift, float velocity, bool legato, float glideMs)
{
    juce::ignoreUnused (index);
    const float target = (float) (note + shift);
    s.channel = channel;
    s.note = note;
    s.shift = shift;
    s.velocity = velocity;
    s.age = ++counter;

    if (legato && s.gate && ! s.inCoda)
    {
        // Legato: glide from where the voice is, no new attack (a new syllable is articulated on the way)
        s.glideFrom = s.semis;
        s.glideTime = juce::jmax (0.001f, glideMs * 0.001f);
        s.glideT = glideMs <= 0.0f ? 1.0f : 0.0f;
        if (glideMs <= 0.0f) s.semis = target;
        if (settings.words && newSyllable)
            beginSyllable (s, true);
        return;
    }
    s.inCoda = false;

    if (! s.active)
    {
        prepareTract (s);
        s.env = 0.0f;
        s.vibPhase = s.phase * twoPi;
    }
    s.active = s.gate = true;
    s.semis = s.glideFrom = target;
    s.glideT = 1.0f;
    s.noteTime = 0.0;
    s.idleTime = 0.0;
    s.inhale = 0.0f;
    s.delaySamples = (int) (settings.human * 0.045f * s.onsetDelay * (float) sampleRate);
    s.stepIndex = -1;
    if (settings.words && (newSyllable || ! s.hasSyl))
        beginSyllable (s, false);
    else if (! settings.words)
        s.hasSyl = false;

    // Random vowel per note: near one of the anchors
    const auto& a = vowelAnchors()[(size_t) (hash (s.rnd.s + (uint32_t) counter) % 8u)];
    s.randX = juce::jlimit (0.0f, 1.0f, a.x + 0.06f * s.rnd.next());
    s.randY = juce::jlimit (0.0f, 1.0f, a.y + 0.06f * s.rnd.next());
}

void Engine::releaseSinger (Singer& s)
{
    if (! s.gate || s.inCoda) return;

    // Words: sing the end of the syllable first (diphthong glide + final consonants), then release
    if (settings.words && s.hasSyl && (s.syl.numCoda > 0 || s.syl.diphthong))
    {
        s.seqLen = 0;
        if (s.syl.diphthong)
        {
            Phone g;
            g.kind = PhoneKind::Glide;
            g.x = s.syl.v2x; g.y = s.syl.v2y;
            g.area = 1.0f;                 // glide towards the target and stay there
            g.dur = 0.12f;
            s.seq[(size_t) s.seqLen++] = g;
        }
        for (int i = 0; i < s.syl.numCoda && s.seqLen < (int) s.seq.size(); ++i)
            s.seq[(size_t) s.seqLen++] = s.syl.coda[(size_t) i];
        s.seqT = 0.0f;
        s.inCoda = true;
        return;
    }

    s.gate = false;
    s.inhale = 0.0f;   // no breathing noises after the phrase
}

int Engine::pickSinger (int note, std::vector<int>& taken)
{
    int best = -1;
    double bestScore = 1.0e18;
    for (int i = 0; i < settings.singers; ++i)
    {
        if (std::find (taken.begin(), taken.end(), i) != taken.end())
            continue;
        const auto& s = singers[(size_t) i];
        double score = std::abs (note - registerOf (i));
        if (s.gate && ! s.inCoda)
            score += 1.0e6 + (double) s.age;          // steal the oldest only when nobody is free
        else if (s.active)
            score += 24.0;                             // prefer singers who are fully quiet
        if (score < bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

void Engine::retuneTutti()
{
    const int n = settings.singers;
    if (held.empty())
    {
        for (int i = 0; i < maxSingers; ++i)
            releaseSinger (singers[(size_t) i]);
        return;
    }

    // Held notes, low to high (divisi: low singers take the low notes)
    std::array<int, 128> idx {};
    int m = 0;
    for (int h = 0; h < (int) held.size() && m < 128; ++h)
        idx[(size_t) m++] = h;
    std::sort (idx.begin(), idx.begin() + m, [this] (int a, int b) { return held[(size_t) a].note < held[(size_t) b].note; });

    std::array<int, maxSingers> order {};
    for (int i = 0; i < n; ++i) order[(size_t) i] = i;
    std::stable_sort (order.begin(), order.begin() + n, [this] (int a, int b) { return registerOf (a) < registerOf (b); });

    for (int j = 0; j < n; ++j)
    {
        const int i = order[(size_t) j];
        auto& s = singers[(size_t) i];
        const auto& h = held[(size_t) idx[(size_t) (j * m / n)]];
        // Each singer sings the note in their own octave
        int shift = 12 * juce::jlimit (-3, 3, juce::roundToInt ((float) (registerOf (i) - h.note) / 12.0f));
        while (h.note + shift > 127) shift -= 12;
        while (h.note + shift < 0) shift += 12;

        if (s.gate && s.note == h.note && s.channel == h.channel && s.shift == shift)
            continue;
        startSinger (s, i, h.channel, h.note, shift, h.velocity, s.gate && ! s.inCoda, juce::jmax (60.0f, settings.glideMs));
    }
}

//==============================================================================
void Engine::noteOn (int channel, int note, float velocity)
{
    held.erase (std::remove_if (held.begin(), held.end(), [=] (const Held& h) { return h.channel == channel && h.note == note; }), held.end());
    if (held.size() < 256)
        held.push_back ({ channel, note, velocity, false, ++counter });

    // A new note starts with neutral per-note expression
    if (settings.mpe && channel != 1)
        channels[(size_t) channel] = {};

    // Words: each new note (a chord counts as one) moves on to the next syllable
    pullLyrics();
    newSyllable = false;
    if (settings.words && ! lyricsAudio.empty())
    {
        if (lyricPos < 0 || globalTime - lastLyricStep > 0.03)
        {
            lyricPos = (lyricPos + 1) % (int) lyricsAudio.size();
            lastLyricStep = globalTime;
            const auto& sy = lyricsAudio[(size_t) lyricPos];
            currentHold = sy.hold;
            if (! sy.hold)
                currentSyl = sy;
            else
                for (int k = 1; k < (int) lyricsAudio.size(); ++k)
                {
                    const auto& prev = lyricsAudio[(size_t) ((lyricPos - k + (int) lyricsAudio.size()) % (int) lyricsAudio.size())];
                    if (! prev.hold) { currentSyl = prev; break; }
                }
            uiSyllable = lyricPos;
        }
        newSyllable = true;
    }

    switch (settings.mode)
    {
        case Mode::Solo:
        {
            auto& s = singers[0];
            startSinger (s, 0, channel, note, 0, velocity, s.gate, settings.glideMs);
            break;
        }
        case Mode::Choir:
        {
            std::vector<int>& taken = scratch;
            taken.clear();
            for (int u = 0; u < settings.unison; ++u)
            {
                const int i = pickSinger (note, taken);
                if (i < 0) break;
                taken.push_back (i);
                auto& s = singers[(size_t) i];
                if (s.gate) { s.gate = false; s.inCoda = false; }    // stolen
                startSinger (s, i, channel, note, 0, velocity, false, 0.0f);
            }
            break;
        }
        case Mode::Tutti:
            retuneTutti();
            break;
    }
    newSyllable = false;
}

void Engine::noteOff (int channel, int note)
{
    for (auto& h : held)
        if (h.channel == channel && h.note == note && sustainDown)
        {
            h.sustained = true;
            return;
        }

    held.erase (std::remove_if (held.begin(), held.end(), [=] (const Held& h) { return h.channel == channel && h.note == note; }), held.end());
    syncWithHeld();
}

void Engine::syncWithHeld()
{
    switch (settings.mode)
    {
        case Mode::Solo:
        {
            auto& s = singers[0];
            if (held.empty())
                releaseSinger (s);
            else
            {
                const auto& top = *std::max_element (held.begin(), held.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });
                if (s.gate && (s.note != top.note || s.channel != top.channel))
                    startSinger (s, 0, top.channel, top.note, 0, top.velocity, true, settings.glideMs);
            }
            break;
        }
        case Mode::Choir:
            for (auto& s : singers)
            {
                if (! s.gate) continue;
                const bool stillHeld = std::any_of (held.begin(), held.end(), [&s] (const Held& h) { return h.channel == s.channel && h.note == s.note; });
                if (! stillHeld) releaseSinger (s);
            }
            break;
        case Mode::Tutti:
            // Letting go of a key only lets its singers go. Re-dividing the whole choir here made
            // everybody jump onto the last held note while a chord was being released (a loud stab).
            for (auto& s : singers)
            {
                if (! s.gate) continue;
                const bool stillHeld = std::any_of (held.begin(), held.end(), [&s] (const Held& h) { return h.channel == s.channel && h.note == s.note; });
                if (! stillHeld) releaseSinger (s);
            }
            break;
    }
}

void Engine::pitchBend (int channel, float value)
{
    channels[(size_t) juce::jlimit (1, 16, channel)].bend = value;
    global.bend = value;
}

void Engine::pressure (int channel, float value)
{
    channels[(size_t) juce::jlimit (1, 16, channel)].pressure = value;
    global.pressure = value;
}

void Engine::timbre (int channel, float value)
{
    channels[(size_t) juce::jlimit (1, 16, channel)].timbre = value;
    global.timbre = value;
}

void Engine::modWheel (float value) { modWheelValue = value; }

void Engine::sustain (bool down)
{
    sustainDown = down;
    if (! down)
    {
        held.erase (std::remove_if (held.begin(), held.end(), [] (const Held& h) { return h.sustained; }), held.end());
        syncWithHeld();
    }
}

void Engine::allNotesOff()
{
    held.clear();
    sustainDown = false;
    for (auto& s : singers)
        if (s.gate) releaseSinger (s);
}

//==============================================================================
void Engine::setPath (const std::vector<juce::Point<float>>& points)
{
    const juce::SpinLock::ScopedLockType lock (pathLock);
    path.clear();
    for (size_t i = 0; i < points.size() && i < (size_t) maxPathPoints; ++i)
        path.push_back (points[i]);
}

std::vector<juce::Point<float>> Engine::getPath() const
{
    const juce::SpinLock::ScopedLockType lock (pathLock);
    return path;
}

juce::Point<float> Engine::pathPoint (float fraction) const
{
    const int n = (int) pathAudio.size();
    if (n == 0) return { settings.ax, settings.ay };
    if (n == 1) return pathAudio[0];

    // Ping-pong along the drawn line, so there is never a jump
    const float f = 1.0f - std::abs (2.0f * (fraction - std::floor (fraction)) - 1.0f);
    float total = 0.0f;
    for (int i = 1; i < n; ++i) total += pathAudio[(size_t) i].getDistanceFrom (pathAudio[(size_t) i - 1]);
    if (total <= 1.0e-6f) return pathAudio[0];

    float want = f * total;
    for (int i = 1; i < n; ++i)
    {
        const float seg = pathAudio[(size_t) i].getDistanceFrom (pathAudio[(size_t) i - 1]);
        if (want <= seg && seg > 0.0f)
            return lerp (pathAudio[(size_t) i - 1], pathAudio[(size_t) i], want / seg);
        want -= seg;
    }
    return pathAudio.back();
}

juce::Point<float> Engine::movementPoint (const Singer& s, int index) const
{
    juce::ignoreUnused (index);
    const juce::Point<float> a (settings.ax, settings.ay), b (settings.bx, settings.by);
    const float rate = settings.moveRate;
    const float offset = settings.spread * s.phase;

    switch (settings.move)
    {
        case Move::Wave:
            return lerp (a, b, 0.5f - 0.5f * std::cos (twoPi * (float) (rate * globalTime + offset)));
        case Move::Envelope:
            return lerp (a, b, smoothstep ((float) s.noteTime * rate * (1.0f + settings.spread * (s.phase - 0.5f))));
        case Move::Steps:
        {
            const auto k = (uint32_t) (int64_t) std::floor (rate * globalTime + offset * 0.5f);
            const auto& an = vowelAnchors()[(size_t) (hash (k * 31u + settings.seed) % 8u)];
            return { an.x, an.y };
        }
        case Move::Path:
            return pathPoint ((float) (rate * 0.5 * globalTime) + offset);
        case Move::Random:
            return { s.randX, s.randY };
        case Move::Fixed:
        default:
            return a;
    }
}

//==============================================================================
void Engine::setLyrics (const juce::String& text)
{
    auto parsed = parseLyrics (text);
    const juce::SpinLock::ScopedLockType lock (lyricsLock);
    lyrics = std::move (parsed);
    if (lyrics.size() > 512) lyrics.resize (512);
    lyricsChanged = true;
}

std::vector<Syllable> Engine::getSyllables() const
{
    const juce::SpinLock::ScopedLockType lock (lyricsLock);
    return lyrics;
}

void Engine::pullLyrics()
{
    if (lyricsLock.tryEnter())
    {
        if (lyricsChanged)
        {
            lyricsAudio.clear();
            for (const auto& sy : lyrics) lyricsAudio.push_back (sy);
            lyricsChanged = false;
            if (lyricPos >= (int) lyricsAudio.size()) lyricPos = -1;
        }
        lyricsLock.exit();
    }
    if (restartPending.exchange (false))
    {
        lyricPos = -1;
        uiSyllable = -1;
    }
}

void Engine::beginSyllable (Singer& s, bool legato)
{
    if (! legato || ! s.hasSyl)
    {
        s.seqLen = 0;
        if (! currentHold)
            for (int i = 0; i < currentSyl.numOnset; ++i)
                s.seq[(size_t) s.seqLen++] = currentSyl.onset[(size_t) i];
        s.syl = currentSyl;
        s.hasSyl = true;
        s.seqT = 0.0f;
        return;
    }
    if (currentHold)
        return;     // melisma: same vowel over the new note

    // Legato: end of the old syllable flows into the start of the new one ("hel-lo")
    s.seqLen = 0;
    if (s.syl.diphthong)
    {
        Phone g;
        g.kind = PhoneKind::Glide;
        g.x = s.syl.v2x; g.y = s.syl.v2y;
        g.area = 1.0f;
        g.dur = 0.07f;
        s.seq[(size_t) s.seqLen++] = g;
    }
    for (int i = 0; i < s.syl.numCoda && s.seqLen < (int) s.seq.size(); ++i)
        s.seq[(size_t) s.seqLen++] = s.syl.coda[(size_t) i];
    for (int i = 0; i < currentSyl.numOnset && s.seqLen < (int) s.seq.size(); ++i)
        s.seq[(size_t) s.seqLen++] = currentSyl.onset[(size_t) i];
    s.syl = currentSyl;
    s.seqT = 0.0f;
}

Engine::Artic Engine::articulate (Singer& s, juce::Point<float> vowel)
{
    Artic a;
    a.p = vowel;
    const float scale = dictionTime();

    // Which phone of the sequence is sounding?
    float t = s.seqT;
    int k = 0;
    float u = 0.0f;
    for (; k < s.seqLen; ++k)
    {
        const float d = s.seq[(size_t) k].dur * scale;
        if (t < d) { u = t / d; break; }
        t -= d;
    }

    if (k >= s.seqLen)
    {
        if (s.inCoda)
        {
            // end of the syllable sung: now let the note go
            s.inCoda = false;
            s.gate = false;
            s.inhale = 0.0f;
            // keep the final shape (closed lips for "m", target vowel of a diphthong)
            if (s.seqLen > 0)
            {
                const auto& last = s.seq[(size_t) s.seqLen - 1];
                if (last.kind == PhoneKind::Glide) a.p = { last.x, last.y };
                if (last.kind == PhoneKind::Nasal) { a.place = last.place; a.area = 0.0003f; a.velum = 1.3f; a.mouthClose = last.place == Lips ? 1.0f : 0.3f; }
            }
            return a;
        }
        if (s.syl.hum)
        {
            const auto& h = s.syl.humPhone;
            a.place = h.place; a.area = 0.0003f; a.velum = 1.3f; a.voice = 0.9f;
            a.mouthClose = h.place == Lips ? 1.0f : 0.3f;
        }
        else if (s.syl.diphthong && ! s.gate)
            a.p = { s.syl.v2x, s.syl.v2y };
        return a;
    }

    const auto& ph = s.seq[(size_t) k];
    switch (ph.kind)
    {
        case PhoneKind::Plosive:
            a.place = ph.place;
            if (u < 0.72f)
            {
                a.area = 0.0003f;                         // closure
                a.voice = ph.voiced ? 0.25f : 0.0f;        // voice bar for b / d / g
                a.mouthClose = ph.place == Lips ? 1.0f : 0.2f;
            }
            else
            {
                const float q = (u - 0.72f) / 0.28f;        // release burst
                a.area = 0.05f + 0.5f * q;
                a.fric = 1.6f * (1.0f - q);
                a.fricPlace = ph.place;
                a.colour = ph.colour;
                a.voice = ph.voiced ? 1.0f : 0.0f;
                a.asp = ph.voiced ? 0.0f : 0.8f * (1.0f - 0.5f * q);
            }
            break;
        case PhoneKind::Fricative:
            a.place = ph.place;
            a.area = ph.area;
            a.fric = std::pow (std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, u)), 0.3f);
            a.fricPlace = ph.place;
            a.colour = ph.colour;
            a.voice = ph.voiced ? 0.55f : 0.0f;
            a.asp = ph.voiced ? 0.0f : 0.2f;
            a.mouthClose = ph.place == Lips ? 0.8f : 0.2f;
            break;
        case PhoneKind::Nasal:
            a.place = ph.place;
            a.area = 0.0003f;
            a.velum = 1.3f;
            a.voice = 0.9f;
            a.mouthClose = ph.place == Lips ? 1.0f : 0.25f;
            break;
        case PhoneKind::Liquid:
            a.place = ph.place;
            a.area = ph.area;
            a.voice = 0.95f;
            if (ph.round) { a.p = lerp (vowel, { ph.x, ph.y }, 0.7f); a.round = true; }
            break;
        case PhoneKind::Glide:
            if (ph.area > 0.5f)   // towards a target (end of a diphthong)
                a.p = lerp (vowel, { ph.x, ph.y }, smoothstep (u));
            else                   // from the glide into the vowel (w, y)
                a.p = lerp ({ ph.x, ph.y }, vowel, smoothstep (u));
            a.round = ph.round && u < 0.6f;
            break;
        case PhoneKind::Aspirate:
            a.voice = 0.0f;
            a.asp = 1.0f;
            break;
        case PhoneKind::Vowel:
            a.p = { ph.x, ph.y };
            break;
    }
    return a;
}

void Engine::controlUpdate (Singer& s, int index, float dt)
{
    const auto& ti = typeInfo (s.type);
    const auto& st = styleInfo (settings.style);
    const auto& ch = settings.mpe ? channels[(size_t) juce::jlimit (1, 16, s.channel)] : global;
    const float t = (float) s.noteTime;

    // ---- pitch
    const float target = (float) (s.note + s.shift);
    if (s.glideT < 1.0f)
    {
        s.glideT = juce::jmin (1.0f, s.glideT + dt / s.glideTime);
        s.semis = s.glideFrom + (target - s.glideFrom) * smoothstep (s.glideT);
    }
    else
        s.semis = target;

    float bend = ch.bend * settings.bendRange;
    if (settings.mpe && s.channel != 1)
        bend += channels[1].bend * 2.0f;           // MPE master channel

    const float vibDepth = st.vibDepth * settings.vibrato * ti.vibDepth * s.vibDepthMul + modWheelValue * 60.0f;
    const float vibFade = juce::jlimit (0.0f, 1.0f, (t - st.vibDelay) / 0.5f);
    s.vibPhase += twoPi * st.vibRate * ti.vibRate * s.vibRateMul * dt;
    if (s.vibPhase > twoPi) s.vibPhase -= twoPi;
    float cents = s.detune + vibDepth * vibFade * std::sin (s.vibPhase);
    if (ti.elder)   // a slow, uneven wobble on top
        cents += 6.0f * settings.vibrato * std::sin (s.vibPhase * 0.31f + s.wobble * twoPi);

    s.driftTimer -= dt;
    if (s.driftTimer <= 0.0f)
    {
        s.driftTarget = s.rnd.next() * settings.drift * st.drift * 25.0f;
        s.driftTimer = 0.4f + 1.2f * s.rnd.uni();
    }
    s.driftValue += (s.driftTarget - s.driftValue) * juce::jmin (1.0f, dt * 2.5f);
    cents += s.driftValue;

    float semis = s.semis + bend + cents * 0.01f;
    if (std::abs (st.scoop) > 1.0e-6f)
        semis += st.scoop * std::exp (-t / st.scoopTime);

    // ---- vowel
    auto p = movementPoint (s, index);
    p += { (s.phase - 0.5f) * 0.06f * settings.spread, (s.onsetDelay - 0.5f) * 0.06f * settings.spread };
    p = lerp (p, { settings.bx, settings.by }, ch.timbre);

    // Words: the syllable decides the vowel, consonants shape the tract on top
    const bool words = settings.words && s.hasSyl;
    Artic ar;
    if (words)
    {
        if (s.delaySamples <= 0)
            s.seqT += dt;
        juce::Point<float> v (s.syl.vx, s.syl.vy);
        v += { (s.phase - 0.5f) * 0.03f * settings.spread, (s.onsetDelay - 0.5f) * 0.03f * settings.spread };
        ar = articulate (s, v);
        p = ar.p;
    }

    float lipsClose = 0.0f, tipClose = 0.0f, velumExtra = 0.0f;
    const Onset onset = settings.onset == Onset::Auto ? (ti.isCat ? Onset::Meow : Onset::Vowel) : settings.onset;
    if (! words && (s.gate || s.env > 0.001f))
    {
        if (onset == Onset::Meow)
        {
            // m - i - a - ow, with the pitch rising then falling
            const float len = (ti.isCat ? 0.55f : 0.75f) * (s.type == SingerType::Kitten ? 0.8f : 1.0f);
            const float u = t / len;
            lipsClose = 1.0f - smoothstep ((u - 0.03f) / 0.12f);
            velumExtra = 0.9f * (1.0f - smoothstep (u / 0.2f));
            juce::Point<float> m;
            if (u < 0.12f)      m = { 0.06f, 0.06f };
            else if (u < 0.45f) m = lerp ({ 0.06f, 0.06f }, { 0.5f, 0.95f }, smoothstep ((u - 0.12f) / 0.33f));
            else                m = lerp ({ 0.5f, 0.95f }, { 0.93f, 0.25f }, smoothstep ((u - 0.45f) / 0.55f));
            const float back = smoothstep ((u - 1.0f) / 0.6f);
            p = lerp (m, p, back);
            float contour;
            if (u < 0.45f)      contour = 2.5f * std::sin (1.5708f * u / 0.45f);
            else if (u < 1.0f)  contour = 2.5f - 5.0f * (u - 0.45f) / 0.55f;
            else                contour = -2.5f * (1.0f - back);
            semis += contour;
        }
        else if (onset == Onset::Hum)
        {
            lipsClose = 1.0f - smoothstep ((t - 0.12f) / 0.1f);
            velumExtra = 1.0f * lipsClose;
        }
        else if (onset == Onset::N)
        {
            tipClose = 1.0f - smoothstep ((t - 0.09f) / 0.07f);
            velumExtra = 1.0f * tipClose;
        }
    }

    // Purring is done with the mouth closed, through the nose
    const float purr = settings.purr * (ti.isCat ? 1.0f : 0.6f);
    lipsClose = juce::jmax (lipsClose, purr * 0.85f);
    velumExtra += purr * 0.7f;

    // Taking a breath: open "ah" shape, noise only
    // (only once the note has faded - switching earlier made some singers yell at the release)
    if (! s.gate && s.inhale > 0.0f && s.env < 0.02f)
        p = { 0.3f, 0.75f };

    p.x = juce::jlimit (0.0f, 1.0f, p.x);
    p.y = juce::jlimit (0.0f, 1.0f, p.y);
    auto shape = shapeAt (p.x, p.y);
    auto& la = shape.logArea;
    la[0] += std::log (1.0f - 0.55f * st.larynx);
    la[5] += 0.45f * settings.jaw;
    la[6] += 0.45f * settings.jaw;
    la[7] += 0.35f * settings.jaw - 1.1f * settings.lips;
    const float closed = std::log (0.015f);
    la[7] += (closed - la[7]) * lipsClose;
    la[6] += (closed - la[6]) * tipClose;

    if (words)
    {
        if (ar.round) la[7] -= 0.9f;
        if (ar.place >= 0 && ar.place < numCtrl)
            la[(size_t) ar.place] = std::log (juce::jmax (0.0003f, juce::jmin (std::exp (la[(size_t) ar.place]), ar.area)));
        velumExtra += ar.velum;
        s.consVoiceTarget = ar.voice;
        s.aspExtra = ar.asp;
        s.fricTarget = ar.fric * (0.5f + settings.diction);
        s.fricColour = ar.colour;
        s.fricIndex = ar.fricPlace >= 0 ? s.tract.sectionAt (juce::jmin (1.0f, (float) ar.fricPlace / 7.0f + 0.06f)) : -1;
        lipsClose = juce::jmax (lipsClose, ar.mouthClose);
    }
    else
    {
        s.consVoiceTarget = 1.0f;
        s.aspExtra = 0.0f;
        s.fricTarget = 0.0f;
    }

    const float velum = juce::jmax (0.0f, ti.nasal + 0.6f * st.nasal + 0.5f * settings.nose) + velumExtra;
    s.tract.setTarget (la, velum);
    s.tract.smooth (1.0f - std::exp (-dt / (words ? 0.004f : 0.012f)));   // consonants need quick articulators

    s.mouthOpen = shape.mouthOpen * (1.0f - juce::jmax (lipsClose, 0.3f * tipClose));
    s.mouthWidth = juce::jlimit (0.1f, 1.0f, shape.mouthWidth - 0.3f * settings.lips + 0.1f * settings.jaw);

    // ---- source
    const float loud = std::pow (s.velocity, 1.2f) * (settings.mpe ? 0.45f + 0.75f * ch.pressure : 1.0f + 0.3f * ch.pressure);
    s.f0 = juce::jlimit (25.0f, 2500.0f, 440.0f * std::pow (2.0f, (semis - 69.0f) / 12.0f));
    s.voicing = loud;
    s.purrDepth = purr;
    s.aspiration = ti.breath + 0.6f * st.breath + 0.45f * settings.breath + 0.3f * purr;
    s.glottis.tenseness = juce::jlimit (0.02f, 1.0f, ti.tense + (st.tense - 0.6f) + 0.2f * settings.chest + 0.25f * (loud - 0.5f));
    s.glottis.jitter = ti.jitter * (1.0f + settings.rasp);
    s.glottis.shimmer = ti.shimmer;
    s.glottis.rasp = juce::jlimit (0.0f, 1.0f, 0.6f * st.rasp + settings.rasp);

    // ---- stage -> microphone pair: level, arrival time and air absorption per capsule
    {
        constexpr float stageW = 5.0f, stageD = 6.0f, c = 343.0f;   // metres
        const float sx = juce::jlimit (-1.0f, 1.0f, stageX[(size_t) index].load()) * stageW;
        const float sy = juce::jlimit (0.0f, 1.0f, stageY[(size_t) index].load()) * stageD;
        const float mx = juce::jlimit (-1.0f, 1.0f, micX.load()) * stageW;
        const float my = juce::jlimit (-0.5f, 1.0f, micY.load()) * stageD;
        const float spacing = 0.2f + 1.8f * settings.micWidth;        // 20 cm .. 2 m between the capsules

        auto capsule = [&] (float cx, float& gain, float& delaySamples, float& dist)
        {
            const float dx = sx - cx, dy = sy - my;
            dist = std::sqrt (dx * dx + dy * dy) + 0.3f;
            const float cosTheta = dy / dist;                       // cardioid pointing at the stage
            gain = juce::jmin (2.4f, 5.2f / dist) * (0.55f + 0.45f * cosTheta);
            delaySamples = dist / c * (float) sampleRate;
        };
        float gl, gr, tl, tr, dl, dr;
        capsule (mx - 0.5f * spacing, gl, tl, dl);
        capsule (mx + 0.5f * spacing, gr, tr, dr);
        const float first = juce::jmin (tl, tr);
        s.targetDelayL = juce::jmin (120.0f, tl - first);
        s.targetDelayR = juce::jmin (120.0f, tr - first);
        s.gainL = gl;
        s.gainR = gr;
        const float avg = 0.5f * (dl + dr);
        s.send = juce::jlimit (0.2f, 1.0f, 0.15f + avg / 9.0f);
        const float fc = juce::jlimit (3000.0f, 16000.0f, 17000.0f - 900.0f * avg * (0.4f + 0.6f * settings.micDist));
        s.lpCoef = 1.0f - std::exp (-twoPi * fc / (float) sampleRate);
        s.facing = juce::jlimit (-1.0f, 1.0f, (mx - sx) / juce::jmax (1.0f, std::abs (my - sy) + 1.0f));
    }
}

//==============================================================================
void Engine::render (float* left, float* right, int numSamples)
{
    if (pathLock.tryEnter())
    {
        pathAudio.clear();
        for (const auto& p : path) pathAudio.push_back (p);
        pathLock.exit();
    }

    if (settings.room != lastRoom)
    {
        lastRoom = settings.room;
        //                            Studio Room   Hall   Cathedral Plate Cave   Forest
        static const float sizes[] = { 0.30f, 0.52f, 0.78f, 0.95f,  0.66f, 0.90f, 0.60f };
        static const float damps[] = { 0.60f, 0.45f, 0.30f, 0.12f,  0.05f, 0.75f, 0.85f };
        static const float preMs[] = { 4.0f, 10.0f, 22.0f, 38.0f,   2.0f, 30.0f, 35.0f };
        const int ri = juce::jlimit (0, 6, lastRoom);
        juce::Reverb::Parameters rp;
        rp.roomSize = sizes[ri];
        rp.damping = damps[ri];
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);
        preDelayLength = juce::jlimit (1, (int) preDelay.size() - 1, (int) (preMs[ri] * 0.001 * sampleRate));
    }

    pullLyrics();

    // With words the consonants need a quick start: attack is capped at 20 ms
    const float attackMs = settings.words ? juce::jmin (settings.attackMs, 20.0f) : settings.attackMs;
    const float attInc = 1.0f / juce::jmax (1.0f, attackMs * 0.001f * (float) sampleRate);
    const float relCoef = std::exp (-6.9f / juce::jmax (1.0f, settings.releaseMs * 0.001f * (float) sampleRate));
    const float dryGain = 1.0f - 0.45f * settings.micDist;
    const float wetGain = settings.roomMix * (0.35f + 0.9f * settings.micDist);

    int done = 0;
    while (done < numSamples)
    {
        const int n = juce::jmin (controlPeriod, numSamples - done, (int) sendBuffer.size());
        float* outL = left + done;
        float* outR = right + done;
        std::fill (sendBuffer.begin(), sendBuffer.begin() + n, 0.0f);
        const float dt = (float) n / (float) sampleRate;

        // Level normalisation: incoherent voices add up as sqrt(N). Count every singer that is still
        // sounding (releases included), so letting go of a chord never turns the fading voices up.
        int sounding = 0;
        for (const auto& s : singers) sounding += s.active ? 1 : 0;
        const float targetNorm = 1.4f / std::pow ((float) juce::jmax (1, sounding), 0.65f);
        norm = targetNorm < norm ? targetNorm : norm + (targetNorm - norm) * juce::jmin (1.0f, dt * 4.0f);

        for (int i = 0; i < maxSingers; ++i)
        {
            auto& s = singers[(size_t) i];
            if (! s.active) continue;

            controlUpdate (s, i, dt);

            float peak = 0.0f;
            for (int j = 0; j < n; ++j)
            {
                if (s.delaySamples > 0) { --s.delaySamples; continue; }

                if (s.gate) s.env = juce::jmin (1.0f, s.env + attInc);
                else        s.env *= relCoef;

                float purrMod = 1.0f;
                if (s.purrDepth > 0.0f)
                {
                    s.purrPhase += s.purrRate / (float) sampleRate;
                    if (s.purrPhase >= 1.0f) s.purrPhase -= 1.0f;
                    const float pulse = std::sin (twoPi * s.purrPhase);
                    purrMod = 1.0f - s.purrDepth + s.purrDepth * 1.6f * pulse * pulse;
                }

                float inhaleNoise = 0.0f;
                if (! s.gate && s.inhale > 0.0f && s.env < 0.02f)
                {
                    s.inhaleDelay += 1.0f / (float) sampleRate;
                    if (s.inhaleDelay >= s.inhale) s.inhale = 0.0f;
                    else if (s.inhaleDelay > 0.0f)
                        inhaleNoise = 0.2f * settings.breath * std::sin (juce::MathConstants<float>::pi * s.inhaleDelay / s.inhale);
                }

                // Consonants: voicing switches off / on smoothly, turbulence noise at the constriction
                s.consVoice += (s.consVoiceTarget - s.consVoice) * 0.006f;
                s.fric += (s.fricTarget - s.fric) * 0.02f;
                float noise = 0.0f;
                if (s.fric > 1.0e-4f)
                {
                    const float white = s.rnd.next();
                    const float a = 0.85f - 0.6f * s.fricColour;            // brighter = higher cut-off
                    s.fHp = a * (s.fHp + white - s.fLast);
                    s.fLast = white;
                    noise = (s.fHp * (0.5f + s.fricColour) + white * (1.0f - s.fricColour) * 0.4f) * s.fric * s.voicing * s.env * 0.9f;
                }

                s.glottis.voicing = s.voicing * s.env * purrMod * s.consVoice;
                s.glottis.aspiration = s.aspiration * s.env * s.voicing * 0.5f * purrMod * (0.3f + 0.7f * s.consVoice) + inhaleNoise
                                     + s.aspExtra * s.voicing * s.env * 0.6f;

                float x = 0.0f;
                for (int k = 0; k < stepsPerSample; ++k)
                    x += s.tract.step (s.glottis.process (s.f0, stepRate), noise, s.fricIndex);
                if (stepsPerSample > 1) x *= 0.5f;

                // DC blocker, then distance (air absorption)
                const float y = x - s.dcX + 0.995f * s.dcY;
                s.dcX = x;
                s.dcY = y;
                s.lpState += s.lpCoef * (y - s.lpState);
                const float v = s.lpState * norm;
                // Spaced pair: each capsule hears the singer a little later / earlier
                s.micBuf[(size_t) s.micPos] = v;
                s.delayL += (s.targetDelayL - s.delayL) * 0.002f;
                s.delayR += (s.targetDelayR - s.delayR) * 0.002f;
                auto tap = [&s] (float d)
                {
                    const float rp = (float) s.micPos - d + 128.0f;
                    const int i0 = (int) rp;
                    const float fr = rp - (float) i0;
                    return s.micBuf[(size_t) (i0 & 127)] * (1.0f - fr) + s.micBuf[(size_t) ((i0 + 1) & 127)] * fr;
                };
                outL[j] += tap (s.delayL) * s.gainL * dryGain;
                outR[j] += tap (s.delayR) * s.gainR * dryGain;
                s.micPos = (s.micPos + 1) & 127;
                sendBuffer[(size_t) j] += v * s.send;
                peak = juce::jmax (peak, std::abs (v));
            }

            if (s.delaySamples <= 0)
                s.noteTime += dt;
            s.level = juce::jmax (peak, s.level * 0.9f);

            // Fully quiet (and done breathing): leave the stage until the next note
            if (! s.gate && s.env < 1.0e-4f && s.inhale <= 0.0f)
            {
                s.idleTime += dt;
                if (s.idleTime > 0.08)
                {
                    s.active = false;
                    s.env = 0.0f;
                    s.tract.clear();
                    s.level = 0.0f;
                }
            }

            auto& view = views[(size_t) i];
            view.level = juce::jlimit (0.0f, 1.0f, s.level * 4.0f);
            view.open = s.active ? s.mouthOpen * juce::jmin (1.0f, s.env * 1.3f + (s.inhale > 0.0f && s.env < 0.02f ? 0.4f : 0.0f)) : 0.0f;
            view.width = s.mouthWidth;
            view.facing = s.facing;
            view.singing = s.gate;
            view.note = s.active ? s.note + s.shift : -1;
            view.type = (int) s.type;
        }

        // Room: pre-delay -> reverb
        for (int j = 0; j < n; ++j)
        {
            preDelay[(size_t) preDelayPos] = sendBuffer[(size_t) j];
            int r = preDelayPos - preDelayLength;
            if (r < 0) r += (int) preDelay.size();
            wetL[(size_t) j] = wetR[(size_t) j] = preDelay[(size_t) r];
            preDelayPos = (preDelayPos + 1) % (int) preDelay.size();
        }
        reverb.processStereo (wetL.data(), wetR.data(), n);

        float peak = 0.0f;
        for (int j = 0; j < n; ++j)
        {
            float l = (outL[j] + wetL[(size_t) j] * wetGain) * settings.volume;
            float r = (outR[j] + wetR[(size_t) j] * wetGain) * settings.volume;
            // safety: smooth level riding + soft knee (never jumps, never clips)
            limiter.processSample (l, r);
            outL[j] = l;
            outR[j] = r;
            peak = juce::jmax (peak, std::abs (l), std::abs (r));
        }
        uiLevel = juce::jmax (peak, uiLevel.load() * 0.9f);

        globalTime += dt;
        done += n;
    }

    // Where the choir is singing (for the vowel chart cursor)
    for (int i = 0; i < settings.singers; ++i)
    {
        const auto& s = singers[(size_t) i];
        if (! s.active) continue;
        auto p = movementPoint (s, i);
        uiVowelX = p.x;
        uiVowelY = p.y;
        return;
    }
    const auto p = movementPoint (singers[0], 0);
    uiVowelX = p.x;
    uiVowelY = p.y;
}

//==============================================================================
void Engine::arrangeStage (int count, int cast)
{
    count = juce::jlimit (1, maxSingers, count);
    // Higher voices on the left, lower on the right, in curved rows like a real choir
    std::array<int, maxSingers> order {};
    for (int i = 0; i < count; ++i) order[(size_t) i] = i;
    std::stable_sort (order.begin(), order.begin() + count, [cast] (int a, int b)
    {
        return typeInfo (castMember (cast, a)).centreNote > typeInfo (castMember (cast, b)).centreNote;
    });

    const int rows = count <= 6 ? 1 : (count <= 16 ? 2 : (count <= 36 ? 3 : 4));
    const int cols = (count + rows - 1) / rows;
    for (int r = 0; r < count; ++r)
    {
        const int i = order[(size_t) r];
        const int col = r / rows, row = r % rows;
        const float x = cols <= 1 ? 0.0f : -0.85f + 1.7f * ((float) col + (row % 2 ? 0.5f : 0.0f)) / (float) (cols - 1 + (rows > 1 ? 0.5f : 0.0f));
        const float y = rows <= 1 ? 0.25f : 0.12f + 0.7f * (float) row / (float) (rows - 1);
        stageX[(size_t) i] = juce::jlimit (-1.0f, 1.0f, x);
        stageY[(size_t) i] = juce::jlimit (0.0f, 1.0f, y - 0.1f * x * x + 0.1f);
    }
}

void Engine::shuffleStage (uint32_t seed, int count)
{
    count = juce::jlimit (1, maxSingers, count);
    Noise r;
    r.seed (hash (seed + 99u));
    for (int i = count - 1; i > 0; --i)
    {
        const int j = (int) (hash (r.s + (uint32_t) i) % (uint32_t) (i + 1));
        r.next();
        const float x = stageX[(size_t) i], y = stageY[(size_t) i];
        stageX[(size_t) i] = stageX[(size_t) j].load();
        stageY[(size_t) i] = stageY[(size_t) j].load();
        stageX[(size_t) j] = x;
        stageY[(size_t) j] = y;
    }
    for (int i = 0; i < count; ++i)
    {
        stageX[(size_t) i] = juce::jlimit (-1.0f, 1.0f, stageX[(size_t) i] + 0.08f * r.next());
        stageY[(size_t) i] = juce::jlimit (0.0f, 1.0f, stageY[(size_t) i] + 0.06f * r.next());
    }
}

} // namespace singcat
