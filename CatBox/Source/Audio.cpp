#include "Audio.h"
#include "Scope.h"
#include <cmath>
#if JUCE_LINUX
 #include <pthread.h>
 #include <sched.h>
#endif

namespace catbox
{
namespace
{
constexpr int subBlock = 32;
}

//==============================================================================
void Audio::TrackRt::fxNoteOn (int note, float vel, bool slide, bool accent)
{
    if (engine == nullptr) return;
    if (modSet != nullptr) mod.noteOn (note, vel, *modSet);
    engine->noteOn (note, vel, slide, accent);
    hitBits |= trackDrums ? (1u << (((note - 36) % 16 + 16) % 16)) : 1u;
}

void Audio::TrackRt::fxNoteOff (int note)
{
    if (engine != nullptr) engine->noteOff (note);
}

//==============================================================================
Audio::Audio()
{
    for (auto& a : stepNow) a = -1;
    for (auto& a : level) a = 0.0f;
    for (auto& a : hits) a = 0u;
    for (auto& s : scope) s = 0.0f;
    for (auto& s : xyL) s = 0.0f;
    for (auto& s : xyR) s = 0.0f;
    scopeInit();
    for (size_t ti = 0; ti < tracks.size(); ++ti)
        for (int e = 0; e < NumEngineTypes; ++e) { tracks[ti].engines[(size_t) e] = makeEngine (e); tracks[ti].engines[(size_t) e]->setTrack ((int) ti); }
    live = Project::makeDefault();
    incoming = std::make_unique<Project>();
}

Audio::~Audio() = default;

void Audio::prepare (double sampleRate, int block)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = juce::jmax (block, 64);
    if (! capturing.load())
    {
        captureL.assign ((size_t) (sr * maxCaptureSeconds), 0.0f);
        captureR.assign ((size_t) (sr * maxCaptureSeconds), 0.0f);
        captureLength = 0;
    }
    for (auto* v : { &tl, &tr_, &ml, &mr, &al, &ar, &bl, &br, &fa, &fb, &cl, &cr }) v->assign ((size_t) subBlock, 0.0f);
    for (auto& t : tracks)
    {
        for (auto& e : t.engines) e->prepare (sr);
        t.fx.prepare (sr);
        for (auto& i : t.ins) i.prepare (sr);
        t.engineType = -1;
        t.fxType = -1;
        t.numQueued = 0;
    }
    reverb.prepare (sr);
    delay.prepare (sr);
    sendFx.prepare (sr);
    master.prepare (sr);
    for (int t = 0; t < numTracks; ++t) syncTrack (t);
}

void Audio::audioDeviceAboutToStart (juce::AudioIODevice* d)
{
    rtAsked = false;
    prepare (d->getCurrentSampleRate(), d->getCurrentBufferSizeSamples());
}

void Audio::feedInput (const float* l, const float* r, int n)
{
    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (l[i]), std::abs (r[i]));
    inputLevel = juce::jmax (pk, inputLevel.load() * 0.9f);
    if (! capturing.load()) return;
    int at = captureLength.load();
    const int room = (int) captureL.size() - at;
    const int m = juce::jmin (n, room);
    for (int i = 0; i < m; ++i) { captureL[(size_t) (at + i)] = l[i]; captureR[(size_t) (at + i)] = r[i]; }
    captureLength = at + m;
    if (m < n) capturing = false;   // full
}

void Audio::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                              const juce::AudioIODeviceCallbackContext&)
{
   #if JUCE_LINUX
    // the Pi: the sound card's thread asks for real-time priority once, so the screen never makes it late
    // (setup_pi.sh allows it for the audio group; without that permission nothing changes)
    if (! rtAsked.exchange (true))
    {
        sched_param sp {};
        sp.sched_priority = 70;
        pthread_setschedparam (pthread_self(), SCHED_FIFO, &sp);
    }
   #endif
    if (numIn > 0 && in[0] != nullptr) feedInput (in[0], numIn > 1 && in[1] != nullptr ? in[1] : in[0], n);
    for (int c = 0; c < numOut; ++c) if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], n);
    if (numOut < 1 || out[0] == nullptr) return;
    float* l = out[0];
    float* r = numOut > 1 && out[1] != nullptr ? out[1] : nullptr;
    if (r == nullptr)
    {
        // mono output: render stereo in pieces, sum
        float tmpR[256];
        for (int s = 0; s < n; s += 256)
        {
            const int m = juce::jmin (256, n - s);
            std::fill (tmpR, tmpR + m, 0.0f);
            render (l + s, tmpR, m);
            for (int i = 0; i < m; ++i) l[s + i] = 0.5f * (l[s + i] + tmpR[i]);
        }
        return;
    }
    render (l, r, n);
}

