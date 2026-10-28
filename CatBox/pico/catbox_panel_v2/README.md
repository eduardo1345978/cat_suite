# Cat Box 2 - the synth panel (box version 2)

A front panel like an analog synth: **28 potentiometers** for the sound (one per job, nothing to page through),
**12 push encoders** under the screen for everything else (they are the 12 knobs you see on the screen),
**16 step buttons with LEDs** (TR-808 colours), a **HOME** button (the apps) and the rest.
A Raspberry Pi Pico reads it all and talks to the Pi 5 (Cat Box) over USB MIDI. Cat Box needs no setup.

The drawing with every hole: `../panel_v2.svg` (1 unit = 1 mm, 420 x 235 mm; print it in two halves along the
dashed line - it misses every hole) and `../panel_v2.png`.

## The panel

| Section | Potentiometers (left to right) |
|---|---|
| OSCILLATORS | Osc 1 wave, Osc 2 wave, Detune, Osc 1, Osc 2, Osc 3 |
| FILTER | **Cutoff** (big knob), Emphasis, Contour, Overload, High pass |
| LFO | Rate, Amount |
| | Glide, Volume |
| FILTER ENV | Attack, Decay, Sustain, Release |
| AMP ENV | Attack, Decay, Sustain, Release |
| EFFECTS | Delay time, Feedback, Delay (amount), Reverb size, Reverb (amount) |

The pots always play the SYNTH (on every screen). When you load another sound a pot doesn't jump: it **takes over
when you turn it past the sound's value** (the screen tells you where that is).

Encoders 1-12 = the 12 knobs on the screen (its pages: ◀ ▶). **Push and turn** = that knob's second layer (like Shift).

