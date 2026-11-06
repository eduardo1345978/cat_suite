// Cat Box Pocket: keys, encoders, the OLED and the LEDs (Pocket Operator style: hold a key, press a pad).
#include "pocket.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace pocket {

// 5x7 font, ASCII 32..95 (lower case is drawn as upper case), one byte per column, bit 0 = top
static const uint8_t FONT[64][5] = {
    {0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},{0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0,0x50,0x30,0,0},{0x08,0x08,0x08,0x08,0x08},{0,0x60,0x60,0,0},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
    {0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{0x08,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0,0x41,0x7F,0x41,0},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0,0x7F,0x41,0x41,0},{0x02,0x04,0x08,0x10,0x20},{0,0x41,0x41,0x7F,0},
    {0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40}};

static const char* FXNAME[17] = {"", "LOOP 1/16", "LOOP 1/8", "LOOP 1/4", "LOOP 1/2", "STUTTER", "TAPE STOP", "REVERSE", "SCRATCH",
                                 "BITCRUSH", "FILTER DOLE", "FILTER HORE", "OKTAVA HORE", "OKTAVA DOLE", "RESTART", "FREEZE", "POL TEMPA"};
static const char* PAGE[5][2] = {{"PITCH", "FILTER"}, {"START", "KONIEC"}, {"HLAS", "MOD"}, {"STRETCH", "FREEZE"}, {"DELAY", "REVERB"}};

void Ui::say (const char* m, uint32_t ms) { std::snprintf (msg, sizeof msg, "%s", m); msgTime = now + ms; }

static void slotName (int s, char* out) { out[0] = (char) ('A' + (s / 16) % NBANK); out[1] = (char) ('0' + (s % 16 + 1) / 10); out[2] = (char) ('0' + (s % 16 + 1) % 10); out[3] = 0; }

