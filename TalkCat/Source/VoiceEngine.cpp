#include "VoiceEngine.h"

namespace talkcat
{
namespace
{
    constexpr float twoPi = 6.28318530718f;
    constexpr float pi = 3.14159265359f;
    constexpr int controlPeriod = 32;

    float smoothstep (float x) noexcept
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    uint32_t hash (uint32_t x) noexcept
    {
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    struct StyleInfo
    {
        const char* name;
        float tense, breath, vibScale, vibRate, scoop, scoopTime, overshoot, porta, rasp, ring, nasal, fall, drift;
        bool robot;
    };

    const StyleInfo& style (int i)
    {
        //        name           tense breath vibS  vibR  scoop scoopT over  porta rasp  ring  nasal fall  drift robot
        static const StyleInfo s[] = {
            { "Natural",      0.55f, 0.04f, 1.0f, 1.00f, -0.4f, 0.06f, 1.0f, 1.0f, 0.00f, 0.0f, 0.00f, 0.3f, 1.0f, false },
            { "Pop",          0.52f, 0.10f, 0.7f, 1.00f, -0.8f, 0.06f, 1.2f, 0.9f, 0.03f, 0.0f, 0.03f, 0.6f, 1.0f, false },
            { "Ballad",       0.48f, 0.14f, 1.0f, 0.95f, -0.6f, 0.09f, 0.8f, 1.4f, 0.00f, 0.0f, 0.02f, 0.4f, 0.9f, false },
            { "R&B",          0.55f, 0.10f, 0.9f, 1.05f, -1.4f, 0.09f, 1.6f, 0.8f, 0.05f, 0.1f, 0.03f, 1.0f, 1.0f, false },
            { "Opera",        0.68f, 0.02f, 1.9f, 1.05f,  0.0f, 0.05f, 0.5f, 1.2f, 0.00f, 0.8f, 0.00f, 0.0f, 0.5f, false },
            { "Anime Idol",   0.50f, 0.12f, 0.8f, 1.15f, -0.5f, 0.05f, 1.4f, 0.7f, 0.00f, 0.0f, 0.08f, 0.3f, 0.8f, false },
            { "Rock",         0.72f, 0.06f, 0.6f, 1.00f, -1.2f, 0.07f, 1.3f, 0.8f, 0.30f, 0.3f, 0.02f, 0.8f, 1.2f, false },
            { "Metal Fry",    0.80f, 0.05f, 0.4f, 1.10f, -1.0f, 0.05f, 1.0f, 0.6f, 0.75f, 0.4f, 0.00f, 0.5f, 1.2f, false },
            { "Jazz",         0.50f, 0.16f, 1.1f, 0.90f, -1.0f, 0.12f, 0.9f, 1.3f, 0.04f, 0.1f, 0.04f, 0.9f, 1.0f, false },
            { "Folk",         0.55f, 0.10f, 0.5f, 1.00f, -0.3f, 0.05f, 0.8f, 1.0f, 0.04f, 0.0f, 0.12f, 0.3f, 1.0f, false },
            { "Gospel",       0.72f, 0.07f, 1.2f, 1.05f, -1.6f, 0.08f, 1.8f, 0.9f, 0.15f, 0.3f, 0.03f, 1.2f, 1.0f, false },
            { "Whisper",      0.15f, 0.75f, 0.3f, 1.00f, -0.2f, 0.05f, 0.6f, 1.0f, 0.00f, 0.0f, 0.00f, 0.2f, 0.6f, false },
            { "Country",      0.60f, 0.08f, 0.8f, 1.10f, -1.0f, 0.07f, 1.3f, 0.9f, 0.06f, 0.1f, 0.15f, 0.9f, 1.0f, false },
            { "Robot",        0.60f, 0.00f, 0.0f, 1.00f,  0.0f, 0.05f, 0.0f, 0.0f, 0.00f, 0.0f, 0.02f, 0.0f, 0.0f, true  },
            { "Cat Diva",     0.58f, 0.08f, 1.3f, 1.20f, -2.0f, 0.10f, 2.0f, 1.2f, 0.10f, 0.2f, 0.06f, 1.4f, 1.6f, false },
        };
        return s[juce::jlimit (0, 14, i)];
    }

    // 7-note scales (semitones) for the harmony voices
    const std::array<std::array<int, 7>, 7>& scales()
    {
        static const std::array<std::array<int, 7>, 7> s { { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 },
                                                              { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 4, 6, 7, 9, 11 },
                                                              { 0, 1, 3, 5, 7, 8, 10 } } };
        return s;
    }
    const int harmonySteps[] = { 0, 2, -2, 3, -3, 4, -4, 5, -5, 7, -7, 0 };
}

juce::StringArray styleNames()
{
    juce::StringArray n;
    for (int i = 0; i < 15; ++i) n.add (style (i).name);
    return n;
}
juce::StringArray scaleNames()   { return { "Major", "Minor", "Dorian", "Mixolydian", "Harmonic Minor", "Lydian", "Phrygian", "Chromatic" }; }
juce::StringArray harmonyNames() { return { "Off", "3rd above", "3rd below", "4th above", "4th below", "5th above", "5th below",
                                            "6th above", "6th below", "Octave above", "Octave below", "Unison" }; }
juce::StringArray reverbNames()  { return { "Dry", "Room", "Studio Plate", "Hall", "Cathedral" }; }
juce::StringArray attackNames()  { return { "Soft", "Normal", "Hard" }; }

//==============================================================================
Engine::Engine()
{
    for (auto& a : uiArea) a = 1.0f;
    for (auto& p : uiPitch) p = -1.0f;
    for (auto& l : uiLevel) l = 0.0f;
    for (int g = 0; g < numGroups; ++g)
        for (int v = 0; v < voicesPerGroup; ++v)
        {
            auto& voice = groups[(size_t) g].voices[(size_t) v];
            voice.role = v;
            voice.rnd.seed (hash ((uint32_t) (g * 31 + v * 7 + 3)));
            voice.detuneCents = v == 0 ? 0.0f : 4.0f * voice.rnd.next();
            voice.timingJitter = voice.rnd.uni();
        }
}

void Engine::prepare (double sr, int maxBlock)
{
    limiter.prepare (sr);
    sampleRate = sr;
    stepRate = (float) (sr < 70000.0 && ! getenv ("ONEX") ? sr * 2.0 : sr);
    maxBlockSize = juce::jmax (512, maxBlock);
    for (int g = 0; g < numGroups; ++g)
        for (auto& v : groups[(size_t) g].voices)
        {
            v.active = v.gate = false;
            v.env = 0.0f;
            v.targetCm = tractCmFor (v, g);
            v.currentSections = (v.targetCm - lipComp()) * stepRate / speedOfSound;
            v.tract.prepare ((int) std::lround (v.currentSections));
            v.tract.loss = std::pow (0.9992f, (float) (sampleRate / stepRate));   // same damping per cm as at 1x
            v.tract.glottalReflection = getenv ("GREF") ? (float) atof (getenv ("GREF")) : 0.9f;
            v.tract.lipLow = getenv ("LLOW") ? (float) atof (getenv ("LLOW")) : -0.97f;
            v.tract.lipHigh = getenv ("LHIGH") ? (float) atof (getenv ("LHIGH")) : -0.6f;
            v.tract.lipCoef = 1.0f - std::exp (-6.2831853f * (getenv ("LFC") ? (float) atof (getenv ("LFC")) : 3200.0f) / stepRate);
            v.glottis.reset (v.rnd.s);
            v.pendingSamples = -1;
            v.releaseDelay = -1;
        }
    delayL.assign ((size_t) (sr * 4.0) + 1, 0.0f);
    delayR.assign ((size_t) (sr * 4.0) + 1, 0.0f);
    delayPos = 0;
    reverb.setSampleRate (sr);
    reverb.reset();
    lastReverb = -1;
    wetL.assign ((size_t) maxBlockSize, 0.0f);
    wetR.assign ((size_t) maxBlockSize, 0.0f);
    voiceBuf.assign ((size_t) maxBlockSize, 0.0f);
    lastLowCut = -1.0f; lastPresence = lastAir = -100.0f;
    compEnv = 0.0f; compGain = 1.0f;
}

void Engine::setSettings (const Settings& s)
{
    settings = s;
}

bool Engine::isSounding (int group) const noexcept
{
    for (const auto& v : groups[(size_t) group].voices)
        if (v.active) return true;
    return false;
}

float Engine::tractCmFor (const Voice& v, int group) const
{
    const auto& e = exprSmooth[(size_t) group];
    const float gender = juce::jlimit (-1.5f, 1.5f, settings.gender + e.gender);
    float human = settings.size * (1.0f - 0.085f * gender);
    if (settings.age < 0.5f) human *= 0.66f + 0.34f * (settings.age / 0.5f);
    else human *= 1.0f + 0.04f * (settings.age - 0.5f) * 2.0f;
    const float cat = juce::jlimit (0.0f, 1.0f, settings.cat + e.cat + (settings.style == 14 ? 0.25f : 0.0f));
    const float catCm = 6.5f * std::sqrt (settings.size / 16.5f);
    float cm = std::exp (std::log (human) * (1.0f - cat) + std::log (catCm) * cat);
    if (v.role == 1) cm *= 1.01f;
    if (v.role >= 2) cm *= 1.0f + (v.role == 3 ? 0.04f : (v.role == 4 ? -0.03f : 0.0f));
    return juce::jlimit (4.0f, 24.0f, cm);
}

float Engine::harmonyPitch (float lead, int h) const
{
    const int choice = settings.harmony[(size_t) h];
    if (choice <= 0) return lead;
    const int steps = harmonySteps[juce::jlimit (0, 11, choice)];
    if (choice == 11) return lead;
    if (settings.scale >= 7)
    {
        static const int chromatic[] = { 0, 2, 4, 5, 7, 9, 11, 12 };   // fixed intervals for "Chromatic"
        const int a = std::abs (steps);
        return lead + (float) (steps >= 0 ? chromatic[a] : -chromatic[a]);
    }
    const auto& sc = scales()[(size_t) juce::jlimit (0, 6, settings.scale)];
    const int note = (int) std::lround (lead);
    const float frac = lead - (float) note;
    const int rel = ((note - settings.key) % 12 + 12) % 12;
    const int oct = (note - settings.key - rel) / 12;
    // nearest scale degree
    int degree = 0, best = 99;
    for (int d = 0; d < 7; ++d)
        if (std::abs (sc[(size_t) d] - rel) < best) { best = std::abs (sc[(size_t) d] - rel); degree = d; }
    const int chroma = rel - sc[(size_t) degree];
    const int target = degree + steps;
    const int tOct = (int) std::floor ((float) target / 7.0f);
    const int tDeg = target - tOct * 7;
    return (float) (settings.key + (oct + tOct) * 12 + sc[(size_t) tDeg] + chroma) + frac;
}

float Engine::lipComp() const
{
    // the lip radiation filter delays the reflection a little: like a longer tube, so shorten it
    static const float c = getenv ("LCOMP") ? (float) atof (getenv ("LCOMP")) : 0.6f;
    return c;
}

//==============================================================================
void Engine::noteStart (int group, const NoteEvent& e)
{
    auto& g = groups[(size_t) group];
    for (int i = 0; i < voicesPerGroup; ++i)
    {
        auto& v = g.voices[(size_t) i];
        const bool used = i == 0 || (i == 1 && settings.doubleLevel > 0.001f) || (i >= 2 && settings.harmony[(size_t) i - 2] > 0);
        if (! used)
        {
            if (v.gate) releaseVoice (v, false);
            continue;
        }
        NoteEvent ev = e;
        if (i == 1) ev.pitch += settings.doubleDetune * 0.01f * (v.rnd.next() > 0.0f ? 1.0f : -1.0f) * 0.5f + settings.doubleDetune * 0.005f;
        if (i >= 2) ev.pitch = harmonyPitch (e.pitch, i - 2);
        if (i >= 2) ev.velocity *= 0.9f;
        const float delayMs = i == 0 ? 0.0f : (i == 1 ? settings.doubleDelayMs * (0.7f + 0.6f * v.timingJitter) : 4.0f + 10.0f * v.timingJitter);
        if (delayMs <= 0.5f)
            startVoice (v, ev, group);
        else
        {
            v.pending = ev;
            v.pendingSamples = (int) (delayMs * 0.001f * (float) sampleRate);
        }
    }
    g.lastLeadPitch = e.pitch;
    g.sounding = true;
    uiGroup = group;
}

void Engine::noteRelease (int group, bool phraseEnd)
{
    auto& g = groups[(size_t) group];
    for (int i = 0; i < voicesPerGroup; ++i)
    {
        auto& v = g.voices[(size_t) i];
        if (v.pendingSamples >= 0)
        {
            // not started yet: start then release right after
            v.pendingRelease = true;
            v.pendingPhraseEnd = phraseEnd;
            continue;
        }
        if (i == 0) releaseVoice (v, phraseEnd);
        else v.releaseDelay = (int) ((i == 1 ? settings.doubleDelayMs : 6.0f) * 0.001f * (float) sampleRate), v.pendingPhraseEnd = phraseEnd;
    }
}

void Engine::breathIn (int group, float seconds)
{
    auto& v = groups[(size_t) group].voices[0];
    if (v.gate) return;
    if (! v.active)
    {
        v.targetCm = tractCmFor (v, group);
        v.currentSections = (v.targetCm - lipComp()) * stepRate / speedOfSound;
        v.tract.prepare ((int) std::lround (v.currentSections));
        v.glottis.reset (v.rnd.s);
        v.env = 0.0f;
        v.hasSyl = false;
        v.seqLen = 0;
    }
    v.active = true;
    v.breathT = 0.0f;
    v.breathLen = juce::jmax (0.08f, seconds);
    v.idle = 0.0f;
}

void Engine::allOff (int group)
{
    for (auto& v : groups[(size_t) group].voices)
    {
        v.pendingSamples = -1;
        v.releaseDelay = -1;
        v.pendingRelease = false;
        if (v.gate) { v.gate = false; v.inCoda = false; v.releasing = true; v.relTime = 0.0f; }
    }
    groups[(size_t) group].sounding = false;
}

void Engine::startVoice (Voice& v, const NoteEvent& e, int group)
{
    const auto& st = style (settings.style);
    const bool legato = e.legato && v.gate && v.active;
    if (! v.active)
    {
        v.targetCm = tractCmFor (v, group);
        v.currentSections = (v.targetCm - lipComp()) * stepRate / speedOfSound;
        v.tract.prepare ((int) std::lround (v.currentSections));
        v.glottis.reset (v.rnd.s);
        v.env = 0.0f;
        v.vibPhase = v.rnd.uni() * twoPi;
        v.dcX = v.dcY = v.tiltLp = 0.0f;
        v.articInit = false;
    }
    v.active = true;
    v.idle = 0.0f;
    v.releasing = false;
    v.relTime = 0.0f;
    v.breathT = -1.0f;
    const float target = e.pitch + v.detuneCents * 0.01f;
    const float porta = (e.portaMs >= 0.0f ? e.portaMs : settings.portaMs) * st.porta;

    if (legato)
    {
        v.from = v.semis;
        v.interval = target - v.from;
        v.glideTime = juce::jmax (0.001f, porta * 0.001f * (0.6f + 0.4f * juce::jmin (1.0f, std::abs (v.interval) / 7.0f)));
        v.glideDelay = juce::jmax (0.0f, e.pitchDelay - 0.5f * v.glideTime);
        v.glideT = (st.robot || porta <= 0.5f) ? 1.0f : 0.0f;
        if (v.glideT >= 1.0f && v.glideDelay <= 0.0f) v.semis = target;
        v.sinceArrive = 10.0f;
        beginSequence (v, e, true);
    }
    else
    {
        v.semis = v.from = target;
        v.interval = 0.0f;
        v.glideT = 1.0f;
        v.glideDelay = 0.0f;
        v.sinceArrive = 10.0f;
        v.scoopAmt = st.scoop * settings.scoop * 2.0f;
        v.noteTime = 0.0f;
        beginSequence (v, e, false);
    }
    v.ev = e;
    v.ev.pitch = target;
    v.gate = true;
    v.inCoda = false;
    v.phraseEnd = false;
}

void Engine::releaseVoice (Voice& v, bool phraseEnd)
{
    if (! v.gate || v.inCoda) return;
    v.phraseEnd = phraseEnd;
    const float scale = settings.consLength * v.ev.consScale;
    if (v.hasSyl && ! v.syl.hold && ! v.syl.breath && (v.syl.numCoda > 0 || v.syl.diphthong))
    {
        v.seqLen = 0;
        if (v.syl.diphthong)
        {
            Phone g;
            g.kind = PK::Glide;
            g.x = v.syl.nucleus2.x; g.y = v.syl.nucleus2.y; g.lips = v.syl.nucleus2.lips;
            g.area = 1.0f;                       // glide towards the target and stay
            g.dur = 0.11f;
            v.seq[(size_t) v.seqLen++] = g;
        }
        for (int i = 0; i < v.syl.numCoda && v.seqLen < (int) v.seq.size(); ++i)
            v.seq[(size_t) v.seqLen++] = v.syl.coda[(size_t) i];
        juce::ignoreUnused (scale);
        v.seqT = 0.0f;
        v.inCoda = true;
        return;
    }
    v.gate = false;
    v.releasing = true;
    v.relTime = 0.0f;
}

void Engine::beginSequence (Voice& v, const NoteEvent& e, bool legato)
{
    const auto& syl = e.syl;
    v.seqLen = 0;
    v.seqT = 0.0f;
    auto push = [&v] (const Phone& p) { if (v.seqLen < (int) v.seq.size()) v.seq[(size_t) v.seqLen++] = p; };
    auto pushOnset = [&]
    {
        for (int i = 0; i < syl.numOnset; ++i)
        {
            push (syl.onset[(size_t) i]);
            if (syl.onset[(size_t) i].aspirated)
            {
                Phone a; a.kind = PK::Aspirate; a.voiced = false; a.dur = 0.035f;
                push (a);
            }
        }
    };

    if (syl.hold)
    {
        // melisma: keep the old syllable's vowel (no new consonants)
        if (! v.hasSyl) { v.syl = syl; v.syl.hold = false; v.syl.nucleus.x = 0.5f; v.syl.nucleus.y = 0.9f; v.hasSyl = true; }
        return;
    }
    if (legato && v.hasSyl && ! v.syl.hold && ! v.syl.breath)
    {
        if (v.syl.diphthong)
        {
            Phone g;
            g.kind = PK::Glide;
            g.x = v.syl.nucleus2.x; g.y = v.syl.nucleus2.y; g.lips = v.syl.nucleus2.lips;
            g.area = 1.0f;
            g.dur = 0.11f;
            push (g);
        }
        for (int i = 0; i < v.syl.numCoda; ++i) push (v.syl.coda[(size_t) i]);
    }
    if (! e.skipOnset) pushOnset();
    v.syl = syl;
    v.hasSyl = true;
}

//==============================================================================
Engine::Artic Engine::articulate (Voice& v, float dt)
{
    Artic a;
    const auto& nuc = v.syl.nucleus;
    a.x = nuc.x; a.y = nuc.y; a.lips = nuc.lips; a.rhotic = nuc.rhotic;
    const float scale = juce::jmax (0.2f, settings.consLength * v.ev.consScale);

    // syllabic consonant nucleus (hum on m / n, Slovak r / l)
    auto applyNucleus = [&]
    {
        if (! v.hasSyl) return;
        if (nuc.kind == PK::Nasal)
        {
            a.place = nuc.place; a.area = 0.0003f; a.velum = 1.3f; a.voice = 0.9f;
            a.mouthClose = nuc.place == Lips ? 1.0f : 0.3f;
            a.x = 0.55f; a.y = 0.5f;
        }
        else if (nuc.kind == PK::Lateral)
        {
            a.place = nuc.place; a.area = 0.35f; a.x = 0.5f; a.y = 0.5f;
        }
        else if (nuc.kind == PK::Trill || nuc.kind == PK::Rhotic || nuc.kind == PK::Tap)
        {
            a.place = Alveolar; a.area = 0.5f; a.x = 0.55f; a.y = 0.45f;
        }
        if (v.syl.breath) { a.voice = 0.0f; a.asp = 1.0f; }
    };

    float t = v.seqT;
    int k = 0;
    float u = 0.0f;
    for (; k < v.seqLen; ++k)
    {
        const float d = v.seq[(size_t) k].dur * scale;
        if (t < d) { u = t / d; break; }
        t -= d;
    }

    if (k >= v.seqLen)
    {
        if (v.inCoda)
        {
            // end of the syllable sung: let the note go, keeping the last shape
            v.inCoda = false;
            v.gate = false;
            v.releasing = true;
            v.relTime = 0.0f;
            if (v.seqLen > 0)
            {
                const auto& last = v.seq[(size_t) v.seqLen - 1];
                if (last.kind == PK::Glide) { a.x = last.x; a.y = last.y; a.lips = last.lips; return a; }
                if (last.kind == PK::Nasal) { a.place = last.place; a.area = 0.0003f; a.velum = 1.3f; a.mouthClose = last.place == Lips ? 1.0f : 0.3f; return a; }
            }
            applyNucleus();
            return a;
        }
        applyNucleus();
        // diphthong: drift towards the second vowel late in long notes (score knows the length)
        if (v.syl.diphthong && v.ev.lengthSec > 0.0f && ! v.inCoda)
        {
            const float onsetDur = v.noteTime;
            juce::ignoreUnused (onsetDur);
        }
        return a;
    }

    const auto& ph = v.seq[(size_t) k];
    const float strength = v.ev.consStrength >= 0.0f ? v.ev.consStrength : settings.consStrength;
    switch (ph.kind)
    {
        case PK::Plosive:
            a.place = ph.place;
            if (u < 0.72f)
            {
                a.area = 0.0003f;
                a.voice = ph.voiced ? 0.25f : 0.0f;
                a.mouthClose = ph.place == Lips ? 1.0f : 0.2f;
            }
            else
            {
                const float q = (u - 0.72f) / 0.28f;
                a.area = 0.05f + 0.5f * q;
                a.fric = (0.9f + 1.4f * strength) * (1.0f - q);
                a.fricPlace = ph.place;
                a.colour = ph.colour;
                a.voice = ph.voiced ? 1.0f : 0.0f;
                a.asp = ph.voiced ? 0.0f : 0.8f * (1.0f - 0.5f * q);
            }
            break;
        case PK::Fricative:
            a.place = ph.place;
            a.area = ph.area;
            a.fric = (0.5f + strength) * std::pow (std::sin (pi * juce::jlimit (0.0f, 1.0f, u)), 0.3f);
            a.fricPlace = ph.place;
            a.colour = ph.colour;
            a.voice = ph.voiced ? 0.55f : 0.0f;
            a.asp = ph.voiced ? 0.0f : 0.2f;
            a.mouthClose = ph.place == Lips ? 0.8f : 0.2f;
            break;
        case PK::Nasal:
            a.place = ph.place;
            a.area = 0.0003f;
            a.velum = 1.3f;
            a.voice = 0.9f;
            a.mouthClose = ph.place == Lips ? 1.0f : 0.25f;
            break;
        case PK::Lateral:
            a.place = ph.place;
            a.area = ph.area;
            a.voice = 0.95f;
            break;
        case PK::Rhotic:
            a.place = ph.place;
            a.area = ph.area;
            a.voice = 0.95f;
            a.x = 0.5f * (a.x + ph.x); a.y = 0.5f * (a.y + ph.y);
            a.lips = ph.lips;
            a.rhotic = true;
            break;
        case PK::Trill:
        {
            v.trillPhase += 27.0f * dt;
            const bool closed = std::fmod (v.trillPhase, 1.0f) < 0.45f;
            a.place = Alveolar;
            a.area = closed ? 0.0012f : ph.area;
            a.voice = 0.9f;
            break;
        }
        case PK::Tap:
            a.place = Alveolar;
            a.area = (u > 0.3f && u < 0.7f) ? 0.0012f : ph.area;
            a.voice = 0.9f;
            break;
        case PK::Glide:
            if (ph.area > 0.5f)
            {
                const float s = smoothstep (u);
                a.x += (ph.x - a.x) * s; a.y += (ph.y - a.y) * s; a.lips += (ph.lips - a.lips) * s;
            }
            else
            {
                const float s = smoothstep (u);
                a.x = ph.x + (a.x - ph.x) * s; a.y = ph.y + (a.y - ph.y) * s; a.lips = ph.lips + (a.lips - ph.lips) * s;
            }
            break;
        case PK::Aspirate:
            a.voice = ph.voiced ? 0.4f : 0.0f;
            a.asp = 1.0f;
            break;
        case PK::Glottal:
            a.voice = u < 0.7f ? 0.0f : 1.0f;
            break;
        case PK::Vowel:
            a.x = ph.x; a.y = ph.y; a.lips = ph.lips;
            break;
    }
    return a;
}

//==============================================================================
void Engine::controlUpdate (Voice& v, int group, int index, float dt)
{
    const auto& st = style (settings.style);
    const auto& e = exprSmooth[(size_t) group];
    const bool robot = st.robot;

    // ---- tract length (size / gender / age / cat, can move while singing)
    v.targetCm = tractCmFor (v, group);
    const float targetSections = (v.targetCm - lipComp()) * stepRate / speedOfSound;
    v.currentSections += (targetSections - v.currentSections) * juce::jmin (1.0f, dt * 12.0f);
    const int sections = (int) std::lround (v.currentSections);
    if (sections != v.tract.size()) v.tract.resize (sections);

    // ---- timing
    if (v.breathT >= 0.0f) v.breathT += dt;
    v.seqT += dt;
    v.noteTime += dt;
    if (v.releasing) v.relTime += dt;
    const float onsetDur = onsetSeconds (v.syl, juce::jmax (0.2f, settings.consLength * v.ev.consScale));
    const float vowelTime = juce::jmax (0.0f, v.noteTime - onsetDur);

    // ---- pitch
    float semis;
    if (v.glideDelay > 0.0f) { v.glideDelay -= dt; semis = v.from; }
    else if (v.glideT < 1.0f)
    {
        v.glideT = juce::jmin (1.0f, v.glideT + dt / v.glideTime);
        const float s = smoothstep (v.glideT);
        const float prep = settings.prep * 0.25f * juce::jmin (3.0f, std::abs (v.interval)) * std::sin (pi * juce::jmin (1.0f, v.glideT * 2.0f)) * (v.glideT < 0.5f ? 1.0f : 0.0f);
        semis = v.from + v.interval * s - (v.interval > 0 ? prep : -prep);
        if (v.glideT >= 1.0f) v.sinceArrive = 0.0f;
    }
    else semis = v.ev.pitch;
    v.semis = semis;

    float offset = 0.0f;
    if (! robot)
    {
        // overshoot after arriving
        v.sinceArrive += dt;
        if (v.sinceArrive < 0.5f && std::abs (v.interval) > 0.2f)
        {
            const float uu = v.sinceArrive / 0.075f;
            offset += (v.interval > 0 ? 1.0f : -1.0f) * juce::jmin (5.0f, std::abs (v.interval)) * 0.11f * settings.overshoot * st.overshoot * uu * std::exp (1.0f - uu);
        }
        // scoop into a fresh note
        offset += v.scoopAmt * std::exp (-vowelTime / juce::jmax (0.02f, st.scoopTime));

        // vibrato (delayed, fades in)
        const float depth = (v.ev.vibDepth >= 0.0f ? v.ev.vibDepth : settings.vibDepth) * st.vibScale * e.vibrato + modWheelValue * 80.0f;
        const float rate = (v.ev.vibRate > 0.0f ? v.ev.vibRate : settings.vibRate) * st.vibRate * (1.0f + 0.04f * std::sin (v.noteTime * 0.7f + (float) index));
        const float delay = v.ev.vibDelay >= 0.0f ? v.ev.vibDelay : settings.vibDelay;
        const float fade = juce::jlimit (0.0f, 1.0f, (vowelTime - delay) / 0.35f) * (v.glideT < 1.0f ? 0.3f : 1.0f);
        v.vibPhase += twoPi * rate * dt;
        if (v.vibPhase > twoPi) v.vibPhase -= twoPi;
        offset += 0.01f * depth * fade * std::sin (v.vibPhase);

        // drift
        v.driftTimer -= dt;
        if (v.driftTimer <= 0.0f)
        {
            v.driftTarget = v.rnd.next() * settings.drift * st.drift * 14.0f;
            v.driftTimer = 0.4f + 1.1f * v.rnd.uni();
        }
        v.driftValue += (v.driftTarget - v.driftValue) * juce::jmin (1.0f, dt * 2.0f);
        offset += 0.01f * v.driftValue;

        // fall-off at the end of a phrase
        if (v.releasing && v.phraseEnd)
            offset -= settings.fall * st.fall * 3.5f * smoothstep (v.relTime / 0.3f);
    }
    offset += 0.01f * e.pitch;
    if (group == 1) offset += bendValue * settings.bendRange;

    // ---- cat gestures
    float am = 1.0f, extraRasp = 0.0f;
    bool hiss = false, purr = false;
    {
        const float L = v.ev.lengthSec > 0.0f ? juce::jlimit (0.15f, 0.9f, 0.85f * v.ev.lengthSec) : 0.6f;
        const float tt = vowelTime;
        switch (v.syl.gesture)
        {
            case CatGesture::Meow:
            {
                const float uu = tt / L;
                offset += uu < 0.3f ? 3.0f * std::sin (1.5708f * uu / 0.3f) : 3.0f - 5.5f * smoothstep ((uu - 0.3f) / 0.7f);
                break;
            }
            case CatGesture::Mew:   offset += 1.8f * std::sin (pi * juce::jmin (1.0f, tt / L)); break;
            case CatGesture::Mrrp:
                v.purrPhase += 25.0f * dt;
                if (v.noteTime < onsetDur + 0.05f) am = 0.35f + 0.65f * std::abs (std::sin (pi * v.purrPhase));
                offset += 2.0f * smoothstep ((tt - 0.05f) / 0.12f);
                break;
            case CatGesture::Purr:
                v.purrPhase += 25.0f * dt;
                am = 0.15f + 0.85f * std::pow (std::abs (std::sin (pi * v.purrPhase)), 2.0f);
                purr = true;
                break;
            case CatGesture::Hiss:  hiss = true; break;
            case CatGesture::Yowl:  offset += 2.5f * std::sin (twoPi * 3.0f * tt) * juce::jmin (1.0f, tt / 0.3f); extraRasp = 0.3f; break;
            case CatGesture::Chirp: offset += -4.0f * std::exp (-tt / 0.05f); break;
            case CatGesture::Trill:
                v.purrPhase += 28.0f * dt;
                am = 0.3f + 0.7f * std::abs (std::sin (pi * v.purrPhase));
                offset += 1.5f * smoothstep (tt / juce::jmax (0.2f, L));
                break;
            case CatGesture::None: default: break;
        }
    }
    v.amMod = am;

    // ---- articulation -> tract shape
    Artic ar = articulate (v, dt);
    float x = juce::jlimit (0.0f, 1.0f, ar.x - 0.12f * settings.tongue);
    float y = juce::jlimit (0.0f, 1.0f, ar.y);
    // breath in: open "ah", noise only
    if (v.breathT >= 0.0f && ! v.gate) { x = 0.35f; y = 0.75f; }
    auto shape = shapeAt (x, y);
    auto& la = shape.logArea;
    const float jaw = juce::jlimit (-1.0f, 1.0f, settings.jaw + e.mouth);
    la[0] += std::log (1.0f - 0.55f * juce::jlimit (0.0f, 1.0f, settings.ring + st.ring * 0.5f));
    la[1] += 0.5f * settings.throat;
    la[2] += 0.5f * settings.throat;
    la[5] += 0.45f * jaw;
    la[6] += 0.45f * jaw;
    la[7] += 0.35f * jaw - 1.1f * settings.lips + 0.8f * ar.lips;
    if (ar.rhotic) { la[5] = std::log (juce::jmin (std::exp (la[5]), 0.6f)); la[7] -= 0.4f; }
    if (ar.place >= 0 && ar.place < numCtrl)
        la[(size_t) ar.place] = std::log (juce::jmax (0.0003f, juce::jmin (std::exp (la[(size_t) ar.place]), ar.area)));
    float mouthClose = ar.mouthClose;
    float velum = juce::jmax (0.0f, settings.nasal * 0.6f + st.nasal + 0.06f * juce::jlimit (0.0f, 1.0f, settings.cat + e.cat)) + ar.velum;
    if (purr) { mouthClose = juce::jmax (mouthClose, 0.85f); velum += 0.9f; }
    // oral stops / fricatives: the soft palate closes the nose, so no sound leaks during the closure
    if (ar.place >= 0 && ar.velum <= 0.0f && ar.area < 0.2f) velum = 0.0f;
    const float closed = std::log (0.015f);
    la[7] += (closed - la[7]) * mouthClose;
    // Articulators move like real ones: closing gestures fast (~5 ms), opening / vowel moves slower
    // (~14 ms). The formant transitions this makes are what tells b / d / g (and m / n) apart.
    if (! v.articInit)
    {
        v.laS = la;
        v.velumS = velum;
        v.articInit = true;
    }
    for (int i = 0; i < numCtrl; ++i)
    {
        const float target = la[(size_t) i];
        const float tau = target < v.laS[(size_t) i] ? 0.005f : 0.014f;
        v.laS[(size_t) i] += (target - v.laS[(size_t) i]) * (1.0f - std::exp (-dt / tau));
    }
    v.velumS += (velum - v.velumS) * (1.0f - std::exp (-dt / 0.012f));
    v.tract.setTarget (v.laS, v.velumS);
    v.tract.smooth (1.0f - std::exp (-dt / 0.0015f));

    if (index == 0 && group == uiGroup.load())
    {
        for (int i = 0; i < numCtrl; ++i) uiArea[(size_t) i] = std::exp (la[(size_t) i]);
    }
    v.mouthOpen = shape.mouthOpen * (1.0f - mouthClose);
    v.mouthWidth = juce::jlimit (0.1f, 1.0f, shape.mouthWidth - 0.3f * settings.lips + 0.1f * jaw);

    // ---- source
    const float dyn = juce::jlimit (0.0f, 2.0f, std::pow (v.ev.velocity, 1.1f) * e.dynamics * (group == 1 ? 1.0f + 0.3f * pressureValue : 1.0f));
    const float gender = settings.gender + e.gender;
    const float elder = juce::jmax (0.0f, settings.age - 0.5f) * 2.0f, child = juce::jmax (0.0f, 0.5f - settings.age) * 2.0f;
    const float cat = juce::jlimit (0.0f, 1.0f, settings.cat + e.cat);

    v.f0 = juce::jlimit (25.0f, 3000.0f, 440.0f * std::pow (2.0f, (semis + offset - 69.0f) / 12.0f));
    v.voicing = hiss ? 0.0f : dyn;
    v.consVoiceTarget = ar.voice;
    v.aspExtra = ar.asp + (hiss ? 1.2f : 0.0f);
    const float strength = v.ev.consStrength >= 0.0f ? v.ev.consStrength : settings.consStrength;
    v.fricTarget = (ar.fric + (hiss ? 1.4f : 0.0f)) * (0.6f + strength);
    v.fricColour = hiss ? 0.85f : ar.colour;
    const int fp = hiss ? Palatal : ar.fricPlace;
    v.fricIndex = fp >= 0 ? v.tract.sectionAt (juce::jmin (1.0f, (float) fp / 7.0f + 0.06f)) : -1;
    v.aspiration = 0.015f + 0.35f * settings.breath + 0.6f * st.breath + 0.6f * e.breath + 0.1f * elder + 0.05f * child + 0.05f * cat;
    v.glottis.tenseness = juce::jlimit (0.02f, 1.0f, st.tense + 0.3f * (settings.tension + e.tension) + 0.25f * (dyn - 0.6f)
                                                     + 0.04f * gender - 0.1f * child - (purr ? 0.25f : 0.0f));
    v.glottis.jitter = robot ? 0.0f : 0.002f + 0.01f * settings.jitter + 0.012f * elder + 0.01f * cat;
    v.glottis.shimmer = robot ? 0.0f : 0.02f + 0.04f * settings.jitter + 0.05f * elder;
    v.glottis.rasp = juce::jlimit (0.0f, 1.0f, settings.rasp + st.rasp + e.growl + extraRasp);
    v.tilt = juce::jlimit (-1.0f, 1.0f, settings.bright + e.bright + 0.15f * gender);
    // vocal effort shapes the source: soft / lax singing is darker, pressed / loud singing brighter
    {
        const float effort = juce::jlimit (0.0f, 1.0f, 0.5f * v.glottis.tenseness + 0.35f * juce::jmin (1.0f, dyn) + 0.25f * v.tilt);
        const float fc = 1800.0f + 7000.0f * effort * effort;
        v.srcCoef = 1.0f - std::exp (-twoPi * fc / stepRate);
    }

    // ---- level / pan
    float level = 1.0f;
    float pan = 0.0f;
    if (v.role == 1) { level = settings.doubleLevel; pan = settings.width * 0.7f * (group == 0 ? 1.0f : -1.0f); }
    if (v.role >= 2) { level = settings.harmonyLevel[(size_t) v.role - 2] * 0.8f; pan = settings.harmonyPan[(size_t) v.role - 2] * settings.width * 1.6f; }
    pan = juce::jlimit (-1.0f, 1.0f, pan);
    v.gain = level;
    v.panL = std::cos ((pan + 1.0f) * 0.25f * pi) * 1.41f;
    v.panR = std::sin ((pan + 1.0f) * 0.25f * pi) * 1.41f;

    if (index == 0)
    {
        uiPitch[(size_t) group] = v.active && (v.gate || v.env > 0.05f) ? semis + offset : -1.0f;
        if (group == uiGroup.load())
        {
            uiMouthOpen = v.mouthOpen * juce::jmin (1.0f, v.env * 1.3f);
            uiMouthWidth = v.mouthWidth;
        }
    }
}

//==============================================================================
void Engine::render (float* left, float* right, int numSamples)
{
    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);
    const auto& st = style (settings.style);
    const float attackMs = settings.attack == 0 ? 55.0f : (settings.attack == 2 ? 3.0f : 14.0f);
    const float attInc = 1.0f / juce::jmax (1.0f, attackMs * 0.001f * (float) sampleRate);
    const float relCoef = std::exp (-6.9f / juce::jmax (1.0f, settings.releaseMs * 0.001f * (float) sampleRate));
    const int ratio = (int) std::lround (stepRate / (float) sampleRate);
    juce::ignoreUnused (st);

