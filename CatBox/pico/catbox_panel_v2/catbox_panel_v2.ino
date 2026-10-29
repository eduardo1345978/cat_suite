// Cat Box 2 - the synth panel (box version 2) on a Raspberry Pi Pico, USB MIDI "Cat Box Panel"
//
//   28 potentiometers  CC 40-67 (absolute 0-127)          two CD74HC4067 multiplexers into ADC0 / ADC1
//   12 push encoders   CC 20-31 (relative: 1 = one click right, 127 = one click left), pushes CC 70-81
//   16 step buttons    notes 60-75 (on / off)               LEDs: the same notes come back (velocity = what to show)
//   HOME               CC 85 (to the apps; on HOME: back)  LED: note 85 (lit on HOME)
//   Play Rec Shift Prev Next Sound Scene Project Chaos       the box's button CCs (102 + button number)
//   (LEDs: Play note 88, Rec note 89)
// everything on MIDI channel 16 - exactly what Cat Box expects (CatBox/README.md, "The box, version 2").
//
// Arduino IDE: Boards Manager "Raspberry Pi Pico/RP2040" (Earle Philhower), board "Raspberry Pi Pico",
// Tools > USB Stack: "Adafruit TinyUSB". Libraries: "MIDI Library" (FortySevenEffects), "Adafruit NeoPixel".
// Wiring: README.md next to this folder.

#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include <MIDI.h>
#include <Wire.h>

Adafruit_USBD_MIDI usbMidi;
MIDI_CREATE_INSTANCE (Adafruit_USBD_MIDI, usbMidi, MIDI);

const uint8_t channel = 16;
const uint8_t firstKnobCC = 20, firstPotCC = 40, firstPushCC = 70, screenCC = 85, firstButtonCC = 102;
const uint8_t firstStepNote = 60, ledScreenNote = 85, ledPlayNote = 88, ledRecNote = 89;

// ---- pins
const uint8_t sdaPin = 4, sclPin = 5;                 // I2C0: the four MCP23017
const uint8_t muxSel[4] = { 10, 11, 12, 13 };         // S0..S3 of both multiplexers
const uint8_t muxA = 26, muxB = 27;                   // their SIG pins: ADC0 (pots 1-16), ADC1 (pots 17-28)
const uint8_t ledPin = 15;                            // the WS2812 / SK6812 chain (data in)
const int numLeds = 19;                               // 16 steps, HOME, Play, Rec

// ---- where every switch is: one of the four MCP23017 (0x20-0x23) or a Pico pin
// GPA7 and GPB7 of the MCP23017 are NOT used: newer chips (Microchip datasheet rev. C and later) have them as
// outputs only. So every chip uses GPA0-6 (bits 0-6) and GPB0-6 (bits 8-14), and the last six switches go
// straight to Pico pins GP16-GP21.
//   0x20  encoders 1-3 on GPA0-5, encoders 4-6 on GPB0-5, push 1 = GPA6, push 2 = GPB6
//   0x21  encoders 7-9 on GPA0-5, encoders 10-12 on GPB0-5, push 3 = GPA6, push 4 = GPB6
//   0x22  pushes 5-11 on GPA0-6, push 12 = GPB0, HOME Play Rec Shift Prev Next on GPB1-6
//   0x23  steps 1-7 on GPA0-6, steps 8-14 on GPB0-6
//   Pico  step 15 = GP16, step 16 = GP17, Sound = GP18, Scene = GP19, Project = GP20, Chaos = GP21
const uint8_t mcp[4] = { 0x20, 0x21, 0x22, 0x23 };
const uint8_t pico = 4;                               // "chip" 4 = a Pico pin (bit = its GP number)
struct Input { uint8_t chip, bit; };
constexpr uint8_t A (int n) { return (uint8_t) n; }          // GPA n = bit n
constexpr uint8_t B (int n) { return (uint8_t) (8 + n); }    // GPB n = bit 8 + n
// encoder n: pin A on bit, pin B on the next bit
const Input encA[12] = { { 0, A (0) }, { 0, A (2) }, { 0, A (4) }, { 0, B (0) }, { 0, B (2) }, { 0, B (4) },
                         { 1, A (0) }, { 1, A (2) }, { 1, A (4) }, { 1, B (0) }, { 1, B (2) }, { 1, B (4) } };
const Input pushIn[12] = { { 0, A (6) }, { 0, B (6) }, { 1, A (6) }, { 1, B (6) },
                           { 2, A (0) }, { 2, A (1) }, { 2, A (2) }, { 2, A (3) }, { 2, A (4) }, { 2, A (5) }, { 2, A (6) },
                           { 2, B (0) } };
const Input stepIn[16] = { { 3, A (0) }, { 3, A (1) }, { 3, A (2) }, { 3, A (3) }, { 3, A (4) }, { 3, A (5) }, { 3, A (6) },
                           { 3, B (0) }, { 3, B (1) }, { 3, B (2) }, { 3, B (3) }, { 3, B (4) }, { 3, B (5) }, { 3, B (6) },
                           { pico, 16 }, { pico, 17 } };