//==============================================================================
void Audio::push (const Project& p, bool atBar)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    *incoming = p;
    hasIncoming = true;
    incomingAtBar = incomingAtBar || (atBar && playing.load());
    if (incomingAtBar) sceneWaiting = true;
}

void Audio::post (const Event& e)
{
    const auto scope = eventFifo.write (1);
    if (scope.blockSize1 > 0) events[(size_t) scope.startIndex1] = e;
    else if (scope.blockSize2 > 0) events[(size_t) scope.startIndex2] = e;
}

bool Audio::popRec (RecNote& r)
{
    const auto scope = recFifo.read (1);
    if (scope.blockSize1 > 0) { r = recs[(size_t) scope.startIndex1]; return true; }
    if (scope.blockSize2 > 0) { r = recs[(size_t) scope.startIndex2]; return true; }
    return false;
}

void Audio::pushRec (const RecNote& r)
{
    const auto scope = recFifo.write (1);
    if (scope.blockSize1 > 0) recs[(size_t) scope.startIndex1] = r;
    else if (scope.blockSize2 > 0) recs[(size_t) scope.startIndex2] = r;
}

void Audio::applyIncoming (bool atBarBoundary)
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked() || ! hasIncoming) return;
    if (incomingAtBar && playing.load() && ! atBarBoundary) return;
    live = *incoming;
    hasIncoming = false;
    incomingAtBar = false;
    sceneWaiting = false;
    for (int t = 0; t < numTracks; ++t) syncTrack (t);
}

/** Engine, parameters and note FX of a track follow the project. */
void Audio::syncTrack (int t)
{
    auto& rt = tracks[(size_t) t];
    const auto& tr = live.tracks[(size_t) t];
    if (rt.engineType != tr.engine)
    {
        if (rt.engine != nullptr) rt.engine->allOff();
        rt.engineType = tr.engine;
        rt.engine = rt.engines[(size_t) juce::jlimit (0, NumEngineTypes - 1, tr.engine)].get();
        rt.engine->allOff();
        rt.engine->setBend (rt.bendNow);
        rt.engine->setMod (rt.modNow);
        rt.applied.fill (std::numeric_limits<float>::quiet_NaN());
        rt.locked = false;
        rt.trackDrums = engineDef (tr.engine).drums;
    }
    rt.modSet = &tr.mods.set;
    rt.engineMods = rt.anyMods = false;
    for (const auto& sl : tr.mods.slots)
        if (sl.active())
        {
            rt.anyMods = true;
            rt.engineMods = rt.engineMods || dest::isEngine (sl.dest);
        }
    if (! rt.locked) rt.base = tr.params;
    refreshEngine (rt, tr, false);
    rt.engine->setTempo (live.m.tempo);
    if (rt.fxType != tr.fxType || rt.fxKey != live.m.key || rt.fxScale != live.m.scale
        || std::memcmp (rt.fxApplied.data(), tr.fx.data(), sizeof (float) * (size_t) numFxParams) != 0)
    {
        if (rt.fxType != tr.fxType) rt.fx.reset (rt);   // a different note FX: what the old one played stops
        rt.fxType = tr.fxType;
        rt.fxKey = live.m.key;
        rt.fxScale = live.m.scale;
        rt.fxApplied = tr.fx;
        rt.fx.set (tr.fxType, tr.fx.data(), live.m.key, live.m.scale);
    }
}

