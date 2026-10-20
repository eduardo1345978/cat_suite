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
void Ambience::render (float* l, float* r, int n, const Sim& sim)
{
    const float dt = (float) (1.0 / sr), storm = sim.weatherLevel, night = 1.0f - sim.daylight;
    const bool deep = sim.place == PlaceDeep || sim.place == PlaceIce || sim.place == PlaceCave;
    const float waveA = 0.05f + 0.06f * storm, rainA = 0.035f * juce::jmax (0.0f, storm - 0.2f) / 0.8f;
    const float lpCoef = 1.0f - std::exp (-2.0f * 3.14159f * 350.0f / (float) sr);
    for (int i = 0; i < n; ++i)
    {
        const float time = (float) t;
        // waves: soft filtered noise that swells in and out
        const float swell = 0.55f + 0.45f * std::sin (time * 0.9f) * std::sin (time * 0.23f + 1.0f);
        lpL += lpCoef * (next() - lpL);
        lpR += lpCoef * (next() - lpR);
        float a = lpL * waveA * swell * 2.5f, b = lpR * waveA * swell * 2.5f;
        // rain: brighter noise
        if (rainA > 0.0f)
        {
            const float nz = next();
            rainLp += 0.3f * (nz - rainLp);
            a += (nz - rainLp) * rainA; b += (next() - rainLp) * rainA;
        }
        // bubbles: little rising blips
        for (auto& bl : blips)
            if (bl.t >= 0.0f)
            {
                const float k = bl.t / bl.len;
                const float f = bl.f * (1.0f + bl.rise * k);
                const float v = std::sin (6.2831853f * f * bl.t) * bl.amp * std::sin (3.14159f * juce::jmin (1.0f, k));
                a += v * (1.0f - bl.pan); b += v * bl.pan;
                bl.t += dt;
                if (bl.t > bl.len) bl.t = -1.0f;
            }
        // a whale far away: a slow, low gliding song
        if (whaleT >= 0.0f)
        {
            const float k = whaleT / whaleLen;
            const float f = whaleBase * (1.0f + 0.35f * std::sin (3.14159f * k) - 0.15f * k) * (1.0f + 0.01f * std::sin (time * 5.5f));
            whalePh += f * dt;
            const float raw = std::sin (6.2831853f * whalePh) + 0.3f * std::sin (6.2831853f * whalePh * 2.01f);
            whaleLp += 0.05f * (raw - whaleLp);
            const float env = std::sin (3.14159f * juce::jmin (1.0f, k));
            const float v = whaleLp * env * (deep ? 0.07f : 0.045f);
            a += v * 0.6f; b += v * 0.4f;
            whaleT += dt;
            if (whaleT > whaleLen) whaleT = -1.0f;
        }
        l[i] += a; r[i] += b;
        t += dt;
    }
    // what happens next (block rate)
    const float blockSecs = n * dt;
    nextBlip -= blockSecs;
    if (nextBlip <= 0.0f)
    {
        for (auto& bl : blips)
            if (bl.t < 0.0f)
            {
                bl.t = 0.0f;
                bl.f = 500.0f + (next() + 1.0f) * 500.0f;
                bl.pan = 0.5f + next() * 0.4f;
                bl.amp = 0.012f + (next() + 1.0f) * 0.01f;
                bl.len = 0.03f + (next() + 1.0f) * 0.03f;
                bl.rise = 1.2f + (next() + 1.0f) * 0.6f;
                break;
            }
        nextBlip = (0.4f + (next() + 1.0f) * 1.2f) / (1.0f + storm * 2.0f);
    }
    nextWhale -= blockSecs;
    if (nextWhale <= 0.0f && whaleT < 0.0f)
    {
        whaleT = 0.0f;
        whaleLen = 4.0f + (next() + 1.0f) * 2.0f;
        whaleBase = 110.0f + (next() + 1.0f) * 60.0f;
        ++whales;
        nextWhale = (deep || night > 0.5f ? 70.0f : 140.0f) + (next() + 1.0f) * 60.0f;
    }
    if (sim.place == PlaceCave)
    {
        nextDrip -= blockSecs;
        if (nextDrip <= 0.0f)
        {
            for (auto& bl : blips)
                if (bl.t < 0.0f) { bl = { 0.0f, 1800.0f + (next() + 1.0f) * 600.0f, 0.5f + next() * 0.4f, 0.03f, 0.08f, -0.3f }; break; }
            nextDrip = 1.5f + (next() + 1.0f) * 2.5f;
        }
    }
}