// the 10 buttons: HOME, Play, Rec, Shift, Prev, Next, Sound, Scene, Project, Chaos
const int numButtons = 10;
const Input buttonIn[numButtons] = { { 2, B (1) }, { 2, B (2) }, { 2, B (3) }, { 2, B (4) }, { 2, B (5) }, { 2, B (6) },
                                     { pico, 18 }, { pico, 19 }, { pico, 20 }, { pico, 21 } };
const uint8_t picoPins[6] = { 16, 17, 18, 19, 20, 21 };
// what each button sends (CC number)
const uint8_t buttonCC[numButtons] = {
    screenCC + 0,                                     // HOME
    firstButtonCC + 18, firstButtonCC + 17,           // Play, Rec
    firstButtonCC + 15, firstButtonCC + 13, firstButtonCC + 14,   // Shift, Prev, Next
    firstButtonCC + 8, firstButtonCC + 16,            // Sound (sounds / browser), Scene
    firstButtonCC + 12, firstButtonCC + 19 };         // Project (twice = MENU), Chaos (Shift + Chaos = undo)
const int stepsPerDetent = 4;                         // most encoders: 4 changes per click (2 for half-detent ones)

Adafruit_NeoPixel leds (numLeds, ledPin, NEO_GRB + NEO_KHZ800);

uint16_t bits[4] {};
uint8_t encState[12] {};
int8_t encAccum[12] {};
int clicks[12] {};
bool pushDown[12] {}, buttonDown[numButtons] {}, stepDown[16] {};
uint32_t pushT[12] {}, buttonT[numButtons] {}, stepT[16] {};
float potF[28] {};
int potSent[28];
uint8_t stepLed[16] {}, screenLed[3] {}, playLed = 0, recLed = 0;
bool ledsDirty = true;

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
    Wire.write (0x12);                 // GPIOA, then GPIOB
    Wire.endTransmission (false);
    Wire.requestFrom (addr, (uint8_t) 2);
    const uint8_t a = Wire.read(), b = Wire.read();
    return (uint16_t) a | ((uint16_t) b << 8);
}

// ---- the LEDs: what Cat Box sends back
void onNoteOn (byte ch, byte note, byte vel)
{
    if (ch != channel) return;
    if (note >= firstStepNote && note < firstStepNote + 16) stepLed[note - firstStepNote] = vel;
    else if (note >= ledScreenNote && note < ledScreenNote + 3) screenLed[note - ledScreenNote] = vel;
    else if (note == ledPlayNote) playLed = vel;
    else if (note == ledRecNote) recLed = vel;
    ledsDirty = true;
}
void onNoteOff (byte ch, byte note, byte) { onNoteOn (ch, note, 0); }

uint32_t stepColour (int k, uint8_t code)
{
    // TR-808 style: steps 1-4 red, 5-8 orange, 9-12 yellow, 13-16 white
    static const uint8_t group[4][3] = { { 255, 30, 10 }, { 255, 110, 0 }, { 255, 210, 0 }, { 230, 230, 230 } };
    const uint8_t* g = group[k / 4];
    switch (code)
    {
        case 1:  return leds.Color (g[0] / 3, g[1] / 3, g[2] / 3);          // a step that is on
        case 2:  return leds.Color (255, 255, 255);                         // the step playing now
        case 3:  return leds.Color (40, 120, 255);                          // the picked step / track
        default: return leds.Color (g[0] / 40, g[1] / 40, g[2] / 40);       // off: a glow so you see the button
    }
}

void showLeds()
{
    for (int k = 0; k < 16; ++k) leds.setPixelColor (k, stepColour (k, stepLed[k]));
    leds.setPixelColor (16, screenLed[0] ? leds.Color (255, 140, 40) : leds.Color (6, 4, 2));   // HOME
    leds.setPixelColor (17, playLed ? leds.Color (0, 220, 60) : leds.Color (0, 6, 2));
    leds.setPixelColor (18, recLed ? leds.Color (255, 20, 20) : leds.Color (6, 0, 0));
    leds.show();
    ledsDirty = false;
}

