# Cat Box 2

A groovebox you play yourself: **8 equal tracks**, and each track picks its own **sound engine**, its
own **note FX** and has its own **pattern** and **mixer strip**. Everything plays together at one tempo,
in one key. Nothing is generated: you play it in or tap it into the steps.

| Engine | What it is | Factory sounds |
|---|---|---|
| **Analog** | 2 oscillators (saw, square + PWM, triangle, sine, noise), sub, unison 4, sync, FM, ring, ladder / state-variable filter, 2 envelopes, pitch envelope, 2 LFOs (tempo-synced), poly / mono / legato, drive, crush, EQ, chorus, width | 39 |
| **Acid** | a TB-303 model: curved saw / shaped square, diode-ladder filter with the high-pass in its resonance loop, decay, accent with the "wow" of repeated accents, slide, soft attack, Tube / Fuzz / Rat distortion and every hidden trim pot on a knob | 17 |
| **Drums** | 16 synthesized drums (kick, snare, clap, hats, toms, rim, cowbell, crash, ride, shaker, conga, snap, zap); kit model 808 / 909 / Lo-fi / Acoustic; tune, decay, tone, level per drum; drive, bus compressor, hat choke, stereo spread | 15 kits |
| **Keys** | piano (stiff-string partials), FM e-piano, nylon guitar (Karplus-Strong), pads, strings, glass bell, chip, pluck + filter, drive, tremolo, chorus, EQ | 25 |
| **Glide** | Glide-Cat's synth: two layers, voices that glide from chord to chord, expression, modulators, matrix, its own effects | 67 |
| **FM** | 4 sine operators, 8 algorithms, feedback, separate carrier / modulator envelopes, brightness, key scaling: DX e-pianos, bells, mallets, basses, organs, brass | 29 |
| **Wavetable** | 8 morphing tables (basic shapes, pulse, vowels, harmonic sweep, digital sync, organ, growl, glass), position with envelope and LFO, warp, unison 7, filter | 22 |
| **Choir** | Sing-Cat's choir: physically modelled singers and cats (glottis + vocal tract each), vowels, styles, rooms; up to 16 singers | 67 |
| **Sampler** | your samples: **record one with a microphone** (Rec on a Sampler track's SOUND page) or put WAVs in the `samples` folder; Keys (across the keyboard, loop forward / ping-pong), Chop (2-16 slices on the pads), One shot; envelope, filter, crush, reverse. 8 starter samples are made by the box itself the first time | 16 |
| **Granular** | clouds of grains from any sample: position and travel, grain size, density, scatter, pitch scatter, reverse grains, freeze | 12 |

**311 factory sounds**, in categories, heard the moment you pick them.
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

## The screen

Calm and readable: grey on black, one accent colour, big type.
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
| **Seq** | the pads are the 16 steps of a bar (◀ ▶ = bar). Tap = step on / off. **Hold a step** and play keys (notes, a chord) or turn knobs: note, velocity, length, slide, accent, chance, nudge, notes 2-4. Nothing held: length, rate, swing, transpose (in the scale), rotate, all velocities / lengths, octave. Drums: all 16 drums on screen at once; **Shift + pad** (or knob 8) picks the drum. **Shift + ◀** copies the bar, **Shift + ▶** pastes it |
| **Note FX** | ◀ ▶ = Off / Arp / Chords / Orbit, the knobs edit it, **Shift + ◀ ▶** = its presets |
| **Mix** | knobs 1-8 = volumes (Shift: pan), 9-12 = master, glue, tape, width; ◀ ▶ = SENDS (reverb and delay per track, reverb / delay settings). Pads 1-8 = mute, 9-16 = solo · **Shift + Mix** = unmute all |
| **Project** | tempo, swing, key, scale, click, master volume, DJ filter, pads (piano / scale), pad octave; Shift: glue, tape, width, reverb, delay |
| **◀ Prev / Next ▶** | pages (see each mode) · **Shift + ◀ ▶** = pad octave |
| **Shift** | the second layer of knobs and the Shift combinations |
| **Scene** | the pads: 1-8 = scenes (recall at the next bar while playing), **Rec + pad** = save; 9-16 = mute tracks |
| **Rec** | record what you play into the steps while it runs (drums: the hit; notes: length, legato = slide, hard = accent) · **Shift + Rec** = clear the track's pattern |
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