// ------------------------------------------------------------------ keys
void Ui::key (int k, bool down) {
    if (k < 0 || k >= NKEY) return;
    held[k] = down;
    char t[24];
    if (k < 16) {
        const int pad = k, slot = bank * 16 + pad;
        if (down) {
            if (held[K_REC] && ! held[K_PLAY]) {
                sel = slot; eng.recStart (slot, false); recUsed = true;
                slotName (slot, t); char m[32]; std::snprintf (m, sizeof m, "REC %s", t); say (m, 600);
            } else if (held[K_SOUND]) {
                soundTouched = true;
                if (held[K_SHIFT] && pad < NBANK) { bank = pad; std::snprintf (t, sizeof t, "BANKA %c", 'A' + pad); say (t); }
                else { sel = slot; eng.noteOn (slot, slot); }
            } else if (held[K_PATTERN]) {
                patHeldPressed = true;
                if (chainN < NCHAIN) chainBuf[chainN++] = (uint8_t) pad;
            } else if (held[K_BPM]) {
                bpmTouched = true;
                if (pad == 0) { eng.lofi = ! eng.lofi; say (eng.lofi ? "LO-FI 24K 12BIT" : "CISTY 48K"); }
                else if (pad >= 12) { eng.master = 0.25f * (pad - 11); std::snprintf (t, sizeof t, "HLASITOST %d%%", (pad - 11) * 25); say (t); }
            } else if (held[K_FX]) {
                fxTouched = true; eng.fx = pad + 1;
                if (held[K_WRITE] && eng.playing () && eng.step >= 0) eng.pats[eng.pat].s[eng.step].fx = (uint8_t) (pad + 1);
                say (FXNAME[pad + 1], 800);
            } else if (stepMode && ! keysMode) {
                heldStep = pad; stepTouched = false;
                if (! eng.hasTrig (eng.pat, pad, sel)) { eng.toggleStep (pad, sel); stepTouched = true; }   // created: keep it
                eng.noteOn (sel, sel);
            } else if (stepMode && keysMode) {
                // keys in step mode: the held step gets this note
                const Trig* tr = heldStep >= 0 ? eng.trig (eng.pat, heldStep, sel) : nullptr;
                if (tr) { const_cast<Trig*> (tr)->pitch = (int8_t) pad; stepTouched = true; }
                eng.noteOn (sel, 100 + pad, (float) pad);
            } else if (keysMode) {
                eng.noteOn (sel, 100 + pad, (float) pad);
                if (held[K_WRITE]) { eng.liveRecord (sel, (int8_t) pad); writeLive = true; }
            } else if (held[K_SHIFT]) {
                shiftTouched = true; sel = slot; eng.rollSlot = slot; eng.rollPad = slot;
            } else {
                sel = slot; eng.noteOn (slot, slot);
                if (held[K_WRITE]) { eng.liveRecord (slot, NOLOCK); writeLive = true; }
            }
        } else {
            if (eng.recording () && eng.recSlot == slot && ! eng.looper ()) eng.recStop ();
            if (eng.fx == pad + 1) eng.fx = 0;
            if (heldStep == pad) {
                if (! stepTouched) eng.toggleStep (pad, sel);
                heldStep = -1;
            }
            if (eng.rollSlot == slot) eng.rollSlot = -1;
            eng.noteOff (slot); eng.noteOff (100 + pad);
        }
        return;
    }
    switch (k) {
        case K_PLAY:
            if (! down) break;
            if (held[K_REC]) { eng.recStart (sel, true); recUsed = true; say ("RESAMPLING", 600); }
            else if (held[K_SHIFT]) {
                shiftTouched = true; eng.looperNext (sel);
                say (eng.looper () == Engine::LOOP_REC ? "LOOPER: NAHRAVAM" : eng.looper () == Engine::LOOP_PLAY ? "LOOPER: HRA" : "LOOPER: VYPNUTY");
            } else eng.play (! eng.playing ());
            break;
        case K_REC:
            if (down) {
                recUsed = false;
                if (held[K_SHIFT]) {
                    shiftTouched = true; recUsed = true;
                    const int b = eng.chop (sel);
                    if (b < 0) say (eng.slots[sel].empty () ? "PRAZDNY ZVUK" : "ZIADNA VOLNA BANKA");
                    else { bank = b; sel = b * 16; std::snprintf (t, sizeof t, "CHOP -> BANKA %c", 'A' + b); say (t); }
                }
            } else if (eng.recording ()) eng.recStop ();
            break;
        case K_WRITE:
            if (down) {
                writeLive = false;
                if (held[K_SHIFT]) { shiftTouched = true; wantSave = true; writeLive = true; say ("UKLADAM..."); }
            } else if (! writeLive && ! fxTouchedWhileWrite) {
                stepMode = ! stepMode; say (stepMode ? "KROKY" : "HRANIE", 600);
            }
            fxTouchedWhileWrite = false;
            break;
        case K_SOUND:
            if (down) {
                soundTouched = false;
                if (held[K_SHIFT]) { shiftTouched = true; soundTouched = true; keysMode = ! keysMode; say (keysMode ? "KLAVESY" : "ZVUKY", 600); }
            }
            break;
        case K_PATTERN:
            if (down) {
                chainN = 0; patHeldPressed = false;
                if (held[K_SHIFT]) { shiftTouched = true; eng.chaos (chaosAmt); say ("CHAOS!", 600); }
            } else if (chainN > 0) {
                eng.setChain (chainBuf, chainN);
                if (chainN == 1) std::snprintf (t, sizeof t, "PATTERN %d", chainBuf[0] + 1);
                else std::snprintf (t, sizeof t, "RETAZ %d PATTERNOV", chainN);
                say (t);
            }
            break;
        case K_BPM:
            if (down) {
                bpmTouched = false;
                // tap tempo
                if (ntap > 0 && now - taps[(ntap - 1) % 4] > 2000) ntap = 0;
                taps[ntap % 4] = now; ++ntap;
                if (ntap >= 2) {
                    const int n = std::min (ntap, 4);
                    const uint32_t span = taps[(ntap - 1) % 4] - taps[(ntap - n) % 4];
                    const float b = 60000.0f * (n - 1) / std::max<uint32_t> (span, 1);
                    if (b >= 60 && b <= 200) { eng.bpm = std::round (b); std::snprintf (t, sizeof t, "TEMPO %d", (int) eng.bpm); say (t); }
                }
            }
            break;
        case K_FX:
            if (down) { fxTouched = false; if (held[K_WRITE]) fxTouchedWhileWrite = true; }
            else eng.fx = 0;
            break;
        case K_ENCA: if (down) page = (page + 1) % 5; break;
        case K_ENCB: if (down) page = (page + 4) % 5; break;
        case K_SHIFT: if (down) shiftTouched = false; break;
        default: break;
    }
}

