#include "BoxAudio.h"
#include <cmath>

namespace catbox
{
namespace
{
constexpr int maxChunk = 2048;   // render() splits longer blocks
constexpr int subBlock = 32;     // events land on 32-sample boundaries (0.7 ms at 48 kHz)
constexpr double stepPpq = 0.25; // Drums and Bass run on 1/16 notes
}

BoxAudio::BoxAudio() : rnd (0xca7b0c5)
{
    bufL.resize ((size_t) maxChunk * NumInstruments);
    bufR.resize ((size_t) maxChunk * NumInstruments);
    mixL.resize ((size_t) maxChunk);
    mixR.resize ((size_t) maxChunk);
}

void BoxAudio::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    drums.prepare (sr);
    keys.prepare (sr);
    bassSynth.prepare (sr);
    acid.prepare (sr);
    arpAcid.prepare (sr);
    supersaw.prepare (sr);
    arpGlide.prepare (sr, maxChunk);
    bassGlide.prepare (sr, maxChunk);
    arpGlideFx.prepare (sr, maxChunk);
    bassGlideFx.prepare (sr, maxChunk);
    master.prepare (sr, maxChunk);
    for (auto& f : fx) f.prepare (sr);
    limiter.prepare (sr);
    numOffs = 0;
}

void BoxAudio::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void BoxAudio::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOutputs,
                                                 int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOutputs; ++c)
        if (outputs[c] != nullptr) juce::FloatVectorOperations::clear (outputs[c], numSamples);
    if (numOutputs <= 0 || outputs[0] == nullptr) return;

    float* l = outputs[0];
    float* r = numOutputs > 1 && outputs[1] != nullptr ? outputs[1] : nullptr;
    if (r != nullptr) { render (l, r, numSamples); return; }

    float tmpR[256];   // mono output: render stereo in chunks and fold down
    for (int pos = 0; pos < numSamples; pos += 256)
    {
        const int n = juce::jmin (256, numSamples - pos);
        juce::FloatVectorOperations::clear (tmpR, n);
        render (l + pos, tmpR, n);
        for (int i = 0; i < n; ++i) l[pos + i] = 0.5f * (l[pos + i] + tmpR[i]);
    }
}

//==============================================================================
void BoxAudio::setSnapshot (const AudioSnapshot& s, bool atBar)
{
    const juce::SpinLock::ScopedLockType lock (snapshotLock);
    pending = s;
    snapshotAtBar = atBar || (snapshotFresh && snapshotAtBar);   // a waiting scene keeps waiting
    snapshotFresh = true;
    if (snapshotAtBar) sceneWaiting = true;
}

//==============================================================================
void BoxAudio::arpNoteOn (int note, int velocity)
{
    if (arpIsGlide()) arpGlide.notesOn ({ { 1, note } }, { (float) juce::jlimit (1, 127, velocity) / 127.0f });
    else if (live.arpSound == arpSoundSupersaw) supersaw.noteOn (note, velocity);
    else if (live.arpSound == arpSoundAcid) arpAcid.noteOn (note, velocity >= 110, false);
    else keys.handle (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, velocity)));
}

void BoxAudio::arpNoteOff (int note)
{
    supersaw.noteOff (note);
    arpAcid.noteOff (note);
    arpGlide.noteOff (1, note);
    keys.handle (juce::MidiMessage::noteOff (1, note));
}

void BoxAudio::arpAllOff()
{
    supersaw.allOff();
    arpAcid.allOff();
    arpGlide.allNotesOff();
    keys.allOff();
}

int BoxAudio::followOffset() const noexcept
{
    if (! live.bassFollow || ! arp.hasNotes()) return 0;
    uint64_t lo = 0, hi = 0;
    arp.activeNotes (lo, hi);
    int lowest = -1;
    for (int n = 0; n < 128 && lowest < 0; ++n)
        if (((n < 64 ? lo >> n : hi >> (n - 64)) & 1u) != 0) lowest = n;
    if (lowest < 0) return 0;
    int off = ((lowest - live.key) % 12 + 12) % 12;
    return off > 6 ? off - 12 : off;   // the shortest way
}

