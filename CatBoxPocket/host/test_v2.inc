// Cat Box Pocket v2 tests: the sequencer, sound, mix, MIDI / sync, disks and the bigger UI.
// (included by test.cpp: uses its helpers)

static void reset2 () {
    reset ();
    E->compAmt = 0; E->duckAmt = 0; E->metronome = false; E->syncMode = 0; E->scale = 0; E->root = 0; E->arpMode = 0; E->arpDiv = 4;
    E->threshold = false; E->autoTrim = true; E->monitor = false; E->fill = false; E->midiChannel = 0;
    while (E->midiOutPop () >= 0) {}
}
static std::vector<int16_t> click (float hz = 200, int n = 2000, float amp = 0.5f) {
    std::vector<int16_t> s (n);
    for (int i = 0; i < n; ++i) s[i] = (int16_t) (amp * 32767 * std::sin (2 * M_PI * hz * i / SR) * (1 - i / (double) n));
    return s;
}
static int countOn (const std::vector<float>& o, float thr = 0.05f) { return (int) onsets (o, thr).size (); }
static float rmsR (const std::vector<float>& o, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += o[2 * i + 1] * o[2 * i + 1]; return (float) std::sqrt (s / std::max (1, b - a)); }

// a block device seen through a partition (for formatting a card with a partition table)
struct Part : BlockDev {
    BlockDev& d; uint32_t off, n;
    Part (BlockDev& dev, uint32_t o, uint32_t len) : d (dev), off (o), n (len) {}
    uint32_t sectors () const override { return n; }
    bool read (uint32_t s, uint8_t* b) override { return s < n && d.read (off + s, b); }
    bool write (uint32_t s, const uint8_t* b) override { return s < n && d.write (off + s, b); }
};

