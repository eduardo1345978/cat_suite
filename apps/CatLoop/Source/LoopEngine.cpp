#include "LoopEngine.h"
#include <algorithm>
#include <cmath>

using namespace juce;

namespace looper
{
namespace
{
enum Action { actNone = 0, actStartRec = 1, actClose = 2, actStartPlay = 3 };

inline int wrapIndex (juce::int64 x, int len) noexcept
{
    auto r = (int) (x % len);
    return r < 0 ? r + len : r;
}
}

String stateName (State s)
{
    switch (s)
    {
        case State::empty:       return "EMPTY";
        case State::armed:       return "WAIT";
        case State::recording:   return "REC";
        case State::playing:     return "PLAY";
        case State::overdubbing: return "DUB";
        case State::stopped:     return "STOP";
    }
    return {};
}

StringArray quantizeNames() { return { "Bar", "Beat", "Off (free)" }; }
StringArray clickNames()    { return { "Off", "Count-in", "Always" }; }
StringArray inputNames()    { return { "Stereo (1 + 2)", "Input 1 (mono)", "Input 2 (mono)" }; }
StringArray beatNames()     { return drumcat::getStyleNames(); }
StringArray kitNames()      { return hub::DrumKit::soundNames(); }

//==============================================================================
Engine::Engine()
{
    setBeatPattern (0, 1);
}

void Engine::prepare (double sampleRate, int maxBlock, double maxSeconds)
{
    const double newSr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    const int newCapacity = (int) (maxSeconds * newSr) + (int) (0.6 * newSr) + 1024;
    scratchL.assign ((size_t) jmax (maxBlock, 4096), 0.0f);
    scratchR.assign ((size_t) jmax (maxBlock, 4096), 0.0f);
    drums.prepare (newSr);
    drumKitNow = -1;

    if (std::abs (newSr - sr) > 0.5 || newCapacity != capacity)
    {
        // a new sample rate: the loops can't stay (they would play at the wrong speed)
        sr = newSr;
        capacity = newCapacity;
        for (auto& t : tracks)
        {
            for (auto& layer : t.data) for (auto& ch : layer) ch.assign ((size_t) capacity, 0.0f);
            t.state = (int) State::empty;
            t.length = 0;
            t.hasUndo = false; t.undone = false;
            t.actionAt = -1; t.tail = false; t.playGain = t.dubGain = 0.0f; t.dubHold = 0; t.stopping = false;
        }
        barLen = sr * 60.0 * beatsPerBar.load() / bpm.load();
        tempoKnown = tempoManual.load() || beatWanted.load();
        running = false;
        transport = 0;
    }
    fade = jmax (16, (int) (0.005 * sr));
    fifo.reset();
    limiter.prepare (sr);
}

bool Engine::anyLoop() const noexcept
{
    for (auto& t : tracks)
        if (t.length.load() > 0 || t.state.load() != (int) State::empty) return true;
    return false;
}

int Engine::getBars (int t) const noexcept
{
    const int len = getLength (t);
    if (len <= 0 || ! tempoKnown.load()) return 0;
    const double bars = len / barLen.load();
    return std::abs (bars - std::round (bars)) < 0.01 ? (int) std::round (bars) : 0;
}

//==== commands ===================================================================================================
void Engine::push (Cmd c, int track)
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0) commands[(size_t) scope.startIndex1] = { c, track };
    else if (scope.blockSize2 > 0) commands[(size_t) scope.startIndex2] = { c, track };
}

void Engine::copyToUndo (int t)
{
    auto& tr = tracks[(size_t) t];
    const int a = tr.active.load(), len = tr.length.load();
    if (len <= 0) return;
    for (int ch = 0; ch < 2; ++ch)
        std::copy (tr.data[a][ch].begin(), tr.data[a][ch].begin() + len, tr.data[1 - a][ch].begin());
    tr.hasUndo = true;
    tr.undone = false;
}

void Engine::press (int t)
{
    if (! isPositiveAndBelow (t, numTracks)) return;
    lastTrack = t;
    auto& tr = tracks[(size_t) t];
    // PLAY -> DUB: keep what is there now for UNDO (the audio thread only reads the loop while it plays)
    if (tr.state.load() == (int) State::playing && ! tr.tail) copyToUndo (t);
    push (Cmd::press, t);
}

