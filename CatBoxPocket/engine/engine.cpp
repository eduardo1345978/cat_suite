// Cat Box Pocket: the sound engine (voices, sequencer, recording, effects).
#include "pocket.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace pocket {

static inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float softclip (float x) { x = clampf (x, -1.5f, 1.5f); return x - x * x * x * (4.0f / 27.0f); }
static inline int16_t toS16 (float x) { return (int16_t) clampf (x * 32767.0f, -32768.0f, 32767.0f); }
static constexpr float PI = 3.14159265358979f;

uint32_t Engine::rng () { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }

void Engine::init (int16_t* pool, uint32_t count, uint32_t s) {
    mem = pool; memSize = count; memUsed = 0; seed = s ? s : 1;
    for (auto& b : blocks) b = Block ();
    for (auto& sl : slots) sl = Slot ();
    for (auto& p : pats) p = Pattern ();
    for (auto& v : voices) v = Voice ();
    chain[0] = 0; chainLen = 1; chainPos = 0; pat = 0; step = -1;
}

// ------------------------------------------------------------------ the sample pool
int Engine::newBlock (uint32_t len) {
    if (len == 0 || memUsed + len > memSize) return -1;
    for (int i = 0; i < NBLOCK; ++i)
        if (blocks[i].refs == 0 && blocks[i].len == 0) {
            blocks[i].off = memUsed; blocks[i].len = len; blocks[i].refs = 1; memUsed += len;
            return i;
        }
    return -1;
}

void Engine::release (int b) {
    if (b < 0 || b >= NBLOCK || blocks[b].refs == 0) return;
    if (--blocks[b].refs > 0) return;
    // free it: move everything behind it down (the audio plays silence meanwhile)
    busy = true;
    const uint32_t off = blocks[b].off, len = blocks[b].len;
    for (auto& v : voices) if (v.on && slots[v.slot].block == b) v.on = false;
    if (off + len < memUsed) std::memmove (mem + off, mem + off + len, (memUsed - off - len) * sizeof (int16_t));
    memUsed -= len;
    for (auto& o : blocks) if (o.len && o.off > off) o.off -= len;
    blocks[b].len = 0; blocks[b].off = 0;
    for (auto& s : slots) if (s.block == b) s = Slot ();
    busy = false;
}

void Engine::compact () {}

float Engine::memoryFree () const { return (memSize - memUsed) / float (SR); }

void Engine::clearSlot (int s) {
    if (s < 0 || s >= NSLOT || slots[s].empty ()) return;
    for (auto& v : voices) if (v.on && v.slot == s) v.on = false;
    const int b = slots[s].block;
    slots[s] = Slot ();
    release (b);
}

bool Engine::assign (int s, const int16_t* data, uint32_t len, uint32_t rate) {
    if (s < 0 || s >= NSLOT || len == 0) return false;
    clearSlot (s);
    const int b = newBlock (len);
    if (b < 0) return false;
    std::memcpy (mem + blocks[b].off, data, len * sizeof (int16_t));
    Slot& sl = slots[s]; sl = Slot (); sl.block = (int16_t) b; sl.start = 0; sl.len = len; sl.rate = rate;
    return true;
}

uint32_t Engine::slotData (int s, const int16_t** data) const {
    if (s < 0 || s >= NSLOT || slots[s].empty ()) { *data = nullptr; return 0; }
    *data = mem + blocks[slots[s].block].off + slots[s].start;
    return slots[s].len;
}

