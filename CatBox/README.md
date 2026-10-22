# Cat Box

A groovebox made of the Cats: **Bubny** (Drum-Cat grooves on the drum kit), **Arp** (an arpeggiator
like the Behringer Poly D's, with a 16-step rhythm pattern) and **Bass** (Bass-Cat lines that follow
the chord you pick). All three play together at one tempo, in one key.

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
| **12 knobs** | the sound of the instrument on screen; with **Shift** held a second layer (key, scale, sound, echo, chorus, drive, master) |
| **16 pads** | Bubny: play the drums · Arp: notes of the key's scale (the arp plays what you hold) · Bass: pick the chord the bass line follows |
| **Play** | start / stop (drums and bass start on bar 1; the arp also runs while stopped when you hold notes) |
| **Hold** | the arp keeps playing after you let go |
| **Shift** | second knob layer; **Shift + pad** = load preset, **Shift + Rec + pad** = save preset |
| **Tap** | tap the tempo; **Shift + Tap** = clear (drums: the hits you recorded, arp: the held notes) |
| **Instrument** | Bubny > Arp > Bass; **Shift + Instrument** = mute the instrument on screen |
| **Oct - / Oct +** | Arp / Bass: octave of the pads · Bubny: previous / next groove variation |
| **Rec** | Bubny: record pad hits into the loop · Arp: pads edit the 16 steps (On > Rest > Tie > Accent) |

Computer keyboard: pads `1 2 3 4 / Q W E R / A S D F / Z X C V` (Y works as Z), Space = Play,
Tab = Instrument, H = Hold, T = Tap, Shift = Shift, `,` `.` = Oct - / Oct +, Enter = Rec, F11 = full screen.
Right-click = sound card and MIDI settings.

### Knobs

| Knob | Bubny | Arp | Bass |
|---|---|---|---|
| 1 | Tempo | Tempo | Tempo |
| 2 | Style (13 Drum-Cat styles) | Division (1/4 ... 1/32, triplets, dotted) | Style (18 Bass-Cat styles) |
| 3 | Density | Mode (up, down, up-down, down-up, as played, random, converge, chord) | Density |
| 4 | Fills | Octaves 1-4 | Ghost notes |
| 5 | Swing | Gate | Slides |
| 6 | Kit | Swing | Sound |
| 7 | Length (1 / 2 / 4 bars) | Pattern length 1-16 | Length |
| 8 | Variation | Ratchet 1-4 | Variation |
| 9 | Human | Chance | Drive |
| 10 | Crush | Accent | Filter |
| 11 | Filter | Filter | Octave |
| 12 | Volume | Volume | Volume |
| Shift + 1 / 2 | Key / Scale | Key / Scale | Key / Scale |
| Shift + 3... | Echo, Drive, Chorus | Sound, Echo, Chorus, Drive | Echo, Chorus, Fills |
| Shift + 12 | Master | Master | Master |

## Presets and memory

Each instrument has 16 presets (one per pad). A preset holds the instrument's knobs, its pattern
(arp steps / recorded drum hits) and the tempo (the tempo is only taken over while stopped).
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
| Buttons Play, Hold, Shift, Tap, Instrument, Oct -, Oct +, Rec | CC 102-109, 127 = pressed, 0 = released |

Any other MIDI keyboard (any other channel) plays the instrument on screen.

## Self test

`Cat-Box --selftest out.wav` plays everything offline and checks it: the MIDI map, each instrument
heard on its own, all three together (written to `out.wav`), the arp's knobs, Rec, presets and that
the box remembers everything after a restart. The result is in `out.txt`; exit code 0 = all ok.
