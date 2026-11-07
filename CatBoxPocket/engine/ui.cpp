// Cat Box Pocket: keys, encoders, the OLED and the LEDs (Pocket Operator style: hold a key, press a pad).
// 16 pads, 20 function keys, 4 encoders with pushes and a crossfader.
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
static const char* PAGENAME[5] = {"ZVUK", "VZORKA", "OBALKA", "LFO", "EFEKTY"};
static const char* PAGE[5][4] = {{"LAD", "FILT", "HLAS", "PAN"}, {"STAR", "KONI", "MOD", "STRE"}, {"ATAK", "DEC", "DRIV", "CHOK"},
                                 {"LFO", "HLBK", "CIEL", "VINT"}, {"DLY", "REV", "DUCK", "SIDE"}};
static const char* ARPNAME[6] = {"VYP", "HORE", "DOLE", "HORE-DOLE", "NAHODNE", "AKO HRAS"};
static const char* SYNCNAME[3] = {"INTERNY", "MIDI CLOCK", "SYNC JACK"};
static const char* NOTENAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H"};

enum MenuItem { M_PROJ, M_LOAD, M_SAVE, M_BROWSE, M_EXPPAT, M_EXPSONG, M_MIDI, M_SYNC, M_TRIM, M_THRESH, M_MONITOR, M_METRO,
                M_SCALE, M_ROOT, M_ARP, M_DISK, M_USB, NMENU };

const char* Ui::menuName (int i) {
    static const char* N[NMENU] = {"PROJEKT", "NACITAJ PROJEKT", "ULOZ PROJEKT", "PREHLIADAC WAV", "EXPORT PATTERN", "EXPORT SONG",
                                   "MIDI KANAL", "SYNC", "AUTO TRIM", "PRAH NAHRAVANIA", "MONITOR VSTUPU", "METRONOM",
                                   "STUPNICA", "TONINA", "ARPEGGIO", "DISK", "USB DISK"};
    return i >= 0 && i < NMENU ? N[i] : "";
}

const char* Ui::help (int k) {
    if (k >= 0 && k < 16) return "PAD: HRA ZVUK. V REZIME KROKOV ZAPNE ALEBO VYPNE KROK. V KLAVESOCH HRA TONY STUPNICE.";
    switch (k) {
        case K_SOUND: return "SOUND + PAD: VYBER ZVUK. +SHIFT+PAD 1-4: BANKA. SHIFT+SOUND: KLAVESY. SOUND+A..D: ARP, RYCHLOST, STUPNICA, TONINA.";
        case K_PATTERN: return "PATTERN + PADY: PATTERN ALEBO RETAZ. SHIFT+PATTERN: CHAOS (SHIFT+PATTERN+A: MIERA).";
        case K_BPM: return "TAP TEMPO. BPM+A..D: TEMPO, SWING, KOMPRESOR, HLASITOST. PAD 1 LO-FI, 2 METRONOM, 3 MONITOR, 4 PRAH.";
        case K_FX: return "FX + PAD: 16 EFEKTOV KYM DRZIS. S WRITE ZAPISE DO KROKU. FX+A..D: DELAY, REVERB, SIDECHAIN, VSTUP.";
        case K_SHIFT: return "DRUHE FUNKCIE. SHIFT+PAD: ROLL. SHIFT+A/B: HLASITOST, VSTUP. PRI ZAPNUTI: USB DISK.";
        case K_REC: return "REC+PAD: NAHRAVA KYM DRZIS. REC+PLAY: RESAMPLING. SHIFT+REC: CHOP DO VOLNEJ BANKY.";
        case K_PLAY: return "SPUSTI / ZASTAVI. SHIFT+PLAY: LOOPER (NAHRAVA, HRA, VYPNUTY).";
        case K_WRITE: return "REZIM KROKOV. DRZ WRITE A HRAJ: LIVE NAHRAVANIE. SHIFT+WRITE: ULOZ PROJEKT.";
        case K_MUTE: return "MUTE+PAD: STLMI ZVUK. SHIFT+MUTE+PAD: SOLO. SHIFT+MUTE: ZRUS VSETKY.";
        case K_COPY: return "COPY+PAD: SKOPIRUJ ZVUK (V KROKOCH KROK). SAMOTNE COPY: CELY PATTERN.";
        case K_PASTE: return "PASTE+PAD: VLOZ ZVUK ALEBO KROK. SAMOTNE PASTE: VLOZ PATTERN.";
        case K_CLEAR: return "CLEAR+PAD: ZMAZ KROKY ZVUKU (V KROKOCH KROK). SHIFT+CLEAR+PAD: ZMAZ VZORKU. SAMOTNE: PATTERN.";
        case K_UNDO: return "VRATI POSLEDNU ZMENU PATTERNU A ZVUKOV. ESTE RAZ = ZNOVA.";
        case K_SONG: return "SONG: PIESEN ZAP/VYP. SONG+PAD: PRIDAJ PATTERN. SONG+A: OPAKOVANIA. SHIFT+SONG: ZMAZ.";
        case K_FILL: return "FILL KYM DRZIS (KROKY S PODMIENKOU FILL). SHIFT+FILL: ZAMKNI.";
        case K_ACCENT: return "DRZ PRI HRANI: PLNA SILA. DRZ KROK + ACCENT: PRIZVUK NA KROKU.";
        case K_KIT: return "KIT+PAD: NACITAJ KIT DO BANKY. SHIFT+KIT+PAD: ULOZ BANKU AKO KIT.";
        case K_MENU: return "MENU: PROJEKTY, PREHLIADAC, EXPORT, MIDI, SYNC... MENU + KLAVES: NAPOVEDA.";
        case K_SCENE: return "SCENE+PAD 1/2: ULOZ SCENU A/B. PAD 3: ZRUS. CROSSFADER (ALEBO SCENE+A) PRELIEVA.";
        case K_LEN: return "LEN+PAD: DLZKA PATTERNU (4-64). LEN+A: PRESNE. LEN+B: VLASTNA DLZKA ZVUKU (POLYMETER).";
        case K_ENCA: return "A: PARAMETER. STLAC: DALSIA STRANKA. V MENU: POTVRD.";
        case K_ENCB: return "B: PARAMETER. STLAC: PREDOSLA STRANKA.";
        case K_ENCC: return "C: PARAMETER. STLAC: OREZ TICHO A NORMALIZUJ ZVUK.";
        case K_ENCD: return "D: PARAMETER. STLAC: STRANKA KROKOV (1-16, 17-32...).";
        default: return "";
    }
}

void Ui::say (const char* m, uint32_t ms) { std::snprintf (msg, sizeof msg, "%s", m); msgTime = now + ms; if (! msgTime) msgTime = 1; }

static void slotName (int s, char* out) { out[0] = (char) ('A' + (s / 16) % NBANK); out[1] = (char) ('0' + (s % 16 + 1) / 10); out[2] = (char) ('0' + (s % 16 + 1) % 10); out[3] = 0; }
static float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