// ------------------------------------------------------------------ voices
void Engine::startVoice (Voice& v, int s, int pad, float semis, float vel, int8_t filterLock) {
    const Slot& sl = slots[s];
    v = Voice ();
    v.on = true; v.held = true; v.slot = (uint8_t) s; v.pad = (uint8_t) (pad & 0xff);
    v.gain = vel * sl.volume;
    v.env = 0; v.rel = 0;
    const float semi = sl.pitch + semis;
    v.inc = (sl.rate / float (SR)) * std::pow (2.0f, semi / 12.0f);
    const float a = sl.trimA * sl.len, b = std::max (a + 1, sl.trimB * sl.len);
    v.pos = sl.reverse ? b - 1 : a;
    float f = sl.filter;
    if (filterLock != NOLOCK) f = filterLock / 100.0f;
    v.cut = f;
    if (sl.stretch || sl.freeze) {
        v.grains = true;
        v.gLen = SR * (sl.freeze ? 0.09f : 0.045f) * (sl.rate / float (SR));
        v.head = v.pos;
        if (sl.freeze) v.headInc = (sl.reverse ? -1.0f : 1.0f) * 0.03f * (sl.rate / float (SR));
        else {
            const float secs = (b - a) / float (sl.rate);
            int beats = sl.beats;
            if (beats == 0) {   // the power of two nearest to the length at this tempo
                const float est = secs * bpm / 60.0f;
                beats = 1; while (beats * 2 <= 64 && std::fabs (beats * 2 - est) < std::fabs (beats - est)) beats *= 2;
            }
            const float target = beats * 60.0f / bpm;
            v.headInc = (sl.reverse ? -1.0f : 1.0f) * (secs / target) * (sl.rate / float (SR));
        }
        v.gPos[0] = v.gPos[1] = v.head; v.gPh[0] = 0; v.gPh[1] = 0.5f;
    }
}

void Engine::noteOn (int s, int pad, float semis, float vel, int8_t filterLock) {
    if (s < 0 || s >= NSLOT || slots[s].empty () || busy) return;
    // the same pad retriggers its own voice (like a Pocket Operator), else take a free or the oldest one
    Voice* pick = nullptr;
    for (auto& v : voices) if (v.on && v.pad == (uint8_t) (pad & 0xff) && v.slot == s) { pick = &v; break; }
    if (! pick) for (auto& v : voices) if (! v.on) { pick = &v; break; }
    if (! pick) { pick = &voices[0]; for (auto& v : voices) if (v.age > pick->age) pick = &v; }
    startVoice (*pick, s, pad, semis, vel, filterLock);
}

void Engine::noteOff (int pad) {
    for (auto& v : voices) if (v.on && v.pad == (uint8_t) (pad & 0xff)) v.held = false;
}

void Engine::allOff () { for (auto& v : voices) v.on = false; }

// time-stretch: start the next grain where it best continues the grain that is playing (WSOLA),
// so a steady tone keeps its pitch and does not flutter
static float align (const int16_t* d, int a, int b, float head, float other, float dir) {
    const int W = 96, R = 240;
    const int o = (int) other;
    if (o - W < a || o + W >= b) return head;
    int best = (int) head; long long bestS = -(1LL << 62);
    for (int c = (int) head - R; c <= (int) head + R; c += 2) {
        if (c - W < a || c + W >= b) continue;
        long long s = 0;
        for (int k = 0; k < W; k += 3) { const int kk = dir > 0 ? k : -k; s += (int) d[c + kk] * (int) d[o + kk]; }
        if (s > bestS) { bestS = s; best = c; }
    }
    return (float) best + (other - o);
}

