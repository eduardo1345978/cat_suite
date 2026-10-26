// Cat Box 2 - the front panel controller (Raspberry Pi Pico, USB MIDI)
//
// 12 encoders (on two MCP23017 port expanders), 20 buttons (on the Pico's own pins),
// a slider and a joystick (the Pico's three analog pins). The Pico shows up as a USB MIDI
// device and sends on MIDI channel 16 - exactly what Cat Box expects (see CatBox/README.md):
//   encoders 1-12  CC 20-31, relative: 1 = one click right, 127 = one click left
//   buttons        CC 102-121 (127 = down, 0 = up), in this order:
//                  Track 1-8, Sound, Seq, Note FX, Mix, Project, Prev, Next, Shift, Scene, Rec, Play, Chaos
//   slider         CC 8 (0-127)          joystick  CC 16 (x), CC 17 (y), 64 = middle
//
// Arduino IDE: Boards Manager "Raspberry Pi Pico/RP2040" (Earle Philhower), board "Raspberry Pi Pico",
// Tools > USB Stack: "Adafruit TinyUSB". Libraries: "MIDI Library" (FortySevenEffects).
// Wiring: see README.md next to this file.

#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Wire.h>

Adafruit_USBD_MIDI usbMidi;
MIDI_CREATE_INSTANCE (Adafruit_USBD_MIDI, usbMidi, MIDI);

const uint8_t channel = 16;
const uint8_t firstKnobCC = 20, firstButtonCC = 102, sliderCC = 8, joyXCC = 16, joyYCC = 17;

// ---- buttons: Pico pins, in the order of the CC numbers (pressed = pin to ground)
const uint8_t buttonPins[20] = { 0, 1, 2, 3, 6, 7, 8, 9,          // Track 1-8
                                 10, 11, 12, 13, 14,              // Sound, Seq, Note FX, Mix, Project
                                 15, 16, 17, 18, 19, 20, 21 };    // Prev, Next, Shift, Scene, Rec, Play, Chaos
// ---- the two MCP23017 (I2C0 on GP4 = SDA, GP5 = SCL): 8 encoders each, A on even bits, B on odd bits
const uint8_t expanders[2] = { 0x20, 0x21 };   // A0..A2 to ground / A0 to 3.3 V
const int stepsPerDetent = 4;                  // most encoders: 4 changes per click (set 2 for half-detent ones)
// ---- analog
const uint8_t sliderPin = 28, joyXPin = 26, joyYPin = 27;

uint8_t encState[16] {};      // last 2-bit state per encoder (12 used)
int8_t encAccum[16] {};
int clicks[12] {};
bool buttonDown[20] {};
uint32_t buttonChanged[20] {};
int lastSlider = -1, lastJoyX = -1, lastJoyY = -1;
float sliderF = 0, joyXF = 2048, joyYF = 2048;

void mcpWrite (uint8_t addr, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission (addr);
    Wire.write (reg);
    Wire.write (value);
    Wire.endTransmission();
}

uint16_t mcpRead (uint8_t addr)
{
    Wire.beginTransmission (addr);
    Wire.write (0x12);                 // GPIOA, then GPIOB (sequential)
    Wire.endTransmission (false);
    Wire.requestFrom (addr, (uint8_t) 2);
    const uint8_t a = Wire.read(), b = Wire.read();
    return (uint16_t) a | ((uint16_t) b << 8);
}

void setup()
{
    TinyUSBDevice.setProductDescriptor ("Cat Box Panel");
    MIDI.begin (MIDI_CHANNEL_OMNI);
    for (auto p : buttonPins) pinMode (p, INPUT_PULLUP);
    Wire.setSDA (4);
    Wire.setSCL (5);
    Wire.begin();
    Wire.setClock (400000);
    for (auto addr : expanders)
    {
        mcpWrite (addr, 0x00, 0xFF);   // IODIRA: inputs
        mcpWrite (addr, 0x01, 0xFF);   // IODIRB
        mcpWrite (addr, 0x0C, 0xFF);   // GPPUA: pull-ups
        mcpWrite (addr, 0x0D, 0xFF);   // GPPUB
    }
    analogReadResolution (12);
    while (! TinyUSBDevice.mounted()) delay (1);
}

// quadrature: +1 / -1 / 0 for (old state, new state)
const int8_t quad[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

void readEncoders()
{
    for (int c = 0; c < 2; ++c)
    {
        const uint16_t bits = mcpRead (expanders[c]);
        for (int e = 0; e < 8; ++e)
        {
            const int k = c * 8 + e;
            if (k >= 12) break;
            const uint8_t s = (uint8_t) ((bits >> (2 * e)) & 3);
            encAccum[k] += quad[(encState[k] << 2) | s];
            encState[k] = s;
            if (encAccum[k] >= stepsPerDetent) { encAccum[k] -= stepsPerDetent; ++clicks[k]; }
            if (encAccum[k] <= -stepsPerDetent) { encAccum[k] += stepsPerDetent; --clicks[k]; }
        }
    }
}

void sendEncoders()
{
    for (int k = 0; k < 12; ++k)
    {
        if (clicks[k] == 0) continue;
        const int c = constrain (clicks[k], -63, 63);
        MIDI.sendControlChange (firstKnobCC + k, c > 0 ? c : 128 + c, channel);   // two's complement
        clicks[k] = 0;
    }
}

void readButtons()
{
    const uint32_t now = millis();
    for (int b = 0; b < 20; ++b)
    {
        const bool down = digitalRead (buttonPins[b]) == LOW;
        if (down != buttonDown[b] && now - buttonChanged[b] > 6)   // 6 ms debounce
        {
            buttonDown[b] = down;
            buttonChanged[b] = now;
            MIDI.sendControlChange (firstButtonCC + b, down ? 127 : 0, channel);
        }
    }
}

void readAnalog()
{
    sliderF += ((float) analogRead (sliderPin) - sliderF) * 0.15f;
    joyXF += ((float) analogRead (joyXPin) - joyXF) * 0.3f;
    joyYF += ((float) analogRead (joyYPin) - joyYF) * 0.3f;
    auto toMidi = [] (float v, bool centred)
    {
        int m = constrain ((int) (v / 4095.0f * 127.0f + 0.5f), 0, 127);
        if (centred && abs (m - 64) <= 3) m = 64;   // a dead zone: the stick at rest is exactly the middle
        return m;
    };
    const int s = toMidi (sliderF, false), x = toMidi (joyXF, true), y = toMidi (joyYF, true);
    if (s != lastSlider && (lastSlider < 0 || abs (s - lastSlider) >= 1)) { MIDI.sendControlChange (sliderCC, s, channel); lastSlider = s; }
    if (x != lastJoyX) { MIDI.sendControlChange (joyXCC, x, channel); lastJoyX = x; }
    if (y != lastJoyY) { MIDI.sendControlChange (joyYCC, y, channel); lastJoyY = y; }
}

void loop()
{
    static uint32_t lastSend = 0, lastSlow = 0;
    readEncoders();                    // as fast as it goes (~3 kHz): no lost steps
    const uint32_t now = millis();
    if (now - lastSend >= 2) { lastSend = now; sendEncoders(); readButtons(); }
    if (now - lastSlow >= 10) { lastSlow = now; readAnalog(); }
    while (MIDI.read()) {}             // (nothing comes back yet)
}
