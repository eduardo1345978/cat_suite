// Cat Box Pocket simulator: the real engine and UI compiled to WebAssembly for the browser.
// The page calls these functions; the audio, the keys and the screen go through them.
#include "../engine/pocket.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <new>
#include <initializer_list>

using namespace pocket;
#define API extern "C" __attribute__((visibility ("default")))

static int16_t pool[4u * 1024 * 1024];            // 8 MB like the PSRAM
static constexpr uint32_t DISK_N = 28672;
static uint8_t diskData[DISK_N * 512];           // 14 MB like the flash disk (kept out of the object: zero memory)
struct RamDisk : BlockDev {
    static constexpr uint32_t N = DISK_N;
    uint8_t* d = diskData;
    uint32_t sectors () const override { return N; }
    bool read (uint32_t s, uint8_t* b) override { if (s >= N) return false; std::memcpy (b, d + s * 512, 512); return true; }
    bool write (uint32_t s, const uint8_t* b) override { if (s >= N) return false; std::memcpy (d + s * 512, b, 512); return true; }
};
static RamDisk disk;
alignas (16) static uint8_t engineMem[sizeof (Engine)];
alignas (16) static uint8_t uiMem[sizeof (Ui)];
static Engine* E = nullptr; static Ui* U = nullptr;
static float inBuf[2048], outBuf[4096];
static uint8_t fb[1024], rgb[48];
static uint8_t xfer[1 << 20];                    // files in and out (1 MB)

static uint32_t seed = 1;
static float noise () { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (seed & 65535) / 32768.0f - 1; }
static int16_t s16 (double x) { return (int16_t) std::fmax (-32767, std::fmin (32767, x * 32767)); }

// a starter kit in bank A, made the way the tests make their demo
static void demoKit () {
    static int16_t b[48000];
    auto put = [&] (int slot, int n) { E->assign (slot, b, (uint32_t) n, SR); };
    for (int i = 0; i < 9000; ++i) { double t = i / 48000.0; b[i] = s16 (0.85 * std::sin (2 * M_PI * (45 * t + 90 * (1 - std::exp (-t * 30)) / 30)) * std::exp (-t * 9)); } put (0, 9000);       // kick
    for (int i = 0; i < 9000; ++i) { double t = i / 48000.0; b[i] = s16 ((0.45 * noise () + 0.3 * std::sin (2 * M_PI * 190 * t)) * std::exp (-t * 16)); } put (1, 9000);                   // snare
    for (int i = 0; i < 2400; ++i) { double t = i / 48000.0; b[i] = s16 (0.3 * noise () * std::exp (-t * 70)); } put (2, 2400);                                                         // closed hat
    for (int i = 0; i < 16000; ++i) { double t = i / 48000.0; b[i] = s16 (0.28 * noise () * std::exp (-t * 9)); } put (3, 16000);                                                     // open hat
    for (int i = 0; i < 12000; ++i) { double t = i / 48000.0; double e = 0; for (int k = 0; k < 3; ++k) { double tt = t - k * 0.011; if (tt > 0) e += std::exp (-tt * 60); }
        b[i] = s16 (0.4 * noise () * (e + 0.4 * std::exp (-t * 12))); } put (4, 12000);                                                                                                // clap
    for (int i = 0; i < 24000; ++i) { double t = i / 48000.0; b[i] = s16 (0.5 * std::tanh (3 * std::sin (2 * M_PI * 55 * t)) * std::exp (-t * 3)); } put (5, 24000);                // bass
    for (int i = 0; i < 14000; ++i) { double t = i / 48000.0; b[i] = s16 (0.7 * std::sin (2 * M_PI * (110 * t + 60 * (1 - std::exp (-t * 20)) / 20)) * std::exp (-t * 8)); } put (6, 14000);  // tom
    for (int i = 0; i < 6000; ++i) { double t = i / 48000.0; b[i] = s16 (0.5 * std::sin (2 * M_PI * 1200 * t) * std::sin (2 * M_PI * 1730 * t) * std::exp (-t * 25)); } put (7, 6000);  // cowbell-ish
    for (int i = 0; i < 36000; ++i) { double t = i / 48000.0; double x = 0; for (double f : {261.6, 311.1, 392.0, 466.2}) x += std::sin (2 * M_PI * f * t) + 0.3 * std::sin (4 * M_PI * f * t);
        b[i] = s16 (0.12 * x * std::exp (-t * 2.5) * std::fmin (1.0, t * 200)); } put (8, 36000);                                                                                     // chord stab
    for (int i = 0; i < 30000; ++i) { double t = i / 48000.0; b[i] = s16 (0.35 * std::sin (2 * M_PI * 880 * t + 2.5 * std::sin (2 * M_PI * 1760 * t) * std::exp (-t * 6)) * std::exp (-t * 4)); } put (9, 30000);  // bell
    for (int i = 0; i < 48000; ++i) { double t = i / 48000.0; b[i] = s16 (0.25 * noise () * (0.5 + 0.5 * std::sin (2 * M_PI * 0.7 * t)) * std::fmin (1.0, t * 4) * std::fmin (1.0, (1 - t) * 4)); } put (10, 48000);   // noise sweep
    for (int i = 0; i < 4000; ++i) { double t = i / 48000.0; b[i] = s16 (0.6 * std::sin (2 * M_PI * 800 * t * std::exp (-t * 30)) * std::exp (-t * 40)); } put (11, 4000);           // zap
    E->slots[5].filter = -0.3f; E->slots[1].reverb = 0.3f; E->slots[8].reverb = 0.4f; E->slots[8].delay = 0.3f; E->slots[9].delay = 0.4f;
    E->slots[2].choke = E->slots[3].choke = 1; E->slots[0].duckSource = true;
    // pattern 1: a beat; pattern 2: a variation
    E->pat = 0;
    for (int s : {0, 6, 10}) E->toggleStep (s, 0);
    for (int s : {4, 12}) E->toggleStep (s, 1);
    for (int s = 0; s < 16; s += 2) E->toggleStep (s, 2);
    E->toggleStep (14, 3);
    for (int s : {0, 3, 6, 10, 13}) { E->toggleStep (s, 5); E->trig (0, s, 5)->pitch = (int8_t) (s == 10 ? 3 : s == 13 ? 5 : 0); }
    E->toggleStep (8, 8);
    E->pats[1] = E->pats[0]; E->pat = 1;
    E->toggleStep (12, 4); E->toggleStep (15, 6); E->trig (1, 15, 6)->cond = C_1_2; E->toggleStep (11, 7);
    E->pat = 0;
    E->bpm = 118; E->swing = 0.1f; E->delayAmt = 0.15f; E->reverbAmt = 0.2f; E->master = 0.85f; E->duckAmt = 0.3f;
    for (int i = 0; i < NSLOT; ++i) E->slots[i].saved = false;
}

