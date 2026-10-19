#include "OceanMusic.h"
#include <cmath>

using namespace cat::muse;

namespace ocean
{
//==============================================================================
void FishSynth::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& v : voices) v = {};
}

int FishSynth::activeVoices() const
{
    int n = 0;
    for (auto& v : voices) n += v.on ? 1 : 0;
    return n;
}

void FishSynth::noteOn (int midiNote, float velocity, float seconds, int voice, float pan, bool shimmer)
{
    // a free voice, else the quietest one
    V* best = nullptr;
    for (auto& v : voices) if (! v.on) { best = &v; break; }
    if (best == nullptr)
    {
        best = &voices[0];
        for (auto& v : voices) if (v.env < best->env) best = &v;
    }
    *best = {};
    best->on = true;
    best->voice = juce::jlimit (0, NumVoices - 1, voice);
    best->freq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
    best->vel = velocity;
    best->dur = juce::jmax (0.05f, seconds);
    best->pan = juce::jlimit (0.0f, 1.0f, pan);
    best->shimmer = shimmer;
    best->noise = 0x9E3779B9u ^ (juce::uint32) (++age * 2654435761u);
}

void FishSynth::render (float* l, float* r, int n)
{
    const float dt = (float) (1.0 / sr), twoPi = 6.2831853f;
    for (auto& v : voices)
    {
        if (! v.on) continue;
        const float gl = std::sqrt (1.0f - v.pan), gr = std::sqrt (v.pan);
        for (int i = 0; i < n; ++i)
        {
            const float t = v.t;
            float s = 0.0f, env = 0.0f;
            const float rel = t - v.dur;
            switch (v.voice)
            {
                case VoiceBubble:
                {
                    const float f = v.freq * (1.0f + 0.7f * std::exp (-t * 45.0f));
                    v.ph += f * dt; s = std::sin (twoPi * v.ph);
                    env = juce::jmin (1.0f, t * 400.0f) * std::exp (-t * 7.0f);
                    break;
                }
                case VoicePluck:
                {
                    v.ph += v.freq * dt; v.ph -= std::floor (v.ph);
                    const float saw = 2.0f * v.ph - 1.0f;
                    const float cutoff = juce::jmin (0.9f, 0.05f + 0.6f * std::exp (-t * 9.0f));
                    v.lp += cutoff * (saw - v.lp); s = v.lp * 1.4f;
                    env = juce::jmin (1.0f, t * 600.0f) * std::exp (-t * 5.0f);
                    break;
                }
                case VoiceBell:
                {
                    v.ph += v.freq * dt; v.ph2 += v.freq * 3.5f * dt;
                    s = std::sin (twoPi * v.ph + 1.8f * std::exp (-t * 3.0f) * std::sin (twoPi * v.ph2));
                    env = juce::jmin (1.0f, t * 800.0f) * std::exp (-t * 2.2f);
                    break;
                }
                case VoiceFlute:
                {
                    v.ph += v.freq * (1.0f + 0.004f * std::sin (twoPi * 5.0f * t) * juce::jmin (1.0f, t * 3.0f)) * dt;
                    v.noise = v.noise * 1664525u + 1013904223u;
                    const float nz = ((float) (v.noise >> 9) / 8388608.0f - 1.0f);
                    v.lp += 0.08f * (nz - v.lp);
                    s = std::sin (twoPi * v.ph) + 0.12f * std::sin (2.0f * twoPi * v.ph) + v.lp * 0.35f;
                    env = juce::jmin (1.0f, t * 22.0f) * (rel > 0 ? std::exp (-rel * 14.0f) : 1.0f);
                    break;
                }
                case VoiceChip:
                {
                    v.ph += v.freq * dt; v.ph -= std::floor (v.ph);
                    s = (v.ph < (v.freq > 600.0f ? 0.25f : 0.5f) ? 1.0f : -1.0f) * 0.45f;
                    env = (t < 0.004f ? t * 250.0f : 1.0f) * (rel > 0 ? std::exp (-rel * 30.0f) : 1.0f) * (0.85f + 0.15f * std::exp (-t * 6.0f));
                    break;
                }
                case VoicePad:
                {
                    v.ph += v.freq * 1.004f * dt; v.ph -= std::floor (v.ph);
                    v.ph2 += v.freq * 0.996f * dt; v.ph2 -= std::floor (v.ph2);
                    const float saw = (2.0f * v.ph - 1.0f) + (2.0f * v.ph2 - 1.0f);
                    v.lp += 0.06f * (saw - v.lp); s = v.lp * 0.8f;
                    env = juce::jmin (1.0f, t * 6.0f) * (rel > 0 ? std::exp (-rel * 3.5f) : 1.0f);
                    break;
                }
                case VoiceMarimba:
                {
                    v.ph += v.freq * dt;
                    s = std::sin (twoPi * v.ph) + 0.35f * std::sin (4.0f * twoPi * v.ph) * std::exp (-t * 25.0f);
                    env = juce::jmin (1.0f, t * 700.0f) * std::exp (-t * 8.0f);
                    break;
                }
                default:   // whistle
                {
                    const float vib = 0.006f * std::sin (twoPi * 5.5f * t) * juce::jmin (1.0f, juce::jmax (0.0f, t - 0.08f) * 6.0f);
                    v.ph += v.freq * (1.0f + vib) * dt; s = std::sin (twoPi * v.ph);
                    env = juce::jmin (1.0f, t * 30.0f) * (rel > 0 ? std::exp (-rel * 18.0f) : 1.0f);
                    break;
                }
            }
            if (v.shimmer) s += 0.25f * std::sin (2.0f * twoPi * v.ph);
            v.env = env;
            const float out = s * env * v.vel * 0.3f;
            l[i] += out * gl; r[i] += out * gr;
            v.t += dt;
            if ((t > v.dur + 0.05f && env < 0.0005f) || t > v.dur + 4.0f) { v.on = false; break; }
        }
    }
}

