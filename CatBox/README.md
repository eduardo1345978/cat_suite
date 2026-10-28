# Cat Box 2

A groovebox you play yourself: **8 equal tracks**, and each track picks its own **sound engine**, its
own **note FX** and has its own **pattern** and **mixer strip**. Everything plays together at one tempo,
in one key. Nothing is generated: you play it in or tap it into the steps.

## Three screens

- **SYNTH** (it starts here): one "digital analog" synthesizer on one panel, like a Moog: controllers, 3 oscillators,
  mixer, filter, filter and amp envelopes, LFO, poly-mod, output - every knob on screen, drag it with the mouse
  (Ctrl = fine), the wheel turns it, a double-click puts it back; switches flip with a click. The small numbers show
  which of your 12 knobs turns it now (◀ ▶ = the knob pages OSC, FILTER, ENV, LFO, OUTPUT).
  **10 models** - a click on the model's name switches - each its own circuit, not just presets:
  MINI (Moog ladder 24 dB, overload, drift), PROPHET (poly-mod), OB (12 dB state variable: low / band / high / notch),
  JUPITER (clean OTA, high pass), JUNO (stable DCO, sub, chorus I / II), SH (punchy mono), ACID (303 diode filter),
  MS (MS-20: the resonance screams), ODYSSEY (sync, ring), BUCHLA (wave folder, lowpass gate). Each has its colours.
  Voices: Mono, Legato, Poly or Unison, always. 45 sounds made for the models.
- **DRUMS**: a drum machine: kit ◀ ▶, a ready-made beat ◀ ▶, tempo, Play, Clear; click a step to turn it on or off,
  click a drum's name to hear it.
- **STUDIO**: everything else - 8 tracks, all engines, SEQ, note FX, mixer, effects, the libraries (below).

| Engine | What it is | Factory sounds |
|---|---|---|
| **Analog** | 2 oscillators (saw, square + PWM, triangle, sine, noise), sub, unison 4, sync, FM, ring, ladder / state-variable filter, 2 envelopes, pitch envelope, 2 LFOs (tempo-synced), poly / mono / legato, drive, crush, EQ, chorus, width | 39 |
| **Acid** | a TB-303 model: curved saw / shaped square, diode-ladder filter with the high-pass in its resonance loop, decay, accent with the "wow" of repeated accents, slide, soft attack, Tube / Fuzz / Rat distortion and every hidden trim pot on a knob | 17 |
| **Drums** | 16 synthesized drums (kick, snare, clap, hats, toms, rim, cowbell, crash, ride, shaker, conga, snap, zap); kit model 808 / 909 / Lo-fi / Acoustic; tune, decay, tone, level per drum; drive, bus compressor, hat choke, stereo spread | 28 kits |
| **Keys** | piano (stiff-string partials), FM e-piano, nylon guitar (Karplus-Strong), pads, strings, glass bell, chip, pluck + filter, drive, tremolo, chorus, EQ | 25 |
| **Glide** | Glide-Cat's synth: two layers, voices that glide from chord to chord, expression, modulators, matrix, its own effects | 67 |
| **FM** | 4 sine operators, 8 algorithms, feedback, separate carrier / modulator envelopes, brightness, key scaling: DX e-pianos, bells, mallets, basses, organs, brass | 29 |
| **Wavetable** | 8 morphing tables (basic shapes, pulse, vowels, harmonic sweep, digital sync, organ, growl, glass), position with envelope and LFO, warp, unison 7, filter | 22 |
| **Choir** | Sing-Cat's choir: physically modelled singers and cats (glottis + vocal tract each), vowels, styles, rooms; up to 16 singers | 67 |
| **Sampler** | your samples: **record one with a microphone** (Rec on a Sampler track's SOUND page) or put WAVs in the `samples` folder; Keys (across the keyboard, loop forward / ping-pong), Chop (2-16 slices on the pads), One shot; envelope, filter, crush, reverse. 8 starter samples are made by the box itself the first time | 16 |
| **Granular** | clouds of grains from any sample: position and travel, grain size, density, scatter, pitch scatter, reverse grains, freeze | 12 + 12 library |
| **Synth** | the SYNTH screen's synthesizer (10 models, above) | 45 |
| **Kit** | a drum kit of 16 samples, one per pad: sample, tune, level, pan, decay, tone per pad; drive, hat choke, stereo | 15 library kits |

**499 factory sounds**, in categories, heard the moment you pick them. **FX Lab** sounds come with their own effect chains;
**Library** sounds use the free sample libraries (below).
**Note FX** (any track): **Arp** (rate, order incl. bounce / random / chord, octaves, gate, swing,
euclidean steps / hits / rotate, ratchet, chance, accents, hold), **Chords** (one key = one chord in
the key and scale: triad ... 11th, sus, power; voicing, inversion, strum, bass note) and **Orbit**
(every held note is a planet that plays when it crosses a line). They work on everything the track
plays: the keys, the pads and its own steps.

The same program runs in two places:
- **On the PC** (`Cat-Box.exe`): the 5" screen plus the whole front panel (12 knobs, 20 buttons, 16 pads,
  slider, joystick) in one window. Drag or scroll the knobs, click the pads and buttons, or use the computer keyboard.