    int done = 0;
    while (done < numSamples)
    {
        const int n = juce::jmin (controlPeriod, numSamples - done);
        const float dt = (float) n / (float) sampleRate;
        float* outL = left + done;
        float* outR = right + done;

        // expression smoothing (~25 ms)
        const float k = 1.0f - std::exp (-dt / 0.025f);
        for (int g = 0; g < numGroups; ++g)
        {
            auto& s = exprSmooth[(size_t) g];
            const auto& t = expr[(size_t) g];
            s.pitch += (t.pitch - s.pitch) * k; s.dynamics += (t.dynamics - s.dynamics) * k; s.breath += (t.breath - s.breath) * k;
            s.bright += (t.bright - s.bright) * k; s.gender += (t.gender - s.gender) * k; s.tension += (t.tension - s.tension) * k;
            s.growl += (t.growl - s.growl) * k; s.vibrato += (t.vibrato - s.vibrato) * k; s.cat += (t.cat - s.cat) * k;
            s.mouth += (t.mouth - s.mouth) * k;
        }

        for (int g = 0; g < numGroups; ++g)
        {
            float groupPeak = 0.0f;
            for (int vi = 0; vi < voicesPerGroup; ++vi)
            {
                auto& v = groups[(size_t) g].voices[(size_t) vi];

                // delayed starts / releases of the double + harmonies
                if (v.pendingSamples >= 0)
                {
                    v.pendingSamples -= n;
                    if (v.pendingSamples < 0)
                    {
                        startVoice (v, v.pending, g);
                        if (v.pendingRelease) { v.pendingRelease = false; v.releaseDelay = 0; }
                    }
                }
                if (v.releaseDelay >= 0)
                {
                    v.releaseDelay -= n;
                    if (v.releaseDelay < 0) releaseVoice (v, v.pendingPhraseEnd);
                }
                if (! v.active) continue;

                controlUpdate (v, g, vi, dt);

                float peak = 0.0f;
                const float hardBoost = settings.attack == 2 ? 1.0f : 0.0f;
                juce::ignoreUnused (hardBoost);
                for (int j = 0; j < n; ++j)
                {
                    if (v.gate) v.env = juce::jmin (1.0f, v.env + attInc);
                    else v.env *= relCoef;

                    float breathNoise = 0.0f;
                    if (v.breathT >= 0.0f && ! v.gate)
                    {
                        const float bt = v.breathT + (float) j / (float) sampleRate;
                        if (bt < v.breathLen) breathNoise = 0.35f * settings.autoBreath * std::sin (pi * bt / v.breathLen);
                    }

                    v.consVoice += (v.consVoiceTarget - v.consVoice) * 0.006f;
                    v.fric += (v.fricTarget - v.fric) * 0.02f;
                    float noise = 0.0f;
                    if (v.fric > 1.0e-4f)
                    {
                        const float white = v.rnd.next();
                        const float a = 0.85f - 0.6f * v.fricColour;
                        v.fHp = a * (v.fHp + white - v.fLast);
                        v.fLast = white;
                        const float consEnv = v.gate ? 1.0f : v.env;
                        noise = (v.fHp * (0.5f + v.fricColour) + white * (1.0f - v.fricColour) * 0.4f) * v.fric * juce::jmax (0.5f, v.voicing) * consEnv * 0.9f;
                    }

                    v.glottis.voicing = v.voicing * v.env * v.consVoice * v.amMod;
                    v.glottis.aspiration = v.aspiration * v.env * juce::jmax (0.3f, v.voicing) * 0.5f * (0.3f + 0.7f * v.consVoice) * v.amMod
                                         + v.aspExtra * juce::jmax (0.5f, v.voicing) * (v.gate ? 1.0f : v.env) * 0.6f + breathNoise;

                    float x = 0.0f;
                    for (int r = 0; r < ratio; ++r)
                    {
                        const float g = v.glottis.process (v.f0, stepRate);
                        v.srcLp += v.srcCoef * (g - v.srcLp);
                        x += v.tract.step (0.55f * g + 0.45f * v.srcLp, noise, v.fricIndex);
                    }
                    x /= (float) ratio;

                    // DC blocker, brightness tilt
                    const float y = x - v.dcX + 0.995f * v.dcY;
                    v.dcX = x;
                    v.dcY = y;
                    v.tiltLp += 0.16f * (y - v.tiltLp);
                    const float out = (v.tiltLp + (1.0f + v.tilt) * (y - v.tiltLp)) * 0.45f * v.gain;
                    outL[j] += out * v.panL;
                    outR[j] += out * v.panR;
                    peak = juce::jmax (peak, std::abs (out));
                }
                v.level = juce::jmax (peak, v.level * 0.9f);
                if (vi == 0) groupPeak = v.level;

                // done?
                if (! v.gate && v.env < 1.0e-4f && (v.breathT < 0.0f || v.breathT > v.breathLen) && v.pendingSamples < 0)
                {
                    v.idle += dt;
                    if (v.idle > 0.06f)
                    {
                        v.active = false;
                        v.env = 0.0f;
                        v.breathT = -1.0f;
                        v.tract.clear();
                        v.level = 0.0f;
                    }
                }
                else v.idle = 0.0f;
            }
            uiLevel[(size_t) g] = juce::jlimit (0.0f, 1.0f, groupPeak * 3.0f);
            if (! isSounding (g)) uiPitch[(size_t) g] = -1.0f;
        }
        done += n;
    }