float Engine::voiceSample (Voice& v) {
    const Slot& sl = slots[v.slot];
    const int16_t* d = mem + blocks[sl.block].off + sl.start;
    const float a = sl.trimA * sl.len, b = std::max (a + 2, sl.trimB * sl.len);
    auto at = [&] (float p) -> float {
        if (p < a) p = a; if (p > b - 2) p = b - 2;
        const int i = (int) p; const float fr = p - i;
        return (d[i] + (d[i + 1] - d[i]) * fr) * (1.0f / 32768.0f);
    };
    float x;
    const float dir = sl.reverse ? -1.0f : 1.0f;
    const float inc = v.inc * transpose;
    if (v.grains) {
        x = 0;
        for (int g = 0; g < 2; ++g) {
            // smoothstep window: two grains half a period apart always sum to 1 (and no sin per sample)
            const float u = v.gPh[g] < 0.5f ? 2 * v.gPh[g] : 2 - 2 * v.gPh[g];
            x += u * u * (3 - 2 * u) * at (v.gPos[g]);
            v.gPos[g] += dir * inc;
            v.gPh[g] += 1.0f / v.gLen;
            if (v.gPh[g] >= 1) {
                v.gPh[g] -= 1;
                if (sl.freeze) v.gPos[g] = v.head + ((rng () & 1023) / 1023.0f - 0.5f) * v.gLen * 0.5f;
                else v.gPos[g] = align (d, (int) a, (int) b, v.head, v.gPos[1 - g], dir);
            }
        }
        v.head += v.headInc;
        const bool out = sl.reverse ? v.head < a : v.head >= b - 2;
        if (out) {
            if (sl.freeze || (sl.loopHold && v.held)) v.head = sl.reverse ? b - 2 : a;
            else v.rel = 1;
        }
    } else {
        x = at (v.pos);
        v.pos += dir * inc;
        const bool out = sl.reverse ? v.pos < a : v.pos >= b - 2;
        if (out) {
            if (sl.loopHold && v.held) v.pos = sl.reverse ? b - 2 : a;
            else { v.on = false; return 0; }
        }
    }
    // envelope: 2 ms in, release when a held sound is let go (loop / freeze) or a grain voice ran out
    if ((! v.held && (sl.loopHold || sl.freeze)) || v.rel > 0) {
        v.env -= 1.0f / (SR * 0.05f);
        if (v.env <= 0) { v.on = false; return 0; }
    } else if (v.env < 1) v.env = std::min (1.0f, v.env + 1.0f / (SR * 0.002f));
    return x * v.env;
}

// ------------------------------------------------------------------ recording
void Engine::recStart (int s, bool resample) {
    if (s < 0 || s >= NSLOT || recSlot >= 0 || busy) return;
    if (loopState != LOOP_OFF && loopSlot == s) { loopState = LOOP_OFF; loopSlot = -1; }
    clearSlot (s);
    const uint32_t room = memSize - memUsed;
    if (room < SR / 10) return;
    recBlock = newBlock (room);
    if (recBlock < 0) return;
    recSlot = s; recResample = resample; recLen = 0; recMax = room; recAcc = 0; recPhase = 0;
}

void Engine::recStop () {
    if (recSlot < 0) return;
    Block& b = blocks[recBlock];
    // trim the unused tail (this block is the last one in the pool)
    memUsed = b.off + recLen; b.len = recLen;
    if (recLen < 64) { b.refs = 0; b.len = 0; memUsed = b.off; recSlot = -1; recBlock = -1; return; }
    Slot& sl = slots[recSlot]; sl = Slot ();
    sl.block = (int16_t) recBlock; sl.start = 0; sl.len = recLen; sl.rate = recRate ();
    recSlot = -1; recBlock = -1;
}

// ------------------------------------------------------------------ sequencer
void Engine::play (bool on) {
    if (on == running) return;
    running = on;
    if (on) { step = -1; nextStep = 0; stepClock = 0; chainPos = 0; pat = chain[0]; loopPos = 0; }
    else { step = -1; seqFx = 0; }
}

Trig* Engine::trig (int p, int st, int s) {
    Step& S = pats[p].s[st];
    for (int i = 0; i < S.n; ++i) if (S.t[i].slot == s) return &S.t[i];
    return nullptr;
}
bool Engine::hasTrig (int p, int st, int s) const { return const_cast<Engine*> (this)->trig (p, st, s) != nullptr; }

void Engine::toggleStep (int st, int s, int8_t pitch) {
    Step& S = pats[pat].s[st];
    for (int i = 0; i < S.n; ++i)
        if (S.t[i].slot == s) { for (int j = i; j < S.n - 1; ++j) S.t[j] = S.t[j + 1]; --S.n; return; }
    if (S.n >= NTRIG) return;
    S.t[S.n++] = Trig { (uint8_t) s, pitch, NOLOCK };
}