//==============================================================================
void Music::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& d : decks) { d.renderer.prepare (sr, 4096); for (auto& m : d.parts) m.ensureSize (2048); }
    deckBuf.setSize (2, 4096);
    fish.prepare (sr);
    ambience.prepare (sr);
    fishVerb.setSampleRate (sr);
    juce::Reverb::Parameters p;
    p.roomSize = 0.85f; p.damping = 0.5f; p.wetLevel = 0.45f; p.dryLevel = 0.75f; p.width = 1.0f;
    fishVerb.setParameters (p);
    limiter.prepare (sr);
    fishBuf.setSize (2, 4096);
}

void Music::startStyle (int st, int key, double bpm, bool crossfade)
{
    style = juce::jlimit (0, NumStyles - 1, st);
    const auto& info = styleInfo (style);
    Settings s;
    s.seed = seed = seed * 1664525u + 1013904223u;
    s.genre = info.genre;
    s.mood = info.mood;
    s.endless = true;
    s.key = ((key % 12) + 12) % 12;
    auto first = compose (s);
    // always relaxed: the style's tempo range, and inside it what the fish like
    first.bpm = std::round (juce::jlimit ((double) info.bpmLo, (double) info.bpmHi, bpm > 0 ? bpm : first.bpm));
    auto newSong = std::make_shared<Song> (std::move (first));

    // soft sounds for every style (KeysSynth: 0 pad 1 e-piano 2 pluck 3 strings 4 chip 5 glass bell;
    // bass 0 sub 1 saw 2 reese 3 pluck; DrumKit 2 = lo-fi)
    SoundSet ss;
    switch (style)
    {
        case StyleLoFi:      ss = { 1, 0, 2, 2, 0 }; break;   // sky lounge: e-piano, soft beat, bell-like plucks
        case StyleDeepSea:   ss = { 0, 0, 2, 3, 0 }; break;   // crystal deep: pads and strings
        case StyleSunrise:   ss = { 1, 3, 2, 2, 2 }; break;   // bubble pop: e-piano, plucky bass and melody
        case StyleGlass:     ss = { 0, 0, 2, 5, 5 }; break;   // glass garden: pad and glass bells
        case StyleBossa:     ss = { 1, 3, 2, 1, 2 }; break;   // bossa breeze: e-piano, plucked bass, a soft kit
        case StylePixel:     ss = { 0, 0, 2, 4, 5 }; break;   // pixel lullaby: soft chip melody over a pad
        case StyleStrings:   ss = { 3, 0, 2, 3, 3 }; break;   // strings at dusk
        case StyleCity:      ss = { 1, 3, 2, 2, 1 }; break;   // city lights: e-piano, plucked bass
        case StyleIsland:    ss = { 1, 0, 2, 5, 2 }; break;   // island drift: e-piano skank, glass bell melody
        case StyleFolk:      ss = { 2, 3, 2, 2, 3 }; break;   // folk tide: plucked "guitar", strings
        case StyleDreamHaze: ss = { 0, 0, 2, 5, 0 }; break;   // dream haze: hazy pads, bells
        case StyleFunky:     ss = { 1, 3, 2, 2, 1 }; break;   // funky reef: e-piano stabs, plucked bass
        case StyleNightJazz: ss = { 1, 3, 2, 1, 3 }; break;   // night jazz: e-piano, walking bass, strings
        default:             ss = { 0, 0, 2, 2, 3 }; break;   // aqua dream: glassy pad, plucks, strings
    }

    auto& old = decks[(size_t) cur];
    const bool fade = crossfade && old.active && song != nullptr;
    const int next = fade ? 1 - cur : cur;
    auto& d = decks[(size_t) next];
    d.renderer.allOff();
    d.renderer.setSounds (ss);
    // calm: soft pads in front, a gentle melody, quiet counter-melody and (only in some styles) a whisper of a beat
    d.renderer.levels = { 0.55f, 0.5f, 0.16f, 0.55f, 0.28f };
    d.baseMelodyLevel = d.renderer.levels[(size_t) Melody];
    d.renderer.mute = {};
    d.renderer.mute[(size_t) Drums] = ! info.drums;
    d.renderer.reverbSend = 0.45f;
    d.player.setSong (newSong);
    d.active = true;
    if (fade)
    {
        // the new song comes in on the old one's next bar (at the same tempo they stay in step) and swells up
        // while the old song fades away under it
        const double oldBpm = juce::jmax (30.0, old.player.songBpm());
        const double beatsToBar = 4.0 - std::fmod (juce::jmax (0.0, old.ppq), 4.0);
        const double seconds = (beatsToBar < 0.05 ? 0.0 : beatsToBar) * 60.0 / oldBpm;
        d.ppq = -seconds * newSong->bpm / 60.0;
        d.gain = 0.0f; d.target = 1.0f; d.step = (float) (1.0 / ((seconds + 7.0) * sr));
        old.target = 0.0f; old.step = (float) (1.0 / (10.0 * sr));
    }
    else
    {
        if (next != cur) old.active = false;
        d.ppq = 0.0;
        d.gain = d.target = 1.0f;
        for (auto& other : decks) if (&other != &d && other.active) { other.renderer.allOff(); other.player.setSong (nullptr); other.active = false; }
    }
    cur = next;
    song = newSong;
    ++pieceCount;
    ++songsStarted;
    piecesInSong = 1;
}