- **In the box** (Raspberry Pi 5 + HiFiBerry + 5" screen + Pico): `Cat-Box --kiosk` shows only the
  800 x 480 screen, full screen. Keys and pads come from a MIDI keyboard (e.g. the M-Audio Oxygen Pro Mini).

## Effects, macros, modulation (every track)

On the SOUND pages, after the engine's own pages:
- **FX**: 4 insert effects in a row (sound > 1 > 2 > 3 > 4 > mixer). Knob 1 = the slot, 2 = the effect, 3-10 = its knobs,
  11 = on / bypass, 12 = its presets; Shift: the 4 effects at once, on / off, and **knob 9 = a whole chain** (25 chains:
  Lo-fi, Dub, Big room, Wide pad, Trance gate, Talk box, Pumping, Glitch, Guitar amp, Shimmer up ...).
  **22 effects**: Filter, Drive, Crusher, Delay, Chorus, Phaser, Flanger, Compressor, EQ, Tremolo / auto-pan, Trance gate,
  Ring mod, Reverb, Width, Tape / lo-fi, Pitch shift, Resonator, Vowel, Stutter, Pump (sidechain), Transient, Amp & cabinet.
- **MACRO**: 8 macro knobs (MENU > More > Assign to macro, then turn any knob: the macro moves it), the 2 LFOs; Shift: LFO shapes,
  rates, tempo sync, restart, the note envelope, random.
- **MOD**: 16 slots: source (LFO 1 / 2, envelope, velocity, note, mod wheel, random, bar, macro 1-8) > destination (any sound,
  effect or mixer knob) > amount (+/-). Knobs 5-12: the amounts of slots 1-8 (Shift: 9-16). MENU > More > Random modulation.
- **MY**: your own page: MENU > More > Add to MY knobs, then turn any knob (24 places).

## Sends (MIX, ◀ ▶)

- **SENDS**: knobs 1-8 = reverb per track (Shift: delay); reverb type and size, delay type and time.
- **FX SEND**: a third send with any of the 22 effects (chorus, phaser, flanger, pitch shift ...): knobs 1-8 = how much of each track.
- **RETURNS**: reverb types Hall, Room, Plate, Shimmer, Cloud, Spring, Dark with pre-delay, ducking (the tail ducks while you
  play) and level; delay types Clean, Tape echo, Dub, Ping-pong, Multi-tap, Diffused, Reverse with wow, ducking and
  *delay into reverb*.

## Free sample libraries

Run **get_libraries.bat** (PC; with git it uses git, else it downloads the zips) or `bash CatBox/pi/get_libraries.sh` (Pi), once.
About 6 GB go into `CatBox/library`:
- **VSCO 2 Community Edition** (CC0): strings (sections, solo violin, contrabass; sustain, tremolo, pizzicato, spiccato), harp,
  horn, trumpet, trombone, tuba, flute, clarinet, oboe, bassoon, piccolo, upright piano, organ, glockenspiel, marimba,
  xylophone, timpani, orchestral percussion.
- **Dirt-Samples** (TidalCycles): about 220 folders of drums (808, 909, DrumTraks, DR-110, jazz, house, techno ...),
  breaks, synths (juno, moog, hoover, rave ...), voices and noises.

Cat Box finds the library by itself (also `CATBOX_LIBRARY=<folder>` or `<data>/library`). Only what is used is read into memory.
Every sample is in the browser: **SOUND, Sound again**, engine Sampler / Granular / Kit, then the **LIB ...** categories (one per folder);
on a Kit it goes onto the pad you hit last. A sampler sound plays from the sample's own note (read from its name).
**Multi-sample** (Sampler, FILTER page Shift, on by itself for instrument folders): the whole folder is one instrument -
every key plays the nearest recorded note, and how hard you play picks the soft or loud recording (up to 4 layers).

## The screen

Calm and readable: grey on black, one accent colour, big type. **PROJECT, ▶ = LOOK**: 8 themes (Graphite, Paper, Midnight,
Forest, Terminal, High contrast, Plum, Sand), 9 accent colours, big text, ring knobs, a colour per track, the help line on / off. **A help line above the knobs** always says what the pads and knobs do right now; **Help** (MENU, F1 or a touch) explains the whole page.
- **Top:** the track number, its engine (and note FX), the **sound's name** (big; `*` = changed), tempo, bar.beat, Play and Rec.
- **Under it:** the modes (SOUND, SEQ, NOTE FX, MIX, PROJECT) and the **pages** of the mode with ◀ ▶.
- **Middle:** what the mode is about (the sound's waveform and envelope, the browser, the steps, the arp /
  chords / planets, the mixer, tempo and key).
- **The 12 knobs are always on screen**: name, value and a bar for each. With **Shift** held: the next 12.
- **Bottom:** the 8 tracks (engine, a dot that lights when it plays, M = muted, S = solo).
Everything can be touched: tracks, modes, pages, knobs (drag), Play.

## Buttons

| Button | What it does |
|---|---|
| **Track 1-8** | pick the track (everything on screen is about it) · **Shift + Track** = mute |
| **Sound** | the knobs edit the sound (◀ ▶ = its pages: e.g. Analog OSC / FILTER / ENV / LFO / FX) · **Sound again** = the **browser**: knob 1 engine, knob 2 category, knob 3 sound, ◀ ▶ = next / previous sound, **Rec** = save as a *User* sound · **Shift + Sound** = Init |
| **Seq** | the pads are the 16 steps of a bar (◀ ▶ = bar). **Tap** an empty step = on and picked, tap the picked step = off. **The knobs change the picked step** (no holding): note, velocity, length, slide, accent, chance, nudge, notes 2-4, ratchet, semitone. **Stopped: play keys and they fill the steps one by one** (keys together = a chord). Drums: **hit a drum** (pad outside SEQ, or a key) to pick it, then tap its steps; knob 3 = roll. **Shift + knobs** = the whole pattern: length, rate, swing, transpose, rotate, all velocities / lengths, octave. **Shift + ◀** copies the bar, **Shift + ▶** pastes it |
| **Note FX** | ◀ ▶ = Off / Arp / Chords / Orbit, the knobs edit it, **Shift + ◀ ▶** = its presets |
| **Mix** | knobs 1-8 = volumes (Shift: pan), 9-12 = master, glue, tape, width; ◀ ▶ = SENDS, FX SEND, RETURNS (see *Sends*). Pads 1-8 = mute, 9-16 = solo · **Shift + Mix** = unmute all |
| **Project** | tempo, swing, key, scale, click, master volume, DJ filter, pads (piano / scale), pad octave; **knob 12 = Factory reset** (turn it all the way right: a fresh project, scenes cleared; your saved sounds, samples and MIDI keyboard stay; Undo brings it all back; on the PC also right-click > *Factory reset...*); Shift: glue, tape, width, reverb, delay |
| **Project again = MENU** | 16 big buttons on the screen, the pads press them: **Save / Load project** (8 places), **New project**, **Clear track**, **Clear all** (red = press twice), **Copy track**, **Drum pattern** (15: house, techno, trap, hip hop, breakbeat, d&b, reggaeton, disco ...), **Bass line** (12, in the key and scale), Click, Pads piano / scale, Learn keyboard, **Help**, Tap tempo, Save sound, Undo, Close |
| **◀ Prev / Next ▶** | pages (see each mode) · **Shift + ◀ ▶** = pad octave |
| **Shift** | the second layer of knobs and the Shift combinations |
| **Scene** | the pads: 1-8 = scenes (recall at the next bar while playing), **Rec + pad** = save; 9-16 = mute tracks |
| **Rec** | record what you play into the steps while it runs (drums: the hit; notes: length, legato = slide, hard = accent); **turn a sound knob while it records and the move is written into the step that plays** (up to 4 knobs per step, shown as KNOB / a dot in SEQ) · **Shift + Rec** = clear the track's pattern |
| **Play** | start / stop · **Shift + Play** = tap tempo |
| **Chaos** | hold 0.6 s: new random sound settings from a random factory sound of the engine (volume stays) · **Shift + Chaos** = **Undo** (up to 40 steps: steps, recording, cleared patterns, scenes, sounds, knob edits) |

**Slider** = DJ filter (down = low-pass, middle = off, up = high-pass). **Joystick** = pitch bend (x) and mod (y).

**Pads** (2 rows of 8): drums = the 16 drums (1 kick, 2 snare, 3 clap, 4 closed hat ...);
other engines = like a piano: the bottom row the white keys C D E F G A B C, the top row the black keys
(PROJECT: *Pads = Scale* puts the scale on them instead).

## The PC keyboard

`1-8` / `Q-I` the pads · `Space` Play · `Enter` Rec · `Shift` Shift · `Tab` next track · `←` `→` Prev / Next ·
`F2` Sound · `F3` Seq · `F4` Note FX · `F5` Mix · `F6` Project · `N` Scene · `C` (hold) Chaos · `Z` / `X` pad octave ·
`F1` all keys. Mouse: drag or scroll a knob, double-click = back to its start; a pad clicked high = hard, low = soft.
Right-click: sound card and MIDI, learn a MIDI keyboard, full screen.

## A MIDI keyboard (e.g. M-Audio Oxygen Pro Mini)

Plug it in: its **keys play the track** on screen, the pitch wheel bends, the mod wheel modulates.
To use its knobs, faders, buttons and pads too, teach them once: right-click > *Learn a MIDI keyboard*
(on the box: the PROJECT page). The screen asks for knobs 1-12, the buttons, the 16 pads (switch the
pad bank for 9-16), the slider and a *knob bank* button (the keyboard's knobs then reach knobs 5-12).
**Skip** what it doesn't have, **Done** whenever. Remembered in `midimap2.json`.

## The PC app and the box together

`Cat-Box.exe` plays by itself, and it can **connect to the box** over your home network: the panel on
the right finds boxes (or type the address the box shows on its PROJECT page). Connected, the PC's
knobs, pads, buttons and keys play the box, the PC's screen shows the box, and the PC stays silent.
The box answers on TCP 50650 and announces itself on UDP 50651.

## The hardware (Raspberry Pi Pico, USB MIDI, channel 16)

The firmware and the wiring are in `CatBox/pico` (Arduino IDE, about 10 minutes to put on the Pico).


| Control | MIDI |
|---|---|
| Encoders 1-12 | CC 20-31, relative: 1..63 = clicks right, 127..65 = clicks left |
| Buttons (20) | CC 102-121 in this order: Track 1-8, Sound, Seq, Note FX, Mix, Project, Prev, Next, Shift, Scene, Rec, Play, Chaos (127 = down, 0 = up) |
| Slider | CC 8 (0-127) |
| Joystick | CC 16 (x) and CC 17 (y), 64 = middle; or pitch bend |
| Pads (if any) | notes 36-51 |

## The box, version 2 (a synth panel)

28 potentiometers on the synth (oscillators, filter, envelopes, LFO, delay and reverb, glide, volume - they pick the
value up when they pass it, nothing jumps), 12 push encoders = the 12 knobs on the screen (push + turn = the second
layer), 16 step buttons with LEDs, SYNTH / DRUMS / STUDIO buttons. Firmware, wiring, parts and the panel drawing:
`CatBox/pico/catbox_panel_v2` and `CatBox/pico/panel_v2.svg`. MIDI channel 16: pots CC 40-67, pushes CC 70-81,
screens CC 85-87, steps notes 60-75 (the LEDs come back on the same notes).
On the box's own screen (`--kiosk`) the SYNTH page doesn't repeat the labelled pots: it shows the sound alive - a
**circular oscilloscope** (one period of the wave around a circle, with a glowing trail), the wave and its spectrum,
the filter's curve (its slope, resonance and mode as you turn), both envelopes - with the 12 knobs below, and the pot
you turn big in the middle for a moment (waiting to take over: a dot shows where the pot is).

## Memory

Everything is remembered: the project (`state2.json`), the 8 scenes (`scenes.json`), your sounds
(`sounds/<Engine>/*.json`) and the MIDI keyboard (`midimap2.json`) in `%APPDATA%\CatSuite\CatBox` on
Windows, `~/.config/CatSuite/CatBox` on the Pi (or `--data <folder>`).

## Build

- PC: double-click **build_catbox.bat** (builds Cat Box, runs its self test, starts it).
- Raspberry Pi: `git clone` the repo, then `bash CatBox/pi/setup_pi.sh`.

## Self test

`Cat-Box --selftest out.wav` checks everything offline: the hardware MIDI map, the modes and pages,
**every factory sound** (heard and clean), the browser, steps (tap, hold + key, hold + knob, drums),
patterns playing, Rec (drums, legato = slide, accents), the three note FX, mixer, every master knob
turned up, the DJ filter, scenes at the bar, Chaos and back, MIDI learn, the screen in every mode
(`out-screen1..6.png`, `out-panel.png`), the PC-box link and memory after a restart. The song is in
`out.wav`, the report in `out.txt`; exit code 0 = all fine.