// ------------------------------------------------------------------ pads
void Ui::padDown (int pad) {
    const int slot = bank * 16 + pad;
    char t[32], nm[4];
    if (held[K_REC] && ! held[K_PLAY]) {
        sel = slot; eng.recStart (slot, false);
        slotName (slot, nm); std::snprintf (t, sizeof t, eng.threshold ? "REC %s (CAKAM)" : "REC %s", nm); say (t, 600);
    } else if (held[K_SOUND]) {
        if (held[K_SHIFT] && pad < NBANK) { bank = pad; std::snprintf (t, sizeof t, "BANKA %c", 'A' + pad); say (t); }
        else { sel = slot; eng.arpSlot = slot; eng.noteOn (slot, slot, 0, vel () / 100.0f); }
    } else if (held[K_PATTERN]) {
        if (chainN < NCHAIN) chainBuf[chainN++] = (uint8_t) pad;
    } else if (held[K_BPM]) {
        if (pad == 0) { eng.lofi = ! eng.lofi; say (eng.lofi ? "LO-FI 24K 12BIT" : "CISTY 48K"); }
        else if (pad == 1) { eng.metronome = ! eng.metronome; say (eng.metronome ? "METRONOM ZAP" : "METRONOM VYP"); }
        else if (pad == 2) { eng.monitor = ! eng.monitor; say (eng.monitor ? "MONITOR ZAP" : "MONITOR VYP"); }
        else if (pad == 3) { eng.threshold = ! eng.threshold; say (eng.threshold ? "REC CAKA NA ZVUK" : "REC HNED"); }
        else if (pad >= 12) { eng.master = 0.25f * (pad - 11); std::snprintf (t, sizeof t, "HLASITOST %d%%", (pad - 11) * 25); say (t); }
    } else if (held[K_FX]) {
        eng.fx = pad + 1;
        if (held[K_WRITE] && eng.playing () && eng.step >= 0) eng.pats[eng.pat].s[eng.step].fx = (uint8_t) (pad + 1);
        say (FXNAME[pad + 1], 800);
    } else if (held[K_MUTE]) {
        if (held[K_SHIFT]) {          // solo: mute the other sounds of the bank
            for (int i = 0; i < 16; ++i) eng.slots[bank * 16 + i].mute = i != pad;
            slotName (slot, nm); std::snprintf (t, sizeof t, "SOLO %s", nm); say (t);
        } else {
            eng.slots[slot].mute = ! eng.slots[slot].mute;
            slotName (slot, nm); std::snprintf (t, sizeof t, "%s %s", nm, eng.slots[slot].mute ? "STLMENY" : "HRA"); say (t, 700);
        }
    } else if (held[K_COPY]) {
        if (stepMode) { clipStep = eng.pats[eng.pat].s[stepOf (pad)]; clipKind = 1; say ("KROK SKOPIROVANY", 700); }
        else { clipSlot = eng.slots[slot]; clipKind = 2; slotName (slot, nm); std::snprintf (t, sizeof t, "%s SKOPIROVANY", nm); say (t, 700); }
    } else if (held[K_PASTE]) {
        eng.snapshot ();
        if (stepMode && clipKind == 1) { eng.pats[eng.pat].s[stepOf (pad)] = clipStep; say ("KROK VLOZENY", 700); }
        else if (clipKind == 2 && ! clipSlot.empty () && clipSlot.block >= 0 && eng.blocks[clipSlot.block].refs > 0) {
            // the same sample, its own parameters (the copy shares the memory)
            if (eng.slots[slot].block != clipSlot.block || eng.slots[slot].empty ()) {
                eng.clearSlot (slot);
                if (eng.blocks[clipSlot.block].refs > 0) { eng.blocks[clipSlot.block].refs++; eng.slots[slot] = clipSlot; }
            } else eng.slots[slot] = clipSlot;
            eng.slots[slot].saved = false; eng.slots[slot].mute = false;
            sel = slot; slotName (slot, nm); std::snprintf (t, sizeof t, "VLOZENE DO %s", nm); say (t, 700);
        } else say ("SCHRANKA PRAZDNA");
    } else if (held[K_CLEAR]) {
        eng.snapshot ();
        if (stepMode) { eng.pats[eng.pat].s[stepOf (pad)] = Step (); say ("KROK ZMAZANY", 700); }
        else if (held[K_SHIFT]) { eng.clearSlot (slot); slotName (slot, nm); std::snprintf (t, sizeof t, "%s ZMAZANY", nm); say (t); }
        else {
            for (int st = 0; st < NSTEP; ++st) if (eng.hasTrig (eng.pat, st, slot)) eng.toggleStep (st, slot);
            slotName (slot, nm); std::snprintf (t, sizeof t, "KROKY %s PREC", nm); say (t, 700);
        }
    } else if (held[K_SONG]) {
        eng.songAdd (pad); std::snprintf (t, sizeof t, "SONG: %d. RIADOK P%d", eng.songLen, pad + 1); say (t, 700);
    } else if (held[K_KIT]) {
        wantKit = pad; kitSave = held[K_SHIFT];
        std::snprintf (t, sizeof t, kitSave ? "UKLADAM KIT %d" : "NACITAVAM KIT %d", pad + 1); say (t);
    } else if (held[K_SCENE]) {
        if (pad == 0 || pad == 1) { eng.sceneStore (pad); say (pad == 0 ? "SCENA A ULOZENA" : "SCENA B ULOZENA"); }
        else if (pad == 2) { eng.sceneClear (); say ("SCENY ZRUSENE"); }
    } else if (held[K_LEN]) {
        if (held[K_SHIFT]) { eng.setSlotLen (eng.pat, sel, pad + 1); slotName (sel, nm); std::snprintf (t, sizeof t, "%s DLZKA %d", nm, eng.slotLen (eng.pat, sel)); }
        else { eng.pats[eng.pat].len = (uint8_t) ((pad + 1) * 4); std::snprintf (t, sizeof t, "PATTERN %d KROKOV", (pad + 1) * 4); }
        say (t);
    } else if (stepMode && ! keysMode) {
        const int st = stepOf (pad);
        heldStep = st; stepTouched = false;
        if (! eng.hasTrig (eng.pat, st, sel)) { eng.snapshot (); eng.toggleStep (st, sel, NOLOCK, vel ()); stepTouched = true; }   // created: keep it
        eng.noteOn (sel, sel, 0, vel () / 100.0f);
    } else if (stepMode && keysMode) {
        // keys in step mode: the held step gets this note
        Trig* tr = heldStep >= 0 ? eng.trig (eng.pat, heldStep, sel) : nullptr;
        if (tr) { tr->pitch = (int8_t) eng.padSemis (pad); stepTouched = true; }
        eng.noteOn (sel, 100 + pad, (float) eng.padSemis (pad), vel () / 100.0f);
    } else if (keysMode) {
        if (eng.arpMode > 0) { eng.arpSlot = sel; eng.arpPress (pad); }
        else eng.noteOn (sel, 100 + pad, (float) eng.padSemis (pad), vel () / 100.0f);
        if (held[K_WRITE]) { eng.liveRecord (sel, (int8_t) eng.padSemis (pad), vel ()); writeLive = true; }
    } else if (held[K_SHIFT]) {
        sel = slot; eng.rollSlot = slot; eng.rollPad = slot;
    } else {
        sel = slot; eng.noteOn (slot, slot, 0, vel () / 100.0f);
        if (held[K_WRITE]) { eng.liveRecord (slot, NOLOCK, vel ()); writeLive = true; }
    }
}

