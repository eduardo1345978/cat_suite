# Cat Box

A groovebox you play yourself: **Drums** (a step sequencer, 16 drums x 16 / 32 / 64 steps), **Arp**
(an arpeggiator like the Behringer Poly D's with a 16-step rhythm, or a plain synth), **Bass** (a
303-style step sequencer with its own Acid synth) and **FX** (master effects and performance pads).
Everything plays together at one tempo, in one key. Nothing is generated: you draw the steps or play them in.

The same program runs in two places:

- **On the PC** (`Cat-Box.exe`): the 5" screen plus the whole front panel in one window. Drag or
  scroll the knobs, click the pads and buttons, or use the computer keyboard.
- **In the box** (Raspberry Pi 5 + HiFiBerry + 5" screen + Pico controller): `Cat-Box --kiosk` shows
  only the 800 x 480 screen, full screen; the real knobs, pads and buttons do the rest.

## The box screen (5")

Kept simple, the box is played with its hands: the page and the preset's name at the top, the tempo,
and a big **oscilloscope** of what you hear. Turn a knob and a card shows its name, a **big number**
and a **bar**. Hold **Shift** (presets) or press **Scene** and the screen shows the 16 slots with their
names. At the bottom: the steps of the page (or the effects held on the FX page).
On the PC: right-click > *Show the box screen (5")* shows the same screen.

## The PC app and the box together

`Cat-Box.exe` on the PC is a full app on its own (it plays by itself), and it can **connect to the box**
over your home network (WiFi or cable; the box needs to be on the same network):

- The panel on the right finds boxes on the network (*Connect to Cat Box*), or type the box's address
  (the box shows it at the top right while you hold **Shift**: *PC link: 192.168.1.23*).
- Connected, the PC's knobs, pads, buttons and keyboard **play the box**; the PC's screen shows what the
  box does, and the PC itself stays silent. *Disconnect* and the PC plays by itself again.
- **Presets**: every preset and scene with its name. *Load*, *Name* (give it a name, it is shown on the
  box), *PC > Box* and *Box > PC* copy a preset between them.

Technically: the box (`Cat-Box --kiosk`, or `--box` in a window) answers on TCP port 50650 and announces
itself on UDP port 50651.

## Build

- PC: double-click **build_catbox.bat** (builds only Cat Box, runs its self test, starts it).
  It is also part of `build_and_install.bat`.
- Raspberry Pi: `git clone` the repo on the Pi, then `bash CatBox/pi/setup_pi.sh` (installs the
  libraries, builds, and starts Cat Box full screen after every boot).

## Controls

| | What it does |
|---|---|
| **12 knobs** | the sound of the page on screen; with **Shift** held a second layer (key, scale, effects, master) |
| **16 pads** (2 rows of 8) | Drums: play the drums (the last one hit is the *selected drum*) · Arp: notes of the scale · Bass: notes of the scale · FX: performance effects while held |
| **Play** | start / stop · **Shift + Play** = tap tempo |
| **Steps** | the pads become 16 steps (per page). Drums: steps of the selected drum (tap: on > accent > off) · Arp: on > rest > tie > accent · Bass: tap an empty step = set it, tap a set step = pick it, tap again = clear; knobs 2 / 3 / 4 edit the picked step's note / slide / accent · **Shift + Steps** = clear (all drums or the selected one in Steps, the bass line, held arp notes, the FX page) |
| **Shift** | second knob layer; **Shift + pad** = load preset, **Shift + Rec + pad** = save preset |
| **Scene** | the pads become 16 scenes: pad = recall (all instruments, FX, tempo, key; at the next bar while playing), **Rec + pad** = save |
| **Instr** | Drums > Arp > Bass > FX (or click the page names on the screen) · **Shift + Instr** = mute |
| **Oct - / Oct +** | Drums: previous / next drum (**Shift**: step pages) · Arp: pad octave · Bass: pad octave (in Steps: step pages) |
| **Rec** | Drums / Bass: record what you play into the steps while it runs (Bass: hold two pads = slide, hit hard = accent) · Arp: Hold |
| **Chaos** | hold it for 0.6 s (a quick tap does nothing, so no accidents): new random sound settings for the page on screen, within musical ranges (patterns, tempo, key and volume stay) · **Shift + Chaos** = take it back (again = back to the chaos) |

Performance pads (FX page, held = on): Repeat 1/4, 1/8, 1/16, 1/32 · Sweep down, Sweep up, Tape stop,
Reverse · Kill drums, Kill arp, Kill bass, Crush · Wash, Riser, Repeat 1/2, Echo throw.

Computer keyboard: pads `1 2 3 4 5 6 7 8 / Q W E R T Y U I` (Z works as Y), Space = Play, B = Steps,
Shift = Shift, N = Scene, Tab = Instr, `,` `.` = Oct - / Oct +, Enter = Rec, C = Chaos, F11 = full screen.
Right-click = sound card and MIDI, light screen, presets folder.

### Knobs

| Knob | Drums | Arp | Bass | FX |
|---|---|---|---|---|
| 1 | Tempo (40-240) | Tempo | Tempo | Tempo |
| 2 | Kit | Division | Sound (Acid, Sub, Saw, Reese, Pluck) · Steps: note | Reverb |
| 3 | Swing | Mode | Cutoff · Steps: slide | Room size |
| 4 | Length (16 / 32 / 64) | Octaves 1-4 | Resonance · Steps: accent | Echo |
| 5 | Human | Gate | Env mod | Pump |
| 6 | Level of the selected drum | Swing | Decay | DJ filter (left LP, right HP) |
| 7 | Chance of the selected drum | Pattern length | Length 1-64 | Tape |
| 8 | Crush | Ratchet 1-4 | Accent | Vinyl |
| 9 | Drive | Chance | Drive | Crush |
| 10 | Filter | Accent | Slide time | Glue |
| 11 | Echo | Filter | Octave | Width |
| 12 | Volume | Volume | Volume | Pump on (Arp + Bass, Bass, Arp, Everything) |
| Shift + 1 / 2 | Key / Scale | Key / Scale | Key / Scale | Key / Scale |
| Shift + 3... | Chorus | Sound (8 + Supersaw, Acid), Echo, Chorus, Drive, Arp on / off | Echo, Chorus, Wave, Swing, Follow arp | Drums / Arp / Bass reverb send |
| Shift + 12 | Master | Master | Master | Master |

## Presets and memory

Each instrument has 16 presets (one per pad). Each page has 16 presets (one per pad); the FX page's presets hold its knobs. A preset holds the instrument's knobs, its pattern
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
| Buttons Play, Steps, Shift, Scene, Instr, Oct -, Oct +, Rec, Chaos | CC 102-110, 127 = pressed, 0 = released |

Any other MIDI keyboard (any other channel) plays the instrument on screen.

## Self test

`Cat-Box --selftest out.wav` plays everything offline and checks it: the MIDI map, drum and bass steps,
each instrument heard on its own, all three together (written to `out.wav`), the acid filter, the arp's
knobs, Rec on drums and bass, presets and that the box remembers everything after a restart. The result is in `out.txt`; exit code 0 = all ok.