void Engine::stop (int t)  { if (isPositiveAndBelow (t, numTracks)) { lastTrack = t; push (Cmd::stop, t); } }
void Engine::clear (int t) { if (isPositiveAndBelow (t, numTracks)) { lastTrack = t; push (Cmd::clear, t); } }

bool Engine::undo (int t)
{
    if (! isPositiveAndBelow (t, numTracks)) return false;
    auto& tr = tracks[(size_t) t];
    const auto st = (State) tr.state.load();
    if (! tr.hasUndo.load() || (st != State::playing && st != State::stopped)) return false;
    lastTrack = t;
    tr.active = 1 - tr.active.load();
    tr.undone = ! tr.undone.load();
    return true;
}

void Engine::playAll()  { push (Cmd::playAll); }
void Engine::stopAll()  { beatWanted = false; push (Cmd::stopAll); }
void Engine::clearAll() { push (Cmd::clearAll); }

void Engine::togglePlayAll()
{
    bool anyPlaying = beatWanted.load();
    for (auto& t : tracks)
    {
        const auto st = (State) t.state.load();
        anyPlaying |= st == State::playing || st == State::overdubbing || st == State::recording || st == State::armed;
    }
    if (anyPlaying) stopAll(); else playAll();
}

void Engine::setBeat (bool on)
{
    if (on && ! tempoKnown.load()) { barLen = sr * 60.0 * beatsPerBar.load() / bpm.load(); tempoKnown = true; }
    beatWanted = on;
}

void Engine::setBeatPattern (int style, juce::int64 seed)
{
    drumcat::GeneratorSettings g;
    g.style = jlimit (0, drumcat::getStyleNames().size() - 1, style);
    g.bars = 2;
    g.density = 0.5f;
    g.fills = 0.35f;
    g.seed = seed;
    auto p = drumcat::generatePattern (g);
    {
        const SpinLock::ScopedLockType sl (patternLock);
        pendingPattern = std::move (p);
        patternPending = true;
    }
    beatStyle = g.style;
    beatSeed = seed;
}

void Engine::newBeat()             { setBeatPattern (beatStyle.load(), beatSeed.load() + 1); }
void Engine::setBeatStyle (int s) { setBeatPattern (s, beatSeed.load()); }

bool Engine::setBpm (double b)
{
    if (anyLoop()) return false;
    b = jlimit (40.0, 240.0, b);
    bpm = b;
    barLen = sr * 60.0 * beatsPerBar.load() / b;
    tempoKnown = true;
    tempoManual = true;
    return true;
}

void Engine::resetTempo()
{
    if (anyLoop()) return;
    tempoManual = false;
    tempoKnown = beatWanted.load();
}

void Engine::tapTempo()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (! taps.isEmpty() && now - taps.getLast() > 2000.0) taps.clear();
    taps.add (now);
    while (taps.size() > 5) taps.remove (0);
    if (taps.size() >= 2) setBpm (60000.0 * (taps.size() - 1) / (taps.getLast() - taps.getFirst()));
}

void Engine::setTempoFromSession (double barLengthSamples, int beats)
{
    beatsPerBar = jlimit (1, 16, beats);
    barLen = barLengthSamples;
    bpm = 60.0 * beatsPerBar.load() * sr / barLengthSamples;
    tempoKnown = true;
}

//==== the audio thread's side of the commands ======================================================================
double Engine::unitLength (bool barOnly) const
{
    return barOnly ? barLen.load() : barLen.load() / jmax (1, beatsPerBar.load());
}

juce::int64 Engine::nextBoundary (juce::int64 from, bool barOnly) const
{
    const double u = unitLength (barOnly);
    const double k = std::ceil ((double) from / u - 1.0e-9);
    return (juce::int64) std::llround (k * u);
}

void Engine::ensureRunning (bool countIn)
{
    if (running) return;
    running = true;
    transport = countIn ? -(juce::int64) std::llround (barLen.load()) : 0;
    if (beatWanted.load()) { beatPlaying = true; beatStart = 0; beatChangeAt = -1; }
}

void Engine::startRecording (Track& tr, juce::int64 at)
{
    tr.startAt = at;
    tr.recPos = 0;
    tr.actionAt = -1;
    tr.tail = false;
    tr.length = 0;
    tr.hasUndo = false; tr.undone = false;
    tr.state = (int) State::recording;
}

