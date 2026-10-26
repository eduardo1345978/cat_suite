# Cat Box panel: Raspberry Pi Pico firmware

The Pico reads the front panel and sends it to the Raspberry Pi 5 (Cat Box) over USB as MIDI,
channel 16. Cat Box understands it without any setup.

## What you need

- Raspberry Pi Pico (or Pico W / Pico 2)
- 2 x MCP23017 port expander (I2C), for the 12 encoders
- 12 rotary encoders (EC11 or similar; the push switch is not used yet)
- 20 push buttons
- 1 slide potentiometer 10 k (linear), 1 analog joystick module (2 x 10 k)
- wires, a breadboard or a perfboard

## Wiring

| Pico pin | Goes to |
|---|---|
| GP0, GP1, GP2, GP3, GP6, GP7, GP8, GP9 | buttons Track 1-8 (other leg of each button to GND) |
| GP10, GP11, GP12, GP13, GP14 | Sound, Seq, Note FX, Mix, Project |
| GP15, GP16, GP17, GP18, GP19, GP20, GP21 | Prev, Next, Shift, Scene, Rec, Play, Chaos |
| GP4 (SDA), GP5 (SCL) | both MCP23017: SDA pin 13, SCL pin 12 |
| 3V3 (pin 36) | both MCP23017 VDD (pin 9) and RESET (pin 18); slider and joystick + |
| GND | MCP23017 VSS (pin 10); encoder middle pins (C); button legs; slider and joystick - |
| GP26 (ADC0) | joystick X |
| GP27 (ADC1) | joystick Y |
| GP28 (ADC2) | slider wiper (middle pin) |

MCP23017 addresses: the first one A0 A1 A2 (pins 15, 16, 17) all to GND = 0x20;
the second one A0 to 3V3, A1 A2 to GND = 0x21.

Encoders: on the first MCP23017, encoder 1 = GPA0 (A) + GPA1 (B), encoder 2 = GPA2 + GPA3,
encoder 3 = GPA4 + GPA5, encoder 4 = GPA6 + GPA7, encoder 5 = GPB0 + GPB1 ... encoder 8 = GPB6 + GPB7.
On the second: encoders 9-12 on GPA0-GPA7 the same way. The pull-ups are inside the chip (no resistors needed).
If an encoder turns the wrong way, swap its A and B wires. If it jumps 2 steps per click, set
`stepsPerDetent` to 2 at the top of the sketch.

## Putting the firmware on the Pico

1. Install the **Arduino IDE** (arduino.cc).
2. File > Preferences > *Additional boards manager URLs*:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
3. Tools > Board > Boards Manager: install **Raspberry Pi Pico/RP2040** (Earle F. Philhower).
4. Tools > Manage Libraries: install **MIDI Library** (FortySevenEffects).
5. Open `catbox_pico/catbox_pico.ino`. Tools > Board: **Raspberry Pi Pico**; Tools > USB Stack: **Adafruit TinyUSB**.
6. Hold the Pico's BOOTSEL button while plugging it in, then Upload.

The Pico now shows up as **Cat Box Panel** (a MIDI device). Plug it into the Pi 5 (or the PC to try it):
Cat Box switches it on by itself.
