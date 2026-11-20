#include "ChopEngine.h"

using namespace juce;

namespace chop
{
StringArray fxNames()       { return { "Off", "Repeat 1/8", "Repeat 1/16", "Repeat 1/32", "Repeat 1/64", "Roll", "Reverse", "Tape Stop",
                                       "Pitch Up", "Pitch Down", "Scatter", "Freeze", "Mute" }; }
StringArray fxShort()       { return { "", "1/8", "1/16", "1/32", "1/64", "ROLL", "REV", "STOP", "UP", "DOWN", "SCAT", "FRZ", "MUTE" }; }
StringArray lockNames()     { return { "Free", "1 bar", "2 bars", "4 bars" }; }
StringArray quantizeNames() { return { "Off", "1/16", "1/8" }; }

namespace
{
/** a small, good integer hash: the same inputs always give the same "random" number (that is how LOCK works) */
inline uint64_t mix64 (uint64_t x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}
inline uint64_t hash3 (uint64_t a, uint64_t b, uint64_t c) { return mix64 (a * 1000003ull ^ mix64 (b * 7919ull ^ mix64 (c + 17ull))); }
inline float unit (uint64_t h) { return (float) (h & 0xffffff) / (float) 0x1000000; }
}

Engine::Engine()
{
    for (auto& a : padMouse) a = false;
    for (auto& a : padLit) a = false;
    for (auto& a : barFx) a = 0;
    for (auto& a : peaks) a = 0.0f;
    for (auto& a : peakFx) a = 0;
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    size = (int) (sr * 4.0);
    bufL.assign ((size_t) size, 0.0f);
    bufR.assign ((size_t) size, 0.0f);
    outGain.reset (sr, 0.05);
    outGain.setCurrentAndTargetValue (1.0f);
    reset();
}

void Engine::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    w = 0; filled = 0;
    seqEv = chaosEv = liveEv = lastEv = {};
    livePad = -1; pendingPad = -2;   // -2: no pad waiting for the grid
    midiHeld.fill (false); prevMouse.fill (false);
    stepIdx = -1; beats = 0.0;
    seqRunLeft = chaosLeft = 0;
    wet = 0.0f;
}

//==============================================================================
std::pair<int, int> Engine::chaosAt (const Settings& s, long long bar, int k) const
{
    if (s.chaos <= 0.0f) return { None, 0 };
    static const int lockBars[] = { 0, 1, 2, 4 };
    const long long lb = s.lock == 0 ? bar : bar % lockBars[jlimit (1, 3, s.lock)];
    const uint64_t h = hash3 ((uint64_t) s.seed, (uint64_t) lb, (uint64_t) k);

    // where in the bar: fewer glitches on the downbeat, more at the end of the bar (a FILL, like a drummer)
    float base = 0.2f;
    if (k >= 12) base += s.fill * 0.45f;
    if (k >= 14) base += s.fill * 0.35f;
    if (k == 0) base *= 0.25f;
    else if (k % 4 == 0) base *= 0.6f;
    if (unit (h) >= jlimit (0.0f, 0.95f, s.chaos * base * 1.7f)) return { None, 0 };

    // what kind: repeats and scatter most often; tape stops only at the end of the bar
    struct W { int fx; float w; };
    const bool end = k >= 12;
    const W ws[] = { { Rpt8, 2 }, { Rpt16, 3 }, { Rpt32, 3 }, { Rpt64, 1 }, { Roll, end ? 4.0f : 1.5f }, { Reverse, 2 }, { TapeStop, end ? 1.5f : 0.0f },
                     { PitchUp, 1 }, { PitchDown, 1 }, { Scatter, 3 }, { Freeze, 0.7f }, { Mute, 1 } };
    float total = 0.0f;
    for (auto& x : ws) total += x.w;
    float u = unit (mix64 (h)) * total;
    int fx = Rpt16;
    for (auto& x : ws) { if (u < x.w) { fx = x.fx; break; } u -= x.w; }
    const int maxLen = jmin (16 - k, end ? 4 : 2);
    const int len = 1 + (int) (mix64 (h + 99) % (uint64_t) jmax (1, maxLen));
    return { fx, len };
}

void Engine::startEvent (Event& e, int fx, int source, int lengthSamples, const Settings& s, long long salt)
{
    e = {};
    e.fx = fx; e.source = source; e.startW = w; e.length = lengthSamples; e.pos = (double) w;
    const int stepLen = samplesPerStep (s.bpm);
    e.segLen = stepLen / 2;
    const int avail = jmax (1, jmin (15, filled / stepLen - 1));
    e.scatterBack = 1 + (int) (mix64 ((uint64_t) (salt + s.seed * 31)) % (uint64_t) avail);
}

float Engine::rd (const std::vector<float>& b, double pos) const
{
    double p = std::fmod (pos, (double) size); if (p < 0) p += size;
    const int i0 = (int) p; const float f = (float) (p - i0);
    const float a = b[(size_t) i0], c = b[(size_t) ((i0 + 1) % size)];
    return a + f * (c - a);
}