void Engine::liveRecord (int s, int8_t pitch) {
    if (! running) return;
    const double frac = stepLenNow > 0 ? 1.0 - stepClock / stepLenNow : 0;
    const int st = frac < 0.5 ? std::max (step, 0) : (step + 1) % NSTEP;
    Trig* t = trig (pat, st, s);
    if (t) { t->pitch = pitch; return; }
    Step& S = pats[pat].s[st];
    if (S.n < NTRIG) S.t[S.n++] = Trig { (uint8_t) s, pitch, NOLOCK };
}

void Engine::euclid (int s, int hits, int rotate) {
    hits = std::max (0, std::min (NSTEP, hits));
    for (int st = 0; st < NSTEP; ++st) {
        const int i = ((st - rotate) % NSTEP + NSTEP) % NSTEP;
        const bool on = hits > 0 && (i * hits) % NSTEP < hits;
        const bool has = hasTrig (pat, st, s);
        if (on != has) toggleStep (st, s);
    }
}

void Engine::chaos (float amt) {
    Pattern& P = pats[pat];
    uint8_t used[NSLOT]; int nu = 0;
    for (auto& S : P.s) for (int i = 0; i < S.n; ++i) {
        bool seen = false; for (int k = 0; k < nu; ++k) seen |= used[k] == S.t[i].slot;
        if (! seen && nu < NSLOT) used[nu++] = S.t[i].slot;
    }
    if (nu == 0) return;
    auto chance = [&] (float p) { return (rng () % 10000) < p * 10000; };
    Pattern out = P;
    for (int st = 0; st < NSTEP; ++st) {
        Step& S = out.s[st];
        for (int i = 0; i < S.n; ++i) {
            if (chance (amt * 0.25f)) {               // move to a neighbour step
                Trig t = S.t[i];
                const int to = (st + ((rng () & 1) ? 1 : NSTEP - 1)) % NSTEP;
                for (int j = i; j < S.n - 1; ++j) S.t[j] = S.t[j + 1];
                --S.n; --i;
                Step& D = out.s[to];
                bool dup = false; for (int k = 0; k < D.n; ++k) dup |= D.t[k].slot == t.slot;
                if (! dup && D.n < NTRIG) D.t[D.n++] = t;
                continue;
            }
            if (chance (amt * 0.15f)) S.t[i].pitch = (int8_t) ((int) (rng () % 25) - 12);
        }
        if (chance (amt * 0.2f) && S.n < NTRIG) {     // a new hit of a sound the pattern uses
            const uint8_t s = used[rng () % nu];
            bool dup = false; for (int k = 0; k < S.n; ++k) dup |= S.t[k].slot == s;
            if (! dup) S.t[S.n++] = Trig { s, NOLOCK, NOLOCK };
        }
        if (chance (amt * 0.1f) && S.n > 0) { S.t[rng () % S.n] = S.t[S.n - 1]; --S.n; }
    }
    P = out;
}

void Engine::clearPattern (int p) { pats[p] = Pattern (); }

void Engine::setChain (const uint8_t* p, int n) {
    if (n <= 0) return;
    n = std::min (n, NCHAIN);
    for (int i = 0; i < n; ++i) chain[i] = p[i];
    chainLen = n; chainPos = 0;
    if (! running) pat = chain[0];
}

void Engine::seqTick () {
    const int fxNow = fx ? fx : seqFx;
    // effect 14 (restart the pattern) loops the playing beat, 16 = half tempo
    int st = nextStep;
    if (fxNow == 14 && st % 4 == 0 && step >= 0) st = (step / 4) * 4;
    step = st;
    if (step == 0) {
        if (fxNow != 14) { pat = chain[chainPos]; }
        loopPos = 0;
    }
    const Step& S = pats[pat].s[step];
    for (int i = 0; i < S.n; ++i) {
        const Trig& t = S.t[i];
        noteOn (t.slot, t.slot, t.pitch == NOLOCK ? 0 : t.pitch, 1, t.filter);
    }
    seqFx = S.fx;
    nextStep = (step + 1) % NSTEP;
    if (nextStep == 0 && fxNow != 14) chainPos = (chainPos + 1) % chainLen;
    double len = stepSamples ();
    len *= (step % 2 == 0) ? (1.0 + swing) : (1.0 - swing);
    if (fxNow == 16) len *= 2;
    stepLenNow = len;
    stepClock += len;
}