void BoxAudio::post (const BoxEvent& e)
{
    const juce::SpinLock::ScopedLockType lock (postLock);
    const auto scope = eventFifo.write (1);
    if (scope.blockSize1 > 0) events[(size_t) scope.startIndex1] = e;
    else if (scope.blockSize2 > 0) events[(size_t) scope.startIndex2] = e;
}

void BoxAudio::setPlaying (bool p)
{
    if (p == playing.load()) return;
    playing = p;
    (p ? startRequest : stopRequest) = true;
}

bool BoxAudio::popRecorded (RecordedNote& out)
{
    const auto scope = recordFifo.read (1);
    if (scope.blockSize1 > 0) { out = recorded[(size_t) scope.startIndex1]; return true; }
    if (scope.blockSize2 > 0) { out = recorded[(size_t) scope.startIndex2]; return true; }
    return false;
}

void BoxAudio::pushRecorded (const RecordedNote& n)
{
    const auto w = recordFifo.write (1);
    if (w.blockSize1 > 0) recorded[(size_t) w.startIndex1] = n;
    else if (w.blockSize2 > 0) recorded[(size_t) w.startIndex2] = n;
}

float BoxAudio::hashNoise (int64_t k, int salt) noexcept
{
    uint32_t h = (uint32_t) (k * 2654435761u) ^ (uint32_t) (salt * 40503u + 0x9e3779b9u);
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
    return (float) (h & 0xffff) / 32767.5f - 1.0f;   // -1 .. 1, the same for the same step
}

//==============================================================================
void BoxAudio::flashDrum (int note)
{
    for (int p = 0; p < numPads; ++p)
        if (drumPadNote (p) == note) { drumFlash.fetch_or (1u << p); drumFlashLink.fetch_or (1u << p); return; }
}

void BoxAudio::scheduleOff (int instrument, int note, double offPpq)
{
    if (numOffs < (int) offs.size()) offs[(size_t) numOffs++] = { instrument, note, offPpq };
}

void BoxAudio::cancelOffs (int instrument, int note)
{
    for (int i = 0; i < numOffs;)
    {
        if (offs[(size_t) i].instrument == instrument && offs[(size_t) i].note == note) offs[(size_t) i] = offs[(size_t) --numOffs];
        else ++i;
    }
}

void BoxAudio::releaseDue (double upTo)
{
    for (int i = 0; i < numOffs;)
    {
        if (offs[(size_t) i].ppq <= upTo)
        {
            const auto o = offs[(size_t) i];
            offs[(size_t) i] = offs[(size_t) --numOffs];
            if (o.instrument == Arp) arpNoteOff (o.note);
            else if (o.instrument == Bass) bassOff (o.note);
        }
        else ++i;
    }
}

void BoxAudio::allNotesOff()
{
    arpAllOff();
    bassSynth.allOff();
    acid.allOff();
    bassGlide.allNotesOff();
    numOffs = 0;
    liveBassHeld = 0;
    bassNoteNow = -1;
}

void BoxAudio::bassOn (int note, int velocity, bool accent, bool slide)
{
    note = juce::jlimit (0, 127, note);
    if (bassIsGlide()) bassGlide.notesOn ({ { 1, note } }, { (float) juce::jlimit (1, 127, accent ? 127 : velocity) / 127.0f });
    else if (live.bassSound == 0) acid.noteOn (note, accent, slide);
    else bassSynth.handle (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, accent ? 127 : velocity)));
    bassNoteNow = note;
}

void BoxAudio::bassOff (int note)
{
    acid.noteOff (note);
    bassSynth.handle (juce::MidiMessage::noteOff (1, note));
    bassGlide.noteOff (1, note);
    if (bassNoteNow.load() == note) bassNoteNow = -1;
}