void Engine::deriveTempo (int len)
{
    // the first loop sets the tempo: 1, 2, 4, 8 ... bars, whichever lands between 70 and 140 BPM
    const int bpb = jmax (1, beatsPerBar.load());
    int bars = 1;
    double b = 60.0 * bpb * sr / len;
    while (b < 70.0 && bars < 64) { bars *= 2; b = 60.0 * bpb * bars * sr / len; }
    while (b >= 140.0 && bars > 1) { bars /= 2; b = 60.0 * bpb * bars * sr / len; }
    barLen = (double) len / bars;
    bpm = b;
    tempoKnown = true;
}

void Engine::closeLoop (Track& tr, int len)
{
    const int a = tr.active.load(), lat = jlimit (0, (int) (0.5 * sr), latency.load());
    len = jlimit (1, capacity - fade - (int) (0.5 * sr) - 8, len);
    tr.length = len;
    tr.actionAt = -1;
    // what was recorded past the end is the beginning's cross-fade (the sound going on into the loop's start)
    tr.tailIdx = tr.recPos - lat;
    tr.tailEnd = len + fade;
    for (int idx = len; idx < jmin (tr.tailIdx, tr.tailEnd); ++idx)
    {
        const int i = idx - len;
        const float s = std::sin (MathConstants<float>::halfPi * ((float) i + 0.5f) / (float) fade);
        const float h = s * s;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& d = tr.data[a][ch];
            d[(size_t) i] = d[(size_t) i] * h + d[(size_t) idx] * (1.0f - h);
        }
    }
    tr.tail = tr.tailIdx < tr.tailEnd;
    tr.playGain = 0.0f;
    tr.volS = tr.vol.load();
    tr.dubGain = 0.0f;
    tr.dubHold = 0;
    tr.hasUndo = false; tr.undone = false;
    tr.state = (int) (recThenDub.load() ? State::overdubbing : State::playing);
}

void Engine::requestClose (Track& tr)
{
    const int R = tr.recPos;
    if (! tempoKnown.load())
    {
        if (R < fade * 4) { doClear ((int) (&tr - tracks.data())); return; }
        deriveTempo (R);
        closeLoop (tr, R);
        return;
    }
    const auto q = (Quantize) quantize.load();
    if (q == Quantize::off) { closeLoop (tr, R); return; }
    const double unit = unitLength (q == Quantize::bar);
    const double k = jmax (1.0, std::round (R / unit));
    const int L = (int) std::llround (k * unit);
    if (L > R && L < capacity - fade - (int) (0.5 * sr) - 8)
    {
        tr.actionAt = tr.startAt + L;             // record on to the bar line, then close
        tr.action = actClose;
    }
    else closeLoop (tr, jmin (L, R));
}

void Engine::startPlaying (Track& tr)
{
    tr.actionAt = -1;
    tr.volS = tr.vol.load();
    tr.playGain = 0.0f;
    tr.stopping = false;
    tr.state = (int) State::playing;
}

void Engine::doPress (int t)
{
    auto& tr = tracks[(size_t) t];
    const auto st = (State) tr.state.load();
    const auto q = (Quantize) quantize.load();
    switch (st)
    {
        case State::empty:
            if (! running)
            {
                if ((Click) click.load() != Click::off)
                {
                    if (! tempoKnown.load()) { barLen = sr * 60.0 * beatsPerBar.load() / bpm.load(); tempoKnown = true; }
                    ensureRunning (true);                  // one bar of clicks, then REC
                    tr.state = (int) State::armed;
                    tr.actionAt = 0;
                    tr.action = actStartRec;
                }
                else
                {
                    ensureRunning (false);
                    startRecording (tr, transport);
                }
            }
            else if (q == Quantize::off || ! tempoKnown.load())
            {
                startRecording (tr, transport);
            }
            else
            {
                const auto at = nextBoundary (transport, q == Quantize::bar);
                if (at <= transport) startRecording (tr, transport);
                else { tr.state = (int) State::armed; tr.actionAt = at; tr.action = actStartRec; }
            }
            break;

        case State::armed:
            tr.actionAt = -1;
            tr.state = (int) State::empty;
            break;

        case State::recording:
            if (tr.actionAt < 0) requestClose (tr);
            break;

        case State::playing:
            tr.dubHold = 0;
            tr.state = (int) State::overdubbing;
            break;

        case State::overdubbing:
            tr.dubHold = jlimit (0, (int) (0.5 * sr), latency.load());
            tr.state = (int) State::playing;
            break;

        case State::stopped:
            if (tr.actionAt >= 0) { tr.actionAt = -1; break; }                       // a second press: never mind
            if (running && q != Quantize::off && tempoKnown.load())
            {
                tr.actionAt = nextBoundary (transport, true);
                tr.action = actStartPlay;
                if (tr.actionAt <= transport) { tr.startAt = transport; startPlaying (tr); }
            }
            else
            {
                const bool wasRunning = running;
                ensureRunning (false);
                tr.startAt = wasRunning ? transport : 0;
                startPlaying (tr);
            }
            break;
    }
}