//==============================================================================
void Music::prepare (double sampleRate)
{
    sr = sampleRate;
    renderer.prepare (sr, 4096);
    fish.prepare (sr);
    fishVerb.setSampleRate (sr);
    juce::Reverb::Parameters p;
    p.roomSize = 0.8f; p.damping = 0.4f; p.wetLevel = 0.35f; p.dryLevel = 0.8f; p.width = 1.0f;
    fishVerb.setParameters (p);
    limiter.prepare (sr);
    fishBuf.setSize (2, 4096);
}

void Music::startStyle (int st, int key, double bpm)
{
    style = juce::jlimit (0, NumStyles - 1, st);
    Settings s;
    s.seed = seed = seed * 1664525u + 1013904223u;
    s.genre = styleGenre (style);
    static const int moods[NumStyles] = { Dreamy, Chill, Chill, Happy, Epic, Happy, Dark, Epic };
    s.mood = moods[style];
    s.endless = true;
    s.key = ((key % 12) + 12) % 12;
    auto first = compose (s);
    // the genre keeps its own feel; the fish pull it a little
    if (bpm > 0) first.bpm = std::round (juce::jlimit (first.bpm * 0.85, first.bpm * 1.15, bpm));
    song = std::make_shared<Song> (std::move (first));
    renderer.setSounds (defaultSounds (s.genre));
    renderer.allOff();
    player.setSong (song);
    ppq = 0.0;
    ++pieceCount;
}

void Music::nextPiece (const Sim& sim)
{
    auto now = player.getSong();
    if (now == nullptr) return;
    Song prev = *now;
    // slowly towards what the fish like: a few bpm and one step round the circle of fifths per piece
    const double target = sim.targetTempo();
    const double base = prev.bpm;
    prev.bpm = std::round (juce::jlimit (base - 3.0, base + 3.0, target));
    const int want = sim.targetKey(), cur = prev.key;
    const int fWant = (want * 7) % 12, fCur = (cur * 7) % 12, diff = (fWant - fCur + 12) % 12;
    if (diff != 0) prev.key = (cur + (diff <= 6 ? 7 : 5)) % 12;
    auto next = std::make_shared<Song> (composeNext (prev));
    player.queueNext (next);
    ++pieceCount;
}

void Music::render (float* l, float* r, int n, Sim& sim)
{
    if (song == nullptr) startStyle (sim.getStyle(), sim.targetKey(), sim.targetTempo());
    const int req = sim.takeStyleRequest();
    if (req >= 0) { sim.setStyle (req); startStyle (req, currentKey(), currentBpm()); }
    if (player.needNext()) nextPiece (sim);

    const double bpm = player.songBpm(), pps = bpm / 60.0 / sr;
    std::array<juce::MidiBuffer*, NumParts> ptrs;
    for (int p = 0; p < NumParts; ++p) { parts[(size_t) p].clear(); ptrs[(size_t) p] = &parts[(size_t) p]; }
    player.process (ppq, pps, n, true, ptrs);
    if (auto now = player.getSong()) song = now;
    for (int p = 0; p < NumParts; ++p) renderer.handle (p, parts[(size_t) p]);
    renderer.render (l, r, n);
    for (int i = 0; i < n; ++i) { l[i] *= 0.8f; r[i] *= 0.8f; }

    if (fishBuf.getNumSamples() < n) fishBuf.setSize (2, n, false, false, true);
    fishBuf.clear();
    singFish (sim, ppq, ppq + pps * n, pps, n, 0);
    auto* fl = fishBuf.getWritePointer (0);
    auto* fr = fishBuf.getWritePointer (1);
    fishVerb.processStereo (fl, fr, n);
    for (int i = 0; i < n; ++i)
    {
        float a = l[i] + fl[i] * 0.9f, b = r[i] + fr[i] * 0.9f;
        limiter.processSample (a, b);
        l[i] = a; r[i] = b;
    }
    ppq += pps * n;
}