float Audio::modded (const TrackRt& rt, const Track& tr, int d, float value, const ParamDef& def) const
{
    bool any = false;
    float n = 0.0f;
    for (const auto& sl : tr.mods.slots)
        if (sl.dest == d && sl.active())
        {
            if (! any) { n = def.toNorm (value); any = true; }
            n += sl.amount * rt.mod.value[(size_t) juce::jlimit (0, NumModSources - 1, sl.source)];
        }
    return any ? def.fromNorm (n) : value;
}

void Audio::refreshEngine (TrackRt& rt, const Track& tr, bool force)
{
    if (rt.engine == nullptr) return;
    if (! rt.engineMods)
    {
        if (force || std::memcmp (rt.applied.data(), rt.base.data(), sizeof (float) * (size_t) maxParams) != 0)
        {
            rt.applied = rt.base;
            rt.engine->setParams (rt.applied.data());
        }
        return;
    }
    auto v = rt.base;
    const auto& def = engineDef (tr.engine);
    for (const auto& sl : tr.mods.slots)
        if (sl.active() && dest::isEngine (sl.dest) && sl.dest < (int) def.params.size())
        {
            const auto& pd = def.params[(size_t) sl.dest];
            v[(size_t) sl.dest] = pd.fromNorm (pd.toNorm (v[(size_t) sl.dest]) + sl.amount * rt.mod.value[(size_t) juce::jlimit (0, NumModSources - 1, sl.source)]);
        }
    if (force || std::memcmp (rt.applied.data(), v.data(), sizeof (float) * (size_t) maxParams) != 0)
    {
        rt.applied = v;
        rt.engine->setParams (rt.applied.data());
    }
}

//==============================================================================
void Audio::queueNote (TrackRt& rt, double ppq, int note, float vel, bool on, bool slide, bool accent)
{
    if (rt.numQueued >= (int) rt.queue.size()) return;
    if (on) ++rt.nextId;   // every caller queues a note-on, then its note-off: they share the id
    rt.queue[(size_t) rt.numQueued++] = { ppq, (int16_t) juce::jlimit (0, 127, note), vel, on, slide, accent, rt.nextId };
}

void Audio::runQueue (TrackRt& rt, double upTo)
{
    for (;;)
    {
        int best = -1;
        for (int i = 0; i < rt.numQueued; ++i)
        {
            const auto& q = rt.queue[(size_t) i];
            if (q.ppq > upTo) continue;
            if (best < 0 || q.ppq < rt.queue[(size_t) best].ppq || (q.ppq == rt.queue[(size_t) best].ppq && q.on && ! rt.queue[(size_t) best].on)) best = i;
        }
        if (best < 0) return;
        const auto q = rt.queue[(size_t) best];
        rt.queue[(size_t) best] = rt.queue[(size_t) --rt.numQueued];
        if (q.on)
        {
            // the same note still held from before (a note as long as the loop, a chord over the whole pattern):
            // its old note-off goes now, BEFORE the new note - not after it, which would cut the new one
            for (int i = 0; i < rt.numQueued;)
            {
                const auto& o = rt.queue[(size_t) i];
                if (! o.on && o.note == q.note && o.id != q.id && o.ppq >= q.ppq - 1.0e-6)
                {
                    rt.fx.noteOff (o.note, q.ppq, rt);
                    rt.queue[(size_t) i] = rt.queue[(size_t) --rt.numQueued];
                }
                else ++i;
            }
        }
        if (q.on) rt.fx.noteOn (q.note, q.vel, q.slide, q.accent, q.ppq, rt);
        else rt.fx.noteOff (q.note, q.ppq, rt);
    }
}