// ------------------------------------------------------------------ chop
int Engine::chop (int s) {
    if (s < 0 || s >= NSLOT || slots[s].empty ()) return -1;
    int bank = -1;
    for (int b = 0; b < NBANK && bank < 0; ++b) {
        bool free = true; for (int i = 0; i < 16; ++i) free &= slots[b * 16 + i].empty ();
        if (free) bank = b;
    }
    if (bank < 0) return -1;
    const Slot src = slots[s];
    const int16_t* d = mem + blocks[src.block].off + src.start;
    const uint32_t len = src.len;
    // onsets: a 256-sample frame whose energy jumps above the recent average
    const int F = 256; uint32_t cuts[17]; int nc = 0; cuts[nc++] = 0;
    float avg = 0; uint32_t last = 0;
    for (uint32_t f = 0; f + F <= len && nc < 16; f += F) {
        float e = 0; for (int i = 0; i < F; ++i) { const float x = d[f + i] / 32768.0f; e += x * x; }
        e /= F;
        if (f > 0 && e > 0.0004f && e > avg * 4.0f && f - last > (uint32_t) (src.rate * 0.06f)) { cuts[nc++] = f; last = f; }
        avg = avg * 0.8f + e * 0.2f;
    }
    if (nc < 4) { nc = 0; for (int i = 0; i < 16; ++i) cuts[nc++] = (uint32_t) ((uint64_t) len * i / 16); }
    cuts[nc] = len;
    for (int i = 0; i < nc; ++i) {
        Slot& o = slots[bank * 16 + i]; o = Slot ();
        o.block = src.block; o.start = src.start + cuts[i]; o.len = cuts[i + 1] - cuts[i]; o.rate = src.rate;
        o.volume = src.volume;
        blocks[src.block].refs++;
    }
    return bank;
}

// ------------------------------------------------------------------ looper
void Engine::looperNext (int s) {
    if (loopState == LOOP_OFF) {
        if (recSlot >= 0) return;
        const uint32_t len = (uint32_t) (stepSamples () * NSTEP);
        if (slots[s].empty () || slots[s].len != len || slots[s].rate != SR) {
            clearSlot (s);
            const int b = newBlock (len);
            if (b < 0) return;
            std::memset (mem + blocks[b].off, 0, len * sizeof (int16_t));
            Slot& sl = slots[s]; sl = Slot (); sl.block = (int16_t) b; sl.len = len; sl.rate = SR;
        }
        loopSlot = s; loopState = LOOP_REC; loopPos = 0;
        if (! running) play (true);
    } else if (loopState == LOOP_REC) loopState = LOOP_PLAY;
    else { loopState = LOOP_OFF; loopSlot = -1; }
}

// ------------------------------------------------------------------ effects
float Engine::histAt (float p) const {
    while (p < 0) p += HLEN;
    while (p >= HLEN) p -= HLEN;
    const int i = (int) p; const float fr = p - i;
    const int j = (i + 1) % HLEN;
    return (hist[i] + (hist[j] - hist[i]) * fr) * (1.0f / 32768.0f);
}

