#include "BoxAudio.h"
#include <cmath>

namespace catbox
{
namespace
{
constexpr int maxChunk = 2048;   // render() splits longer blocks
constexpr int subBlock = 32;     // events land on 32-sample boundaries (0.7 ms at 48 kHz)
}

BoxAudio::BoxAudio()
{
    bufL.resize ((size_t) maxChunk * NumInstruments);
    bufR.resize ((size_t) maxChunk * NumInstruments);
}

void BoxAudio::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    drums.prepare (sr);
    keys.prepare (sr);
    bass.prepare (sr);
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
    if (r != nullptr)
    {
        render (l, r, numSamples);
        return;
    }
    // mono output: render stereo in chunks and fold down
    float tmpR[256];
    for (int pos = 0; pos < numSamples; pos += 256)
    {
        const int n = juce::jmin (256, numSamples - pos);
        juce::FloatVectorOperations::clear (tmpR, n);
        render (l + pos, tmpR, n);
        for (int i = 0; i < n; ++i) l[pos + i] = 0.5f * (l[pos + i] + tmpR[i]);
    }
}

//==============================================================================
void BoxAudio::setSnapshot (const AudioSnapshot& s)
{
    const juce::SpinLock::ScopedLockType lock (snapshotLock);
    pending = s;
    snapshotFresh = true;
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

bool BoxAudio::popRecordedHit (DrumUserHit& out)
{
    const auto scope = recordFifo.read (1);
    if (scope.blockSize1 > 0) { out = recorded[(size_t) scope.startIndex1]; return true; }
    if (scope.blockSize2 > 0) { out = recorded[(size_t) scope.startIndex2]; return true; }
    return false;
}

//==============================================================================
void BoxAudio::flashDrum (int note)
{
    for (int p = 0; p < numPads; ++p)
        if (drumPadNote (p) == note) { drumFlash.fetch_or (1u << p); return; }
    // generated hits use a few other GM notes: show them on the closest pad
    const int fallback = note == 44 ? 4 : (note == 43 || note == 47 || note == 50) ? 9 : (note == 55 || note == 52) ? 7 : -1;
    if (fallback >= 0) drumFlash.fetch_or (1u << fallback);
}

void BoxAudio::scheduleOff (int instrument, int note, double offPpq)
{
    if (numOffs < (int) offs.size()) offs[(size_t) numOffs++] = { instrument, note, offPpq };
}

void BoxAudio::releaseDue (double upTo)
{
    for (int i = 0; i < numOffs;)
    {
        if (offs[(size_t) i].ppq <= upTo)
        {
            const auto o = offs[(size_t) i];
            const auto msg = juce::MidiMessage::noteOff (1, o.note);
            if (o.instrument == Arp) keys.handle (msg);
            else if (o.instrument == Bass) bass.handle (msg);
            offs[(size_t) i] = offs[(size_t) --numOffs];
        }
        else ++i;
    }
}

void BoxAudio::allNotesOff()
{
    keys.allOff();
    bass.allOff();
    numOffs = 0;
}

void BoxAudio::handleEvents()
{
    const int ready = eventFifo.getNumReady();
    if (ready == 0) return;
    const auto scope = eventFifo.read (ready);
    auto handle = [this] (const BoxEvent& e)
    {
        switch (e.type)
        {
            case BoxEvent::DrumHit:
            {
                if (! live.muted[Drums])
                {
                    drums.handle (juce::MidiMessage::noteOn (10, e.a, (juce::uint8) juce::jlimit (1, 127, e.b)));
                    flashDrum (e.a);
                }
                if (recording && running)
                {
                    const int steps = juce::jmax (1, juce::roundToInt (live.drumLoop * 4.0));
                    const double pos = std::fmod (clock - songStart, live.drumLoop);
                    const DrumUserHit h { juce::roundToInt (pos * 4.0) % steps, e.a, e.b };
                    const auto w = recordFifo.write (1);
                    if (w.blockSize1 > 0) recorded[(size_t) w.startIndex1] = h;
                    else if (w.blockSize2 > 0) recorded[(size_t) w.startIndex2] = h;
                }
                break;
            }
            case BoxEvent::ArpOn:    arp.noteOn (e.a, e.b); break;
            case BoxEvent::ArpOff:   arp.noteOff (e.a); break;
            case BoxEvent::ArpLatch: arp.setLatch (e.a != 0); break;
            case BoxEvent::ArpClear: arp.clear(); break;
            case BoxEvent::BassOn:
                if (! live.muted[Bass]) bass.handle (juce::MidiMessage::noteOn (1, e.a, (juce::uint8) juce::jlimit (1, 127, e.b)));
                break;
            case BoxEvent::BassOff:  bass.handle (juce::MidiMessage::noteOff (1, e.a)); break;
            case BoxEvent::BassChord:
                chord.valid = true;
                chord.rootPc = e.a;
                chord.third = e.b;
                chord.fifth = e.c;
                chord.seventh = e.d;
                chord.hasThird = e.e != 0;
                break;
            case BoxEvent::AllOff:   allNotesOff(); arp.clear(); break;
        }
    };
    for (int i = 0; i < scope.blockSize1; ++i) handle (events[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) handle (events[(size_t) (scope.startIndex2 + i)]);
}

//==============================================================================
void BoxAudio::playDrums (double from, double to, double songFrom)
{
    const double loop = juce::jmax (0.25, live.drumLoop);
    const double a = std::fmod (songFrom, loop);
    const double span = to - from;
    drumStep = (int) (a / 0.25);
    if (live.muted[Drums]) return;
    for (const auto& h : live.drumHits)
    {
        const bool now = (h.ppq >= a && h.ppq < a + span) || (a + span > loop && h.ppq + loop < a + span);
        if (! now) continue;
        drums.handle (juce::MidiMessage::noteOn (10, h.note, (juce::uint8) juce::jlimit (1, 127, h.velocity)));
        flashDrum (h.note);
    }
}

void BoxAudio::playBass (double from, double to, double songFrom)
{
    if (live.muted[Bass] || live.bassNotes.empty()) return;
    const double loop = juce::jmax (0.25, live.bassLoop);
    const double a = std::fmod (songFrom, loop);
    const double span = to - from;
    for (const auto& n : live.bassNotes)
    {
        double p = std::fmod (n.pos + n.micro + loop, loop);
        double offset = -1.0;
        if (p >= a && p < a + span) offset = p - a;
        else if (a + span > loop && p + loop < a + span) offset = p + loop - a;
        if (offset < 0.0) continue;

        basscat::Chord c = chord;
        if (! c.valid) { c.valid = true; c.rootPc = 0; c.third = 4; c.fifth = 7; c.hasThird = true; }
        const int note = basscat::resolveDegree (n.degree, n.octave, c, live.bassLowNote, bassPrev);
        if (note < 0) continue;
        bassPrev = note;
        // a note already sounding at this pitch is released first, so its own note-off cannot cut this one
        for (int i = 0; i < numOffs; ++i)
            if (offs[(size_t) i].instrument == Bass && offs[(size_t) i].note == note) offs[(size_t) i].ppq = -1.0;
        releaseDue (-0.5);
        const int vel = n.degree == basscat::Ghost ? juce::jmin (n.velocity, 60) : n.velocity;
        bass.handle (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, vel)));
        scheduleOff (Bass, note, from + offset + juce::jmax (0.05, n.len) + (n.slide ? 0.08 : 0.0));
    }
}