void Engine::doStop (int t)
{
    auto& tr = tracks[(size_t) t];
    switch ((State) tr.state.load())
    {
        case State::armed:
            tr.actionAt = -1;
            tr.state = (int) State::empty;
            break;
        case State::recording:
        {
            const int R = tr.recPos;
            if (! tempoKnown.load())
            {
                if (R < fade * 4) { doClear (t); return; }
                deriveTempo (R);
                closeLoop (tr, R);
            }
            else if ((Quantize) quantize.load() == Quantize::off) closeLoop (tr, R);
            else
            {
                const double unit = unitLength ((Quantize) quantize.load() == Quantize::bar);
                const double k = std::floor (R / unit);
                if (k < 1.0) { doClear (t); return; }
                closeLoop (tr, (int) std::llround (k * unit));
            }
            tr.state = (int) State::stopped;
            break;
        }
        case State::overdubbing:
            tr.dubHold = jlimit (0, (int) (0.5 * sr), latency.load());
            [[fallthrough]];
        case State::playing:
            tr.actionAt = -1;
            tr.state = (int) State::stopped;
            break;
        case State::stopped:
            tr.actionAt = -1;
            break;
        case State::empty:
            break;
    }
}

void Engine::doClear (int t)
{
    auto& tr = tracks[(size_t) t];
    tr.state = (int) State::empty;
    tr.length = 0;
    tr.actionAt = -1;
    tr.tail = false;
    tr.playGain = tr.dubGain = 0.0f;
    tr.dubHold = 0;
    tr.hasUndo = false;
    tr.undone = false;
    tr.pos = 0.0f;
    tr.level = 0.0f;
}

void Engine::apply (const Command& c)
{
    switch (c.cmd)
    {
        case Cmd::press: doPress (c.track); break;
        case Cmd::stop:  doStop (c.track); break;
        case Cmd::clear: doClear (c.track); break;
        case Cmd::playAll:
        {
            const bool wasRunning = running;
            const bool quantized = wasRunning && tempoKnown.load() && (Quantize) quantize.load() != Quantize::off;
            const auto at = quantized ? nextBoundary (transport, true) : transport;
            for (auto& tr : tracks)
            {
                if ((State) tr.state.load() != State::stopped || tr.length.load() <= 0) continue;
                if (quantized && at > transport) { tr.actionAt = at; tr.action = actStartPlay; }
                else
                {
                    ensureRunning (false);
                    tr.startAt = wasRunning ? transport : 0;
                    startPlaying (tr);
                }
            }
            break;
        }
        case Cmd::stopAll:
            for (int t = 0; t < numTracks; ++t) doStop (t);
            break;
        case Cmd::clearAll:
            for (int t = 0; t < numTracks; ++t) doClear (t);
            tempoKnown = tempoManual.load() || beatWanted.load();
            break;
    }
}

void Engine::loadPendingPattern()
{
    const SpinLock::ScopedTryLockType sl (patternLock);
    if (sl.isLocked() && patternPending)
    {
        std::swap (pattern, pendingPattern);
        patternPending = false;
    }
}

//==== audio ======================================================================================================
void Engine::tailSample (Track& tr, float l, float r)
{
    const int idx = tr.tailIdx++;
    const int len = tr.length.load();
    const int a = tr.active.load();
    if (idx >= 0 && idx < len)
    {
        tr.data[a][0][(size_t) idx] = l;
        tr.data[a][1][(size_t) idx] = r;
    }
    else if (idx >= len && idx < len + fade)
    {
        const int i = idx - len;
        const float s = std::sin (MathConstants<float>::halfPi * ((float) i + 0.5f) / (float) fade);
        const float h = s * s;
        auto& d0 = tr.data[a][0];
        auto& d1 = tr.data[a][1];
        d0[(size_t) i] = d0[(size_t) i] * h + l * (1.0f - h);
        d1[(size_t) i] = d1[(size_t) i] * h + r * (1.0f - h);
    }
    if (tr.tailIdx >= tr.tailEnd) tr.tail = false;
}

