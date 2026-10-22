#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cstdint>

/**
    Cat Box: the data model shared by the engine, the controller logic and the screen.

    The box has three instruments that always play together (Bubny = drums, Arp, Bass).
    The 12 knobs edit the instrument on screen; with Shift held they edit a second layer.
    So every instrument has 24 parameter slots: 0..11 normal, 12..23 with Shift.
    A few slots are global (tempo, key, scale, master) and are the same on every page.
*/
namespace catbox
{
enum Instrument { Drums = 0, Arp, Bass, NumInstruments };

enum Button { BtnPlay = 0, BtnHold, BtnShift, BtnTap, BtnInstrument, BtnOctDown, BtnOctUp, BtnRec, NumButtons };

constexpr int numKnobs = 12;
constexpr int numPads = 16;
constexpr int numSlots = 24;
constexpr int numPresetSlots = 16;

enum Global { GTempo = 0, GKey, GScale, GMaster, NumGlobals };

/** Slot numbers (knob index, +12 with Shift). Slots 0, 12, 13 and 23 are global on every page. */
namespace slot
{
    constexpr int Tempo = 0, Key = 12, Scale = 13, Master = 23;

    // Bubny
    constexpr int DStyle = 1, DDensity = 2, DFills = 3, DSwing = 4, DKit = 5, DBars = 6, DSeed = 7,
                  DHuman = 8, DCrush = 9, DFilter = 10, DVolume = 11,
                  DEcho = 14, DDrive = 15, DChorus = 16;
    // Arp
    constexpr int ADivision = 1, AMode = 2, AOctaves = 3, AGate = 4, ASwing = 5, ALength = 6, ARatchet = 7,
                  AProbability = 8, AAccent = 9, AFilter = 10, AVolume = 11,
                  ASound = 14, AEcho = 15, AChorus = 16, ADrive = 17;
    // Bass
    constexpr int BStyle = 1, BDensity = 2, BGhosts = 3, BSlides = 4, BSound = 5, BBars = 6, BSeed = 7,
                  BDrive = 8, BFilter = 9, BOctave = 10, BVolume = 11,
                  BEcho = 14, BChorus = 15, BFills = 16;
}

enum class Kind { None, Percent, Choice, Integer, Bpm };

struct ParamDesc
{
    juce::String name;          // shown on the screen (Slovak)
    Kind kind = Kind::None;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    juce::StringArray choices;  // Kind::Choice
    int global = -1;            // Global index, or -1 = belongs to the instrument

    bool used() const noexcept { return kind != Kind::None; }
    /** How much one click of a knob changes the value. */
    float step() const noexcept { return kind == Kind::Percent ? 0.01f : 1.0f; }
    juce::String format (float value) const;
};

const ParamDesc& paramDesc (int instrument, int slotIndex);
juce::String instrumentName (int instrument);   // Bubny, Arp, Bass
juce::String instrumentFolder (int instrument); // file-system friendly: Drums, Arp, Bass

/** UTF-8 literal to juce::String (the screen texts are Slovak). */
inline juce::String sk (const char* utf8) { return juce::String::fromUTF8 (utf8); }

//==============================================================================
// Music helpers
juce::StringArray keyNames();     // C, C#, D ...
juce::StringArray scaleNames();
const std::vector<int>& scaleIntervals (int scale);
/** MIDI note of a scale degree (degree 0 = the key's root at baseNote's octave; degrees may go past one octave). */
int scaleNote (int key, int scale, int degree, int baseNote);
/** Chord name for a triad built on a scale degree, e.g. "Am". */
juce::String degreeChordName (int key, int scale, int degree);

/** Arp: note length of one step in quarter notes (1/4, 1/8, 1/8 T ...). */
double arpDivisionPpq (int index);
enum ArpMode { ModeUp = 0, ModeDown, ModeUpDown, ModeDownUp, ModeAsPlayed, ModeRandom, ModeConverge, ModeChord, NumArpModes };
/** Bass / drum loop length choice -> bars (1, 2, 4). */
int barsFromChoice (float choice);

/** The drum sound each pad plays (General MIDI notes, read by hub::DrumKit). */
int drumPadNote (int pad);
juce::String drumPadName (int pad);

//==============================================================================
/** Arp pattern step states. */
enum StepState : uint8_t { StepOn = 0, StepRest, StepTie, StepAccent, NumStepStates };

struct DrumUserHit
{
    int step = 0;      // 16th notes from the loop start
    int note = 36;
    int velocity = 100;
    bool operator== (const DrumUserHit& o) const { return step == o.step && note == o.note; }
};

/** One instrument's sound + pattern: what a preset holds. */
struct InstrumentState
{
    std::array<float, numSlots> values {};          // instrument slots (global slots are unused here)
    std::array<uint8_t, 16> arpSteps {};            // Arp only
    std::vector<DrumUserHit> drumHits;              // Bubny only: hits played in with Rec
    float tempo = 110.0f;                           // saved with the preset, applied only while stopped
};

/** Everything the box remembers between power-ons. */
struct BoxData
{
    std::array<float, NumGlobals> globals {};
    std::array<InstrumentState, NumInstruments> inst;
    std::array<bool, NumInstruments> muted {};
    std::array<int, NumInstruments> padOctave {};   // Arp / Bass: octave shift of the pads
    std::array<int, NumInstruments> loadedPreset { -1, -1, -1 };
    int bassChordDegree = 0;
    int page = Drums;

    BoxData();   // defaults

    float get (int instrument, int slotIndex) const;
    void set (int instrument, int slotIndex, float value);   // clamps, rounds choices

    juce::var toVar() const;
    void fromVar (const juce::var&);

    static juce::var instrumentToVar (int instrument, const InstrumentState&);
    static bool instrumentFromVar (int instrument, const juce::var&, InstrumentState&);
};

} // namespace catbox