void Audio::runSequencer (int t, double from, double to)
{
    auto& rt = tracks[(size_t) t];
    const auto& tr = live.tracks[(size_t) t];
    const auto& pat = tr.pat;
    const double stepLen = rateBeats (pat.rate);
    const int length = juce::jlimit (1, maxSteps, pat.length);
    while (rt.nextStep < to)
    {
        const double t0 = rt.nextStep;
        const int s = rt.step % length;
        // swing (every second step later) and the step's nudge
        double at = t0;
        if (s % 2 == 1) at += live.m.swing * stepLen * 0.5;
        at = juce::jmax (from, at);
        // recorded knob moves: this step's values on top of the sound (and back on a step without them)
        if (pat.hasLocks (s))
        {
            rt.base = tr.params;
            const auto& def = engineDef (tr.engine);
            for (const auto& l : pat.locks[(size_t) s])
                if (l.param >= 0 && l.param < (int) def.params.size()) rt.base[(size_t) l.param] = l.value;
            rt.locked = true;
            refreshEngine (rt, tr, false);
        }
        else if (rt.locked)
        {
            rt.base = tr.params;
            rt.locked = false;
            refreshEngine (rt, tr, false);
        }
        auto skipped = [&rt, s, t0] (int note)
        {
            for (int i = 0; i < rt.numSkips; ++i)
                if (rt.skips[(size_t) i].step == s && rt.skips[(size_t) i].note == note && rt.skips[(size_t) i].until > t0)
                {
                    rt.skips[(size_t) i] = rt.skips[(size_t) --rt.numSkips];
                    return true;
                }
            return false;
        };
        if (rt.trackDrums)
        {
            for (int lane = 0; lane < numLanes; ++lane)
            {
                const int v = pat.drums[(size_t) lane][(size_t) s];
                if (v <= 0 || skipped (36 + lane)) continue;
                const int roll = juce::jlimit (1, 4, (int) pat.rolls[(size_t) lane][(size_t) s]);
                for (int k = 0; k < roll; ++k)   // a roll: the hits share the step, a little softer each
                {
                    const double tk = at + stepLen * k / roll;
                    queueNote (rt, tk, 36 + lane, (float) v / 127.0f * (1.0f - 0.12f * (float) k), true, false, v >= 120 && k == 0);
                    queueNote (rt, tk + stepLen * 0.5 / roll, 36 + lane, 0.0f, false, false, false);
                }
            }
        }
        else
        {
            const auto& st = pat.steps[(size_t) s];
            if (st.on() && (st.chance >= 100 || rng.nextInt (100) < st.chance))
            {
                const double on = juce::jmax (from, at + st.nudge * 0.01 * stepLen);
                const bool slideIn = rt.lastSlideNote >= 0;
                const int ratchet = juce::jlimit (1, 4, (int) st.ratchet);
                for (auto n : st.notes)
                {
                    if (n < 0 || skipped (n)) continue;
                    for (int k = 0; k < ratchet; ++k)
                    {
                        const double onK = on + stepLen * k / ratchet;
                        queueNote (rt, onK, n, (float) st.vel / 127.0f * (st.accent ? 1.0f : 0.85f) * (1.0f - 0.1f * (float) k), true, slideIn && k == 0, st.accent && k == 0);
                        // a slide holds the note into the next step (the next note glides in)
                        double len = st.slide && k == ratchet - 1 ? stepLen * 1.02 / ratchet : juce::jmax (0.01, stepLen * st.len * 0.01);
                        if (ratchet > 1) len = juce::jmin (len, stepLen / ratchet * 0.85);
                        if (st.slide && k == ratchet - 1) len = stepLen * (1.02 - (double) k / ratchet);
                        queueNote (rt, onK + len, n, 0.0f, false, false, false);
                    }
                }
                rt.lastSlideNote = st.slide ? st.notes[0] : -1;
            }
            else rt.lastSlideNote = -1;
        }
        stepNow[(size_t) t] = s;
        rt.step = (s + 1) % length;
        rt.nextStep = t0 + stepLen;
    }
}