void Engine::renderBeatAndClick (float* l, float* r, int n)
{
    if (drumKitNow != kit.load()) { drumKitNow = kit.load(); drums.setSound (drumKitNow); }

    struct Ev { int offset; int note; float vel; bool click; bool accent; };
    std::array<Ev, 96> ev {};
    int numEv = 0;
    auto add = [&] (Ev e) { if (numEv < (int) ev.size()) ev[(size_t) numEv++] = e; };

    const int bpb = jmax (1, beatsPerBar.load());
    const double spq = barLen.load() / bpb;
    const juce::int64 T0 = transport;

    if (running)
    {
        // a beat change (on / off) inside this block happens at its sample
        int changeAt = -1;
        if (beatChangeAt >= 0 && beatChangeAt < T0 + n) changeAt = (int) jmax ((juce::int64) 0, beatChangeAt - T0);

        const double P = jmax (1.0, pattern.lengthPpq());
        auto hitsIn = [&] (int from, int to)
        {
            if (from >= to) return;
            const double q0 = (double) (T0 + from - beatStart) / spq, q1 = (double) (T0 + to - beatStart) / spq;
            if (q1 <= 0.0) return;
            for (const auto& h : pattern.hits)
            {
                double hq = std::fmod (h.pos + h.micro, P);
                if (hq < 0.0) hq += P;
                for (double t = std::ceil ((jmax (0.0, q0) - hq) / P) * P + hq; t < q1; t += P)
                {
                    if (t < jmax (0.0, q0)) continue;
                    const int off = (int) std::floor (beatStart + t * spq - (double) T0);
                    if (off >= from && off < to)
                        add ({ off, drumcat::noteFor (0, h.lane, h.variant), (float) h.velocity / 127.0f, false, false });
                }
            }
        };
        if (changeAt >= 0)
        {
            if (beatPlaying) hitsIn (0, changeAt);
            beatPlaying = ! beatPlaying;
            if (beatPlaying) beatStart = beatChangeAt;
            beatChangeAt = -1;
            if (beatPlaying) hitsIn (changeAt, n);
        }
        else if (beatPlaying) hitsIn (0, n);

        // the click: every beat (accent on the bar), or only while counting in
        const auto mode = (Click) click.load();
        if (mode != Click::off)
        {
            for (auto b = (juce::int64) std::ceil ((double) T0 / spq - 1.0e-9); (double) b * spq < (double) (T0 + n); ++b)
            {
                const double at = (double) b * spq;
                const bool countIn = at < 0.0;
                if (! countIn && ! (mode == Click::always && tempoKnown.load())) continue;
                const int off = (int) std::llround (at - (double) T0);
                if (off >= 0 && off < n) add ({ off, 0, 1.0f, true, ((b % bpb) + bpb) % bpb == 0 });
            }
        }
    }

    std::sort (ev.begin(), ev.begin() + numEv, [] (const Ev& a, const Ev& b) { return a.offset < b.offset; });

    // drums, in pieces between the hits
    int pos = 0;
    for (int e = 0; e < numEv; ++e)
    {
        if (ev[(size_t) e].click) continue;
        if (ev[(size_t) e].offset > pos) { drums.render (l + pos, r + pos, ev[(size_t) e].offset - pos); pos = ev[(size_t) e].offset; }
        drums.handle (MidiMessage::noteOn (10, ev[(size_t) e].note, ev[(size_t) e].vel));
    }
    if (pos < n) drums.render (l + pos, r + pos, n - pos);
    const float bl = beatLevel.load();
    for (int i = 0; i < n; ++i) { l[i] *= bl; r[i] *= bl; }

    // the click
    const float cl = clickLevel.load() * 0.45f;
    const float decay = std::exp (-1.0f / (0.015f * (float) sr));
    int e = 0;
    for (int i = 0; i < n; ++i)
    {
        while (e < numEv && ev[(size_t) e].offset <= i)
        {
            if (ev[(size_t) e].click) { clickEnv = 1.0f; clickPhase = 0.0f; clickHz = ev[(size_t) e].accent ? 1760.0f : 1175.0f; }
            ++e;
        }
        if (clickEnv > 1.0e-4f)
        {
            const float v = std::sin (clickPhase) * clickEnv * cl;
            clickPhase += MathConstants<float>::twoPi * clickHz / (float) sr;
            clickEnv *= decay;
            l[i] += v; r[i] += v;
        }
    }
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    // the commands from the screen / keys / MIDI
    {
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) apply (commands[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) apply (commands[(size_t) (scope.startIndex2 + i)]);
    }
    loadPendingPattern();

    // the beat goes on / off (at the next bar when something plays)
    const bool want = beatWanted.load();
    if (want != beatPlaying && beatChangeAt < 0)
    {
        if (! running)
        {
            if (want) { ensureRunning (false); beatPlaying = true; beatStart = 0; }
            else beatPlaying = false;
        }
        else
        {
            const bool quantized = (Quantize) quantize.load() != Quantize::off && tempoKnown.load();
            beatChangeAt = quantized ? nextBoundary (transport, true) : transport;
        }
    }
    else if (want == beatPlaying && beatChangeAt >= 0) beatChangeAt = -1;     // changed its mind before the bar

    const auto inMode = (Input) input.load();
    const float mon = monitor.load(), mast = master.load();
    const int lat = jlimit (0, (int) (0.5 * sr), latency.load());
    const float rampStep = 1.0f / (float) fade;
    float inPeak = 0.0f, outPeak = 0.0f;

    for (int done = 0; done < n;)
    {
        const int m = jmin (n - done, (int) scratchL.size());
        float* sl = scratchL.data();
        float* sr2 = scratchR.data();
        std::fill (sl, sl + m, 0.0f);
        std::fill (sr2, sr2 + m, 0.0f);
        renderBeatAndClick (sl, sr2, m);

        std::array<int, numTracks> act {};
        for (int t = 0; t < numTracks; ++t) act[(size_t) t] = tracks[(size_t) t].active.load();

        for (int i = 0; i < m; ++i)
        {
            float il = 0.0f, ir = 0.0f;
            if (inL != nullptr)
            {
                const float a = inL[done + i], b = inR != nullptr ? inR[done + i] : a;
                switch (inMode)
                {
                    case Input::stereo: il = a; ir = b; break;
                    case Input::left:   il = ir = a; break;
                    case Input::right:  il = ir = b; break;
                }
                inPeak = jmax (inPeak, std::abs (il), std::abs (ir));
            }

            float ol = 0.0f, orr = 0.0f;
            for (int t = 0; t < numTracks; ++t)
            {
                auto& tr = tracks[(size_t) t];
                if (tr.actionAt >= 0 && running && transport >= tr.actionAt)
                {
                    const int what = tr.action;
                    tr.actionAt = -1;
                    if (what == actStartRec && tr.state.load() == (int) State::armed) startRecording (tr, transport);
                    else if (what == actClose && tr.state.load() == (int) State::recording) closeLoop (tr, (int) (transport - tr.startAt));
                    else if (what == actStartPlay && tr.state.load() == (int) State::stopped) { tr.startAt = transport; startPlaying (tr); }
                    act[(size_t) t] = tr.active.load();
                }

                const auto st = (State) tr.state.load();
                const int a = act[(size_t) t];

                if (st == State::recording)
                {
                    const int idx = tr.recPos - lat;
                    if (idx >= 0 && idx < capacity)
                    {
                        tr.data[a][0][(size_t) idx] = il;
                        tr.data[a][1][(size_t) idx] = ir;
                    }
                    ++tr.recPos;
                    if (tr.recPos >= capacity - fade - (int) (0.5 * sr) - 16 && tr.actionAt < 0) requestClose (tr);   // full
                    if ((State) tr.state.load() == State::recording && tr.actionAt >= 0 && tr.recPos >= capacity - fade - lat - 8)
                        closeLoop (tr, jmin (tr.recPos, (int) (tr.actionAt - tr.startAt)));
                    continue;
                }

                if (tr.tail) tailSample (tr, il, ir);

                const int len = tr.length.load();
                const bool sounding = st == State::playing || st == State::overdubbing || (st == State::stopped && tr.playGain > 0.0f);
                if (len <= 0 || ! sounding) continue;

                const int p = wrapIndex (transport - tr.startAt, len);
                const float target = st == State::stopped ? 0.0f : 1.0f;
                if (tr.playGain < target) tr.playGain = jmin (target, tr.playGain + rampStep);
                else if (tr.playGain > target) tr.playGain = jmax (target, tr.playGain - rampStep);
                tr.volS += 0.002f * (tr.vol.load() - tr.volS);
                const float g = tr.playGain * tr.volS;
                const float yl = tr.data[a][0][(size_t) p] * g, yr = tr.data[a][1][(size_t) p] * g;
                ol += yl; orr += yr;
                tr.peak = jmax (tr.peak, std::abs (yl), std::abs (yr));

                // DUB: what comes in is added (where it was heard: LATENCY earlier), fading in and out
                const bool dubbing = st == State::overdubbing || tr.dubHold > 0;
                if (tr.dubHold > 0) --tr.dubHold;
                const float dubTarget = dubbing ? 1.0f : 0.0f;
                if (tr.dubGain < dubTarget) tr.dubGain = jmin (1.0f, tr.dubGain + rampStep);
                else if (tr.dubGain > dubTarget) tr.dubGain = jmax (0.0f, tr.dubGain - rampStep);
                if (tr.dubGain > 0.0f)
                {
                    const int w = wrapIndex ((juce::int64) p - lat, len);
                    tr.data[a][0][(size_t) w] += il * tr.dubGain;
                    tr.data[a][1][(size_t) w] += ir * tr.dubGain;
                }
            }

            float yl = (ol + sl[i] + il * mon) * mast, yr = (orr + sr2[i] + ir * mon) * mast;
            outL[done + i] = yl;
            outR[done + i] = yr;
            if (running) ++transport;
        }

        if (safety.load()) limiter.process (outL + done, outR + done, m);
        for (int i = 0; i < m; ++i) outPeak = jmax (outPeak, std::abs (outL[done + i]), std::abs (outR[done + i]));
        done += m;
    }

    // stop the clock when nothing needs it (the next start begins at bar 1 again)
    bool needed = beatWanted.load() || beatPlaying || beatChangeAt >= 0;
    for (auto& tr : tracks)
    {
        const auto st = (State) tr.state.load();
        needed |= st == State::armed || st == State::recording || st == State::playing || st == State::overdubbing
                  || tr.playGain > 0.0f || tr.tail || tr.dubGain > 0.0f || tr.actionAt >= 0;
    }
    if (running && ! needed) { running = false; transport = 0; }

    // for the screen
    runningView = running;
    barView = running ? (double) transport / barLen.load() : 0.0;
    for (auto& tr : tracks)
    {
        const auto st = (State) tr.state.load();
        const int len = tr.length.load();
        if (st == State::recording) tr.pos = (float) (tr.recPos / sr);
        else if (len > 0 && (st == State::playing || st == State::overdubbing)) tr.pos = (float) wrapIndex (transport - tr.startAt, len) / (float) len;
        tr.level = jmax (tr.peak, tr.level.load() * 0.8f);
        tr.peak = 0.0f;
    }
    inLevel = jmax (inPeak, inLevel.load() * 0.8f);
    outLevel = jmax (outPeak, outLevel.load() * 0.8f);
}