void BoxAudio::tick (double from, double to)
{
    if (running)
    {
        playDrums (from, to, from - songStart);
        playBass (from, to, from - songStart);
    }

    // the arp runs whenever notes are held (with Play it locks to the bar)
    if (arp.hasNotes())
    {
        if (! arp.isRunning())
        {
            arp.setRunning (true);
            double start = from;
            if (running)
            {
                const double grid = arpDivisionPpq (live.arp.division);
                start = songStart + std::ceil ((from - songStart) / grid - 1.0e-9) * grid;
            }
            arp.restartAt (start);
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
        for (int i = 0; i < numOffs; ++i)
            if (offs[(size_t) i].instrument == Arp && offs[(size_t) i].note == note) offs[(size_t) i].ppq = -1.0;
        releaseDue (-0.5);
        keys.handle (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel));
        scheduleOff (Arp, note, off);
    });
}

void BoxAudio::render (float* left, float* right, int numSamples)
{
    for (int pos = 0; pos < numSamples; pos += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - pos);
        float* outL = left + pos;
        float* outR = right + pos;

        {
            const juce::SpinLock::ScopedTryLockType lock (snapshotLock);
            if (lock.isLocked() && snapshotFresh)
            {
                std::swap (live, pending);
                snapshotFresh = false;
                drums.setSound (live.drumKit);
                keys.setSound (live.arpSound);
                bass.setSound (live.bassSound);
            }
        }

        handleEvents();

        if (startRequest.exchange (false))
        {
            songStart = clock;
            running = true;
            bassPrev = -1;
            if (arp.isRunning()) arp.restartAt (clock);
        }
        if (stopRequest.exchange (false))
        {
            running = false;
            for (int i = 0; i < numOffs; ++i) if (offs[(size_t) i].instrument == Bass) offs[(size_t) i].ppq = -1.0;
            releaseDue (-0.5);
            bass.allOff();
            drumStep = -1;
        }

        const double ppqPerSample = live.bpm / 60.0 / sr;
        float* dl = bufL.data();               float* dr = bufR.data();
        float* al = bufL.data() + maxChunk;    float* ar = bufR.data() + maxChunk;
        float* bl = bufL.data() + 2 * maxChunk; float* br = bufR.data() + 2 * maxChunk;
        for (auto* p : { dl, dr, al, ar, bl, br }) juce::FloatVectorOperations::clear (p, n);

        for (int s = 0; s < n; s += subBlock)
        {
            const int m = juce::jmin (subBlock, n - s);
            const double from = clock, to = clock + m * ppqPerSample;
            releaseDue (from);
            tick (from, to);
            drums.render (dl + s, dr + s, m);
            keys.render (al + s, ar + s, m);
            bass.render (bl + s, br + s, m);
            clock = to;
        }

        float* il[NumInstruments] = { dl, al, bl };
        float* ir[NumInstruments] = { dr, ar, br };
        for (int i = 0; i < NumInstruments; ++i)
        {
            auto settings = live.fx[(size_t) i];
            settings.bpm = live.bpm;
            fx[(size_t) i].process (il[i], ir[i], n, settings);
            const float g = live.muted[(size_t) i] ? 0.0f : live.volume[(size_t) i] * live.master;
            juce::FloatVectorOperations::addWithMultiply (outL, il[i], g, n);
            juce::FloatVectorOperations::addWithMultiply (outR, ir[i], g, n);
        }

        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            limiter.processSample (outL[i], outR[i]);
            pk = juce::jmax (pk, std::abs (outL[i]), std::abs (outR[i]));
        }
        peak = juce::jmax (pk, peak.load() * 0.9f);
    }

    arpStep = arp.isRunning() ? arp.currentStep() : -1;
    uint64_t lo = 0, hi = 0;
    arp.activeNotes (lo, hi);
    arpNotesLo = lo;
    arpNotesHi = hi;
    arpLatched = arp.getLatch();
    bassChordRoot = chord.valid ? chord.rootPc : -1;
}

} // namespace catbox
