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
| MX-style key switches (or Kailh Choc): 16 steps + 10 buttons, +4 spare | 30 | 10 EUR |
| Keycaps, transparent / light (to see the LEDs) | 26 | 10 EUR |
| SK6812 MINI-E LEDs (or a WS2812 strip, 60 / m, cut) | 19 | 5 EUR |
| 5" HDMI screen 800 x 480 (Waveshare 5inch or similar) | 1 | 40 EUR |
| 74AHCT125 (DIP-14) level shifter for the LED data (3.3 V to 5 V) | 1 | 1 EUR |
| Resistors: 4.7 k (I2C pull-ups) 2x, 330 ohm (LED data) 1x | 3 | 0.20 EUR |
| Capacitors: 100 nF ceramic 8x (one per chip), 470 uF 10 V electrolytic 1x (LED 5 V), 10 uF 1x (3V3) | 10 | 1.50 EUR |
| Perfboard, wires (silicone, 24-26 AWG), headers, IC sockets | | 10 EUR |
| PLA / PETG for the case | | 10 EUR |

## Wiring

**I2C** (GP4 = SDA, GP5 = SCL) to all four MCP23017 (SDA pin 13, SCL pin 12). VDD (pin 9) and RESET (pin 18) to 3V3,
VSS (pin 10) to GND, a 100 nF capacitor across each chip (as close to pins 9 / 10 as you can).
**Pull-ups: one 4.7 k resistor from SDA to 3V3 and one from SCL to 3V3** (once, not per chip; MCP23017 modules
often have them already - then leave these out). Without them I2C does not work. The firmware runs I2C at 400 kHz.

**GPA7 and GPB7 (pins 28 and 8) stay unconnected.** Newer MCP23017 chips have them as outputs only
(Microchip datasheet rev. C), so every chip uses only GPA0-6 and GPB0-6, and six switches go straight to the Pico.

| Chip | A2 A1 A0 | Address | GPA0-6 (pins 21-27) | GPB0-6 (pins 1-7) |
|---|---|---|---|---|
| 1 | GND GND GND | 0x20 | enc 1 A/B = GPA0/1, enc 2 = GPA2/3, enc 3 = GPA4/5, push 1 = GPA6 | enc 4 = GPB0/1, enc 5 = GPB2/3, enc 6 = GPB4/5, push 2 = GPB6 |
| 2 | GND GND 3V3 | 0x21 | enc 7 = GPA0/1, enc 8 = GPA2/3, enc 9 = GPA4/5, push 3 = GPA6 | enc 10 = GPB0/1, enc 11 = GPB2/3, enc 12 = GPB4/5, push 4 = GPB6 |
| 3 | GND 3V3 GND | 0x22 | pushes 5-11 = GPA0-6 | push 12 = GPB0, HOME GPB1, Play GPB2, Rec GPB3, Shift GPB4, Prev GPB5, Next GPB6 |
| 4 | GND 3V3 3V3 | 0x23 | steps 1-7 = GPA0-6 | steps 8-14 = GPB0-6 |

Straight to the Pico (pull-ups inside the Pico): step 15 = GP16 (pin 21), step 16 = GP17 (pin 22), Sound = GP18
(pin 24), Scene = GP19 (pin 25), Project = GP20 (pin 26), Chaos = GP21 (pin 27).

Every encoder's middle pin (C) and the other leg of every switch go to GND (the pull-ups are inside the chips).
If an encoder turns the wrong way, swap its A and B. If it jumps two steps per click, set `stepsPerDetent` to 2.

**Potentiometers**: both CD74HC4067 get S0-S3 from GP10, GP11, GP12, GP13 (shared), EN (or E) to GND, VCC to 3V3.
Multiplexer 1 SIG to GP26 (ADC0): channels 0-15 = pots 1-16; multiplexer 2 SIG to GP27 (ADC1): channels 0-11 = pots 17-28.
Each pot: one end to 3V3, the other to GND (ADC_GND, pin 33, is quieter), the wiper to its multiplexer channel.
Pot order (multiplexer channels, in this order): Osc 1 wave, Osc 2 wave, Detune, Osc 1, Osc 2, Osc 3, Cutoff,
Emphasis, Contour, Overload, High pass, F attack, F decay, F sustain, F release, A attack, A decay, A sustain,
A release, LFO rate, LFO amount, Delay time, Feedback, Delay, Reverb size, Reverb, Glide, Volume.

**LEDs**: the LEDs run on 5 V, the Pico's data is 3.3 V - too low for them to read reliably. So GP15 goes into a
**74AHCT125** (pin 2 = 1A in, pin 3 = 1Y out, pin 1 = 1OE to GND, pin 14 = VCC to 5 V, pin 7 = GND; tie the unused
inputs 2A / 3A / 4A = pins 5, 9, 12 to GND), then through the 330 ohm resistor to DIN of the first LED.
5 V for the chain from VBUS (Pico pin 40), GND to GND, and a **470 uF capacitor across 5 V / GND at the first LED**
(the stripe / minus side to GND). Order along the chain: steps 1-16, HOME, Play, Rec.
19 LEDs at the firmware's brightness (80 of 255) take at most about 0.4 A.

**Speaker (optional)**: a PAM8403 module, 5 V from the Pi's GPIO header (pin 2 or 4, GND pin 6) - not from a USB
port. Its inputs L / R / GND on the same wires as the line jacks. The speaker (40 mm, 4 ohm, 3 W) on OUT+ and OUT- of one
channel: **never connect OUT- to GND** - the amplifier is bridged and burns. About 80-85 dB at 1 m: enough for a room,
little bass under about 200 Hz.

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