//==== sessions and export ===========================================================================================
AudioBuffer<float> Engine::getLoop (int t) const
{
    if (! isPositiveAndBelow (t, numTracks)) return {};
    const auto& tr = tracks[(size_t) t];
    const int len = tr.length.load(), a = tr.active.load();
    if (len <= 0 || (State) tr.state.load() == State::recording) return {};
    AudioBuffer<float> b (2, len);
    for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, tr.data[a][ch].data(), len);
    return b;
}

void Engine::overview (int t, float* peaks, int bins) const
{
    std::fill (peaks, peaks + bins, 0.0f);
    if (! isPositiveAndBelow (t, numTracks) || bins <= 0) return;
    const auto& tr = tracks[(size_t) t];
    const int len = tr.length.load(), a = tr.active.load();
    if (len <= 0) return;
    const int step = jmax (1, len / (bins * 48));
    for (int i = 0; i < len; i += step)
    {
        const int b = (int) ((juce::int64) i * bins / len);
        peaks[b] = jmax (peaks[b], std::abs (tr.data[a][0][(size_t) i]), std::abs (tr.data[a][1][(size_t) i]));
    }
}

bool Engine::setLoop (int t, const AudioBuffer<float>& b)
{
    if (! isPositiveAndBelow (t, numTracks) || b.getNumSamples() <= 0) return false;
    auto& tr = tracks[(size_t) t];
    if ((State) tr.state.load() != State::empty) return false;
    const int len = jmin (b.getNumSamples(), capacity - fade - (int) (0.5 * sr) - 16);
    const int a = tr.active.load();
    for (int ch = 0; ch < 2; ++ch)
        std::copy (b.getReadPointer (jmin (ch, b.getNumChannels() - 1)), b.getReadPointer (jmin (ch, b.getNumChannels() - 1)) + len, tr.data[a][ch].begin());
    tr.hasUndo = false;
    tr.undone = false;
    tr.length = len;
    tr.state = (int) State::stopped;
    return true;
}