void Music::nextPiece (Sim& sim)
{
    auto& d = decks[(size_t) cur];
    auto now = d.player.getSong();
    if (now == nullptr) return;
    if (++piecesInSong > 3)
    {
        // every few pieces a brand new song (new chords and melodies), crossfaded in so it never gets samey
        // and never jumps: same key and tempo, the new one starts on a bar line
        const int key = now->key;
        const double bpm = now->bpm;
        startStyle (style, key, bpm, true);
        sim.announce ("Now playing: " + currentTitle(), 0xff9ff3ff);
        return;
    }
    Song prev = *now;
    // slowly towards what the fish like: a few bpm and one step round the circle of fifths per piece
    const double target = sim.targetTempo() * (0.88 + 0.12 * sim.daylight);   // slower at night
    const double base = prev.bpm;
    const auto& info = styleInfo (style);
    prev.bpm = std::round (juce::jlimit ((double) info.bpmLo, (double) info.bpmHi, juce::jlimit (base - 3.0, base + 3.0, target)));
    const int want = sim.targetKey(), curKey = prev.key;
    const int fWant = (want * 7) % 12, fCur = (curKey * 7) % 12, diff = (fWant - fCur + 12) % 12;
    if (diff != 0) prev.key = (curKey + (diff <= 6 ? 7 : 5)) % 12;
    d.player.queueNext (std::make_shared<Song> (composeNext (prev)));
    ++pieceCount;
}

void Music::renderDeck (Deck& d, float* l, float* r, int n, const Sim& sim)
{
    const double bpm = juce::jmax (30.0, d.player.songBpm()), pps = bpm / 60.0 / sr;
    std::array<juce::MidiBuffer*, NumParts> ptrs;
    for (int p = 0; p < NumParts; ++p) { d.parts[(size_t) p].clear(); ptrs[(size_t) p] = &d.parts[(size_t) p]; }
    d.player.process (d.ppq, pps, n, true, ptrs);
    d.renderer.levels[(size_t) Melody] = d.baseMelodyLevel * (0.55f + 0.45f * sim.daylight);   // softer at night
    for (int p = 0; p < NumParts; ++p) d.renderer.handle (p, d.parts[(size_t) p]);
    d.renderer.render (l, r, n);
    d.ppq += pps * n;
}