void Ui::padUp (int pad) {
    const int slot = bank * 16 + pad;
    if (eng.recording () && eng.recSlot == slot) eng.recStop ();
    if (eng.fx == pad + 1) eng.fx = 0;
    if (heldStep >= 0 && heldStep == stepOf (pad)) {
        if (! stepTouched) { eng.snapshot (); eng.toggleStep (heldStep, sel); }
        heldStep = -1;
    }
    if (eng.rollSlot == slot) eng.rollSlot = -1;
    eng.arpRelease (pad);
    eng.noteOff (slot); eng.noteOff (100 + pad);
}

// ------------------------------------------------------------------ keys
void Ui::key (int k, bool down) {
    if (k < 0 || k >= NKEY) return;
    if (down) { for (int j = 0; j < NKEY; ++j) if (held[j] && j != k) used[j] = true; used[k] = false; }
    held[k] = down;
    char t[32];
    // MENU + a key: what it does
    if (down && k != K_MENU && held[K_MENU]) { helpKey = k; helpTime = now + 5000; return; }
    if (! down && k != K_MENU && helpKey == k) return;
    // the menu and the browser take the encoders and a few keys
    if (menu) {
        if (k == K_MENU && ! down && ! used[K_MENU]) {
            if (browsing) browsing = false; else menu = false;
            return;
        }
        if (down && (k == K_ENCA || k == K_WRITE)) { menuEnter (); return; }
        if (k == K_PLAY && down) eng.play (! eng.playing ());
        return;
    }
    if (k < 16) { if (down) padDown (k); else padUp (k); return; }
    switch (k) {
        case K_PLAY:
            if (! down) break;
            if (held[K_REC]) { eng.recStart (sel, true); say ("RESAMPLING", 600); }
            else if (held[K_SHIFT]) {
                eng.looperNext (sel);
                say (eng.looper () == Engine::LOOP_REC ? "LOOPER: NAHRAVAM" : eng.looper () == Engine::LOOP_PLAY ? "LOOPER: HRA" : "LOOPER: VYPNUTY");
            } else eng.play (! eng.playing ());
            break;
        case K_REC:
            if (down) {
                if (held[K_SHIFT]) {
                    const int b = eng.chop (sel);
                    if (b < 0) say (eng.slots[sel].empty () ? "PRAZDNY ZVUK" : "ZIADNA VOLNA BANKA");
                    else { bank = b; sel = b * 16; std::snprintf (t, sizeof t, "CHOP -> BANKA %c", 'A' + b); say (t); }
                }
            } else if (eng.recording () && eng.recSlot == sel) eng.recStop ();
            break;
        case K_WRITE:
            if (down) {
                writeLive = false;
                if (held[K_SHIFT]) { wantSave = true; writeLive = true; say ("UKLADAM..."); }
            } else if (! writeLive && ! used[K_WRITE]) {
                stepMode = ! stepMode; heldStep = -1; say (stepMode ? "KROKY" : "HRANIE", 600);
            }
            break;
        case K_SOUND:
            if (down && held[K_SHIFT]) { keysMode = ! keysMode; say (keysMode ? "KLAVESY" : "ZVUKY", 600); }
            break;
        case K_PATTERN:
            if (down) {
                chainN = 0;
                if (held[K_SHIFT]) { eng.snapshot (); eng.chaos (chaosAmt); say ("CHAOS!", 600); }
            } else if (chainN > 0) {
                eng.setChain (chainBuf, chainN);
                if (chainN == 1) std::snprintf (t, sizeof t, "PATTERN %d", chainBuf[0] + 1);
                else std::snprintf (t, sizeof t, "RETAZ %d PATTERNOV", chainN);
                say (t); chainN = 0;
            }
            break;
        case K_BPM:
            if (down) {
                // tap tempo
                if (ntap > 0 && now - taps[(ntap - 1) % 4] > 2000) ntap = 0;
                taps[ntap % 4] = now; ++ntap;
                if (ntap >= 2) {
                    const int n = std::min (ntap, 4);
                    const uint32_t span = taps[(ntap - 1) % 4] - taps[(ntap - n) % 4];
                    const float b = 60000.0f * (n - 1) / std::max<uint32_t> (span, 1);
                    if (b >= 40 && b <= 240) { eng.bpm = std::round (b); std::snprintf (t, sizeof t, "TEMPO %d", (int) eng.bpm); say (t); }
                }
            }
            break;
        case K_FX: if (! down) eng.fx = 0; break;
        case K_MUTE:
            if (! down && ! used[K_MUTE] && held[K_SHIFT]) { for (auto& s : eng.slots) s.mute = false; say ("VSETKO HRA"); }
            break;
        case K_COPY:
            if (! down && ! used[K_COPY]) { clipPat = eng.pats[eng.pat]; clipKind = 3; std::snprintf (t, sizeof t, "PATTERN %d SKOPIROVANY", eng.pat + 1); say (t); }
            break;
        case K_PASTE:
            if (! down && ! used[K_PASTE]) {
                if (clipKind == 3) { eng.snapshot (); eng.pats[eng.pat] = clipPat; std::snprintf (t, sizeof t, "VLOZENE DO P%d", eng.pat + 1); say (t); }
                else say ("SCHRANKA PRAZDNA");
            }
            break;
        case K_CLEAR:
            if (! down && ! used[K_CLEAR]) { eng.snapshot (); eng.clearPattern (eng.pat); std::snprintf (t, sizeof t, "PATTERN %d ZMAZANY", eng.pat + 1); say (t); }
            break;
        case K_UNDO:
            if (down) { eng.undo (); say ("UNDO", 600); }
            break;
        case K_SONG:
            if (down && held[K_SHIFT]) { eng.songLen = 0; eng.songMode = false; say ("SONG ZMAZANY"); }
            else if (! down && ! used[K_SONG]) {
                if (eng.songLen == 0) say ("SONG JE PRAZDNY");
                else { eng.songMode = ! eng.songMode; say (eng.songMode ? "SONG ZAP" : "SONG VYP"); }
            }
            break;
        case K_FILL:
            if (down && held[K_SHIFT]) { fillLatch = ! fillLatch; say (fillLatch ? "FILL ZAMKNUTY" : "FILL VYP"); }
            eng.fill = fillLatch || held[K_FILL];
            break;
        case K_ACCENT:
            if (down && stepMode && heldStep >= 0) {
                if (Trig* tr = eng.trig (eng.pat, heldStep, sel)) { tr->vel = tr->vel >= 120 ? 100 : 127; stepTouched = true; say (tr->vel >= 120 ? "PRIZVUK" : "BEZ PRIZVUKU", 600); }
            }
            break;
        case K_MENU:
            if (! down && ! used[K_MENU]) { menu = true; browsing = false; menuProj = project; }
            if (! down) helpKey = -1;
            break;
        case K_ENCA: if (down) page = (page + 1) % 5; break;
        case K_ENCB: if (down) page = (page + 4) % 5; break;
        case K_ENCC:
            if (down) {
                if (eng.slots[sel].empty ()) say ("PRAZDNY ZVUK");
                else { eng.trimNormalize (sel); say ("OREZANE + NORMALIZ."); }
            }
            break;
        case K_ENCD:
            if (down) {
                const int pages = std::max (1, (std::max<int> (eng.pats[eng.pat].len, eng.slotLen (eng.pat, sel)) + 15) / 16);
                stepPage = (stepPage + 1) % pages;
                std::snprintf (t, sizeof t, "KROKY %d-%d", stepPage * 16 + 1, stepPage * 16 + 16); say (t, 700);
            }
            break;
        default: break;
    }
}