void BoxAudio::handleEvents()
{
    const int ready = eventFifo.getNumReady();
    if (ready == 0) return;
    const auto scope = eventFifo.read (ready);
    auto handle = [this] (const BoxEvent& e)
    {
        const bool rec = recording && running;
        const double songPos = clock - songStart;
        switch (e.type)
        {
            case BoxEvent::DrumHit:
                if (! live.muted[Drums])
                {
                    drums.handle (juce::MidiMessage::noteOn (10, e.a, (juce::uint8) juce::jlimit (1, 127, e.b)));
                    flashDrum (e.a);
                    if ((e.a == 36 || e.a == 35) && numKicks < (int) kickOffsets.size()) kickOffsets[(size_t) numKicks++] = 0;
                }
                if (rec && e.c >= 0)
                    pushRecorded ({ Drums, (int) (std::llround (songPos / stepPpq) % juce::jmax (1, live.drumLength)), e.c, e.b, -1 });
                break;

            case BoxEvent::BassOn:
            {
                const bool legato = liveBassHeld > 0;
                ++liveBassHeld;
                cancelOffs (Bass, e.a + 12 * live.bassOctave);
                if (! live.muted[Bass]) bassOn (e.a + 12 * live.bassOctave, e.b, e.b >= 110, legato);
                if (rec)
                {
                    const int step = (int) (std::llround (songPos / stepPpq) % juce::jmax (1, live.bassLength));
                    pushRecorded ({ Bass, step, e.a, e.b, legato ? lastRecordedBassStep : -1 });
                    lastRecordedBassStep = step;
                }
                lastLiveBass = e.a;
                break;
            }
            case BoxEvent::BassOff:
                liveBassHeld = juce::jmax (0, liveBassHeld - 1);
                bassOff (e.a + 12 * live.bassOctave);
                break;

            case BoxEvent::ArpOn:
                if (e.c > 0 && numDelayed < (int) delayedOns.size())   // strummed: a little later
                    delayedOns[(size_t) numDelayed++] = { e.a, e.b, clock + (double) e.c * 0.001 * live.bpm / 60.0 };
                else arpKeyOn (e.a, e.b);
                break;
            case BoxEvent::ArpOff:
                for (int i = 0; i < numDelayed;)   // released before it was strummed: it never starts
                {
                    if (delayedOns[(size_t) i].note == e.a) delayedOns[(size_t) i] = delayedOns[(size_t) --numDelayed];
                    else ++i;
                }
                arp.noteOff (e.a);
                if (! live.arpOn) arpNoteOff (e.a);
                break;
            case BoxEvent::PerfOn:   perfHeld.fetch_or (1u << juce::jlimit (0, 31, e.a)); break;
            case BoxEvent::PerfOff:  perfHeld.fetch_and (~(1u << juce::jlimit (0, 31, e.a))); break;
            case BoxEvent::ArpLatch: arp.setLatch (e.a != 0); break;
            case BoxEvent::ArpClear: arp.clear(); break;
            case BoxEvent::AllOff:   allNotesOff(); arp.clear(); numDelayed = 0; orbit.reset(); orbitKeyDown.fill (false); numOrbitNotes = 0; break;
            case BoxEvent::OrbitOn:
                orbitKeyDown[(size_t) juce::jlimit (0, 127, e.a)] = true;
                orbit.addNote (juce::jlimit (0, 127, e.a), 1, live.orbitGlobal, live.orbitPlanets, rnd);
                break;
            case BoxEvent::OrbitOff:
                orbitKeyDown[(size_t) juce::jlimit (0, 127, e.a)] = false;
                if (! orbitLatch) orbit.removeNote (e.a, 1);
                break;
            case BoxEvent::OrbitLatch:
                orbitLatch = e.a != 0;
                if (! orbitLatch)   // Hold off: the planets whose keys are up leave
                    for (auto& b : orbit.bodies)
                        if (b.active && ! orbitKeyDown[(size_t) juce::jlimit (0, 127, b.note)]) orbit.removeNote (b.note, b.channel);
                break;
            case BoxEvent::OrbitClear: orbit.reset(); orbitKeyDown.fill (false); break;
        }
    };
    for (int i = 0; i < scope.blockSize1; ++i) handle (events[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) handle (events[(size_t) (scope.startIndex2 + i)]);
}

//==============================================================================
// The Orbit page: every held note is a planet; it plays when it crosses one of the lines
void BoxAudio::stepOrbit (double from, double to)
{
    if (orbit.numActive() == 0) return;
    const double grid = orbitcat::quantizeToPpq (live.orbitGrid);
    const double secondsToPpq = live.bpm / 60.0;
    orbit.step (to - from, live.orbitGlobal, live.orbitPlanets, [&] (int i, double frac, double speedRatio)
    {
        const auto& b = orbit.bodies[(size_t) i];
        const auto& p = live.orbitPlanets[(size_t) b.slot];
        double t = from + frac * (to - from);
        if (grid > 0.0)   // on the grid: the next grid point (from the start of the song while playing)
        {
            const double origin = running ? songStart : 0.0;
            t = origin + std::ceil ((t - origin) / grid - 1.0e-9) * grid;
        }
        const float fast = juce::jlimit (0.0f, 1.0f, (float) (speedRatio - 1.0) * 0.5f);
        const float level = juce::jlimit (0.1f, 1.0f, 0.45f + 0.35f * p.mass / 100.0f + 0.3f * p.boost * fast);
        const int vel = juce::jlimit (1, 127, juce::roundToInt (127.0f * level * (0.35f + 0.65f * live.orbitDynamics)));
        const int note = juce::jlimit (0, 127, b.note + 12 * p.octave);
        const double len = juce::jmax (0.03, (double) p.gate * (double) live.orbitGate * secondsToPpq);
        if (numOrbitNotes < (int) orbitNotes.size()) orbitNotes[(size_t) numOrbitNotes++] = { note, vel, t, t + len };
        planetFlash.fetch_or (1u << i);
        planetFlashLink.fetch_or (1u << i);
    });
}

void BoxAudio::playOrbitNotes (double upTo)
{
    for (int i = 0; i < numOrbitNotes;)
    {
        if (orbitNotes[(size_t) i].ppq > upTo) { ++i; continue; }
        const auto n = orbitNotes[(size_t) i];
        orbitNotes[(size_t) i] = orbitNotes[(size_t) --numOrbitNotes];
        const int inst = live.orbitTarget == Bass ? Bass : Arp;
        if (live.muted[(size_t) inst]) continue;
        for (int k = 0; k < numOffs;)   // the same pitch still ringing is released first
        {
            if (offs[(size_t) k].instrument == inst && offs[(size_t) k].note == n.note)
            {
                offs[(size_t) k] = offs[(size_t) --numOffs];
                inst == Arp ? arpNoteOff (n.note) : bassOff (n.note);
            }
            else ++k;
        }
        if (inst == Arp) arpNoteOn (n.note, n.velocity);
        else bassOn (n.note, n.velocity, n.velocity >= 115, false);
        scheduleOff (inst, n.note, n.offPpq);
        orbitHits.fetch_add (1);
    }
}

void BoxAudio::arpKeyOn (int note, int velocity)
{
    if (live.arpOn) arp.noteOn (note, velocity);
    else if (! live.muted[Arp]) arpNoteOn (note, velocity);
}

void BoxAudio::playDelayed (double upTo)
{
    for (int i = 0; i < numDelayed;)
    {
        if (delayedOns[(size_t) i].ppq <= upTo)
        {
            const auto d = delayedOns[(size_t) i];
            delayedOns[(size_t) i] = delayedOns[(size_t) --numDelayed];
            arpKeyOn (d.note, d.velocity);
        }
        else ++i;
    }
}

//==============================================================================
void BoxAudio::playDrumStep (int64_t k, double from, double to)
{
    const int len = juce::jmax (1, live.drumLength);
    const int idx = (int) (k % len);
    const double t0 = (double) k * stepPpq + ((k & 1) != 0 ? live.drumSwing * 0.125 : 0.0);
    for (int p = 0; p < numPads; ++p)
    {
        const int v = live.drumSteps[(size_t) p][(size_t) idx];
        if (v <= 0) continue;
        const double t = t0 + live.drumHuman * 0.03 * hashNoise (k, p);
        if (t < from || t >= to) continue;
        if (live.drumChance[(size_t) p] < 0.999f && rnd.nextFloat() >= live.drumChance[(size_t) p]) continue;
        const int vel = juce::roundToInt ((float) v * live.drumLevel[(size_t) p]);
        if (vel <= 0) continue;
        drums.handle (juce::MidiMessage::noteOn (10, drumPadNote (p), (juce::uint8) juce::jlimit (1, 127, vel)));
        flashDrum (drumPadNote (p));
        if ((p == 0 || p == 13) && numKicks < (int) kickOffsets.size()) kickOffsets[(size_t) numKicks++] = subOffset;
    }
}

void BoxAudio::playBassStep (int64_t k, double from, double to)
{
    const int len = juce::jmax (1, live.bassLength);
    const int idx = (int) (k % len);
    const double t = (double) k * stepPpq + ((k & 1) != 0 ? live.bassSwing * 0.125 : 0.0);
    if (t < from || t >= to) return;
    const auto& s = live.bassSteps[(size_t) idx];
    if (s.note < 0) return;

    const auto& prev = live.bassSteps[(size_t) ((idx + len - 1) % len)];
    const auto& next = live.bassSteps[(size_t) ((idx + 1) % len)];
    const int note = juce::jlimit (0, 127, s.note + 12 * live.bassOctave + followOffset());
    const bool slideIn = prev.note >= 0 && prev.slide && len > 1;
    cancelOffs (Bass, note);
    bassOn (note, s.accent ? 120 : 96, s.accent, slideIn);
    // a slide holds the note into the next step (that note then glides in without restarting)
    const double off = (s.slide && next.note >= 0) ? t + stepPpq + 0.02 : t + stepPpq * 0.55;
    scheduleOff (Bass, note, off);
}

void BoxAudio::tick (double from, double to)
{
    if (running)
    {
        const double a = from - songStart, b = to - songStart;
        const int64_t first = (int64_t) std::floor ((a - stepPpq) / stepPpq);
        const int64_t last = (int64_t) std::floor (b / stepPpq);
        for (int64_t k = juce::jmax ((int64_t) 0, first); k <= last; ++k)
        {
            if (! live.muted[Drums]) playDrumStep (k, a, b);
            if (! live.muted[Bass]) playBassStep (k, a, b);
        }
        const int64_t now = (int64_t) std::floor (a / stepPpq);
        drumStep = (int) (now % juce::jmax (1, live.drumLength));
        bassStep = (int) (now % juce::jmax (1, live.bassLength));
    }

    // the arp runs whenever notes are held (with Play it locks to the grid)
    if (arp.hasNotes())
    {
        if (! arp.isRunning())
        {
            arp.setRunning (true);
            double start = from;
            bool now = false;
            if (running)
            {
                const double grid = arpDivisionPpq (live.arp.division);
                start = songStart + std::floor ((from - songStart) / grid + 1.0e-9) * grid;
                now = start < from - 1.0e-9;
            }
            arp.restartAt (start, now);
        }
    }
    else if (arp.isRunning())
    {
        arp.setRunning (false);
    }

    const bool arpMuted = live.muted[Arp];
    arp.process (from, to, live.arp, [this, arpMuted] (int note, int vel, double, double off)
    {
        if (arpMuted) return;
        for (int i = 0; i < numOffs;)   // the same pitch still ringing is released first
        {
            if (offs[(size_t) i].instrument == Arp && offs[(size_t) i].note == note)
            {
                offs[(size_t) i] = offs[(size_t) --numOffs];
                arpNoteOff (note);
            }
            else ++i;
        }
        arpNoteOn (note, vel);
        scheduleOff (Arp, note, off);
    });
}

void BoxAudio::render (float* left, float* right, int numSamples)
{
    if (mirror.load())   // linked to a box: the box plays
    {
        juce::FloatVectorOperations::clear (left, numSamples);
        juce::FloatVectorOperations::clear (right, numSamples);
        return;
    }
    for (int pos = 0; pos < numSamples; pos += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - pos);
        float* outL = left + pos;
        float* outR = right + pos;

        {
            const juce::SpinLock::ScopedTryLockType lock (snapshotLock);
            // a scene waits for the start of the next bar while playing
            const bool barNow = ! running || snapshotAtBar == false
                                || std::fmod (clock - songStart, 4.0) + (live.bpm / 60.0 / sr) * n >= 4.0
                                || std::fmod (clock - songStart, 4.0) < 1.0e-6;
            if (lock.isLocked() && snapshotFresh && barNow)
            {
                const int oldSound = live.bassSound, oldArpSound = live.arpSound;
                const bool oldArpOn = live.arpOn;
                std::swap (live, pending);
                snapshotFresh = false;
                snapshotAtBar = false;
                sceneWaiting = false;
                drums.setSound (live.drumKit);
                if (live.arpSound < arpSoundSupersaw) keys.setSound (live.arpSound);
                if (live.bassSound > 0 && ! bassIsGlide()) bassSynth.setSound (live.bassSound - 1);
                arpGlide.setSettings (live.arpGlide);
                bassGlide.setSettings (live.bassGlide);
                acid.setParams (live.acid);
                AcidSynth::Params ap = live.acid;   // the arp's acid: brighter, shorter
                ap.cutoff = 0.5f; ap.envMod = 0.5f; ap.decay = 0.3f; ap.resonance = 0.55f;
                arpAcid.setParams (ap);
                if (oldSound != live.bassSound) { acid.allOff(); bassSynth.allOff(); bassGlide.allNotesOff(); }
                if (oldArpSound != live.arpSound || oldArpOn != live.arpOn) arpAllOff();
            }
        }

        handleEvents();

        if (startRequest.exchange (false))
        {
            songStart = clock;
            running = true;
            if (arp.isRunning()) arp.restartAt (clock);
        }
        if (stopRequest.exchange (false))
        {
            running = false;
            for (int i = 0; i < numOffs;)
            {
                if (offs[(size_t) i].instrument == Bass) { const int note = offs[(size_t) i].note; offs[(size_t) i] = offs[(size_t) --numOffs]; bassOff (note); }
                else ++i;
            }
            drumStep = -1;
            bassStep = -1;
        }

        const double ppqPerSample = live.bpm / 60.0 / sr;
        float* dl = bufL.data();                float* dr = bufR.data();
        float* al = bufL.data() + maxChunk;     float* ar = bufR.data() + maxChunk;
        float* bl = bufL.data() + 2 * maxChunk; float* br = bufR.data() + 2 * maxChunk;
        for (auto* p : { dl, dr, al, ar, bl, br }) juce::FloatVectorOperations::clear (p, n);

        const double songPpqStart = running ? clock - songStart : clock;
        for (int s = 0; s < n; s += subBlock)
        {
            const int m = juce::jmin (subBlock, n - s);
            const double from = clock, to = clock + m * ppqPerSample;
            subOffset = s;
            if (numDelayed > 0) playDelayed (to);
            stepOrbit (from, to);
            if (numOrbitNotes > 0) playOrbitNotes (to);
            releaseDue (from);
            tick (from, to);
            drums.render (dl + s, dr + s, m);
            keys.render (al + s, ar + s, m);
            supersaw.render (al + s, ar + s, m);
            arpAcid.render (al + s, ar + s, m);
            if (arpIsGlide()) arpGlide.render (al + s, ar + s, m);
            if (bassIsGlide()) bassGlide.render (bl + s, br + s, m);
            else if (live.bassSound == 0) acid.render (bl + s, br + s, m);
            else bassSynth.render (bl + s, br + s, m);
            clock = to;
        }

        // the Glide engine's own effects and volume
        auto glideFx = [this, n] (glidecat::Effects& e, const glidecat::FxSettings& fs, float vol, float* l, float* r)
        {
            auto f = fs;
            f.bpm = live.bpm;
            e.process (l, r, n, f);
            juce::FloatVectorOperations::multiply (l, vol, n);
            juce::FloatVectorOperations::multiply (r, vol, n);
        };
        if (arpIsGlide()) glideFx (arpGlideFx, live.arpGlideFx, live.arpGlide.volume, al, ar);
        if (bassIsGlide()) glideFx (bassGlideFx, live.bassGlideFx, live.bassGlide.volume, bl, br);

        float* il[NumInstruments] = { dl, al, bl };
        float* ir[NumInstruments] = { dr, ar, br };
        for (int i = 0; i < NumInstruments; ++i)
        {
            auto settings = live.fx[(size_t) i];
            settings.bpm = live.bpm;
            fx[(size_t) i].process (il[i], ir[i], n, settings);
            const float g = live.muted[(size_t) i] ? 0.0f : live.volume[(size_t) i] * live.masterGain;
            juce::FloatVectorOperations::multiply (il[i], g, n);
            juce::FloatVectorOperations::multiply (ir[i], g, n);
        }

        // the FX page: reverb sends, pump, echo, glue, tape, vinyl, crush, DJ filter, width, performance pads
        auto ms = live.master;
        ms.bpm = live.bpm;
        master.process (il, ir, n, mixL.data(), mixR.data(), ms, perfHeld.load(), songPpqStart, ppqPerSample,
                        kickOffsets.data(), numKicks);
        numKicks = 0;
        pumpNow = master.pumpLevel();
        juce::FloatVectorOperations::add (outL, mixL.data(), n);
        juce::FloatVectorOperations::add (outR, mixR.data(), n);

        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            limiter.processSample (outL[i], outR[i]);
            pk = juce::jmax (pk, std::abs (outL[i]), std::abs (outR[i]));
            if (++spectrumCount >= 2)
            {
                spectrumCount = 0;
                const int w = spectrumWrite.load (std::memory_order_relaxed);
                spectrumRing[(size_t) w].store (0.5f * (outL[i] + outR[i]), std::memory_order_relaxed);
                spectrumWrite.store ((w + 1) % spectrumSize, std::memory_order_relaxed);
            }
            if (++scopeCount >= 64)   // a coarse wave picture for the screen
            {
                scopeCount = 0;
                scope[(size_t) scopeWrite].store (0.5f * (outL[i] + outR[i]), std::memory_order_relaxed);
                scopeWrite = (scopeWrite + 1) % (int) scope.size();
            }
        }
        peak = juce::jmax (pk, peak.load() * 0.9f);
    }

    arpStep = arp.isRunning() ? arp.currentStep() : -1;
    uint64_t lo = 0, hi = 0;
    arp.activeNotes (lo, hi);
    arpNotesLo = lo;
    arpNotesHi = hi;
    arpLatched = arp.getLatch();
    orbitLatched = orbitLatch;
    for (int i = 0; i < orbitcat::maxBodies; ++i)
    {
        const auto& b = orbit.bodies[(size_t) i];
        auto& v = planetView[(size_t) i];
        v.active = b.active;
        if (! b.active) continue;
        v.theta = (float) b.theta;
        v.radius = (float) b.radiusNow;
        v.mass = live.orbitPlanets[(size_t) b.slot].mass;
        v.note = b.note;
    }
}

} // namespace catbox