static AudioBuffer<float> renderBeat (const drumcat::Pattern& pattern, int kitIndex, double sr, double barLen, int bpb, int len, float level)
{
    AudioBuffer<float> out (2, len);
    out.clear();
    hub::DrumKit kit;
    kit.prepare (sr);
    kit.setSound (kitIndex);
    struct Ev { int at; int note; float vel; };
    std::vector<Ev> ev;
    const double spq = barLen / jmax (1, bpb), P = jmax (1.0, pattern.lengthPpq());
    for (const auto& h : pattern.hits)
    {
        double hq = std::fmod (h.pos + h.micro, P);
        if (hq < 0.0) hq += P;
        for (double t = hq; t * spq < len; t += P)
            ev.push_back ({ (int) (t * spq), drumcat::noteFor (0, h.lane, h.variant), (float) h.velocity / 127.0f });
    }
    std::sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    int pos = 0;
    for (auto& e : ev)
    {
        if (e.at > pos) { kit.render (out.getWritePointer (0, pos), out.getWritePointer (1, pos), e.at - pos); pos = e.at; }
        kit.handle (MidiMessage::noteOn (10, e.note, e.vel));
    }
    if (pos < len) kit.render (out.getWritePointer (0, pos), out.getWritePointer (1, pos), len - pos);
    out.applyGain (level);
    return out;
}