// ------------------------------------------------------------------ the menu
void Ui::menuChange (int d) {
    char t[32];
    switch (menuItem) {
        case M_PROJ: menuProj = (menuProj + d + NPROJ * 4) % NPROJ; break;
        case M_MIDI: eng.midiChannel = (eng.midiChannel + d + 64) % 16; break;
        case M_SYNC: eng.syncMode = (eng.syncMode + d + 30) % 3; break;
        case M_TRIM: eng.autoTrim = ! eng.autoTrim; break;
        case M_THRESH: eng.threshold = ! eng.threshold; break;
        case M_MONITOR: eng.monitor = ! eng.monitor; break;
        case M_METRO: eng.metronome = ! eng.metronome; break;
        case M_SCALE: eng.scale = (eng.scale + d + NSCALE * 4) % NSCALE; break;
        case M_ROOT: eng.root = (eng.root + d + 48) % 12; break;
        case M_ARP: eng.arpMode = (eng.arpMode + d + 60) % 6; break;
        default: return;
    }
    (void) t;
}

void Ui::menuEnter () {
    if (browsing) {
        if (nFiles > 0 && browseIdx < nFiles) { browseLoad = sel; say ("NACITAVAM..."); }
        return;
    }
    switch (menuItem) {
        case M_LOAD: wantLoad = true; menu = false; say ("NACITAVAM..."); break;
        case M_SAVE: wantSave = true; project = menuProj; menu = false; say ("UKLADAM..."); break;
        case M_BROWSE: wantBrowse = true; browsing = true; browseIdx = 0; nFiles = 0; break;
        case M_EXPPAT: wantExport = 1; menu = false; say ("EXPORT...", 4000); break;
        case M_EXPSONG:
            if (eng.songLen == 0) say ("SONG JE PRAZDNY");
            else { wantExport = 2; menu = false; say ("EXPORT...", 4000); }
            break;
        case M_DISK:
            if (! sdPresent) say ("ZIADNA SD KARTA");
            else { wantSwapDisk = true; menu = false; }
            break;
        case M_USB: wantUsb = true; menu = false; break;
        default: menuChange (1); break;
    }
}