// ------------------------------------------------------------------ encoders
void Ui::encoder (int which, int d) {
    char t[24];
    Slot& s = eng.slots[sel];
    if (held[K_BPM]) {
        bpmTouched = true;
        if (which == 0) { eng.bpm = std::max (60.0f, std::min (200.0f, eng.bpm + d)); std::snprintf (t, sizeof t, "TEMPO %d", (int) eng.bpm); }
        else { eng.swing = std::max (0.0f, std::min (0.5f, eng.swing + d * 0.02f)); std::snprintf (t, sizeof t, "SWING %d%%", (int) std::round (50 + eng.swing * 50)); }
        say (t);
    } else if (held[K_FX]) {
        fxTouched = true;
        float& v = which == 0 ? eng.delayAmt : eng.reverbAmt;
        v = std::max (0.0f, std::min (1.0f, v + d * 0.05f));
        std::snprintf (t, sizeof t, "%s %d%%", which == 0 ? "DELAY" : "REVERB", (int) std::round (v * 100)); say (t);
    } else if (held[K_PATTERN] && held[K_SHIFT]) {
        chaosAmt = std::max (0.05f, std::min (1.0f, chaosAmt + d * 0.05f));
        std::snprintf (t, sizeof t, "CHAOS %d%%", (int) std::round (chaosAmt * 100)); say (t);
    } else if (stepMode && heldStep >= 0) {
        Trig* tr = eng.trig (eng.pat, heldStep, sel);
        if (! tr) return;
        stepTouched = true;
        if (which == 0) { int p = tr->pitch == NOLOCK ? 0 : tr->pitch; p = std::max (-24, std::min (24, p + d)); tr->pitch = (int8_t) p; std::snprintf (t, sizeof t, "KROK PITCH %+d", p); }
        else { int f = tr->filter == NOLOCK ? 0 : tr->filter; f = std::max (-100, std::min (100, f + d * 5)); tr->filter = (int8_t) f; std::snprintf (t, sizeof t, "KROK FILTER %+d", f); }
        say (t);
    } else if (stepMode && held[K_SHIFT]) {
        shiftTouched = true;
        if (which == 0) euclidHits = std::max (0, std::min (16, euclidHits + d)); else euclidRot = (euclidRot + d + 16) % 16;
        eng.euclid (sel, euclidHits, euclidRot);
        std::snprintf (t, sizeof t, "EUKLID %d/16 +%d", euclidHits, euclidRot); say (t);
    } else if (held[K_SHIFT]) {
        shiftTouched = true;
        if (which == 0) { eng.master = std::max (0.0f, std::min (1.0f, eng.master + d * 0.05f)); std::snprintf (t, sizeof t, "HLASITOST %d%%", (int) std::round (eng.master * 100)); }
        else { eng.inGain = std::max (0.1f, std::min (8.0f, eng.inGain * (d > 0 ? 1.12f : 0.89f))); std::snprintf (t, sizeof t, "VSTUP x%.1f", eng.inGain); }
        say (t);
    } else if (eng.rollSlot >= 0 && which == 1) {
        static const float divs[5] = {2, 3, 4, 6, 8};
        int i = 0; while (i < 4 && divs[i] != eng.rollDiv) ++i;
        i = std::max (0, std::min (4, i + (d > 0 ? 1 : -1))); eng.rollDiv = divs[i];
        std::snprintf (t, sizeof t, "ROLL 1/%d", (int) (eng.rollDiv * 4)); say (t);
    } else {
        switch (page) {
            case 0: if (which == 0) s.pitch = std::max (-24.0f, std::min (24.0f, s.pitch + d));
                    else s.filter = std::max (-1.0f, std::min (1.0f, s.filter + d * 0.04f));
                    if (std::fabs (s.filter) < 0.02f) s.filter = 0; break;
            case 1: if (which == 0) s.trimA = std::max (0.0f, std::min (s.trimB - 0.01f, s.trimA + d * 0.01f));
                    else s.trimB = std::max (s.trimA + 0.01f, std::min (1.0f, s.trimB + d * 0.01f)); break;
            case 2: if (which == 0) s.volume = std::max (0.0f, std::min (1.0f, s.volume + d * 0.05f));
                    else { int m = (s.reverse ? 1 : 0) + (s.loopHold ? 2 : 0); m = (m + (d > 0 ? 1 : 3)) % 4; s.reverse = m & 1; s.loopHold = m & 2; } break;
            case 3: if (which == 0) s.stretch = d > 0; else s.freeze = d > 0; break;
            case 4: if (which == 0) s.delay = std::max (0.0f, std::min (1.0f, s.delay + d * 0.05f));
                    else s.reverb = std::max (0.0f, std::min (1.0f, s.reverb + d * 0.05f)); break;
        }
    }
}