static int longest (const Engine& e)
{
    int m = 0;
    for (int t = 0; t < numTracks; ++t) m = jmax (m, e.getLength (t));
    return m;
}

AudioBuffer<float> Engine::stem (int t) const
{
    const int total = longest (*this);
    auto loop = getLoop (t);
    if (total <= 0 || loop.getNumSamples() <= 0) return {};
    AudioBuffer<float> out (2, total);
    const float v = tracks[(size_t) t].vol.load();
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = loop.getReadPointer (ch);
        float* dst = out.getWritePointer (ch);
        for (int i = 0; i < total; ++i) dst[i] = src[i % loop.getNumSamples()] * v;
    }
    return out;
}

AudioBuffer<float> Engine::beatStem() const
{
    int total = longest (*this);
    if (total <= 0) total = (int) std::llround (barLen.load() * 2.0);
    drumcat::Pattern p;
    {
        const SpinLock::ScopedLockType sl (patternLock);
        p = patternPending ? pendingPattern : pattern;
    }
    return renderBeat (p, kit.load(), sr, barLen.load(), beatsPerBar.load(), total, beatLevel.load());
}

AudioBuffer<float> Engine::mixdown (bool withBeat) const
{
    const int total = longest (*this);
    if (total <= 0 && ! withBeat) return {};
    AudioBuffer<float> out (2, jmax (total, 1));
    out.clear();
    for (int t = 0; t < numTracks; ++t)
    {
        auto s = stem (t);
        if (s.getNumSamples() == out.getNumSamples())
            for (int ch = 0; ch < 2; ++ch) out.addFrom (ch, 0, s, ch, 0, s.getNumSamples());
    }
    if (withBeat)
    {
        auto b = beatStem();
        if (b.getNumSamples() > out.getNumSamples()) out.setSize (2, b.getNumSamples(), true, true);
        for (int ch = 0; ch < 2; ++ch) out.addFrom (ch, 0, b, ch, 0, b.getNumSamples());
    }
    const float peak = out.getMagnitude (0, out.getNumSamples());
    if (peak > 0.98f) out.applyGain (0.98f / peak);
    return out;
}
} // namespace looper