//==============================================================================
void Audio::liveNoteOn (int t, int note, float vel)
{
    auto& rt = tracks[(size_t) t];
    note = juce::jlimit (0, 127, note);
    const bool legato = rt.liveCount > 0;
    if (! rt.liveDown[(size_t) note]) { rt.liveDown[(size_t) note] = true; ++rt.liveCount; }
    rt.fx.noteOn (note, vel, legato, vel > 0.92f, clock, rt);

    if (! (recording.load() && playing.load())) return;
    // Rec: into the nearest step (a note a little early belongs to the step that comes)
    auto& pat = live.tracks[(size_t) t].pat;
    const double stepLen = rateBeats (pat.rate);
    const int length = juce::jlimit (1, maxSteps, pat.length);
    const double pos = (clock - playStart) / stepLen;
    const long long nearest = std::llround (pos);
    const int s = (int) (((nearest % length) + length) % length);
    const int velocity = juce::jlimit (1, 127, juce::roundToInt (vel * 127.0f));
    if ((double) nearest > pos && rt.numSkips < (int) rt.skips.size())   // the step is still to come: don't play it twice
        rt.skips[(size_t) rt.numSkips++] = { s, note, clock + stepLen };
    if (rt.trackDrums)
    {
        const int lane = ((note - 36) % numLanes + numLanes) % numLanes;
        pat.drums[(size_t) lane][(size_t) s] = (uint8_t) velocity;
        pushRec ({ RecNote::DrumHit, (int8_t) t, (int16_t) s, (int16_t) lane, (int16_t) velocity, false });
        return;
    }
    auto& st = pat.steps[(size_t) s];
    st.add (note);
    st.vel = (uint8_t) velocity;
    st.accent = vel > 0.92f;
    rt.recStart[(size_t) note] = clock;
    rt.recStep[(size_t) note] = s;
    pushRec ({ RecNote::NoteAdd, (int8_t) t, (int16_t) s, (int16_t) note, (int16_t) velocity, st.accent });
    if (legato && rt.lastRecNote >= 0 && rt.liveDown[(size_t) rt.lastRecNote] && rt.lastRecNote != note)   // played legato: the one before slides
    {
        const int ps = rt.recStep[(size_t) rt.lastRecNote];
        pat.steps[(size_t) ps].slide = true;
        pushRec ({ RecNote::SlideOn, (int8_t) t, (int16_t) ps, (int16_t) rt.lastRecNote, 0, false });
    }
    rt.lastRecNote = note;
}

void Audio::liveNoteOff (int t, int note)
{
    auto& rt = tracks[(size_t) t];
    note = juce::jlimit (0, 127, note);
    if (rt.liveDown[(size_t) note]) { rt.liveDown[(size_t) note] = false; rt.liveCount = juce::jmax (0, rt.liveCount - 1); }
    rt.fx.noteOff (note, clock, rt);
    if (! (recording.load() && playing.load()) || rt.trackDrums || rt.recStart[(size_t) note] <= 0.0) return;
    auto& pat = live.tracks[(size_t) t].pat;
    const double stepLen = rateBeats (pat.rate);
    const int pct = juce::jlimit (10, 1600, juce::roundToInt ((clock - rt.recStart[(size_t) note]) / stepLen * 100.0));
    const int s = rt.recStep[(size_t) note];
    pat.steps[(size_t) s].len = (uint16_t) pct;
    rt.recStart[(size_t) note] = 0.0;
    pushRec ({ RecNote::NoteLength, (int8_t) t, (int16_t) s, (int16_t) note, (int16_t) pct, false });
}

void Audio::startPlaying()
{
    playStart = clock;
    for (auto& rt : tracks) { rt.nextStep = clock; rt.step = 0; rt.lastSlideNote = -1; rt.numSkips = 0; }
    playing = true;
}

void Audio::stopPlaying()
{
    playing = false;
    recording = false;
    for (int t = 0; t < numTracks; ++t)
    {
        auto& rt = tracks[(size_t) t];
        // the sequencer's notes stop; what is held on the keys keeps sounding
        for (int i = 0; i < rt.numQueued;)
            if (! rt.queue[(size_t) i].on) { rt.fx.noteOff (rt.queue[(size_t) i].note, clock, rt); rt.queue[(size_t) i] = rt.queue[(size_t) --rt.numQueued]; }
            else ++i;
        rt.numQueued = 0;
        stepNow[(size_t) t] = -1;
    }
}