void Ui::tick (uint32_t ms) {
    now = ms;
    if (msgTime && now >= msgTime) msgTime = 0;
}

// ------------------------------------------------------------------ the screen
namespace {
struct Fb {
    uint8_t* p;
    void px (int x, int y, bool on = true) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
        uint8_t& b = p[(y >> 3) * 128 + x];
        if (on) b |= (uint8_t) (1 << (y & 7)); else b &= (uint8_t) ~(1 << (y & 7));
    }
    void hline (int x0, int x1, int y, bool on = true) { for (int x = x0; x <= x1; ++x) px (x, y, on); }
    void vline (int x, int y0, int y1, bool on = true) { if (y0 > y1) std::swap (y0, y1); for (int y = y0; y <= y1; ++y) px (x, y, on); }
    void rect (int x, int y, int w, int h, bool fill, bool on = true) {
        for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i)
            if (fill || i == 0 || j == 0 || i == w - 1 || j == h - 1) px (x + i, y + j, on);
    }
    int text (int x, int y, const char* s, bool on = true) {
        for (; *s; ++s) {
            int c = (unsigned char) *s; if (c >= 'a' && c <= 'z') c -= 32;
            if (c < 32 || c > 95) c = '?';
            for (int col = 0; col < 5; ++col) for (int row = 0; row < 7; ++row)
                if (FONT[c - 32][col] >> row & 1) px (x + col, y + row, on);
            x += 6;
        }
        return x;
    }
};
}