API void sim_init (int seedIn, int demo) {
    seed = (uint32_t) seedIn | 1;
    E = new (engineMem) Engine (); U = new (uiMem) Ui (*E);
    E->init (pool, sizeof (pool) / 2, (uint32_t) seedIn);
    U->disk = &disk;
    if (! store::mounted (disk)) store::format (disk, "CATPOCKET");
    if (demo) demoKit ();
}
API int sim_load_project (int p) { return store::load (disk, *E, *U, p); }
API float* sim_in () { return inBuf; }
API float* sim_out () { return outBuf; }
API void sim_process (int n) { E->process (inBuf, outBuf, n); }
API void sim_key (int k, int down) { U->key (k, down != 0); }
API void sim_encoder (int i, int d) { U->encoder (i, d); }
API void sim_fader (float x) { U->fader (x); }
API void sim_tick (uint32_t ms) { U->tick (ms); }
API int sim_service () {     // 1 = the disk changed (the page keeps a copy)
    const bool disky = U->wantSave || U->wantKit >= 0 || U->wantExport;
    U->service ();
    return disky ? 1 : 0;
}
API int sim_busy () { return U->wantSave || U->wantLoad || U->wantKit >= 0 || U->wantExport || U->wantBrowse || U->browseLoad >= 0; }
API uint8_t* sim_fb () { U->render (fb); return fb; }
API uint8_t* sim_leds () { U->leds (rgb); return rgb; }
API int sim_step () { return E->playing () ? E->step : -1; }
API int sim_playing () { return E->playing (); }
API float sim_level () { return E->level; }
API int sim_wants_usb () { const int w = U->wantUsb; U->wantUsb = false; return w; }
// files: the page puts bytes into xfer, then names them
API uint8_t* sim_xfer () { return xfer; }
API int sim_xfer_size () { return (int) sizeof (xfer); }
API int sim_write_file (const char* name, int len) { return store::writeFile (disk, name, xfer, (uint32_t) len); }
API int sim_read_file (const char* name) { uint32_t got = 0; return store::readFile (disk, name, xfer, sizeof (xfer), &got) ? (int) got : -1; }
API int sim_load_wav (const char* name, int slot) { return store::loadWavInto (disk, name, *E, slot); }
// raw mono samples straight into a pad (the page decodes any audio file)
static int16_t pcm[48000 * 20];
API int16_t* sim_pcm () { return pcm; }
API int sim_pcm_max () { return (int) (sizeof (pcm) / 2); }
API int sim_assign (int slot, int len, int rate) { const bool ok = E->assign (slot, pcm, (uint32_t) len, (uint32_t) rate); if (ok) E->noteOn (slot, slot); return ok; }
API int sim_sel () { return U->sel; }
API int sim_bank () { return U->bank; }
API uint8_t* sim_disk () { return disk.d; }
API int sim_disk_size () { return (int) sizeof (diskData); }
API int sim_list (char* out, int max) {        // names, one per line
    static char names[128][13]; const int n = store::list (disk, names, nullptr, 128);
    int k = 0; for (int i = 0; i < n; ++i) { const int l = (int) std::strlen (names[i]); if (k + l + 1 >= max) break; std::memcpy (out + k, names[i], l); k += l; out[k++] = '\n'; }
    out[k] = 0; return k;
}
