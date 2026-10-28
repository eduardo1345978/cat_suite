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

// ---- the four MCP23017 (addresses 0x20-0x23)
//   0x20  encoders 1-8 (A on even bits, B on odd bits)
//   0x21  encoders 9-12 (bits 0-7), pushes 1-8 (bits 8-15)
//   0x22  pushes 9-12 (bits 0-3), buttons (bits 4-13, below; 14-15 free)
//   0x23  the 16 step buttons (bit = step)
const uint8_t mcp[4] = { 0x20, 0x21, 0x22, 0x23 };
// the buttons on 0x22 bits 4-13: what each one sends (CC number)
const int numButtons = 10;
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
    Wire.setClock (1000000);           // the MCP23017 runs up to 1.7 MHz: fast polling, no lost encoder steps
    for (auto a : mcp)
    {
        mcpWrite (a, 0x00, 0xFF); mcpWrite (a, 0x01, 0xFF);   // all inputs
        mcpWrite (a, 0x0C, 0xFF); mcpWrite (a, 0x0D, 0xFF);   // pull-ups inside
    }
    for (auto p : muxSel) pinMode (p, OUTPUT);
    analogReadResolution (12);
    for (int i = 0; i < 28; ++i) potSent[i] = -1;
    leds.begin();
    leds.setBrightness (80);
    showLeds();
    while (! TinyUSBDevice.mounted()) delay (1);
}

const int8_t quad[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

void readExpanders()
{
    for (int c = 0; c < 4; ++c) bits[c] = mcpRead (mcp[c]);
    for (int k = 0; k < 12; ++k)
    {
        const uint16_t src = k < 8 ? bits[0] : bits[1];
        const int e = k < 8 ? k : k - 8;
        const uint8_t s = (uint8_t) ((src >> (2 * e)) & 3);
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
    // pushes: 1-8 on 0x21 bits 8-15, 9-12 on 0x22 bits 0-3 (pressed = low)
    for (int k = 0; k < 12; ++k)
    {
        const bool down = k < 8 ? ! ((bits[1] >> (8 + k)) & 1) : ! ((bits[2] >> (k - 8)) & 1);
        if (debounced (down, pushDown[k], pushT[k])) MIDI.sendControlChange (firstPushCC + k, down ? 127 : 0, channel);
    }
    for (int b = 0; b < numButtons; ++b)
    {
        const bool down = ! ((bits[2] >> (4 + b)) & 1);
        if (debounced (down, buttonDown[b], buttonT[b])) MIDI.sendControlChange (buttonCC[b], down ? 127 : 0, channel);
    }
    for (int s = 0; s < 16; ++s)
    {
        const bool down = ! ((bits[3] >> s) & 1);
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