void Ui::render (uint8_t* buf) const {
    std::memset (buf, 0, 1024);
    Fb f { buf };
    char t[32], nm[4];
    if (usbDisk) {
        f.text (40, 12, "USB DISK");
        f.text (4, 30, "KOPIRUJ WAV Z PC");
        f.text (4, 42, "VYSUN DISK ALEBO");
        f.text (4, 52, "PLAY = HOTOVO");
        return;
    }
    // top line: slot, mode, pattern, tempo
    slotName (sel, nm);
    f.text (0, 0, nm);
    const char* mode = keysMode ? (stepMode ? "KR+KL" : "KLAV") : (stepMode ? "KROKY" : "");
    f.text (24, 0, mode);
    std::snprintf (t, sizeof t, "P%d", eng.pat + 1); f.text (68, 0, t);
    std::snprintf (t, sizeof t, "%d", (int) eng.bpm); f.text (128 - 6 * (int) std::strlen (t), 0, t);
    if (eng.playing ()) { for (int i = 0; i < 4; ++i) f.vline (88 + i, 1 + i, 7 - i); }   // a play triangle
    if (eng.lofi) f.text (96, 0, "LF");
    f.hline (0, 127, 9);
    // middle: recording / steps / waveform
    const Slot& s = eng.slots[sel];
    if (eng.recording ()) {
        std::snprintf (t, sizeof t, "REC %.1fS", eng.recSeconds ()); f.text (34, 16, t);
        const int w = (int) std::min (120.0f, eng.level * 240); f.rect (4, 30, 120, 8, false); f.rect (4, 30, w, 8, true);
        std::snprintf (t, sizeof t, "%s  VOLNE %dS", eng.recSlot >= 0 && lineIn ? "LINE" : "MIC", (int) eng.memoryFree ()); f.text (16, 42, t);
    } else if (stepMode) {
        for (int i = 0; i < NSTEP; ++i) {
            const int x = 2 + i * 8 - (i >= 8 ? 0 : 0), y = i < 8 ? 14 : 28;
            const int xx = 4 + (i % 8) * 15;
            (void) x;
            const bool on = eng.hasTrig (eng.pat, i, sel);
            f.rect (xx, y, 12, 11, on);
            if (i == eng.step && eng.playing ()) f.rect (xx + 3, y + 3, 6, 5, ! on);
            if (eng.pats[eng.pat].s[i].fx) f.px (xx + 6, y + 12);
        }
    } else if (! s.empty ()) {
        const int16_t* d; const uint32_t len = eng.slotData (sel, &d);
        const int y0 = 12, h = 30;
        for (int x = 0; x < 128; ++x) {
            const uint32_t a = (uint64_t) len * x / 128, b = std::max<uint32_t> (a + 1, (uint64_t) len * (x + 1) / 128);
            int lo = 0, hi = 0;
            for (uint32_t i = a; i < b; i += std::max<uint32_t> (1, (b - a) / 32)) { lo = std::min<int> (lo, d[i]); hi = std::max<int> (hi, d[i]); }
            f.vline (x, y0 + h / 2 - hi * h / 65536, y0 + h / 2 - lo * h / 65536);
        }
        const int xa = (int) (s.trimA * 127), xb = (int) (s.trimB * 127);
        for (int y = y0; y < y0 + h; y += 2) { f.px (xa, y); f.px (xb, y); }
        std::snprintf (t, sizeof t, "%.1fS", s.len / float (s.rate)); f.text (128 - 6 * (int) std::strlen (t), y0 + h - 7, t);
    } else {
        f.text (16, 18, "PRAZDNY ZVUK");
        f.text (10, 30, "REC + PAD = NAHRAJ");
    }
    // bottom: the encoder page or what the held key does
    f.hline (0, 127, 45);
    if (held[K_BPM]) {
        std::snprintf (t, sizeof t, "TEMPO %d", (int) eng.bpm); f.text (0, 48, t);
        std::snprintf (t, sizeof t, "SWING %d%%", (int) std::round (50 + eng.swing * 50)); f.text (0, 57, t);
        if (battery >= 0) { std::snprintf (t, sizeof t, "BAT %d%%", battery); f.text (80, 57, t); }
        std::snprintf (t, sizeof t, "%dS", (int) eng.memoryFree ()); f.text (80, 48, t);
    } else if (held[K_FX]) {
        std::snprintf (t, sizeof t, "DELAY %d%%", (int) std::round (eng.delayAmt * 100)); f.text (0, 48, t);
        std::snprintf (t, sizeof t, "REVERB %d%%", (int) std::round (eng.reverbAmt * 100)); f.text (0, 57, t);
    } else {
        char a[16] = "", b[16] = "";
        switch (page) {
            case 0: std::snprintf (a, sizeof a, "%+d", (int) s.pitch); std::snprintf (b, sizeof b, "%s%d", s.filter < 0 ? "LP " : s.filter > 0 ? "HP " : "", (int) std::round (std::fabs (s.filter) * 100)); break;
            case 1: std::snprintf (a, sizeof a, "%d%%", (int) std::round (s.trimA * 100)); std::snprintf (b, sizeof b, "%d%%", (int) std::round (s.trimB * 100)); break;
            case 2: std::snprintf (a, sizeof a, "%d%%", (int) std::round (s.volume * 100)); std::snprintf (b, sizeof b, "%s", s.reverse ? (s.loopHold ? "REV+LOOP" : "REVERSE") : (s.loopHold ? "LOOP" : "NORMAL")); break;
            case 3: std::snprintf (a, sizeof a, "%s", s.stretch ? "ZAP" : "VYP"); std::snprintf (b, sizeof b, "%s", s.freeze ? "ZAP" : "VYP"); break;
            case 4: std::snprintf (a, sizeof a, "%d%%", (int) std::round (s.delay * 100)); std::snprintf (b, sizeof b, "%d%%", (int) std::round (s.reverb * 100)); break;
        }
        f.text (0, 48, PAGE[page][0]); f.text (0, 57, a);
        f.text (66, 48, PAGE[page][1]); f.text (66, 57, b);
        for (int i = 0; i < 5; ++i) f.px (60 + 0, 48 + i * 3, i == page);
    }
    // a message over everything
    if (msgTime) {
        const int w = 6 * (int) std::strlen (msg) + 8, x = (128 - w) / 2;
        f.rect (x - 1, 22, w + 2, 15, true, false);
        f.rect (x, 23, w, 13, false);
        f.text (x + 4, 26, msg);
    }
}

void Ui::leds (uint8_t* rgb) const {
    std::memset (rgb, 0, 48);
    auto set = [&] (int i, int r, int g, int b) { rgb[i * 3] = (uint8_t) r; rgb[i * 3 + 1] = (uint8_t) g; rgb[i * 3 + 2] = (uint8_t) b; };
    if (eng.fx) { set (eng.fx - 1, 160, 0, 160); return; }
    if (stepMode) {
        for (int i = 0; i < NSTEP; ++i) if (eng.hasTrig (eng.pat, i, sel)) set (i, 60, 22, 0);
    } else {
        for (int i = 0; i < 16; ++i) if (! eng.slots[bank * 16 + i].empty ()) set (i, 0, 14, 4);
        if (sel / 16 == bank) set (sel % 16, 90, 40, 0);
    }
    if (eng.playing () && eng.step >= 0) set (eng.step, 120, 120, 120);
    if (eng.recording () && eng.recSlot / 16 == bank) set (eng.recSlot % 16, 200, 0, 0);
}

} // namespace pocket