    renderFx (left, right, numSamples);
}

//==============================================================================
void Engine::renderFx (float* left, float* right, int n)
{
    const float sr = (float) sampleRate;
    auto design = [sr] (Biquad& b, int type, float f, float gainDb, float q)
    {
        const float w = twoPi * juce::jlimit (10.0f, sr * 0.45f, f) / sr;
        const float cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0f * q);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        float b0, b1, b2, a0, a1, a2;
        if (type == 0)           // high-pass
        {
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        }
        else if (type == 1)      // peak
        {
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
        }
        else                     // high shelf
        {
            const float sq = 2 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cw + sq); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sq);
            a0 = (A + 1) - (A - 1) * cw + sq; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sq;
        }
        b.b0 = b0 / a0; b.b1 = b1 / a0; b.b2 = b2 / a0; b.a1 = a1 / a0; b.a2 = a2 / a0;
    };
    if (settings.lowCut != lastLowCut) { lastLowCut = settings.lowCut; design (lowCut, 0, settings.lowCut, 0.0f, 0.707f); }
    if (settings.presence != lastPresence) { lastPresence = settings.presence; design (presence, 1, 3000.0f, settings.presence, 0.8f); }
    if (settings.air != lastAir) { lastAir = settings.air; design (air, 2, 10000.0f, settings.air, 0.7f); }

    if (settings.reverb != lastReverb || std::abs (settings.reverbSize - lastReverbSize) > 0.01f)
    {
        lastReverb = settings.reverb;
        lastReverbSize = settings.reverbSize;
        static const float sizes[] = { 0.0f, 0.35f, 0.55f, 0.78f, 0.93f };
        static const float damps[] = { 0.5f, 0.55f, 0.25f, 0.35f, 0.15f };
        juce::Reverb::Parameters rp;
        const int r = juce::jlimit (0, 4, lastReverb);
        rp.roomSize = juce::jlimit (0.0f, 1.0f, sizes[r] * (0.6f + 0.8f * settings.reverbSize));
        rp.damping = damps[r];
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);
    }

    // compressor settings
    const float thresh = juce::Decibels::decibelsToGain (-8.0f - 20.0f * settings.comp);
    const float ratio = 1.0f + 5.0f * settings.comp;
    const float makeup = juce::Decibels::decibelsToGain (7.0f * settings.comp);
    const float att = 1.0f - std::exp (-1.0f / (0.004f * sr)), rel = 1.0f - std::exp (-1.0f / (0.12f * sr));
    const float drive = 1.0f + settings.drive * 6.0f;

    const float delaySamples = juce::jlimit (1.0f, (float) delayL.size() - 2.0f, (float) (settings.delayBeats * 60.0 / juce::jmax (30.0, settings.bpm)) * sr);
    const int dsize = (int) delayL.size();

    for (int i = 0; i < n; ++i)
    {
        float l = left[i], r = right[i];
        l = air.process (presence.process (lowCut.process (l, 0), 0), 0);
        r = air.process (presence.process (lowCut.process (r, 1), 1), 1);

        if (settings.comp > 0.001f)
        {
            const float lev = juce::jmax (std::abs (l), std::abs (r));
            compEnv += (lev > compEnv ? att : rel) * (lev - compEnv);
            float gain = 1.0f;
            if (compEnv > thresh) gain = std::pow (compEnv / thresh, 1.0f / ratio - 1.0f);
            compGain += (gain - compGain) * 0.01f;
            l *= compGain * makeup;
            r *= compGain * makeup;
        }
        if (settings.drive > 0.001f)
        {
            l = std::tanh (l * drive) / std::sqrt (drive);
            r = std::tanh (r * drive) / std::sqrt (drive);
        }

        // ping-pong delay
        if (settings.delayMix > 0.001f)
        {
            delaySmooth += (delaySamples - delaySmooth) * 0.0005f;
            if (delaySmooth < 1.0f) delaySmooth = delaySamples;
            float rp = (float) delayPos - delaySmooth;
            while (rp < 0.0f) rp += (float) dsize;
            const int i0 = (int) rp;
            const float fr = rp - (float) i0;
            const float dl = delayL[(size_t) i0] * (1.0f - fr) + delayL[(size_t) ((i0 + 1) % dsize)] * fr;
            const float dr = delayR[(size_t) i0] * (1.0f - fr) + delayR[(size_t) ((i0 + 1) % dsize)] * fr;
            delayL[(size_t) delayPos] = 0.5f * (l + r) * 0.7f + dr * settings.delayFeedback;
            delayR[(size_t) delayPos] = dl * settings.delayFeedback;
            delayPos = (delayPos + 1) % dsize;
            l += dl * settings.delayMix;
            r += dr * settings.delayMix;
        }
        left[i] = l;
        right[i] = r;
    }

    // reverb (block-wise)
    if (settings.reverb > 0 && settings.reverbMix > 0.001f)
    {
        int done = 0;
        while (done < n)
        {
            const int m = juce::jmin (n - done, (int) wetL.size());
            for (int i = 0; i < m; ++i) { wetL[(size_t) i] = left[done + i]; wetR[(size_t) i] = right[done + i]; }
            reverb.processStereo (wetL.data(), wetR.data(), m);
            for (int i = 0; i < m; ++i)
            {
                left[done + i] += wetL[(size_t) i] * settings.reverbMix * 0.6f;
                right[done + i] += wetR[(size_t) i] * settings.reverbMix * 0.6f;
            }
            done += m;
        }
    }

    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float l = left[i] * settings.volume, r = right[i] * settings.volume;
        limiter.processSample (l, r);
        left[i] = l;
        right[i] = r;
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }
    uiOutLevel = juce::jmax (peak, uiOutLevel.load() * 0.9f);
}

} // namespace talkcat
