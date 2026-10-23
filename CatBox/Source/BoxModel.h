#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cstdint>
#include "Engines.h"

/**
    Cat Box: the data model shared by the engine, the controller logic and the screen.

    Three instruments play together: Drums (a step sequencer, 16 drums x up to 64 steps),
    Arp (an arpeggiator with a 16-step rhythm) and Bass (a 303-style step sequencer).
    Nothing is generated: everything is played in on the pads or set step by step.

    The 12 knobs edit the instrument on screen; with Shift held they edit a second layer,
    so every instrument has 24 parameter slots (0..11 normal, 12..23 with Shift). A few slots
    are global (tempo, key, scale, master) and are the same on every page.
*/
namespace catbox
{
enum Instrument { Drums = 0, Arp, Bass, NumInstruments };

/** The FX page (master effects, performance pads, scenes) comes after the three instruments. */
constexpr int PageFx = 3, NumPages = 4;

enum Button { BtnPlay = 0, BtnSteps, BtnShift, BtnScene, BtnInstrument, BtnOctDown, BtnOctUp, BtnRec, BtnChaos, BtnSound, NumButtons };

constexpr int numKnobs = 12;
constexpr int numPads = 16;
constexpr int numSlots = 24;
constexpr int numPresetSlots = 16;
constexpr int maxSteps = 64;

enum Global { GTempo = 0, GKey, GScale, GMaster, NumGlobals };

/** Slot numbers (knob index, +12 with Shift). Slots 0, 12, 13 and 23 are global on every page. */
namespace slot
{
    constexpr int Tempo = 0, Key = 12, Scale = 13, Master = 23;

    // Drums (Level and Chance belong to the selected drum)
    constexpr int DKit = 1, DSwing = 2, DLength = 3, DHuman = 4, DLevel = 5, DChance = 6,
                  DCrush = 7, DDrive = 8, DFilter = 9, DEcho = 10, DVolume = 11,
                  DChorus = 14;
    // Arp
    constexpr int ADivision = 1, AMode = 2, AOctaves = 3, AGate = 4, ASwing = 5, ALength = 6, ARatchet = 7,
                  AProbability = 8, AAccent = 9, AFilter = 10, AVolume = 11,
                  ASound = 14, AEcho = 15, AChorus = 16, ADrive = 17, AArpOn = 18, AKeys = 19;
    // Bass
    constexpr int BSound = 1, BCutoff = 2, BResonance = 3, BEnvMod = 4, BDecay = 5, BLength = 6, BAccent = 7,
                  BDrive = 8, BSlide = 9, BOctave = 10, BVolume = 11,
                  BEcho = 14, BChorus = 15, BWave = 16, BSwing = 17, BFollow = 18, BKeys = 19;
    // FX page
    constexpr int FReverb = 1, FSize = 2, FDelay = 3, FPump = 4, FDjFilter = 5, FTape = 6, FVinyl = 7,
                  FCrush = 8, FGlue = 9, FWidth = 10, FPumpTarget = 11,
                  FSendDrums = 14, FSendArp = 15, FSendBass = 16;
}

enum class Kind { None, Percent, Choice, Integer, Bpm };

struct ParamDesc
{
    juce::String name;          // shown on the screen
    Kind kind = Kind::None;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    juce::StringArray choices;  // Kind::Choice
    int global = -1;            // Global index, or -1 = belongs to the instrument
    bool perDrum = false;       // Drums: stored per drum (the selected one)