void Engine::readEvent (Event& e, float& l, float& r, int stepLen, float fade)
{
    auto at = [this] (int idx, float& L, float& R) { idx %= size; if (idx < 0) idx += size; L = bufL[(size_t) idx]; R = bufR[(size_t) idx]; };
    auto edge = [fade] (double p, double len) { return (float) jmin (1.0, p / fade, (len - p) / fade); };
    const long long t = e.t;
    float g = 1.0f;
    switch (e.fx)
    {
        case Rpt8: case Rpt16: case Rpt32: case Rpt64:
        {
            static const double mul[] = { 2.0, 1.0, 0.5, 0.25 };
            const int len = jmax (32, (int) (stepLen * mul[e.fx - Rpt8]));
            const int p = (int) (t % len);
            at (e.startW + p, l, r);
            if (t >= len) g = edge (p, len); else g = jmin (1.0f, (float) (len - p) / fade);
            break;
        }
        case Roll:
        {
            // the same bit, again and again, faster and faster (a trap hi-hat roll)
            at (e.startW + e.segPos, l, r);
            g = e.seg == 0 ? jmin (1.0f, (float) (e.segLen - e.segPos) / fade) : edge (e.segPos, e.segLen);
            if (++e.segPos >= e.segLen) { e.segPos = 0; ++e.seg; e.segLen = jmax (stepLen / 8, (int) (e.segLen * 0.8)); }
            break;
        }
        case Reverse:
            at (e.startW - 1 - (int) t, l, r);
            break;
        case TapeStop:
        {
            const double plan = e.length > 0 ? e.length : stepLen * 4.0;
            const double progress = jmin (1.0, (double) t / plan);
            l = rd (bufL, e.pos); r = rd (bufR, e.pos);
            e.pos += std::pow (1.0 - progress, 1.6);
            g = progress > 0.85 ? (float) ((1.0 - progress) / 0.15) : 1.0f;
            break;
        }
        case PitchUp:
        {
            const double p = std::fmod ((double) t * 2.0, (double) stepLen);
            l = rd (bufL, e.startW - stepLen + p); r = rd (bufR, e.startW - stepLen + p);
            g = edge (p, stepLen);
            break;
        }
        case PitchDown:
        {
            const double p = std::fmod ((double) t * 0.5, (double) stepLen);
            l = rd (bufL, e.startW - stepLen + p); r = rd (bufR, e.startW - stepLen + p);
            g = edge (p, stepLen);
            break;
        }
        case Scatter:
        {
            const int p = (int) (t % stepLen);
            at (e.startW - e.scatterBack * stepLen + p, l, r);
            g = edge (p, stepLen);
            break;
        }
        case Freeze:
        {
            // two windowed grains, half a grain apart: together they keep the moment sounding
            const int G = jmax (64, (int) (sr * 0.07));
            float L = 0.0f, R = 0.0f;
            for (int k = 0; k < 2; ++k)
            {
                const long long tt = t + k * (G / 2);
                const int ph = (int) (tt % G);
                const long long grain = tt / G;
                const int jitter = (int) (mix64 ((uint64_t) (grain * 2 + k)) % (uint64_t) (sr * 0.02));
                const float win = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) ph / (float) G);
                float a, b;
                at (e.startW - G - jitter + ph, a, b);
                L += a * win; R += b * win;
            }
            l = L; r = R;
            break;
        }
        case Mute: l = r = 0.0f; break;
        default: break;
    }
    l *= g; r *= g;
    ++e.t;
}