// ------------------------------------------------------------------ encoders
void Ui::encoder (int which, int d) {
    for (int j = 0; j < NKEY; ++j) if (held[j]) used[j] = true;
    char t[32], nm[4];
    Slot& s = eng.slots[sel];
    if (menu) {
        if (browsing) { if (nFiles > 0) browseIdx = std::max (0, std::min (nFiles - 1, browseIdx + d)); }
        else if (which == 0) menuItem = std::max (0, std::min (NMENU - 1, menuItem + d));
        else menuChange (d > 0 ? 1 : -1);
        return;
    }
    if (held[K_BPM]) {
        switch (which) {
            case 0: eng.bpm = clampf (eng.bpm + d, 40, 240); std::snprintf (t, sizeof t, "TEMPO %d", (int) eng.bpm); break;
            case 1: eng.swing = clampf (eng.swing + d * 0.02f, 0, 0.5f); std::snprintf (t, sizeof t, "SWING %d%%", (int) std::round (50 + eng.swing * 50)); break;
            case 2: eng.compAmt = clampf (eng.compAmt + d * 0.05f, 0, 1); std::snprintf (t, sizeof t, "KOMPRESOR %d%%", (int) std::round (eng.compAmt * 100)); break;
            default: eng.master = clampf (eng.master + d * 0.05f, 0, 1); std::snprintf (t, sizeof t, "HLASITOST %d%%", (int) std::round (eng.master * 100)); break;
        }
        say (t);
    } else if (held[K_FX]) {
        switch (which) {
            case 0: eng.delayAmt = clampf (eng.delayAmt + d * 0.05f, 0, 1); std::snprintf (t, sizeof t, "DELAY %d%%", (int) std::round (eng.delayAmt * 100)); break;
            case 1: eng.reverbAmt = clampf (eng.reverbAmt + d * 0.05f, 0, 1); std::snprintf (t, sizeof t, "REVERB %d%%", (int) std::round (eng.reverbAmt * 100)); break;
            case 2: eng.duckAmt = clampf (eng.duckAmt + d * 0.05f, 0, 1); std::snprintf (t, sizeof t, "SIDECHAIN %d%%", (int) std::round (eng.duckAmt * 100)); break;
            default: eng.inGain = clampf (eng.inGain * (d > 0 ? 1.12f : 0.89f), 0.1f, 8); std::snprintf (t, sizeof t, "VSTUP x%.1f", eng.inGain); break;
        }
        say (t);
    } else if (held[K_PATTERN] && held[K_SHIFT]) {
        chaosAmt = clampf (chaosAmt + d * 0.05f, 0.05f, 1);
        std::snprintf (t, sizeof t, "CHAOS %d%%", (int) std::round (chaosAmt * 100)); say (t);
    } else if (held[K_LEN]) {
        Pattern& P = eng.pats[eng.pat];
        if (which == 0) { P.len = (uint8_t) std::max (1, std::min (NSTEP, P.len + d)); std::snprintf (t, sizeof t, "PATTERN %d KROKOV", P.len); }
        else { eng.setSlotLen (eng.pat, sel, std::max (1, std::min (NSTEP, eng.slotLen (eng.pat, sel) + d))); slotName (sel, nm); std::snprintf (t, sizeof t, "%s DLZKA %d", nm, eng.slotLen (eng.pat, sel)); }
        say (t);
    } else if (held[K_SONG]) {
        if (eng.songLen > 0) {
            SongRow& r = eng.song[eng.songLen - 1];
            r.repeats = (uint8_t) std::max (1, std::min (16, r.repeats + d));
            std::snprintf (t, sizeof t, "P%d X%d", r.pat + 1, r.repeats); say (t);
        }
    } else if (held[K_SCENE]) {
        fader (clampf (eng.xfade + d * 0.05f, 0, 1));
    } else if (held[K_SOUND]) {
        switch (which) {
            case 0: eng.arpMode = (eng.arpMode + (d > 0 ? 1 : 5)) % 6; std::snprintf (t, sizeof t, "ARP %s", ARPNAME[eng.arpMode]); break;
            case 1: { static const float divs[6] = {1, 2, 3, 4, 6, 8};
                      int i = 0; while (i < 5 && divs[i] != eng.arpDiv) ++i;
                      i = std::max (0, std::min (5, i + (d > 0 ? 1 : -1))); eng.arpDiv = divs[i];
                      std::snprintf (t, sizeof t, "ARP 1/%d", (int) (eng.arpDiv * 4)); break; }
            case 2: eng.scale = (eng.scale + (d > 0 ? 1 : NSCALE - 1)) % NSCALE; std::snprintf (t, sizeof t, "STUPNICA %s", Engine::scaleName (eng.scale)); break;
            default: eng.root = (eng.root + (d > 0 ? 1 : 11)) % 12; std::snprintf (t, sizeof t, "TONINA %s", NOTENAME[eng.root]); break;
        }
        say (t);
    } else if (stepMode && heldStep >= 0) {
        Trig* tr = eng.trig (eng.pat, heldStep, sel);
        if (! tr) return;
        stepTouched = true;
        if (held[K_SHIFT] && which == 0) { int p = tr->pan == NOLOCK ? 0 : tr->pan; p = std::max (-100, std::min (100, p + d * 10)); tr->pan = (int8_t) p; std::snprintf (t, sizeof t, "KROK PAN %+d", p); }
        else switch (which) {
            case 0: { int p = tr->pitch == NOLOCK ? 0 : tr->pitch; p = std::max (-24, std::min (24, p + d)); tr->pitch = (int8_t) p; std::snprintf (t, sizeof t, "KROK PITCH %+d", p); break; }
            case 1: { int f = tr->filter == NOLOCK ? 0 : tr->filter; f = std::max (-100, std::min (100, f + d * 5)); tr->filter = (int8_t) f; std::snprintf (t, sizeof t, "KROK FILTER %+d", f); break; }
            case 2: tr->vel = (uint8_t) std::max (1, std::min (127, tr->vel + d * 4)); std::snprintf (t, sizeof t, "KROK SILA %d", tr->vel); break;
            default: tr->cond = (uint8_t) (((tr->cond + d) % NCOND + NCOND) % NCOND); std::snprintf (t, sizeof t, "PODMIENKA %s", Engine::condName (tr->cond)); break;
        }
        say (t);
    } else if (stepMode && held[K_SHIFT]) {
        const int L = eng.slotLen (eng.pat, sel);
        if (which == 0) euclidHits = std::max (0, std::min (L, euclidHits + d));
        else if (which == 1) euclidRot = (euclidRot + d + L * 8) % L;
        if (which <= 1) { eng.euclid (sel, euclidHits, euclidRot); std::snprintf (t, sizeof t, "EUKLID %d/%d +%d", euclidHits, L, euclidRot); }
        else { eng.nudge (eng.pat, which == 2 ? sel : -1, d > 0 ? 1 : -1); std::snprintf (t, sizeof t, which == 2 ? "POSUN ZVUKU %s" : "POSUN VSETKEHO %s", d > 0 ? ">" : "<"); }
        say (t);
    } else if (held[K_SHIFT]) {
        if (which == 0) { eng.master = clampf (eng.master + d * 0.05f, 0, 1); std::snprintf (t, sizeof t, "HLASITOST %d%%", (int) std::round (eng.master * 100)); }
        else if (which == 1) { eng.inGain = clampf (eng.inGain * (d > 0 ? 1.12f : 0.89f), 0.1f, 8); std::snprintf (t, sizeof t, "VSTUP x%.1f", eng.inGain); }
        else return;
        say (t);
    } else if (eng.rollSlot >= 0 && which == 1) {
        static const float divs[5] = {2, 3, 4, 6, 8};
        int i = 0; while (i < 4 && divs[i] != eng.rollDiv) ++i;
        i = std::max (0, std::min (4, i + (d > 0 ? 1 : -1))); eng.rollDiv = divs[i];
        std::snprintf (t, sizeof t, "ROLL 1/%d", (int) (eng.rollDiv * 4)); say (t);
    } else {
        s.saved = s.saved;       // parameters live in the project file, the WAV stays the same
        switch (page * 4 + which) {
            case 0: s.pitch = clampf (s.pitch + d, -24, 24); break;
            case 1: s.filter = clampf (s.filter + d * 0.04f, -1, 1); if (std::fabs (s.filter) < 0.02f) s.filter = 0; break;
            case 2: s.volume = clampf (s.volume + d * 0.05f, 0, 1); break;
            case 3: s.pan = clampf (s.pan + d * 0.1f, -1, 1); if (std::fabs (s.pan) < 0.05f) s.pan = 0; break;
            case 4: s.trimA = clampf (s.trimA + d * 0.01f, 0, s.trimB - 0.01f); break;
            case 5: s.trimB = clampf (s.trimB + d * 0.01f, s.trimA + 0.01f, 1); break;
            case 6: { int m = (s.reverse ? 1 : 0) + (s.loopHold ? 2 : 0); m = (m + (d > 0 ? 1 : 3)) % 4; s.reverse = m & 1; s.loopHold = m & 2; break; }
            case 7: {   // off, auto, 1, 2, 4, 8, 16 beats, freeze
                int m = s.freeze ? 7 : ! s.stretch ? 0 : s.beats == 0 ? 1 : 2 + (int) std::round (std::log2 ((float) s.beats));
                m = std::max (0, std::min (7, m + d));
                s.freeze = m == 7; s.stretch = m >= 1 && m <= 6; s.beats = (uint8_t) (m >= 2 && m <= 6 ? 1 << (m - 2) : 0);
                break;
            }
            case 8: s.attack = clampf (s.attack + d * 0.01f, 0, 2); break;
            case 9: s.decay = clampf (s.decay + d * 0.05f, 0, 4); break;
            case 10: s.drive = clampf (s.drive + d * 0.05f, 0, 1); break;
            case 11: s.choke = (uint8_t) ((s.choke + (d > 0 ? 1 : 4)) % 5); break;
            case 12: s.lfoRate = clampf (s.lfoRate * (d > 0 ? 1.15f : 0.87f), 0.05f, 30); break;
            case 13: s.lfoDepth = clampf (s.lfoDepth + d * 0.05f, 0, 1); break;
            case 14: s.lfoTarget = (uint8_t) ((s.lfoTarget + (d > 0 ? 1 : 3)) % 4); break;
            case 15: s.vintage = (uint8_t) ((s.vintage + (d > 0 ? 1 : 3)) % 4); break;
            case 16: s.delay = clampf (s.delay + d * 0.05f, 0, 1); break;
            case 17: s.reverb = clampf (s.reverb + d * 0.05f, 0, 1); break;
            case 18: s.duckSource = d > 0; break;
            default: eng.duckAmt = clampf (eng.duckAmt + d * 0.05f, 0, 1); break;
        }
    }
}