Buttons: **HOME** (the apps: then step 1-6 opens SYNTH, DRUMS, STUDIO, SCOPE, SAMPLER, SETTINGS; HOME again = back where you were) · Sound (sounds: once = the knobs, twice = the browser) · Scene · Project (twice =
MENU) · Shift · ◀ ▶ · Chaos (Shift + Chaos = undo) · Play · Rec.
Steps: HOME = the apps 1-6; DRUMS = the steps of the drum you picked (**Shift + step** = pick a drum and hear it); SYNTH = the synth's own
steps; STUDIO + Shift = step 1-8 picks the track.
LEDs: a step that is on (dim, its group's colour), the step playing (white), the picked one (blue); the screen
buttons, Play (green), Rec (red).

## What to buy (about 150 EUR without the Pi 5)

| Part | How many | About |
|---|---|---|
| Raspberry Pi Pico (or Pico 2) | 1 | 5 EUR |
| MCP23017 I2C port expander (DIP-28 or a module) | 4 | 6 EUR |
| CD74HC4067 16-channel analog multiplexer (module) | 2 | 4 EUR |
| Potentiometer 10 k linear (B10K), Alpha 9 mm vertical, 15 mm D-shaft | 28 | 17 EUR |
| Knobs for them (one bigger for Cutoff) | 28 | 12 EUR |
| Rotary encoder EC11 with push switch, 20 mm shaft | 12 | 15 EUR |
| Encoder knobs | 12 | 5 EUR |
| MX-style key switches (or Kailh Choc) | 29 | 10 EUR |
| Keycaps, transparent / light (to see the LEDs) | 29 | 10 EUR |
| SK6812 MINI-E LEDs (or a WS2812 strip, 60 / m, cut) | 19 | 5 EUR |
| 5" HDMI screen 800 x 480 (Waveshare 5inch or similar) | 1 | 40 EUR |
| Perfboard, wires (silicone, 24-26 AWG), headers, 100 nF capacitors, a 330 ohm resistor | | 12 EUR |
| PLA / PETG for the case | | 10 EUR |

## Wiring

**I2C** (GP4 = SDA, GP5 = SCL) to all four MCP23017 (SDA pin 13, SCL pin 12). VDD (pin 9) and RESET (pin 18) to 3V3,
VSS (pin 10) to GND, a 100 nF capacitor across each chip. Addresses with A0-A2 (pins 15-17):

| Chip | A2 A1 A0 | Address | Inputs |
|---|---|---|---|
| 1 | GND GND GND | 0x20 | encoders 1-8: encoder n = A on GPA/GPB bit 2(n-1), B on the next bit (GPA0+1 = enc 1 ... GPB6+7 = enc 8) |
| 2 | GND GND 3V3 | 0x21 | encoders 9-12 on GPA0-7 (the same way); pushes 1-8 on GPB0-7 |
| 3 | GND 3V3 GND | 0x22 | pushes 9-12 on GPA0-3; buttons on GPA4-GPB5 in this order: HOME, Play, Rec, Shift, Prev, Next, Sound, Scene, Project, Chaos (GPB6-7 free) |
| 4 | GND 3V3 3V3 | 0x23 | the 16 step buttons: GPA0 = step 1 ... GPB7 = step 16 |

Every encoder's middle pin (C) and the other leg of every switch go to GND (the pull-ups are inside the chips).
If an encoder turns the wrong way, swap its A and B. If it jumps two steps per click, set `stepsPerDetent` to 2.

**Potentiometers**: both CD74HC4067 get S0-S3 from GP10, GP11, GP12, GP13 (shared), EN (or E) to GND, VCC to 3V3.
Multiplexer 1 SIG to GP26 (ADC0): channels 0-15 = pots 1-16; multiplexer 2 SIG to GP27 (ADC1): channels 0-11 = pots 17-28.
Each pot: one end to 3V3, the other to GND (ADC_GND, pin 33, is quieter), the wiper to its multiplexer channel.
Pot order (multiplexer channels, in this order): Osc 1 wave, Osc 2 wave, Detune, Osc 1, Osc 2, Osc 3, Cutoff,
Emphasis, Contour, Overload, High pass, F attack, F decay, F sustain, F release, A attack, A decay, A sustain,
A release, LFO rate, LFO amount, Delay time, Feedback, Delay, Reverb size, Reverb, Glide, Volume.

**LEDs**: one chain from GP15 (through a 330 ohm resistor) to DIN; 5 V from VBUS (pin 40), GND. Order along the chain:
steps 1-16, HOME, Play, Rec.

**USB**: the Pico into the Pi 5. It shows up as **Cat Box Panel**.

## Putting the firmware on the Pico

1. Arduino IDE, Preferences > *Additional boards manager URLs*:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Boards Manager: install **Raspberry Pi Pico/RP2040** (Earle F. Philhower).
3. Library Manager: **MIDI Library** (FortySevenEffects) and **Adafruit NeoPixel**.
4. Open `catbox_panel_v2.ino`. Tools > Board: **Raspberry Pi Pico**; Tools > USB Stack: **Adafruit TinyUSB**.
5. Hold BOOTSEL while plugging the Pico in, then Upload.

Try it on the PC first: plug the Pico into the PC with Cat Box running - the knobs move the synth on the screen,
the step LEDs follow.

## MIDI (channel 16)

| Control | Message |
|---|---|
| Potentiometers 1-28 | CC 40-67, 0-127 |
| Encoders 1-12 | CC 20-31, relative (1 = right, 127 = left) |
| Encoder pushes 1-12 | CC 70-81 (127 down, 0 up) |
| HOME | CC 85 (CC 86 / 87 = DRUMS / STUDIO still work, for the first firmware) |
| Other buttons | CC 102 + button number (Sound 110, Project 114, Prev 115, Next 116, Shift 117, Scene 118, Rec 119, Play 120, Chaos 121) |
| Step buttons 1-16 | notes 60-75 |
| LEDs (Cat Box > panel) | notes 60-75 (velocity 1 on, 2 playing, 3 picked), 85 HOME (lit on HOME), 88 Play, 89 Rec |