float Engine::fxProcess (float x) {
    const int want = fx ? fx : seqFx;
    hist[hpos] = toS16 (x);
    if (want != fxActive) {
        fxActive = want; fxStart = hpos; fxT = 0; fxRead = (float) hpos;
        if (want == 10 || want == 11) { fic1 = fic2 = 0; }
    }
    // octave effects act on the voices; tempo effects act in the sequencer
    transpose = fxActive == 12 ? 2.0f : (fxActive == 13 ? 0.5f : 1.0f);
    float y = x;
    const float st = stepSamples ();
    auto loopLen = [&] (float steps) { return std::min ((float) HLEN - 1, std::max (64.0f, steps * st)); };
    switch (fxActive) {
        case 1: case 2: case 3: case 4: {
            const float L = loopLen (fxActive == 1 ? 1 : fxActive == 2 ? 2 : fxActive == 3 ? 4 : 8);
            y = histAt (fxStart - L + std::fmod ((float) fxT, L)); break;
        }
        case 5: {     // stutter: 1/32 loop, gated
            const float L = loopLen (0.5f); const float ph = std::fmod ((float) fxT, L);
            y = ph < L * 0.6f ? histAt (fxStart - L + ph) : 0; break;
        }
        case 6: {     // tape stop over one beat
            const float r = std::max (0.0f, 1.0f - fxT / (st * 4));
            fxRead += r; y = histAt (fxRead) * std::min (1.0f, r * 4); break;
        }
        case 7: y = histAt (fxStart - (float) std::min<uint32_t> (fxT, HLEN - 2)); break;     // reverse
        case 8: {     // scratch
            const float r = std::sin (2 * PI * fxT / (st * 2)) * 1.6f;
            fxRead += r; if (fxRead > fxStart) fxRead = (float) fxStart;
            if (fxStart - fxRead > HLEN - 2) fxRead = fxStart - (HLEN - 2);
            y = histAt (fxRead); break;
        }
        case 9: {     // bitcrush: 5 bits, 1/6 rate
            if (crushCount++ % 6 == 0) crushHold = std::round (x * 16) / 16;
            y = crushHold; break;
        }
        case 10: case 11: {   // filter sweeps over two beats
            if ((fxT & 31) == 0) {
                const float t = std::min (1.0f, fxT / (st * 8));
                const float fc = fxActive == 10 ? 12000.0f * std::pow (2.0f, -7.0f * t) : 60.0f * std::pow (2.0f, 7.0f * t);
                sweepG = std::tan (PI * std::min (fc, 18000.0f) / SR);
            }
            const float g = sweepG, k = 0.7f;
            const float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - fic2, v1 = a1 * fic1 + a2 * v3, v2 = fic2 + a2 * fic1 + a3 * v3;
            fic1 = 2 * v1 - fic1; fic2 = 2 * v2 - fic2;
            y = fxActive == 10 ? v2 : x - k * v1 - v2; break;
        }
        case 15: {    // freeze: two grains over the last 90 ms
            const float L = SR * 0.09f; y = 0;
            for (int g = 0; g < 2; ++g) {
                const float ph = std::fmod (fxT / L + g * 0.5f, 1.0f);
                const float u = ph < 0.5f ? 2 * ph : 2 - 2 * ph;
                y += u * u * (3 - 2 * u) * histAt (fxStart - L * 1.5f + ph * L);
            }
            break;
        }
        default: break;
    }
    const bool any = fxActive != 0 && fxActive != 12 && fxActive != 13 && fxActive != 14 && fxActive != 16;
    fxEnv = std::max (0.0f, std::min (1.0f, fxEnv + (any ? 1.0f : -1.0f) / 240.0f));
    hpos = (hpos + 1) % HLEN; ++fxT;
    return x + (y - x) * fxEnv;
}