void Ui::fader (float x) {
    x = clampf (x, 0, 1);
    if (lastFader >= 0 && std::fabs (x - lastFader) < 0.01f) return;     // the pot jitters
    lastFader = x; eng.xfade = x;
}

void Ui::tick (uint32_t ms) {
    now = ms;
    if (msgTime && now >= msgTime) msgTime = 0;
    if (helpTime && now >= helpTime) { helpTime = 0; helpKey = -1; }
    eng.fill = fillLatch || held[K_FILL];
}

// ------------------------------------------------------------------ disk work (main loop)
void Ui::service () {
    if (! disk || usbDisk) return;
    char t[32];
    if (wantSave) {
        wantSave = false;
        const bool ok = store::save (*disk, eng, *this, project);
        std::snprintf (t, sizeof t, ok ? "ULOZENE: PROJEKT %d" : "PLNY DISK!", project + 1); say (t);
    }
    if (wantLoad) {
        wantLoad = false;
        const int p = menuProj;
        std::snprintf (t, sizeof t, store::load (*disk, eng, *this, p) ? "PROJEKT %d" : "CHYBA DISKU", p + 1); say (t);
    }
    if (wantKit >= 0) {
        const int k = wantKit; wantKit = -1;
        const bool ok = kitSave ? store::saveKit (*disk, eng, bank, k) : store::loadKit (*disk, eng, bank, k);
        if (kitSave) std::snprintf (t, sizeof t, ok ? "KIT %d ULOZENY" : "PLNY DISK!", k + 1);
        else std::snprintf (t, sizeof t, ok ? "KIT %d -> BANKA %c" : "KIT %d NIE JE", k + 1, 'A' + bank);
        say (t);
    }
    if (wantExport) {
        const bool song = wantExport == 2; wantExport = 0;
        char name[13];
        const bool ok = store::exportWav (*disk, eng, song, name);
        std::snprintf (t, sizeof t, ok ? "ULOZENE %s" : "EXPORT ZLYHAL", name); say (t, 2500);
    }
    if (wantBrowse) {
        wantBrowse = false;
        static char names[96][13]; static char longs[96][24]; static uint32_t sizes[96];
        const int n = store::list (*disk, names, sizes, 96, longs);
        nFiles = 0;
        for (int i = 0; i < n; ++i) {
            const char* dot = std::strrchr (names[i], '.');
            if (! dot || std::strcmp (dot, ".WAV")) continue;
            std::memcpy (files[nFiles], names[i], 13); std::memcpy (fileLong[nFiles], longs[i], 24); ++nFiles;
        }
        browseIdx = 0;
        if (nFiles == 0) say ("ZIADNE WAV");
    }
    if (browseLoad >= 0) {
        const int slot = browseLoad; browseLoad = -1;
        char nm[4]; slotName (slot, nm);
        if (browseIdx < nFiles && store::loadWavInto (*disk, files[browseIdx], eng, slot)) { std::snprintf (t, sizeof t, "-> %s", nm); say (t); eng.noteOn (slot, slot); }
        else say ("NEDA SA NACITAT");
    }
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
    // word wrapped text, 21 characters a line
    void para (int y, const char* s) {
        char line[22]; int n = 0;
        while (*s && y < 64) {
            const char* w = s; int wl = 0; while (w[wl] && w[wl] != ' ') ++wl;
            if (n > 0 && n + 1 + wl > 21) { line[n] = 0; text (0, y, line); y += 9; n = 0; }
            if (n > 0) line[n++] = ' ';
            for (int i = 0; i < wl && n < 21; ++i) line[n++] = w[i];
            s = w + wl; while (*s == ' ') ++s;
        }
        if (n > 0 && y < 64) { line[n] = 0; text (0, y, line); }
    }
};
}

static void valueText (const Engine& e, const Slot& s, int page, int i, char* v, int n) {
    static const char* MODE[4] = {"NORM", "REV", "LOOP", "R+L"};
    static const char* TARGET[4] = {"PITCH", "FILTR", "HLAS", "PAN"};
    static const char* VINT[4] = {"VYP", "SP12", "S950", "8BIT"};
    switch (page * 4 + i) {
        case 0: std::snprintf (v, n, "%+d", (int) s.pitch); break;
        case 1: std::snprintf (v, n, "%s%d", s.filter < 0 ? "L" : s.filter > 0 ? "H" : "", (int) std::round (std::fabs (s.filter) * 100)); break;
        case 2: std::snprintf (v, n, "%d", (int) std::round (s.volume * 100)); break;
        case 3: if (s.pan == 0) std::snprintf (v, n, "STRED"); else std::snprintf (v, n, "%c%d", s.pan < 0 ? 'L' : 'P', (int) std::round (std::fabs (s.pan) * 100)); break;
        case 4: std::snprintf (v, n, "%d%%", (int) std::round (s.trimA * 100)); break;
        case 5: std::snprintf (v, n, "%d%%", (int) std::round (s.trimB * 100)); break;
        case 6: std::snprintf (v, n, "%s", MODE[(s.reverse ? 1 : 0) + (s.loopHold ? 2 : 0)]); break;
        case 7: if (s.freeze) std::snprintf (v, n, "FRZ"); else if (! s.stretch) std::snprintf (v, n, "VYP"); else if (s.beats == 0) std::snprintf (v, n, "AUTO"); else std::snprintf (v, n, "%dB", s.beats); break;
        case 8: std::snprintf (v, n, "%dMS", (int) std::round (s.attack * 1000)); break;
        case 9: if (s.decay == 0) std::snprintf (v, n, "VYP"); else std::snprintf (v, n, "%.2f", s.decay); break;
        case 10: std::snprintf (v, n, "%d", (int) std::round (s.drive * 100)); break;
        case 11: if (s.choke == 0) std::snprintf (v, n, "VYP"); else std::snprintf (v, n, "SK %d", s.choke); break;
        case 12: std::snprintf (v, n, s.lfoRate < 10 ? "%.1fHZ" : "%.0fHZ", s.lfoRate); break;
        case 13: std::snprintf (v, n, "%d", (int) std::round (s.lfoDepth * 100)); break;
        case 14: std::snprintf (v, n, "%s", TARGET[s.lfoTarget % 4]); break;
        case 15: std::snprintf (v, n, "%s", VINT[s.vintage % 4]); break;
        case 16: std::snprintf (v, n, "%d", (int) std::round (s.delay * 100)); break;
        case 17: std::snprintf (v, n, "%d", (int) std::round (s.reverb * 100)); break;
        case 18: std::snprintf (v, n, "%s", s.duckSource ? "ZDROJ" : "-"); break;
        default: std::snprintf (v, n, "%d", (int) std::round (e.duckAmt * 100)); break;
    }
}