void Music::render (float* l, float* r, int n, Sim& sim)
{
    if (song == nullptr) startStyle (sim.getStyle(), sim.targetKey(), sim.targetTempo());
    const int req = sim.takeStyleRequest();
    if (req >= 0) { sim.setStyle (req); startStyle (req, currentKey(), currentBpm(), true); }
    if (decks[(size_t) cur].player.needNext()) nextPiece (sim);

    if (deckBuf.getNumSamples() < n) deckBuf.setSize (2, n, false, false, true);
    juce::FloatVectorOperations::clear (l, n);
    juce::FloatVectorOperations::clear (r, n);
    for (auto& d : decks)
    {
        if (! d.active) continue;
        deckBuf.clear();
        auto* dl = deckBuf.getWritePointer (0);
        auto* dr = deckBuf.getWritePointer (1);
        renderDeck (d, dl, dr, n, sim);
        for (int i = 0; i < n; ++i)
        {
            d.gain = d.gain < d.target ? juce::jmin (d.target, d.gain + d.step) : juce::jmax (d.target, d.gain - d.step);
            // equal-power curve, so the mix never dips in the middle of a crossfade
            const float g = std::sin (d.gain * 1.5707963f) * 0.8f;
            l[i] += dl[i] * g;
            r[i] += dr[i] * g;
        }
        if (d.target <= 0.0f && d.gain <= 0.0f)
        {
            // faded out: free the deck
            d.renderer.allOff();
            d.player.setSong (nullptr);
            d.active = false;
        }
    }
    if (auto now = decks[(size_t) cur].player.getSong()) song = now;

    const double pps = currentBpm() / 60.0 / sr;
    const double ppq = decks[(size_t) cur].ppq - pps * n;
    if (fishBuf.getNumSamples() < n) fishBuf.setSize (2, n, false, false, true);
    fishBuf.clear();
    singFish (sim, ppq, ppq + pps * n, pps, n, 0);
    ambience.render (fishBuf.getWritePointer (0), fishBuf.getWritePointer (1), n, sim);   // shares the fish's reverb
    auto* fl = fishBuf.getWritePointer (0);
    auto* fr = fishBuf.getWritePointer (1);
    fishVerb.processStereo (fl, fr, n);
    for (int i = 0; i < n; ++i)
    {
        float a = l[i] + fl[i] * 0.4f, b = r[i] + fr[i] * 0.4f;
        limiter.processSample (a, b);
        l[i] = a; r[i] = b;
    }
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
    const int maxStrong = 2;
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
        const double local = decks[(size_t) cur].player.getPlayPosition();
        const auto* chord = song->chordAt (local);
        int sung = 0;
        const int maxPerStep = strong ? maxStrong : (stepInBar % 2 == 0 ? 1 : 0);   // no busy 16ths: calm
        if (singers.empty()) continue;
        const int start = singRotate++ % (int) singers.size();
        for (size_t k = 0; k < singers.size() && sung < maxPerStep; ++k)
        {
            auto& f = *singers[(size_t) ((start + (int) k) % (int) singers.size())];
            const auto& g = f.genes;
            if ((g.rhythmPattern & (1u << stepInBar)) == 0) continue;
            const float chance = g.rhythm == 0 ? 0.9f : g.rhythm == 1 ? 0.7f : 0.55f;
            if (rng.nextFloat() > chance * 0.2f) continue;   // named or not: everyone sings now and then

            const int deg = g.melody[(size_t) (f.melodyPos % juce::jmax (1, g.melodyLength))];
            ++f.melodyPos;
            static const int base[3] = { 48, 60, 72 };
            int note = base[juce::jlimit (0, 2, g.pitch)] + keyOff + iv[(size_t) (deg % 7)] + 12 * (deg / 7);
            if (chord != nullptr && ! chord->isTone (note))
            {
                // every note lands on a chord note, so it always fits the music
                for (int d : { 1, -1, 2, -2, 3, -3 })
                    if (chord->isTone (note + d)) { note += d; break; }
            }
            const int pc = ((note - key) % 12 + 12) % 12;
            bool inScale = false;
            for (auto s : iv) inScale = inScale || s == pc;
            if (! inScale && (chord == nullptr || ! chord->isTone (note))) ++outOfKey;

            const float beats = g.noteLength == 1 ? (g.rhythm == 2 ? 0.5f : 0.9f) : (g.rhythm == 2 ? 0.2f : 0.3f);
            const float seconds = beats * 60.0f / (float) song->bpm;
            const float bright = g.voice == VoiceChip || g.voice == VoiceWhistle ? 0.55f : 1.0f;   // the sharp voices softer
            const float vel = (0.5f + rng.nextFloat() * 0.2f) * level * bright * (strong ? 1.0f : 0.8f);
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
        const int si = song->sectionIndexAt (decks[(size_t) cur].player.getPlayPosition());
        m.title = song->title;
        if (si >= 0) m.section = sectionName (song->sections[(size_t) si].type).toUpperCase();
    }
    m.piece = pieceCount - 1;
    m.targetBpm = sim.targetTempo();
    m.targetKey = keyName (sim.targetKey());
    m.place = placeName (sim.place).toUpperCase();
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