// a small mono 16-bit WAV (1000 frames) like one from a computer
static const std::vector<uint8_t>& w0wav () {
    static std::vector<uint8_t> w;
    if (w.empty ()) { static int16_t d[1000]; for (int i = 0; i < 1000; ++i) d[i] = (int16_t) (8000 * std::sin (i * 0.05)); w.resize (44 + 2000); store::makeWav (d, 1000, SR, w.data (), (uint32_t) w.size ()); }
    return w;
}
// give a file in the FAT16 root a long name, the way Windows writes it (long name entries before the short one)
static void addLfn (RamDisk& disk, const char* shortName, const char* longName) {
    uint8_t* b = disk.d.data ();
    uint16_t res, fs; std::memcpy (&res, b + 14, 2); std::memcpy (&fs, b + 22, 2);
    uint8_t* root = b + (res + b[16] * fs) * 512;
    uint8_t sn[11]; std::memset (sn, ' ', 11);
    { int i = 0; const char* n = shortName; for (; *n && *n != '.'; ++n) sn[i++] = (uint8_t) *n; if (*n == '.') { ++n; for (int j = 0; *n; ++n) sn[8 + j++] = (uint8_t) *n; } }
    int at = -1; for (int i = 0; i < 512; ++i) if (! std::memcmp (root + i * 32, sn, 11)) { at = i; break; }
    if (at < 0) return;
    const int len = (int) std::strlen (longName), pieces = (len + 12) / 13;
    std::memmove (root + (at + pieces) * 32, root + at * 32, 32);
    uint8_t sum = 0; for (int i = 0; i < 11; ++i) sum = (uint8_t) (((sum & 1) << 7) + (sum >> 1) + sn[i]);
    static const int OFF[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    for (int p = 0; p < pieces; ++p) {
        const int seq = pieces - p;           // the last piece comes first
        uint8_t* e = root + (at + p) * 32; std::memset (e, 0, 32);
        e[0] = (uint8_t) (seq | (p == 0 ? 0x40 : 0)); e[11] = 0x0F; e[13] = sum;
        for (int k = 0; k < 13; ++k) {
            const int ci = (seq - 1) * 13 + k; uint16_t c = ci < len ? (uint16_t) longName[ci] : (ci == len ? 0 : 0xFFFF);
            e[OFF[k]] = (uint8_t) c; e[OFF[k] + 1] = (uint8_t) (c >> 8);
        }
    }
}

static void v2Tests (const std::string& dir) {
    // ---- pattern length and polymeter
    reset2 ();
    { auto k = click (); E->assign (0, k.data (), (uint32_t) k.size (), SR); E->assign (1, k.data (), (uint32_t) k.size (), SR); }
    E->pats[0].len = 12; E->toggleStep (0, 0);
    E->play (true); auto o = run (3.2f); E->play (false);
    { auto on = onsets (o); CHECK (on.size () == 3 && std::abs (on[1] - (int) (1.5f * SR)) < 64, "12-step pattern repeats every 1.5 s: %zu", on.size ()); }
    E->clearPattern (0); E->toggleStep (0, 1); E->setSlotLen (0, 1, 3);
    CHECK (E->slotLen (0, 1) == 3 && E->slotLen (0, 0) == 16, "a sound with its own length (3 of 16)");
    E->play (true); o = run (1.95f); E->play (false);
    { auto on = onsets (o); CHECK (on.size () == 6 && std::abs (on[1] - on[0] - (int) (3 * 0.125f * SR)) < 64, "polymeter: every 3 steps (%zu hits)", on.size ()); }
    E->setSlotLen (0, 1, 16); CHECK (E->pats[0].nPoly == 0, "back to the pattern's length");

    // ---- step conditions, fill, probability
    reset2 (); E->bpm = 240;     // one bar = 1 s
    { auto k = click (); E->assign (0, k.data (), (uint32_t) k.size (), SR); }
    E->toggleStep (0, 0); E->trig (0, 0, 0)->cond = C_1_2;
    E->play (true); o = run (3.95f); E->play (false);
    CHECK (countOn (o) == 2, "1:2 plays every second loop: %d of 4", countOn (o));
    E->trig (0, 0, 0)->cond = C_3_4;
    E->play (true); o = run (3.95f); E->play (false);
    { auto on = onsets (o); CHECK (on.size () == 1 && std::abs (on[0] - 2 * SR) < 64, "3:4 plays on the third loop"); }
    E->trig (0, 0, 0)->cond = C_FILL;
    E->play (true); o = run (1.95f); E->play (false);
    CHECK (countOn (o) == 0, "FILL steps are quiet without FILL");
    E->fill = true; E->play (true); o = run (1.95f); E->play (false); E->fill = false;
    CHECK (countOn (o) == 2, "and play while FILL is held: %d", countOn (o));
    E->trig (0, 0, 0)->cond = C_NOTFIRST;
    E->play (true); o = run (2.95f); E->play (false);
    CHECK (countOn (o) == 2, "!PRVY skips the first loop: %d", countOn (o));
    E->trig (0, 0, 0)->cond = C_P50;
    E->play (true); o = run (39.9f); E->play (false);
    CHECK (countOn (o) >= 10 && countOn (o) <= 30, "50%% plays about half the time: %d of 40", countOn (o));

    // ---- velocity, pan, choke, attack / decay, drive
    reset2 ();
    { auto s = sine (440, 1.0f); E->assign (0, s.data (), (uint32_t) s.size (), SR); E->assign (1, s.data (), (uint32_t) s.size (), SR); }
    E->noteOn (0, 0, 0, 1.0f); o = run (0.2f); const float full = rms (o, 2000, 9000);
    E->allOff (); E->noteOn (0, 0, 0, 0.5f); o = run (0.2f);
    CHECK (std::fabs (rms (o, 2000, 9000) / full - 0.5f) < 0.02f, "velocity 50 = half as loud");
    E->allOff (); E->slots[0].pan = -1; E->noteOn (0, 0); o = run (0.2f); E->slots[0].pan = 0;
    CHECK (rms (o, 2000, 9000) > 0.2f && rmsR (o, 2000, 9000) < 1e-4f, "pan hard left");
    E->allOff (); E->noteOn (0, 0, 0, 1, NOLOCK, 100); o = run (0.2f);
    CHECK (rms (o, 2000, 9000) < 1e-4f && rmsR (o, 2000, 9000) > 0.2f, "a pan lock hard right");
    E->allOff (); E->slots[0].choke = E->slots[1].choke = 1;
    E->noteOn (0, 0); run (0.1f); E->noteOn (1, 1); run (0.02f);
    { int on = 0; for (auto& v : E->voices) on += v.on; CHECK (on == 1, "choke group: the new sound cuts the old one (%d voices)", on); }
    E->slots[0].choke = E->slots[1].choke = 0;
    E->allOff (); E->slots[0].decay = 0.1f; E->noteOn (0, 0); o = run (0.4f); E->slots[0].decay = 0;
    CHECK (rms (o, 200, 1000) > 0.1f && rms (o, (int) (0.25f * SR), (int) (0.35f * SR)) < 0.001f, "decay 0.1 s ends a 1 s sound early");
    E->allOff (); E->slots[0].attack = 0.2f; E->noteOn (0, 0); o = run (0.4f); E->slots[0].attack = 0;
    CHECK (rms (o, 0, 480) < 0.02f && rms (o, (int) (0.25f * SR), (int) (0.3f * SR)) > 0.25f, "attack 0.2 s fades in");
    E->allOff (); E->slots[0].drive = 1; E->noteOn (0, 0); o = run (0.2f); E->slots[0].drive = 0;
    { float pk = 0; for (int i = 2000; i < 9000; ++i) pk = std::max (pk, std::fabs (o[2 * i]));
      CHECK (rms (o, 2000, 9000) / pk > 0.8f, "drive squares the sine off (crest %.2f)", rms (o, 2000, 9000) / pk); }
    // LFO on pitch: the frequency moves
    E->allOff (); E->slots[0].lfoDepth = 1; E->slots[0].lfoRate = 2; E->slots[0].lfoTarget = 0; E->noteOn (0, 0); o = run (0.6f); E->slots[0].lfoDepth = 0;
    { const float f1 = freq (o, (int) (0.1f * SR), (int) (0.15f * SR)), f2 = freq (o, (int) (0.35f * SR), (int) (0.4f * SR));
      CHECK (f1 > 460 && f2 < 420, "LFO vibrato: %.0f Hz up, %.0f Hz down", f1, f2); }
    // vintage: SP-1200 style sample-and-hold
    E->allOff (); E->slots[0].vintage = 3; E->noteOn (0, 0); o = run (0.2f); E->slots[0].vintage = 0;
    { int changes = 0; for (int i = 2001; i < 7000; ++i) changes += o[2 * i] != o[2 * (i - 1)];
      CHECK (changes < 2500 && changes > 1500, "8-bit 22 kHz holds samples (%d changes in 5000)", changes); }

    // ---- mute, song, undo, nudge, copy of steps
    reset2 ();
    { auto k = click (); E->assign (0, k.data (), (uint32_t) k.size (), SR); }
    E->toggleStep (0, 0); E->slots[0].mute = true;
    E->play (true); o = run (1.0f); E->play (false); E->slots[0].mute = false;
    CHECK (countOn (o) == 0, "a muted sound is quiet in the sequencer");
    E->pat = 1; E->toggleStep (0, 0); E->toggleStep (8, 0); E->pat = 0;
    E->songAdd (0); E->song[0].repeats = 2; E->songAdd (1); E->songMode = true;
    E->play (true); o = run (7.9f); E->play (false); E->songMode = false;
    { auto on = onsets (o); CHECK (on.size () == 5, "song P1 x2, P2, then P1 again = 1+1+2+1 hits: %zu", on.size ());
      if (on.size () == 5) CHECK (std::abs (on[2] - 4 * SR) < 64 && std::abs (on[3] - 5 * SR) < 64 && std::abs (on[4] - 6 * SR) < 64, "song rows land on bars 3 and 4"); }
    E->snapshot (); E->toggleStep (5, 0); E->undo ();
    CHECK (! E->hasTrig (0, 5, 0) && E->hasTrig (0, 0, 0), "undo takes the step back");
    E->undo (); CHECK (E->hasTrig (0, 5, 0), "undo again = redo");
    E->clearPattern (0); E->toggleStep (0, 0); E->toggleStep (15, 0); E->nudge (0, 0, 1);
    CHECK (E->hasTrig (0, 1, 0) && E->hasTrig (0, 0, 0) && ! E->hasTrig (0, 15, 0), "nudge moves the sound a step later (wrapping)");
    E->pats[0].s[3].fx = 9; E->nudge (0, -1, -1);
    CHECK (E->pats[0].s[2].fx == 9 && E->hasTrig (0, 15, 0), "nudge everything earlier");

    // ---- scales and the arpeggiator
    reset2 ();
    E->scale = 1; CHECK (E->padSemis (2) == 4 && E->padSemis (7) == 12 && E->padSemis (9) == 16, "major scale on the pads");
    E->scale = 8; E->root = 2; CHECK (E->padSemis (0) == 2 && E->padSemis (5) == 14, "minor pentatonic from D");
    E->scale = 0; E->root = 0;
    { auto s = sine (440, 0.04f); E->assign (0, s.data (), (uint32_t) s.size (), SR); }
    E->arpMode = 1; E->arpSlot = 0; E->arpPress (0); E->arpPress (12);
    o = run (1.0f);
    { auto on = onsets (o); CHECK (on.size () == 8, "arp 1/16 at 120 = 8 notes a second: %zu", on.size ());
      if (on.size () >= 3) { const float a = freq (o, on[1] + 50, on[1] + 800), b = freq (o, on[2] + 50, on[2] + 800);
        CHECK ((std::fabs (a - 440) < 5 && std::fabs (b - 880) < 8) || (std::fabs (a - 880) < 8 && std::fabs (b - 440) < 5), "arp alternates the held notes: %.0f / %.0f", a, b); } }
    E->arpRelease (0); E->arpRelease (12); E->arpMode = 0; run (0.05f); o = run (0.5f);
    CHECK (countOn (o) == 0, "arp stops when let go");

    // ---- metronome, compressor, sidechain ducking
    reset2 (); E->metronome = true; E->play (true); o = run (1.0f); E->play (false); E->metronome = false;
    CHECK (countOn (o, 0.1f) == 2, "metronome: 2 clicks a second at 120: %d", countOn (o, 0.1f));
    { auto s = sine (200, 1.0f, 0.9f); E->assign (0, s.data (), (uint32_t) s.size (), SR); }
    E->allOff (); E->noteOn (0, 0); o = run (0.5f); const float dry = rms (o, SR / 4, SR / 2);
    E->allOff (); E->compAmt = 1; E->noteOn (0, 0); o = run (0.5f); E->compAmt = 0;
    CHECK (rms (o, SR / 4, SR / 2) < dry * 0.85f && E->gr > 0.2f, "compressor squashes a loud sound (%.2f -> %.2f)", dry, rms (o, SR / 4, SR / 2));
    { std::vector<int16_t> q (2000, 1); E->assign (1, q.data (), 2000, SR); }      // a silent trigger (a sidechain source)
    { auto s = sine (300, 1.0f, 0.3f); E->assign (2, s.data (), (uint32_t) s.size (), SR); }
    E->allOff (); E->slots[1].duckSource = true; E->duckAmt = 1;
    E->noteOn (2, 2); run (0.2f); E->noteOn (1, 1); o = run (0.3f);
    CHECK (rms (o, 0, 2000) < 0.05f && rms (o, (int) (0.2f * SR), (int) (0.3f * SR)) > 0.1f, "sidechain ducks the pad and lets it back");

    // ---- scenes and the crossfader
    reset2 ();
    { auto s = sine (440, 1.0f); E->assign (0, s.data (), (uint32_t) s.size (), SR); }
    E->sceneStore (0); E->slots[0].pitch = 12; E->sceneStore (1); E->slots[0].pitch = 0;
    E->xfade = 1; E->noteOn (0, 0); o = run (0.2f);
    CHECK (std::fabs (freq (o, 2000, 9000) - 880) < 6, "crossfader on B: the scene's pitch: %.0f", freq (o, 2000, 9000));
    E->xfade = 0.5f; E->allOff (); E->noteOn (0, 0); o = run (0.2f);
    CHECK (std::fabs (freq (o, 2000, 9000) - 622.3f) < 6, "half way: +6 semitones: %.0f", freq (o, 2000, 9000));
    E->sceneClear (); E->xfade = 0;

    // ---- MIDI in and out
    reset2 ();
    { auto k = click (); E->assign (0, k.data (), (uint32_t) k.size (), SR); E->assign (5, k.data (), (uint32_t) k.size (), SR); }
    E->midiIn (0x90); E->midiIn (36 + 5); E->midiIn (100);
    { bool on = false; for (auto& v : E->voices) on |= v.on && v.slot == 5; CHECK (on, "MIDI note 41 plays pad A06"); }
    E->midiIn (0xB0); E->midiIn (1); E->midiIn (127); CHECK (E->xfade == 1, "MIDI CC 1 = crossfader");
    E->midiIn (0x91); E->midiIn (36); E->midiIn (100);
    { int on = 0; for (auto& v : E->voices) on += v.on && v.slot == 0; CHECK (on == 0, "another channel is ignored"); }
    E->xfade = 0; E->toggleStep (0, 0);
    E->play (true); run (0.5f); E->play (false);
    { std::vector<int> b; int x; while ((x = E->midiOutPop ()) >= 0) b.push_back (x);
      int clocks = 0; bool note = false, start = b.size () && b[0] == 0xFA, stop = b.size () && b.back () == 0xFC;
      for (size_t i = 0; i < b.size (); ++i) { clocks += b[i] == 0xF8; if (b[i] == 0x90 && i + 2 < b.size () && b[i + 1] == 36) note = true; }
      CHECK (start && stop && note && clocks == 24, "MIDI out: start, 24 clocks a beat (%d in half a second), the note, stop", clocks); }
    // follow a MIDI clock at 100 BPM: one clock every 1200 samples
    E->syncMode = 1; E->clearPattern (0); for (int s : {0, 4, 8, 12}) E->toggleStep (s, 0);
    E->midiIn (0xFA);
    { std::vector<float> all; for (int c = 0; c < 96; ++c) { E->midiIn (0xF8); auto x = run (1200.0f / SR); all.insert (all.end (), x.begin (), x.end ()); }
      auto on = onsets (all); CHECK (on.size () == 4 && on.size () >= 2 && std::abs (on[1] - on[0] - 24 * 1200) < 80, "MIDI clock in: a kick every beat at 100 BPM (%zu)", on.size ());
      CHECK (std::fabs (E->bpm - 100) < 1, "the tempo follows the clock: %.1f", E->bpm); }
    E->midiIn (0xFC); run (0.01f); CHECK (! E->playing (), "MIDI stop");
    // follow sync pulses (2 a beat) at 90 BPM
    E->syncMode = 2; E->syncPulseOut = false;
    { std::vector<float> all; const float every = 60.0f / 90 / 2;
      for (int p = 0; p < 8; ++p) { E->syncPulseIn (); auto x = run (every); all.insert (all.end (), x.begin (), x.end ()); }
      auto on = onsets (all); CHECK (on.size () == 4, "sync pulses start it and drive 16 steps: %zu kicks", on.size ());
      if (on.size () >= 3) CHECK (std::abs (on[2] - on[1] - (int) (2 * every * SR)) < 80, "the steps between pulses are evenly spaced");
      CHECK (std::fabs (E->bpm - 90) < 1, "pulse tempo: %.1f", E->bpm); }
    E->play (false); E->syncMode = 0;
    E->play (true); E->syncPulseOut = false; run (0.3f); E->play (false);
    CHECK (E->syncPulseOut, "sync pulses go out while playing");

    // ---- recording: wait for the sound, cut the silence; trim + normalize
    reset2 ();
    { std::vector<float> in (SR / 2, 0.0f); for (int i = SR / 5; i < SR / 2; ++i) in[i] = 0.3f * std::sin (2 * M_PI * 300 * i / SR);
      E->threshold = true; E->autoTrim = false; E->recStart (3, false); run (0.5f, in.data ()); E->recStop ();
      CHECK (std::abs ((int) E->slots[3].len - (int) (0.3f * SR)) < 200, "REC waits for the sound: %u", E->slots[3].len);
      E->threshold = false; E->autoTrim = true; E->recStart (4, false); run (0.5f, in.data ()); E->recStop ();
      CHECK (std::abs ((int) E->slots[4].len - (int) (0.305f * SR)) < 200, "auto trim cuts the silence before it: %u", E->slots[4].len); }
    { std::vector<int16_t> s (SR, 0); for (int i = SR / 4; i < SR / 2; ++i) s[i] = (int16_t) (3000 * std::sin (2 * M_PI * 200 * i / SR));
      E->assign (6, s.data (), SR, SR); E->trimNormalize (6);
      const int16_t* d; const uint32_t n = E->slotData (6, &d); int pk = 0; for (uint32_t i = 0; i < n; ++i) pk = std::max (pk, std::abs ((int) d[i]));
      CHECK (n < SR / 3 && n > SR / 4 && std::abs (pk - 29163) < 100, "trim + normalize: %u samples, peak %d", n, pk); }

    // ---- disks: projects, kits, export, FAT32 card with a partition table, long names
    reset2 ();
    {
        RamDisk disk (28672);
        store::format (disk);
        { auto k = click (); E->assign (0, k.data (), (uint32_t) k.size (), SR); auto s = sine (440, 0.3f); E->assign (17, s.data (), (uint32_t) s.size (), SR); }
        E->slots[0].choke = 2; E->slots[0].lfoDepth = 0.3f; E->slots[17].pan = -0.5f; E->slots[17].vintage = 1;
        E->toggleStep (0, 0); E->trig (0, 0, 0)->cond = C_2_4; E->trig (0, 0, 0)->vel = 127; E->pats[0].len = 24; E->toggleStep (40, 17);
        E->setSlotLen (0, 17, 5); E->songAdd (0); E->songAdd (3); E->compAmt = 0.4f; E->sceneStore (1);
        const Pattern keep = E->pats[0];
        E->bpm = 101; CHECK (store::save (disk, *E, *U, 2), "save as project 3");
        E->bpm = 140; E->clearSlot (17); CHECK (store::save (disk, *E, *U, 0), "save something else as project 1");
        reset2 ();
        CHECK (store::load (disk, *E, *U, 2) && U->project == 2, "load project 3");
        CHECK (E->bpm == 101 && ! E->slots[17].empty () && E->slots[17].pan == -0.5f && E->slots[17].vintage == 1 && E->slots[0].choke == 2, "its sounds and settings are back");
        CHECK (std::memcmp (&keep, &E->pats[0], sizeof (Pattern)) == 0 && E->songLen == 2 && E->scenes[1].set && std::fabs (E->compAmt - 0.4f) < 1e-6f, "64-step pattern, polymeter, conditions, song and scene are back");
        CHECK (store::load (disk, *E, *U, 0) && E->bpm == 140 && E->slots[17].empty (), "project 1 is a different one");
        // kits
        reset2 ();
        { for (int i = 0; i < 4; ++i) { auto s = sine (200 + 100 * i, 0.1f + 0.05f * i); E->assign (16 + i, s.data (), (uint32_t) s.size (), SR); E->slots[16 + i].pitch = (float) i; } }
        CHECK (store::saveKit (disk, *E, 1, 4), "save bank B as kit 5");
        for (int i = 0; i < 4; ++i) E->clearSlot (16 + i);
        CHECK (store::loadKit (disk, *E, 3, 4), "load kit 5 into bank D");
        CHECK (! E->slots[48 + 3].empty () && E->slots[48 + 3].pitch == 3 && E->slots[48 + 3].len == (uint32_t) (0.25f * SR) && E->slots[48 + 4].empty (), "the kit's sounds and parameters are in bank D");
        // export
        { auto k = click (); E->assign (0, k.data (), (uint32_t) k.size (), SR); }
        E->clearPattern (0); E->toggleStep (0, 0); E->toggleStep (8, 0); E->bpm = 120;
        char name[13]; CHECK (store::exportWav (disk, *E, false, name) && ! std::strcmp (name, "EXP01.WAV"), "export the pattern: %s", name);
        { static uint8_t w[800000]; uint32_t got = 0; store::readFile (disk, "EXP01.WAV", w, sizeof w, &got);
          const uint32_t frames = (uint32_t) (16 * E->stepSamples ()) + SR;
          uint16_t ch; std::memcpy (&ch, w + 22, 2);
          CHECK (got == 44 + frames * 4 && ch == 2, "a stereo WAV of 2 s + 1 s tail: %u bytes", got);
          std::vector<float> o2 (frames * 2); for (uint32_t i = 0; i < frames * 2; ++i) { int16_t v; std::memcpy (&v, w + 44 + i * 2, 2); o2[i] = v / 32768.0f; }
          auto on = onsets (o2); CHECK (on.size () == 2 && on[0] < 64 && std::abs (on[1] - SR) < 64, "the export has the two hits (%zu)", on.size ()); }
        CHECK (! E->playing () && ! E->hold, "the export stops and lets the audio play again");
        CHECK (store::exportWav (disk, *E, false, name) && ! std::strcmp (name, "EXP02.WAV"), "the next export gets the next name");
        // a WAV with a long name (as Windows writes it) shows its long name
        CHECK (store::writeFile (disk, "BASSLI~1.WAV", w0wav ().data (), (uint32_t) w0wav ().size ()), "a file from the computer");
        addLfn (disk, "BASSLI~1.WAV", "Bass line 2.wav");
        { char n[64][13]; char ln[64][24]; uint32_t sz[64]; const int k = store::list (disk, n, sz, 64, ln); bool found = false;
          for (int i = 0; i < k; ++i) if (! std::strcmp (n[i], "BASSLI~1.WAV")) found = ! std::strcmp (ln[i], "Bass line 2.wav");
          CHECK (found, "the long name is read for the browser"); }
        CHECK (store::loadWavInto (disk, "BASSLI~1.WAV", *E, 9) && E->slots[9].len == 1000, "load it into a pad");
        CHECK (store::remove (disk, "BASSLI~1.WAV") && ! store::exists (disk, "BASSLI~1.WAV"), "delete it (and its long name)");
        { char n[64][13]; char ln[64][24]; const int k = store::list (disk, n, nullptr, 64, ln); bool stray = false;
          for (int i = 0; i < k; ++i) stray |= ! std::strcmp (ln[i], "Bass line 2.wav");
          CHECK (! stray, "no stray long name"); }
    }
    {
        RamDisk card (140000);                      // 68 MB with a partition from sector 2048 (like a real card)
        Part part (card, 2048, 140000 - 2048);
        CHECK (store::format (part, "CARD", true), "format FAT32");
        uint8_t mbr[512] = {0}; mbr[446 + 4] = 0x0C; uint32_t v = 2048; std::memcpy (mbr + 446 + 8, &v, 4); v = 140000 - 2048; std::memcpy (mbr + 446 + 12, &v, 4);
        mbr[510] = 0x55; mbr[511] = 0xAA; card.write (0, mbr);
        CHECK (store::mounted (card), "the card mounts through its partition table");
        reset2 ();
        for (int i = 0; i < 40; ++i) { auto s = sine (100 + 10 * i, 0.05f); E->assign (i, s.data (), (uint32_t) s.size (), SR); }
        E->bpm = 97;
        CHECK (store::save (card, *E, *U, 5), "save 40 sounds on FAT32 (the root grows past one cluster)");
        { char n[80][13]; const int k = store::list (card, n, nullptr, 80); CHECK (k == 41, "41 files listed: %d", k); }
        reset2 ();
        CHECK (store::load (card, *E, *U, 5) && E->bpm == 97, "load from the card");
        { int n = 0; for (auto& s : E->slots) n += ! s.empty (); CHECK (n == 40 && E->slots[39].len == (uint32_t) (0.05f * SR), "all 40 sounds came back: %d", n); }
    }

    // ---- the UI: the new keys
    reset2 ();
    { auto k = click (); E->assign (0, k.data (), (uint32_t) k.size (), SR); E->assign (1, k.data (), (uint32_t) k.size (), SR); }
    U->tick (1000);
    press (K_MUTE); press (K_PAD0 + 1); lift (K_PAD0 + 1); lift (K_MUTE);
    CHECK (E->slots[1].mute, "MUTE + pad mutes");
    press (K_SHIFT); press (K_MUTE); lift (K_MUTE); lift (K_SHIFT);
    CHECK (! E->slots[1].mute, "SHIFT + MUTE unmutes all");
    U->sel = 0; E->slots[0].pitch = 7;
    press (K_COPY); press (K_PAD0); lift (K_PAD0); lift (K_COPY); press (K_PASTE); press (K_PAD0 + 5); lift (K_PAD0 + 5); lift (K_PASTE);
    CHECK (E->slots[5].pitch == 7 && E->slots[5].block == E->slots[0].block && E->blocks[E->slots[0].block].refs == 2, "COPY / PASTE a sound (it shares the sample)");
    CHECK (U->sel == 5, "the pasted sound is selected"); U->sel = 0;
    press (K_WRITE); lift (K_WRITE); press (K_PAD0 + 3); lift (K_PAD0 + 3); press (K_WRITE); lift (K_WRITE);
    CHECK (E->hasTrig (0, 3, 0), "a step");
    press (K_COPY); lift (K_COPY); E->pat = 4; press (K_PASTE); lift (K_PASTE);
    CHECK (E->hasTrig (4, 3, 0), "COPY alone, PASTE alone = copy the pattern");
    press (K_CLEAR); lift (K_CLEAR); CHECK (! E->hasTrig (4, 3, 0), "CLEAR alone clears the pattern");
    press (K_UNDO); lift (K_UNDO); CHECK (E->hasTrig (4, 3, 0), "UNDO brings it back");
    E->pat = 0;
    press (K_SONG); press (K_PAD0 + 0); lift (K_PAD0); press (K_PAD0 + 4); lift (K_PAD0 + 4); U->encoder (0, 2); lift (K_SONG);
    CHECK (E->songLen == 2 && E->song[1].pat == 4 && E->song[1].repeats == 3 && ! E->songMode, "SONG + pads builds the song, SONG + A repeats");
    press (K_SONG); lift (K_SONG); CHECK (E->songMode, "SONG alone = song mode");
    press (K_LEN); press (K_PAD0 + 7); lift (K_PAD0 + 7); lift (K_LEN);
    CHECK (E->pats[0].len == 32, "LEN + pad 8 = 32 steps");
    press (K_LEN); U->encoder (1, -11); lift (K_LEN);
    CHECK (E->slotLen (0, 0) == 21, "LEN + B = the sound's own length");
    press (K_FILL); CHECK (E->fill, "FILL while held"); lift (K_FILL); U->tick (1100); CHECK (! E->fill, "FILL let go");
    press (K_WRITE); lift (K_WRITE);
    press (K_ENCD); lift (K_ENCD); CHECK (U->stepPage == 1, "push D = steps 17-32");
    press (K_PAD0 + 2); lift (K_PAD0 + 2); CHECK (E->hasTrig (0, 18, 0), "a pad on page 2 = step 19");
    press (K_PAD0 + 2); press (K_ACCENT); lift (K_ACCENT); U->encoder (3, 2); lift (K_PAD0 + 2);
    CHECK (E->trig (0, 18, 0) && E->trig (0, 18, 0)->vel == 127 && E->trig (0, 18, 0)->cond == C_2_2, "hold a step: ACCENT, D = condition (step stays)");
    press (K_WRITE); lift (K_WRITE);
    press (K_SCENE); press (K_PAD0 + 1); lift (K_PAD0 + 1); U->encoder (0, 10); lift (K_SCENE);
    CHECK (E->scenes[1].set && std::fabs (E->xfade - 0.5f) < 1e-4f, "SCENE + pad 2 stores B, SCENE + A moves the crossfader");
    U->fader (0.0f); CHECK (E->xfade == 0, "the crossfader");
    U->page = 2; U->encoder (3, 1); U->encoder (1, 4); CHECK (E->slots[0].choke == 1 && std::fabs (E->slots[0].decay - 0.2f) < 1e-4f, "page OBALKA: choke, decay");
    U->page = 1; U->encoder (3, 2); CHECK (E->slots[0].stretch && E->slots[0].beats == 1, "page VZORKA: stretch to 1 beat");
    press (K_SOUND); U->encoder (2, 1); U->encoder (0, 1); lift (K_SOUND); CHECK (E->scale == 1 && E->arpMode == 1, "SOUND + C / A = scale, arp");
    E->arpMode = 0; E->scale = 0;
    press (K_MENU); press (K_KIT); lift (K_KIT); CHECK (U->helpKey == K_KIT, "MENU + KIT = help"); writeScreen (dir + "/screen_help.pgm"); lift (K_MENU);
    CHECK (! U->menu, "the help does not open the menu");
    press (K_MENU); lift (K_MENU); CHECK (U->menu, "MENU opens the menu");
    U->encoder (0, 6); U->encoder (1, 1); CHECK (E->midiChannel == 1, "menu: MIDI channel 2");
    U->encoder (0, -2); writeScreen (dir + "/screen_menu.pgm"); press (K_ENCA); lift (K_ENCA);
    CHECK (U->wantExport == 1 && ! U->menu, "menu: EXPORT PATTERN");
    {
        RamDisk disk (28672); store::format (disk); U->disk = &disk;
        U->service (); CHECK (store::exists (disk, "EXP01.WAV") && std::strstr (U->message (), "EXP01"), "the main loop exports");
        store::writeFile (disk, "SNARE.WAV", w0wav ().data (), (uint32_t) w0wav ().size ());
        press (K_MENU); lift (K_MENU); U->menuItem = 3; press (K_ENCA); lift (K_ENCA); U->service ();
        CHECK (U->browsing && U->nFiles == 2, "the browser lists the WAVs: %d", U->nFiles);
        writeScreen (dir + "/screen_browser.pgm");
        int idx = -1; for (int i = 0; i < U->nFiles; ++i) if (! std::strcmp (U->files[i], "SNARE.WAV")) idx = i;
        U->encoder (0, idx); U->sel = 12; press (K_ENCA); lift (K_ENCA); U->service ();
        CHECK (E->slots[12].len == 1000, "and loads one into the pad");
        press (K_MENU); lift (K_MENU); press (K_MENU); lift (K_MENU); CHECK (! U->menu, "MENU closes the browser, then the menu");
        press (K_SHIFT); press (K_KIT); press (K_PAD0 + 1); lift (K_PAD0 + 1); lift (K_KIT); lift (K_SHIFT); U->service ();
        CHECK (store::exists (disk, "KIT02.CBK"), "SHIFT + KIT + pad 2 saves kit 2");
        press (K_SHIFT); press (K_WRITE); lift (K_WRITE); lift (K_SHIFT); U->service ();
        CHECK (store::exists (disk, "PROJ01.CBP"), "SHIFT + WRITE saves the project");
        U->disk = nullptr;
    }
    U->stepMode = true; U->stepPage = 0; writeScreen (dir + "/screen_steps.pgm"); U->stepMode = false;
    U->page = 3; writeScreen (dir + "/screen_play.pgm");
}