void Ui::render (uint8_t* buf) const {
    std::memset (buf, 0, 1024);
    Fb f { buf };
    char t[40], nm[4];
    if (usbDisk) {
        f.text (40, 6, "USB DISK");
        f.text (4, 24, sdCard ? "SD KARTA" : "VNUTORNA PAMAT");
        f.text (4, 36, "KOPIRUJ WAV Z PC");
        f.text (4, 48, "VYSUN / PLAY = SPAT");
        return;
    }
    if (helpKey >= 0) {
        f.para (0, help (helpKey));
        return;
    }
    if (menu && browsing) {
        f.text (0, 0, "PREHLIADAC");
        slotName (sel, nm); std::snprintf (t, sizeof t, "-> %s", nm); f.text (128 - 6 * (int) std::strlen (t), 0, t);
        f.hline (0, 127, 9);
        if (nFiles == 0) f.text (4, 24, "ZIADNE WAV SUBORY");
        const int top = std::max (0, std::min (browseIdx - 2, nFiles - 5));
        for (int i = 0; i < 5 && top + i < nFiles; ++i) {
            const int y = 12 + i * 10;
            const bool cur = top + i == browseIdx;
            if (cur) f.rect (0, y - 1, 128, 9, true);
            char line[22]; std::snprintf (line, sizeof line, "%.20s", fileLong[top + i]);
            f.text (2, y, line, ! cur);
        }
        return;
    }
    if (menu) {
        f.text (0, 0, "MENU"); std::snprintf (t, sizeof t, "%d/%d", menuItem + 1, NMENU); f.text (128 - 6 * (int) std::strlen (t), 0, t);
        f.hline (0, 127, 9);
        const int top = std::max (0, std::min (menuItem - 2, NMENU - 5));
        for (int i = 0; i < 5 && top + i < NMENU; ++i) {
            const int it = top + i, y = 12 + i * 10;
            const bool cur = it == menuItem;
            char v[16] = "";
            switch (it) {
                case M_PROJ: std::snprintf (v, sizeof v, "%d", menuProj + 1); break;
                case M_MIDI: std::snprintf (v, sizeof v, "%d", eng.midiChannel + 1); break;
                case M_SYNC: std::snprintf (v, sizeof v, "%s", SYNCNAME[eng.syncMode % 3]); break;
                case M_TRIM: std::snprintf (v, sizeof v, "%s", eng.autoTrim ? "ZAP" : "VYP"); break;
                case M_THRESH: std::snprintf (v, sizeof v, "%s", eng.threshold ? "ZAP" : "VYP"); break;
                case M_MONITOR: std::snprintf (v, sizeof v, "%s", eng.monitor ? "ZAP" : "VYP"); break;
                case M_METRO: std::snprintf (v, sizeof v, "%s", eng.metronome ? "ZAP" : "VYP"); break;
                case M_SCALE: std::snprintf (v, sizeof v, "%s", Engine::scaleName (eng.scale)); break;
                case M_ROOT: std::snprintf (v, sizeof v, "%s", NOTENAME[eng.root % 12]); break;
                case M_ARP: std::snprintf (v, sizeof v, "%s", ARPNAME[eng.arpMode % 6]); break;
                case M_DISK: std::snprintf (v, sizeof v, "%s", sdCard ? "SD" : "FLASH"); break;
                default: break;
            }
            if (cur) f.rect (0, y - 1, 128, 9, true);
            const char* name = menuName (it);
            char line[22];
            if (v[0]) std::snprintf (line, sizeof line, "%.*s", 20 - (int) std::strlen (v), name); else std::snprintf (line, sizeof line, "%.21s", name);
            f.text (2, y, line, ! cur);
            if (v[0]) f.text (126 - 6 * (int) std::strlen (v), y, v, ! cur);
        }
        return;
    }
    // top line: slot, mode, pattern, tempo
    slotName (sel, nm);
    f.text (0, 0, nm);
    const char* mode = keysMode ? (stepMode ? "K+KR" : (eng.arpMode ? "ARP" : "KLAV")) : (stepMode ? "KROK" : "");
    f.text (22, 0, mode);
    std::snprintf (t, sizeof t, "P%d", eng.pat + 1); f.text (50, 0, t);
    if (eng.songMode) f.text (68, 0, "S");
    if (eng.fill) f.text (74, 0, "F");
    std::snprintf (t, sizeof t, "%d", (int) eng.bpm); f.text (128 - 6 * (int) std::strlen (t), 0, t);
    if (eng.playing ()) { for (int i = 0; i < 4; ++i) f.vline (82 + i, 1 + i, 7 - i); }   // a play triangle
    if (eng.lofi) f.text (90, 0, "LF");
    if (eng.syncMode) f.text (90, 0, eng.syncMode == 1 ? "MI" : "SY");
    if (eng.scenes[0].set || eng.scenes[1].set) { f.rect (0, 10, 128, 2, false); f.hline (0, (int) std::round (eng.xfade * 127), 11); }
    f.hline (0, 127, 9);
    // middle: recording / steps / waveform
    const Slot& s = eng.slots[sel];
    if (eng.recording ()) {
        std::snprintf (t, sizeof t, eng.recWaiting () ? "REC: CAKAM..." : "REC %.1fS", eng.recSeconds ()); f.text (28, 16, t);
        const int w = (int) std::min (120.0f, eng.level * 240); f.rect (4, 27, 120, 7, false); f.rect (4, 27, w, 7, true);
        std::snprintf (t, sizeof t, "%s  VOLNE %dS", lineIn ? "LINE" : "MIC", (int) eng.memoryFree ()); f.text (16, 37, t);
    } else if (stepMode) {
        const int L = eng.slotLen (eng.pat, sel);
        for (int i = 0; i < 16; ++i) {
            const int st = stepOf (i), xx = 1 + (i % 8) * 13, y = i < 8 ? 13 : 27;
            if (st >= L) { f.px (xx + 5, y + 5); continue; }
            const Trig* tr = const_cast<Engine&> (eng).trig (eng.pat, st, sel);
            f.rect (xx, y, 11, 11, tr != nullptr);
            if (tr && tr->cond != C_ALWAYS) f.rect (xx + 3, y + 3, 5, 5, false, false);
            if (tr && tr->vel >= 120) f.hline (xx, xx + 10, y + 12);
            if (st == heldStep) f.rect (xx - 1, y - 1, 13, 13, false);
            if (st == eng.step && eng.playing ()) f.rect (xx + 4, y + 4, 3, 3, ! tr);
            if (eng.pats[eng.pat].s[st].fx) f.px (xx + 5, y - 1);
        }
        std::snprintf (t, sizeof t, "%d/%d", stepPage + 1, std::max (1, (L + 15) / 16)); f.text (106, 15, t);
    } else if (! s.empty ()) {
        const int16_t* d; const uint32_t len = eng.slotData (sel, &d);
        const int y0 = 12, h = 30;
        for (int x = 0; x < 128; ++x) {
            const uint32_t a = (uint64_t) len * x / 128, b = std::max<uint32_t> (a + 1, (uint64_t) len * (x + 1) / 128);
            int lo = 0, hi = 0;
            for (uint32_t i = a; i < b && i < len; i += std::max<uint32_t> (1, (b - a) / 32)) { lo = std::min<int> (lo, d[i]); hi = std::max<int> (hi, d[i]); }
            f.vline (x, y0 + h / 2 - hi * h / 65536, y0 + h / 2 - lo * h / 65536);
        }
        const int xa = (int) (s.trimA * 127), xb = (int) (s.trimB * 127);
        for (int y = y0; y < y0 + h; y += 2) { f.px (xa, y); f.px (xb, y); }
        std::snprintf (t, sizeof t, "%.1fS", s.len / float (s.rate)); f.text (128 - 6 * (int) std::strlen (t), y0 + h - 7, t);
        if (s.mute) f.text (0, y0 + h - 7, "MUTE");
    } else {
        f.text (16, 18, "PRAZDNY ZVUK");
        f.text (10, 30, "REC + PAD = NAHRAJ");
    }
    // bottom: the encoder page or what the held key does
    f.hline (0, 127, 45);
    auto four = [&] (const char* a, const char* b, const char* c, const char* d, const char* va, const char* vb, const char* vc, const char* vd) {
        const char* L[4] = {a, b, c, d}; const char* V[4] = {va, vb, vc, vd};
        for (int i = 0; i < 4; ++i) { f.text (i * 32, 47, L[i]); f.text (i * 32, 56, V[i]); }
    };
    char a[12], b[12], c[12], d[12];
    if (held[K_BPM]) {
        std::snprintf (a, sizeof a, "%d", (int) eng.bpm); std::snprintf (b, sizeof b, "%d%%", (int) std::round (50 + eng.swing * 50));
        std::snprintf (c, sizeof c, "%d", (int) std::round (eng.compAmt * 100)); std::snprintf (d, sizeof d, "%d", (int) std::round (eng.master * 100));
        four ("TEMP", "SWNG", "KOMP", "HLAS", a, b, c, d);
        if (battery >= 0) { std::snprintf (t, sizeof t, "BAT %d%%  %dS", battery, (int) eng.memoryFree ()); f.text (0, 36, t); }
    } else if (held[K_FX]) {
        std::snprintf (a, sizeof a, "%d", (int) std::round (eng.delayAmt * 100)); std::snprintf (b, sizeof b, "%d", (int) std::round (eng.reverbAmt * 100));
        std::snprintf (c, sizeof c, "%d", (int) std::round (eng.duckAmt * 100)); std::snprintf (d, sizeof d, "X%.1f", eng.inGain);
        four ("DLY", "REV", "SIDE", "VST", a, b, c, d);
    } else if (held[K_SOUND]) {
        std::snprintf (a, sizeof a, "%.5s", ARPNAME[eng.arpMode % 6]); std::snprintf (b, sizeof b, "1/%d", (int) (eng.arpDiv * 4));
        std::snprintf (c, sizeof c, "%.5s", Engine::scaleName (eng.scale)); std::snprintf (d, sizeof d, "%s", NOTENAME[eng.root % 12]);
        four ("ARP", "RYCH", "STUP", "TON", a, b, c, d);
    } else if (held[K_LEN]) {
        std::snprintf (a, sizeof a, "%d", eng.pats[eng.pat].len); std::snprintf (b, sizeof b, "%d", eng.slotLen (eng.pat, sel));
        four ("PATT", "ZVUK", "", "", a, b, "", "");
    } else if (held[K_SONG]) {
        std::snprintf (t, sizeof t, "SONG %d RIADKOV %s", eng.songLen, eng.songMode ? "ZAP" : "VYP"); f.text (0, 47, t);
        int x = 0; for (int i = std::max (0, eng.songLen - 5); i < eng.songLen; ++i) { std::snprintf (t, sizeof t, "%dX%d", eng.song[i].pat + 1, eng.song[i].repeats); x = f.text (x, 56, t) + 4; }
    } else if (stepMode && heldStep >= 0 && const_cast<Engine&> (eng).trig (eng.pat, heldStep, sel)) {
        const Trig* tr = const_cast<Engine&> (eng).trig (eng.pat, heldStep, sel);
        std::snprintf (a, sizeof a, "%+d", tr->pitch == NOLOCK ? 0 : tr->pitch); std::snprintf (b, sizeof b, tr->filter == NOLOCK ? "-" : "%+d", tr->filter);
        std::snprintf (c, sizeof c, "%d", tr->vel); std::snprintf (d, sizeof d, "%.5s", Engine::condName (tr->cond));
        four ("LAD", "FILT", "SILA", "PODM", a, b, c, d);
    } else {
        for (int i = 0; i < 4; ++i) { char v[12]; valueText (eng, s, page, i, v, sizeof v); f.text (i * 32, 47, PAGE[page][i]); f.text (i * 32, 56, v); }
        (void) PAGENAME;
        for (int i = 0; i < 5; ++i) f.px (127, 47 + i * 3, i == page);
    }
    // a message over everything
    if (msgTime) {
        const int w = std::min (124, 6 * (int) std::strlen (msg) + 8), x = (128 - w) / 2;
        f.rect (x - 1, 22, w + 2, 15, true, false);
        f.rect (x, 23, w, 13, false);
        f.text (x + 4, 26, msg);
    }
}