void Music::singFish (Sim& sim, double fromBeat, double toBeat, double pps, int n, int)
{
    auto* fl = fishBuf.getWritePointer (0);
    auto* fr = fishBuf.getWritePointer (1);
    if (song == nullptr) { fish.render (fl, fr, n); return; }

    const auto iv = scaleIntervals (song->scale);
    const int key = song->key, keyOff = key > 6 ? key - 12 : key;
    std::vector<Fish*> singers;
    for (auto& f : sim.fish) if (f.present && ! f.dying && ! f.leaving) singers.push_back (&f);
    // a few voices at a time, so the song stays clear: two on the beat, one in between
    const int maxStrong = singers.size() <= 8 ? 3 : 2;
    const float level = 1.0f / std::sqrt (juce::jmax (1.0f, (float) singers.size() / 6.0f));

    int done = 0;
    double step = std::ceil (fromBeat * 4.0 - 1e-9);
    for (; step / 4.0 < toBeat; step += 1.0)
    {
        const double beat = step / 4.0;
        const int at = juce::jlimit (0, n, (int) ((beat - fromBeat) / pps));
        if (at > done) { fish.render (fl + done, fr + done, at - done); done = at; }

        const int stepInBar = ((int) std::llround (step)) % 16;
        const bool strong = stepInBar % 4 == 0;
        const double local = player.getPlayPosition();
        const auto* chord = song->chordAt (local);
        int sung = 0;
        const int maxPerStep = strong ? maxStrong : 1;
        if (singers.empty()) continue;
        const int start = singRotate++ % (int) singers.size();
        for (size_t k = 0; k < singers.size() && sung < maxPerStep; ++k)
        {
            auto& f = *singers[(size_t) ((start + (int) k) % (int) singers.size())];
            const auto& g = f.genes;
            if ((g.rhythmPattern & (1u << stepInBar)) == 0) continue;
            const float chance = g.rhythm == 0 ? 0.9f : g.rhythm == 1 ? 0.7f : 0.55f;
            if (rng.nextFloat() > chance * (f.viewer() ? 1.0f : 0.35f)) continue;

            const int deg = g.melody[(size_t) (f.melodyPos % juce::jmax (1, g.melodyLength))];
            ++f.melodyPos;
            static const int base[3] = { 48, 60, 72 };
            int note = base[juce::jlimit (0, 2, g.pitch)] + keyOff + iv[(size_t) (deg % 7)] + 12 * (deg / 7);
            if (strong && chord != nullptr && ! chord->isTone (note))
            {
                // land on a chord note on the strong beats
                for (int d : { 1, -1, 2, -2, 3, -3 })
                    if (chord->isTone (note + d)) { note += d; break; }
            }
            const int pc = ((note - key) % 12 + 12) % 12;
            bool inScale = false;
            for (auto s : iv) inScale = inScale || s == pc;
            if (! inScale && (chord == nullptr || ! chord->isTone (note))) ++outOfKey;

            const float beats = g.noteLength == 1 ? (g.rhythm == 2 ? 0.5f : 0.9f) : (g.rhythm == 2 ? 0.2f : 0.3f);
            const float seconds = beats * 60.0f / (float) song->bpm;
            const float vel = (0.55f + rng.nextFloat() * 0.2f) * level * (strong ? 1.0f : 0.85f);
            fish.noteOn (note, vel, seconds, g.voice, 0.1f + 0.8f * juce::jlimit (0.0f, 1.0f, f.x / (float) W), g.rare != RareNone);
            f.singFlash = 1.0f;
            ++sung;
            ++fishNotes;
        }
    }
    if (done < n) fish.render (fl + done, fr + done, n - done);
}

MusicInfo Music::info (const Sim& sim) const
{
    MusicInfo m;
    m.style = styleName (style).toUpperCase();
    if (song != nullptr)
    {
        m.key = keyName (song->key);
        m.scale = scaleName (song->scale).toUpperCase();
        m.bpm = (int) std::lround (song->bpm);
        const int si = song->sectionIndexAt (player.getPlayPosition());
        if (si >= 0) m.section = sectionName (song->sections[(size_t) si].type).toUpperCase();
    }
    m.piece = pieceCount - 1;
    m.targetBpm = sim.targetTempo();
    m.targetKey = keyName (sim.targetKey());
    return m;
}

juce::ValueTree Music::save() const
{
    juce::ValueTree t ("MUSIC");
    t.setProperty ("style", style, nullptr);
    t.setProperty ("seed", (int) seed, nullptr);
    t.setProperty ("key", currentKey(), nullptr);
    t.setProperty ("bpm", currentBpm(), nullptr);
    t.setProperty ("pieces", pieceCount, nullptr);
    return t;
}

void Music::load (const juce::ValueTree& t)
{
    if (! t.hasType ("MUSIC")) return;
    seed = (juce::uint32) (int) t.getProperty ("seed", 1);
    pieceCount = juce::jmax (0, (int) t.getProperty ("pieces", 0) - 1);
    startStyle (t.getProperty ("style", 0), t.getProperty ("key", 0), t.getProperty ("bpm", 90.0));
}
} // namespace ocean
