# Cat Box

A groovebox you play yourself: **Drums** (a step sequencer, 16 drums x 16 / 32 / 64 steps), **Arp**
(an arpeggiator like the Behringer Poly D's, with a 16-step rhythm pattern) and **Bass** (a 303-style
step sequencer with its own Acid synth). All three play together at one tempo, in one key.
Nothing is generated: you draw the steps or play them in with Rec.

The same program runs in two places:

- **On the PC** (`Cat-Box.exe`): the 5" screen plus the whole front panel in one window. Drag or
  scroll the knobs, click the pads and buttons, or use the computer keyboard.
- **In the box** (Raspberry Pi 5 + HiFiBerry + 5" screen + Pico controller): `Cat-Box --kiosk` shows
  only the 800 x 480 screen, full screen; the real knobs, pads and buttons do the rest.

## Build

- PC: double-click **build_catbox.bat** (builds only Cat Box, runs its self test, starts it).
  It is also part of `build_and_install.bat`.
- Raspberry Pi: `git clone` the repo on the Pi, then `bash CatBox/pi/setup_pi.sh` (installs the
  libraries, builds, and starts Cat Box full screen after every boot).

## Controls

| | What it does |
|---|---|
| **12 knobs** | the sound of the instrument on screen; with **Shift** held a second layer (key, scale, effects, master) |
| **16 pads** | Drums: play the drums (the last one hit is the *selected drum*) · Arp: notes of the scale, the arp plays what you hold · Bass: notes of the scale |
| **Play** | start / stop (the arp also runs while stopped when you hold notes) |
| **Steps** | the pads become 16 steps. Drums: steps of the selected drum (tap: on > accent > off) · Arp: on > rest > tie > accent · Bass: tap an empty step = set it (last note you played), tap a set step = pick it, tap the picked step = clear; knobs 2 / 3 / 4 edit the picked step's note / slide / accent |
| **Shift** | second knob layer; **Shift + pad** = load preset, **Shift + Rec + pad** = save preset |
| **Tap** | tap the tempo; **Shift + Tap** = clear (Drums: all, or the selected drum in Steps; Bass: the line; Arp: held notes) |
| **Instr** | Drums > Arp > Bass; **Shift + Instr** = mute the instrument on screen |
| **Oct - / Oct +** | Drums: select the previous / next drum (**Shift**: step pages 1-16, 17-32 ...) · Arp: pad octave · Bass: pad octave (in Steps: step pages) |
| **Rec** | Drums / Bass: record what you play into the steps while it runs (Bass: hold two pads = slide, hit hard = accent) · Arp: Hold |

Computer keyboard: pads `1 2 3 4 / Q W E R / A S D F / Z X C V` (Y works as Z), Space = Play, B = Steps,
Shift = Shift, T = Tap, Tab = Instr, `,` `.` = Oct - / Oct +, Enter = Rec, F11 = full screen.
Right-click = sound card and MIDI, light screen, presets folder.

### Knobs

| Knob | Drums | Arp | Bass |
|---|---|---|---|
| 1 | Tempo (40-240) | Tempo | Tempo |
| 2 | Kit | Division (1/4 ... 1/32, triplets, dotted) | Sound (Acid, Sub, Saw, Reese, Pluck) · in Steps: step note |
| 3 | Swing | Mode (up, down, up-down, down-up, as played, random, converge, chord) | Cutoff · in Steps: slide |
| 4 | Length (16 / 32 / 64 steps) | Octaves 1-4 | Resonance · in Steps: accent |
| 5 | Human | Gate | Env mod |
| 6 | Level of the selected drum | Swing | Decay |
| 7 | Chance of the selected drum | Pattern length 1-16 | Length 1-64 steps |
| 8 | Crush | Ratchet 1-4 | Accent |
| 9 | Drive | Chance | Drive |
| 10 | Filter | Accent | Slide time |
| 11 | Echo | Filter | Octave |
| 12 | Volume | Volume | Volume |
| Shift + 1 / 2 | Key / Scale | Key / Scale | Key / Scale |
| Shift + 3... | Chorus | Sound, Echo, Chorus, Drive | Echo, Chorus, Wave (saw / square), Swing |
| Shift + 12 | Master | Master | Master |

## Presets and memory

Each instrument has 16 presets (one per pad). A preset holds the instrument's knobs, its pattern
(drum steps / arp steps / bass line) and the tempo (the tempo is only taken over while stopped).
Everything is also remembered on its own: switch the box off and it starts where you left it.
Files: `%APPDATA%\CatSuite\CatBox` on Windows, `~/.config/CatSuite/CatBox` on the Pi
(or `--data <folder>`). Files are written safely (a temporary file is swapped in), so pulling the plug
never leaves a broken preset.

## The hardware controller (Raspberry Pi Pico, USB MIDI)

The Pico shows up as a normal USB MIDI device and sends on **channel 16**:

| Control | MIDI |
|---|---|
| Knobs 1-12 | CC 20-31, relative: 1..63 = clicks right, 127..65 = clicks left (two's complement) |
| Pads 1-16 | notes 36-51, velocity = how hard |
| Buttons Play, Steps, Shift, Tap, Instr, Oct -, Oct +, Rec | CC 102-109, 127 = pressed, 0 = released |

Any other MIDI keyboard (any other channel) plays the instrument on screen.

## Self test

`Cat-Box --selftest out.wav` plays everything offline and checks it: the MIDI map, drum and bass steps,
each instrument heard on its own, all three together (written to `out.wav`), the acid filter, the arp's
knobs, Rec on drums and bass, presets and that the box remembers everything after a restart. The result is in `out.txt`; exit code 0 = all ok.