// ------------------------------------------------------------------ audio
void Engine::process (const float* in, float* out, int n) {
    if (busy) { std::memset (out, 0, sizeof (float) * 2 * n); return; }
    const float rollEvery = SR * 60.0f / bpm / std::max (1.0f, rollDiv);
    for (int i = 0; i < n; ++i) {
        const float input = (in ? in[i] : 0) * inGain;
        // sequencer and roll
        if (running) {
            stepClock -= 1;
            while (stepClock <= 0) seqTick ();
        }
        if (rollSlot >= 0) {
            rollClock -= 1;
            if (rollClock <= 0) { noteOn (rollSlot, rollPad, 0, 0.9f); rollClock += rollEvery; }
        } else rollClock = 0;
        // voices
        float dry = 0, dsend = 0, rsend = 0;
        for (auto& v : voices) {
            if (! v.on) continue;
            ++v.age;
            const Slot& sl = slots[v.slot];
            float x = voiceSample (v);
            if (! v.on) continue;
            if (v.cut < -0.02f || v.cut > 0.02f) {
                if (v.hp == 0) {   // coefficient once per note
                    const float fc = v.cut < 0 ? 18000.0f * std::pow (2.0f, 9.0f * v.cut) : 20.0f * std::pow (2.0f, 9.0f * v.cut);
                    v.hp = std::tan (PI * clampf (fc, 20.0f, 16000.0f) / SR);
                }
                const float g = v.hp, k = 1.2f;
                const float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const float v3 = x - v.bp, v1 = a1 * v.lp + a2 * v3, v2 = v.bp + a2 * v.lp + a3 * v3;
                v.lp = 2 * v1 - v.lp; v.bp = 2 * v2 - v.bp;
                x = v.cut < 0 ? v2 : x - k * v1 - v2;
            }
            x *= v.gain;
            dry += x;
            dsend += x * clampf (delayAmt + sl.delay, 0, 1);
            rsend += x * clampf (reverbAmt + sl.reverb, 0, 1);
        }
        // looper (one bar, overdub from the input)
        if (loopState != LOOP_OFF && loopSlot >= 0 && ! slots[loopSlot].empty ()) {
            Slot& ls = slots[loopSlot];
            int16_t* d = mem + blocks[ls.block].off + ls.start;
            const uint32_t p = loopPos % ls.len;
            const float old = d[p] / 32768.0f;
            dry += old * ls.volume;
            if (loopState == LOOP_REC) d[p] = toS16 (old + input);
            ++loopPos;
        }
        // punch-in effects on the dry mix
        dry = fxProcess (dry);
        // delay: 3/16, filtered feedback
        const int dl = std::min (DLEN - 1, (int) (stepSamples () * 3));
        int rp = dpos - dl; if (rp < 0) rp += DLEN;
        const float dout = dbuf[rp] / 32768.0f;
        dlp += (dout - dlp) * 0.35f;
        dbuf[dpos] = toS16 (dsend * 0.6f + dlp * 0.45f);
        dpos = (dpos + 1) % DLEN;
        // reverb
        float cs = 0;
        for (int c = 0; c < NC; ++c) {
            float& z = comb[c][combPos[c]];
            const float o = z;
            combLp[c] = o * 0.6f + combLp[c] * 0.4f;
            z = rsend * 0.25f + combLp[c] * 0.82f;
            combPos[c] = (combPos[c] + 1) % combLen[c];
            cs += (c & 1) ? -o : o;
        }
        float rv[2];
        for (int a = 0; a < 2; ++a) {
            float& z = ap[a][apPos[a]];
            const float buf = z; const float inp = a == 0 ? cs : -cs * 0.9f + buf * 0.1f;
            z = inp + buf * 0.5f; rv[a] = buf - inp;
            apPos[a] = (apPos[a] + 1) % apLen[a];
        }
        float L = (dry + dlp * 0.8f + rv[0] * 0.5f) * master;
        float R = (dry + dlp * 0.8f + rv[1] * 0.5f) * master;
        if (lofi) { L = std::round (L * 2048) / 2048; R = std::round (R * 2048) / 2048; }
        L = softclip (L); R = softclip (R);
        out[2 * i] = L; out[2 * i + 1] = R;
        level = std::max (level * 0.9995f, std::fabs (L));
        // recording (mic / line, or the output = resampling); lo-fi stores every second sample
        if (recSlot >= 0) {
            const float src = recResample ? 0.5f * (L + R) : input;
            int16_t* d = mem + blocks[recBlock].off;
            if (lofi) {
                recAcc += src;
                if (++recPhase == 2) { recPhase = 0; if (recLen < recMax) d[recLen++] = toS16 (std::round (recAcc * 0.5f * 2048) / 2048); recAcc = 0; }
            } else if (recLen < recMax) d[recLen++] = toS16 (src);
            if (recLen >= recMax) recStop ();
        }
        ++frames;
    }
}

} // namespace pocket