void setup()
{
    TinyUSBDevice.setProductDescriptor ("Cat Box Panel");
    MIDI.begin (MIDI_CHANNEL_OMNI);
    MIDI.setHandleNoteOn (onNoteOn);
    MIDI.setHandleNoteOff (onNoteOff);
    MIDI.turnThruOff();
    Wire.setSDA (sdaPin);
    Wire.setSCL (sclPin);
    Wire.begin();
    Wire.setClock (400000);            // 400 kHz: safe over the long panel wires (needs the 4.7 k pull-ups on SDA / SCL)
    for (auto a : mcp)
    {
        mcpWrite (a, 0x00, 0x7F); mcpWrite (a, 0x01, 0x7F);   // GPA0-6 / GPB0-6 inputs, GPA7 / GPB7 outputs (low, unused)
        mcpWrite (a, 0x0C, 0x7F); mcpWrite (a, 0x0D, 0x7F);   // pull-ups inside on the inputs
        mcpWrite (a, 0x14, 0x00); mcpWrite (a, 0x15, 0x00);   // OLATA / OLATB: the unused pins low
    }
    for (auto p : picoPins) pinMode (p, INPUT_PULLUP);
    for (auto p : muxSel) pinMode (p, OUTPUT);
    analogReadResolution (12);
    for (int i = 0; i < 28; ++i) potSent[i] = -1;
    leds.begin();
    leds.setBrightness (80);
    showLeds();
    while (! TinyUSBDevice.mounted()) delay (1);
}

const int8_t quad[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

// true = the pin is high (a switch to GND that is pressed reads low)
bool level (const Input& in)
{
    return in.chip == pico ? digitalRead (in.bit) != 0 : ((bits[in.chip] >> in.bit) & 1) != 0;
}

void readExpanders()
{
    for (int c = 0; c < 4; ++c) bits[c] = mcpRead (mcp[c]);
    for (int k = 0; k < 12; ++k)
    {
        const Input pinB { encA[k].chip, (uint8_t) (encA[k].bit + 1) };
        const uint8_t s = (uint8_t) ((level (encA[k]) ? 1 : 0) | (level (pinB) ? 2 : 0));
        encAccum[k] += quad[(encState[k] << 2) | s];
        encState[k] = s;
        if (encAccum[k] >= stepsPerDetent) { encAccum[k] -= stepsPerDetent; ++clicks[k]; }
        if (encAccum[k] <= -stepsPerDetent) { encAccum[k] += stepsPerDetent; --clicks[k]; }
    }
}

bool debounced (bool now, bool& state, uint32_t& t)
{
    const uint32_t ms = millis();
    if (now != state && ms - t > 6) { state = now; t = ms; return true; }
    return false;
}

void sendControls()
{
    for (int k = 0; k < 12; ++k)
        if (clicks[k] != 0)
        {
            const int c = constrain (clicks[k], -63, 63);
            MIDI.sendControlChange (firstKnobCC + k, c > 0 ? c : 128 + c, channel);
            clicks[k] = 0;
        }
    for (int k = 0; k < 12; ++k)
    {
        const bool down = ! level (pushIn[k]);          // pressed = low
        if (debounced (down, pushDown[k], pushT[k])) MIDI.sendControlChange (firstPushCC + k, down ? 127 : 0, channel);
    }
    for (int b = 0; b < numButtons; ++b)
    {
        const bool down = ! level (buttonIn[b]);
        if (debounced (down, buttonDown[b], buttonT[b])) MIDI.sendControlChange (buttonCC[b], down ? 127 : 0, channel);
    }
    for (int s = 0; s < 16; ++s)
    {
        const bool down = ! level (stepIn[s]);
        if (debounced (down, stepDown[s], stepT[s]))
        {
            if (down) MIDI.sendNoteOn (firstStepNote + s, 100, channel);
            else MIDI.sendNoteOff (firstStepNote + s, 0, channel);
        }
    }
}

void readPots()
{
    // one channel of both multiplexers at a time
    static int ch = 0;
    for (int b = 0; b < 4; ++b) digitalWrite (muxSel[b], (ch >> b) & 1);
    delayMicroseconds (8);   // let the analog settle after switching
    for (int m = 0; m < 2; ++m)
    {
        const int pot = m * 16 + ch;
        if (pot >= 28) continue;
        const float raw = (float) analogRead (m == 0 ? muxA : muxB);
        potF[pot] += (raw - potF[pot]) * 0.25f;
        // hysteresis: a new value only when it is clearly past the old one (no flicker between two values)
        const float pos = potF[pot] / 4095.0f * 127.0f;
        const int sent = potSent[pot];
        if (sent < 0 || pos > sent + 0.7f || pos < sent - 0.7f)
        {
            const int v = constrain ((int) (pos + 0.5f), 0, 127);
            if (v != sent) { MIDI.sendControlChange (firstPotCC + pot, v, channel); potSent[pot] = v; }
        }
    }
    ch = (ch + 1) & 15;
}

void loop()
{
    static uint32_t lastSend = 0, lastPot = 0, lastLed = 0;
    readExpanders();                       // as fast as it goes: no lost encoder steps
    const uint32_t now = micros();
    if (now - lastSend >= 2000) { lastSend = now; sendControls(); }
    if (now - lastPot >= 600) { lastPot = now; readPots(); }   // every pot ~100 times a second
    while (MIDI.read()) {}
    if (ledsDirty && now - lastLed >= 15000) { lastLed = now; showLeds(); }
}