void Audio::handleEvents()
{
    const int ready = eventFifo.getNumReady();
    if (ready <= 0) return;
    const auto scope = eventFifo.read (ready);
    auto handle = [this] (const Event& e)
    {
        const int t = juce::jlimit (0, numTracks - 1, (int) e.track);
        auto& rt = tracks[(size_t) t];
        switch (e.type)
        {
            case Event::NoteOn:  liveNoteOn (t, e.note, juce::jlimit (0.01f, 1.0f, e.value)); break;
            case Event::NoteOff: liveNoteOff (t, e.note); break;
            case Event::Play:    if (! playing.load()) startPlaying(); break;
            case Event::Stop:    if (playing.load()) stopPlaying(); break;
            case Event::RecOn:   recording = true; break;
            case Event::RecOff:  recording = false; break;
            case Event::AllOff:
                for (auto& x : tracks)
                {
                    x.fx.reset (x);
                    if (x.engine != nullptr) x.engine->allOff();
                    x.numQueued = 0;
                    x.liveDown.fill (false);
                    x.liveCount = 0;
                }
                break;
            case Event::Bend:    rt.bendTarget = juce::jlimit (-24.0f, 24.0f, e.value); break;   // glided in render (no zipper steps)
            case Event::Mod:     rt.modTarget = juce::jlimit (0.0f, 1.0f, e.value); break;
            case Event::Preview:
            {
                // a short phrase while browsing sounds (when it isn't playing)
                if (rt.trackDrums)
                {
                    const int lanes[] = { 0, 3, 1, 3 };
                    for (int i = 0; i < 4; ++i)
                    {
                        queueNote (rt, clock + i * 0.25, 36 + lanes[i], 0.85f, true, false, false);
                        queueNote (rt, clock + i * 0.25 + 0.1, 36 + lanes[i], 0.0f, false, false, false);
                    }
                }
                else
                {
                    const int n = juce::jlimit (0, 127, (int) e.note);
                    queueNote (rt, clock, n, 0.8f, true, false, false);
                    queueNote (rt, clock + 0.9, n, 0.0f, false, false, false);
                }
                break;
            }
        }
    };
    for (int i = 0; i < scope.blockSize1; ++i) handle (events[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) handle (events[(size_t) (scope.startIndex2 + i)]);
}

//==============================================================================
void Audio::render (float* outL, float* outR, int n)
{
    // tiny numbers (a reverb or delay tail fading to silence) are flushed to zero: without this the CPU computes them
    // 10-100 times slower, the sound card misses its turn and everything stutters (and the last bit of sound repeats)
    const juce::ScopedNoDenormals noDenormals;
    applyIncoming (false);
    handleEvents();
    wasPlaying = playing.load();
    if (silent) { updateViews(); return; }

    const double ppqPerSample = live.m.tempo / 60.0 / sr;
    bool anySolo = false;
    for (const auto& t : live.tracks) anySolo = anySolo || t.solo;
    float trackPeak[numTracks] {};
    float outPeak = 0.0f;

    for (int s0 = 0; s0 < n; s0 += subBlock)
    {
        const int m = juce::jmin (subBlock, n - s0);
        const double from = clock, to = clock + m * ppqPerSample;

        // the bar line: scenes waiting for it come in now
        if (playing.load())
        {
            const double barFrom = (from - playStart) / 4.0, barTo = (to - playStart) / 4.0;
            if (std::floor (barTo) > std::floor (barFrom)) applyIncoming (true);
        }

        std::fill (al.begin(), al.end(), 0.0f); std::fill (ar.begin(), ar.end(), 0.0f);
        std::fill (bl.begin(), bl.end(), 0.0f); std::fill (br.begin(), br.end(), 0.0f);
        std::fill (ml.begin(), ml.end(), 0.0f); std::fill (mr.begin(), mr.end(), 0.0f);
        std::fill (cl.begin(), cl.end(), 0.0f); std::fill (cr.begin(), cr.end(), 0.0f);

        for (int t = 0; t < numTracks; ++t)
        {
            auto& rt = tracks[(size_t) t];
            const auto& tr = live.tracks[(size_t) t];
            if (playing.load()) runSequencer (t, from, to);
            runQueue (rt, to);
            rt.fx.advance (from, to, playing.load() ? playStart : 0.0, live.m.tempo, rt);

            // modulation: the sources move, the engine follows (every 64 samples)
            // bend and mod glide to where the joystick / wheel is (about 15 ms): moving it never steps or clicks
            {
                const float k = 1.0f - std::exp (-(float) m / (0.015f * (float) sr));
                if (rt.bendNow != rt.bendTarget)
                {
                    rt.bendNow += (rt.bendTarget - rt.bendNow) * k;
                    if (std::abs (rt.bendNow - rt.bendTarget) < 1.0e-4f) rt.bendNow = rt.bendTarget;
                    rt.engine->setBend (rt.bendNow);
                }
                if (rt.modNow != rt.modTarget)
                {
                    rt.modNow += (rt.modTarget - rt.modNow) * k;
                    if (std::abs (rt.modNow - rt.modTarget) < 1.0e-4f) rt.modNow = rt.modTarget;
                    rt.modWheel = rt.modNow;
                    rt.engine->setMod (rt.modNow);
                }
            }
            rt.mod.advance (m, sr, from, ppqPerSample, playing.load(), from - playStart, rt.modWheel, tr.mods.set, tr.mods.macros);
            if (rt.engineMods && (++rt.modTick & 1) == 0) refreshEngine (rt, tr, false);

            std::fill (tl.begin(), tl.end(), 0.0f);
            std::fill (tr_.begin(), tr_.end(), 0.0f);
            rt.engine->render (tl.data(), tr_.data(), m);

            // the insert effects, in order
            for (int k = 0; k < numInserts; ++k)
            {
                const auto& in = tr.mods.inserts[(size_t) k];
                if (in.type == InsOff || ! in.on) { if (rt.ins[(size_t) k].lastType != -1 && in.type == InsOff) rt.ins[(size_t) k].lastType = -1; continue; }
                float pv[insertParams];
                const auto& idef = insertDef (in.type);
                for (int q = 0; q < insertParams; ++q)
                    pv[q] = rt.anyMods && q < (int) idef.params.size() ? modded (rt, tr, dest::insert0 + k * insertParams + q, in.p[(size_t) q], idef.params[(size_t) q]) : in.p[(size_t) q];
                rt.ins[(size_t) k].process (in.type, pv, tl.data(), tr_.data(), m, from, ppqPerSample, live.m.tempo);
            }

            if (t == viewTrack.load (std::memory_order_relaxed))   // the XY scope: this track alone, before the mixer
            {
                int xw = xyWrite.load (std::memory_order_relaxed);
                for (int i = 0; i < m; ++i)
                {
                    xyL[(size_t) xw].store (tl[(size_t) i], std::memory_order_relaxed);
                    xyR[(size_t) xw].store (tr_[(size_t) i], std::memory_order_relaxed);
                    xw = (xw + 1) % xySize;
                }
                xyWrite.store (xw, std::memory_order_relaxed);
            }
            float volume = tr.volume, panV = tr.pan, sendA = tr.sendA, sendB = tr.sendB, sendC = tr.sendC;
            if (rt.anyMods)
            {
                static const ParamDef unit = defs::pct ("u", "u", 0.0f), bip = defs::bi ("b", "b", 0.0f);
                volume = modded (rt, tr, dest::mix0, volume, unit);
                panV = modded (rt, tr, dest::mix0 + 1, panV, bip);
                sendA = modded (rt, tr, dest::mix0 + 2, sendA, unit);
                sendB = modded (rt, tr, dest::mix0 + 3, sendB, unit);
                sendC = modded (rt, tr, dest::mix0 + 4, sendC, unit);
            }
            const bool audible = ! tr.mute && (! anySolo || tr.solo);
            const float vol = audible ? volume * volume * 1.4f : 0.0f;   // a curve that feels even on the knob
            const float pan = juce::jlimit (-1.0f, 1.0f, panV);
            const float gl = vol * std::sqrt (0.5f * (1.0f - pan)) * 1.4142f, gr = vol * std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
            const float dgL = (gl - rt.gainL) / (float) m, dgR = (gr - rt.gainR) / (float) m;
            for (int i = 0; i < m; ++i)
            {
                rt.gainL += dgL; rt.gainR += dgR;
                const float a = tl[(size_t) i] * rt.gainL, b = tr_[(size_t) i] * rt.gainR;
                ml[(size_t) i] += a; mr[(size_t) i] += b;
                al[(size_t) i] += a * sendA; ar[(size_t) i] += b * sendA;
                bl[(size_t) i] += a * sendB; br[(size_t) i] += b * sendB;
                cl[(size_t) i] += a * sendC; cr[(size_t) i] += b * sendC;
                trackPeak[t] = juce::jmax (trackPeak[t], std::abs (a), std::abs (b));
            }
            if (rt.hitBits != 0) { hits[(size_t) t].fetch_or (rt.hitBits); rt.hitBits = 0; }
        }

        // how loud the dry mix is: the sends can duck under it
        {
            float pk = 0.0f;
            for (int i = 0; i < m; ++i) pk = juce::jmax (pk, std::abs (ml[(size_t) i]), std::abs (mr[(size_t) i]));
            duckEnv = pk > duckEnv ? duckEnv + (pk - duckEnv) * 0.5f : duckEnv * 0.985f;
        }
        delay.process (bl.data(), br.data(), ml.data(), mr.data(), al.data(), ar.data(), m, live.m, duckEnv);
        reverb.process (al.data(), ar.data(), ml.data(), mr.data(), m, live.m, duckEnv);
        sendFx.process (cl.data(), cr.data(), ml.data(), mr.data(), m, live.m, from, ppqPerSample, duckEnv);

        // metronome: on every beat while playing
        if (live.m.metronome && playing.load())
        {
            const double b0 = (from - playStart), b1 = (to - playStart);
            if (std::floor (b1) > std::floor (b0) || b0 == 0.0)
            {
                clickEnv = 1.0f;
                clickHz = ((long long) std::floor (b1) % 4 == 0) ? 1600.0f : 1000.0f;
            }
        }
        if (clickEnv > 0.0005f)
            for (int i = 0; i < m; ++i)
            {
                clickPhase += clickHz / (float) sr;
                if (clickPhase >= 1.0f) clickPhase -= 1.0f;
                const float c = std::sin (dsp::twoPi * clickPhase) * clickEnv * 0.25f;
                clickEnv *= 0.9985f;
                ml[(size_t) i] += c; mr[(size_t) i] += c;
            }

        master.process (ml.data(), mr.data(), m, live.m);
        for (int i = 0; i < m; ++i)
        {
            outL[s0 + i] += ml[(size_t) i];
            outR[s0 + i] += mr[(size_t) i];
            outPeak = juce::jmax (outPeak, std::abs (ml[(size_t) i]), std::abs (mr[(size_t) i]));
            if (++scopeDecim >= 2)
            {
                scopeDecim = 0;
                const int w = scopeWrite.load (std::memory_order_relaxed);
                scope[(size_t) w].store (0.5f * (ml[(size_t) i] + mr[(size_t) i]), std::memory_order_relaxed);
                scopeWrite.store ((w + 1) % scopeSize, std::memory_order_relaxed);
            }
        }
        clock = to;
    }

    for (int t = 0; t < numTracks; ++t)
    {
        float cur = level[(size_t) t].load();
        while (trackPeak[t] > cur && ! level[(size_t) t].compare_exchange_weak (cur, trackPeak[t])) {}
    }
    float cur = peak.load();
    while (outPeak > cur && ! peak.compare_exchange_weak (cur, outPeak)) {}
    songBeats = playing.load() ? clock - playStart : 0.0;
    updateViews();
}

void Audio::updateViews()
{
    const int vt = juce::jlimit (0, numTracks - 1, viewTrack.load());
    const auto& orbit = tracks[(size_t) vt].fx.orbitState();
    const bool isOrbit = tracks[(size_t) vt].fx.getType() == FxOrbit;
    for (int i = 0; i < orbitcat::maxBodies; ++i)
    {
        const auto& b = orbit.bodies[(size_t) i];
        auto& v = planets[(size_t) i];
        v.active = isOrbit && b.active;
        if (! v.active.load()) continue;
        v.note = b.note;
        v.theta = (float) b.theta;
        v.radius = (float) b.radiusNow;
        v.mass = live.tracks[(size_t) vt].fx[5];
    }
}
} // namespace catbox