void Ui::leds (uint8_t* rgb) const {
    std::memset (rgb, 0, 48);
    auto set = [&] (int i, int r, int g, int b) { rgb[i * 3] = (uint8_t) r; rgb[i * 3 + 1] = (uint8_t) g; rgb[i * 3 + 2] = (uint8_t) b; };
    if (eng.fx) { set (eng.fx - 1, 160, 0, 160); return; }
    if (held[K_MUTE]) {
        for (int i = 0; i < 16; ++i) { const Slot& s = eng.slots[bank * 16 + i]; if (! s.empty ()) set (i, s.mute ? 120 : 0, s.mute ? 0 : 60, 0); }
        return;
    }
    if (held[K_PATTERN] || held[K_SONG]) {
        for (int i = 0; i < 16; ++i) { bool any = false; for (const Step& st : eng.pats[i].s) any |= st.n > 0; if (any) set (i, 0, 20, 40); }
        set (eng.pat, 0, 90, 160);
        return;
    }
    if (stepMode) {
        const int L = eng.slotLen (eng.pat, sel);
        for (int i = 0; i < 16; ++i) {
            const int st = stepOf (i);
            if (st >= L) continue;
            const Trig* tr = const_cast<Engine&> (eng).trig (eng.pat, st, sel);
            if (tr) set (i, tr->vel >= 120 ? 120 : 60, 22, tr->cond != C_ALWAYS ? 40 : 0);
        }
        if (eng.playing () && eng.step >= 0 && eng.step / 16 == stepPage) set (eng.step % 16, 120, 120, 120);
    } else {
        for (int i = 0; i < 16; ++i) { const Slot& s = eng.slots[bank * 16 + i]; if (! s.empty ()) set (i, 0, s.mute ? 2 : 14, s.mute ? 0 : 4); }
        if (sel / 16 == bank) set (sel % 16, 90, 40, 0);
        if (eng.playing () && eng.step >= 0) set (eng.step % 16, 120, 120, 120);
    }
    if (eng.recording () && eng.recSlot / 16 == bank) set (eng.recSlot % 16, 200, 0, 0);
}

} // namespace pocket