    bool used() const noexcept { return kind != Kind::None; }
    /** How much one click of a knob changes the value. */
    float step() const noexcept { return kind == Kind::Percent ? 0.01f : 1.0f; }
    juce::String format (float value) const;
};

const ParamDesc& paramDesc (int instrument, int slotIndex);
juce::String instrumentName (int page);   // Drums, Arp, Bass, FX
juce::String instrumentFolder (int page); // file-system friendly (FX presets live in "FX")

//==============================================================================
// Music helpers
juce::StringArray keyNames();
juce::StringArray scaleNames();
const std::vector<int>& scaleIntervals (int scale);
/** MIDI note of a scale degree (degree 0 = the key's root at baseNote's octave; degrees may go past one octave). */
int scaleNote (int key, int scale, int degree, int baseNote);
juce::String noteName (int midiNote);

/** Arp: note length of one step in quarter notes (1/4, 1/8, 1/8 T ...). */
double arpDivisionPpq (int index);
enum ArpMode { ModeUp = 0, ModeDown, ModeUpDown, ModeDownUp, ModeAsPlayed, ModeRandom, ModeConverge, ModeChord, NumArpModes };
/** Drum pattern length choice -> steps (16, 32, 64). */
int drumStepsFromChoice (float choice);

/** Pads laid out like a piano (two rows of 8): the bottom row is the white keys C D E F G A B C,
    the top row the black keys right above them (C# D# - F# G# A# - C#). -1 = a pad with no key. */
int pianoPadNote (int pad, int lowC);
/** Arp / Bass pad layouts: 0 = Piano, 1 = Scale (the notes of the key, in order). */
enum PadLayout { LayoutPiano = 0, LayoutScale };

/** The drum sound each pad plays (General MIDI notes, read by hub::DrumKit). */
int drumPadNote (int pad);
juce::String drumPadName (int pad);

/** Bass sounds: 0 = Acid (Cat Box's own 303-style synth), 1.. = hub::BassSynth sounds + 1. */
juce::StringArray bassSoundNames();
/** Arp sounds: the hub::KeysSynth sounds, then Supersaw and Acid (Cat Box's own). */
juce::StringArray arpSoundNames();
constexpr int arpSoundSupersaw = 8, arpSoundAcid = 9, arpSoundGlide = 10;
/** The Glide engine's index in bassSoundNames() (the last one). */
int bassSoundGlide();

//==============================================================================
/** Arp pattern step states. */
enum StepState : uint8_t { StepOn = 0, StepRest, StepTie, StepAccent, NumStepStates };

/** One step of the bass line. */
struct BassStep
{
    int note = -1;          // MIDI note, -1 = rest
    bool slide = false;     // glide into the next step (the note keeps sounding)
    bool accent = false;    // louder, brighter, snappier
    bool operator== (const BassStep& o) const { return note == o.note && slide == o.slide && accent == o.accent; }
};

/** One instrument's sound + pattern: what a preset holds. */
struct InstrumentState
{
    std::array<float, numSlots> values {};                                   // instrument slots (global slots unused)
    std::array<uint8_t, 16> arpSteps {};                                     // Arp
    std::array<std::array<uint8_t, maxSteps>, numPads> drumSteps {};         // Drums: velocity per step, 0 = off
    std::array<float, numPads> drumLevel {}, drumChance {};                  // Drums: per drum
    std::array<BassStep, maxSteps> bassSteps {};                             // Bass
    float tempo = 110.0f;                                                    // saved with presets, applied only while stopped
    EngineState glide;                                                       // Arp / Bass: the Glide engine (when the Sound is Glide)
};

/** Everything the box remembers between power-ons. */
struct BoxData
{
    std::array<float, NumGlobals> globals {};
    std::array<InstrumentState, NumInstruments> inst;
    std::array<float, numSlots> fx {};              // the FX page knobs
    std::array<bool, NumInstruments> muted {};
    std::array<int, NumInstruments> padOctave {};   // Arp / Bass: octave shift of the pads
    std::array<int, NumPages> loadedPreset { -1, -1, -1, -1 };
    int loadedScene = -1;
    int page = Drums;
    int selectedDrum = 0;
    bool lightScreen = false;

    BoxData();   // defaults

    float get (int instrument, int slotIndex) const;
    void set (int instrument, int slotIndex, float value);   // clamps, rounds choices

    int drumLength() const { return drumStepsFromChoice (inst[Drums].values[slot::DLength]); }
    int bassLength() const { return juce::jlimit (1, maxSteps, juce::roundToInt (inst[Bass].values[slot::BLength])); }

    juce::var toVar() const;
    void fromVar (const juce::var&);

    static juce::var instrumentToVar (int instrument, const InstrumentState&);
    static bool instrumentFromVar (int instrument, const juce::var&, InstrumentState&);
    static InstrumentState defaults (int instrument);
    static std::array<float, numSlots> fxDefaults();
    static juce::var fxToVar (const std::array<float, numSlots>&);
    static bool fxFromVar (const juce::var&, std::array<float, numSlots>&);

    /** A scene: all three instruments, the FX page, tempo, key and scale. */
    juce::var sceneToVar() const;
    bool sceneFromVar (const juce::var&);
};

} // namespace catbox