//==============================================================================
void Engine::process (AudioBuffer<float>& buffer, const MidiBuffer& midi, const Settings& s)
{
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const double inc = s.bpm / 60.0 / sr;
    const int stepLen = samplesPerStep (s.bpm);
    const float wetCoef = 1.0f - std::exp (-1.0f / ((0.5f + s.smooth * 10.0f) * 0.001f * (float) sr));
    const float fade = jmax (16.0f, (0.5f + s.smooth * 3.0f) * 0.001f * (float) sr);
    outGain.setTargetValue (Decibels::decibelsToGain (s.outDb));

    // the mouse pads: a newly pressed pad is "the latest" (the latest held pad wins)
    for (int p = 0; p < numPads; ++p)
    {
        const bool m = padMouse[(size_t) p].load();
        if (m && ! prevMouse[(size_t) p]) pressOrder[(size_t) p] = ++pressCounter;
        prevMouse[(size_t) p] = m;
    }

    auto wantedPad = [this]
    {
        int best = -1; long long bestOrder = -1;
        for (int p = 0; p < numPads; ++p)
            if ((prevMouse[(size_t) p] || midiHeld[(size_t) p]) && pressOrder[(size_t) p] > bestOrder) { best = p; bestOrder = pressOrder[(size_t) p]; }
        return best;
    };
    auto applyPad = [&] (int pad)
    {
        livePad = pad;
        if (pad >= 0) startEvent (liveEv, pad + 1, SrcLive, 0, s, pressCounter);
        else liveEv.fx = None;
    };

    auto it = midi.cbegin();
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // 1. MIDI keys: C .. B (any octave) hold pads 1 .. 12
        bool padsChanged = i == 0;
        for (; it != midi.cend() && (*it).samplePosition <= i; ++it)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn()) { const int p = m.getNoteNumber() % 12; midiHeld[(size_t) p] = true; pressOrder[(size_t) p] = ++pressCounter; padsChanged = true; }
            else if (m.isNoteOff()) { midiHeld[(size_t) (m.getNoteNumber() % 12)] = false; padsChanged = true; }
            else if (m.isAllNotesOff() || m.isAllSoundOff()) { midiHeld.fill (false); padsChanged = true; }
        }
        if (padsChanged)
        {
            const int want = wantedPad();
            if (want != livePad && want != pendingPad)
            {
                if (s.quantize == 0 || want < 0) { applyPad (want); pendingPad = -2; }
                else pendingPad = want;
            }
        }

        // 2. the clock: the DAW's, or its own when the DAW is stopped
        if (s.playing) beats = s.ppq + i * inc; else beats += inc;
        const long long idx = (long long) std::floor (beats * 4.0 + 1e-9);
        if (idx != stepIdx)
        {
            stepIdx = idx;
            const int k = (int) (((idx % 16) + 16) % 16);
            const long long bar = idx >= 0 ? idx / 16 : (idx - 15) / 16;
            step = k;
            if (k == 0) for (auto& b : barFx) b = 0;

            // the sequencer: one glitch per step, the same one on the next steps runs on
            if (seqRunLeft > 0) --seqRunLeft;
            if (seqRunLeft == 0)
            {
                seqEv.fx = None;
                const int fx = s.seqOn ? s.steps[(size_t) k] : None;
                if (fx != None)
                {
                    int run = 1;
                    while (k + run < 16 && s.steps[(size_t) (k + run)] == fx) ++run;
                    startEvent (seqEv, fx, SrcSeq, run * stepLen, s, idx);
                    seqRunLeft = run;
                }
            }
            // chaos: only where the sequencer has nothing
            if (chaosLeft > 0) --chaosLeft;
            if (chaosLeft == 0) chaosEv.fx = None;
            if (seqEv.fx != None) { chaosEv.fx = None; chaosLeft = 0; }
            else if (chaosLeft == 0)
            {
                const auto [fx, len] = chaosAt (s, bar, k);
                if (fx != None) { startEvent (chaosEv, fx, SrcChaos, len * stepLen, s, idx * 3 + 1); chaosLeft = len; }
            }
            // a pad waiting for the grid
            if (pendingPad >= -1 && pendingPad != livePad && (s.quantize == 1 || (idx % 2) == 0)) { applyPad (pendingPad); pendingPad = -2; }
        }

        // 3. remember the input
        const float inL = L[i], inR = R != nullptr ? R[i] : inL;
        bufL[(size_t) w] = inL; bufR[(size_t) w] = inR;

        // 4. the strongest glitch plays (live > sequencer > chaos); it fades in and out
        Event* act = liveEv.active() ? &liveEv : seqEv.active() ? &seqEv : chaosEv.active() ? &chaosEv : nullptr;
        float gl = inL, gr = inR;
        if (act != nullptr)
        {
            readEvent (*act, gl, gr, stepLen, fade);
            lastEv = *act;
            wet += (1.0f - wet) * wetCoef;
        }
        else
        {
            if (wet > 0.0005f) readEvent (lastEv, gl, gr, stepLen, fade);
            wet -= wet * wetCoef;
        }
        const float m = wet * s.mix;
        float oL = inL + (gl - inL) * m, oR = inR + (gr - inR) * m;
        const float g = outGain.getNextValue();
        oL *= g; oR *= g;
        L[i] = oL;
        if (R != nullptr) R[i] = oR;

        w = (w + 1) % size;
        filled = jmin (size, filled + 1);

        // 5. for the screen
        const int shownFx = act != nullptr ? act->fx : None;
        const int shownSrc = act != nullptr ? act->source : SrcNone;
        if (shownFx != None && stepIdx >= 0) barFx[(size_t) step.load()] = shownFx | (shownSrc << 8);
        peakAcc = jmax (peakAcc, std::abs (oL), std::abs (oR));
        peak = jmax (peak, std::abs (oL), std::abs (oR));
        if (++peakN >= 128)
        {
            const int p = peakPos.load();
            peaks[(size_t) (p % peakCount)] = peakAcc;
            peakFx[(size_t) (p % peakCount)] = shownFx | (shownSrc << 8);
            peakPos = (p + 1) % (peakCount * 1000);
            peakAcc = 0.0f; peakN = 0;
        }
        nowFx = shownFx; nowSource = shownSrc;
    }
    for (int p = 0; p < numPads; ++p) padLit[(size_t) p] = prevMouse[(size_t) p] || midiHeld[(size_t) p];
    level = peak;
}
} // namespace chop
