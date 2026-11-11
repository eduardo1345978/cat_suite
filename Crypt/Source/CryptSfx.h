#pragma once

#include "CryptDsp.h"
#include <juce_audio_formats/juce_audio_formats.h>

/**
    Crypt's sound-effect maker (like sfxr, but dark): pick what it is (a sword, a spell, a door ...), press
    GENERATE and get a new one every time; MUTATE changes it a little; save it as a .wav for the game.
    Everything about one effect is in Sfx (and its seed), so it can be saved, recalled and made again exactly.
*/
namespace tomb
{
enum class Kind { Sword = 0, Hit, Spell, Fire, Door, Trap, Death, Chest, Step, Pickup, Growl, Ghost, LevelUp, Bell, count };
juce::StringArray kindNames();
juce::String kindFileName (Kind);       // "sword", "level_up" ...

struct Sfx
{
    Kind kind = Kind::Sword;
    Wave wave = Wave::Noise;
    float freq = 440.0f;          // Hz at the start
    float slide = 0.0f;           // octaves per second
    float accel = 0.0f;           // how the slide changes (octaves / s^2)
    float vibDepth = 0.0f, vibRate = 6.0f;
    float duty = 0.5f, dutySweep = 0.0f;
    float attack = 0.0f, sustain = 0.1f, punch = 0.0f, decay = 0.3f;   // seconds (punch 0..1)
    float noise = 0.0f;           // noise mixed in, 0..1
    float lowpass = 1.0f, lowpassSweep = 0.0f, highpass = 0.0f;       // 0..1 (lowpass 1 = open)
    int arpPattern = 0;           // 0 none, 1 up a fifth, 2 a minor run up, 3 a run down, 4 up an octave
    float arpSpeed = 0.1f;        // seconds per step
    float repeat = 0.0f;          // the pitch starts again this many times a second (0 = never)
    float bits = 16.0f;
    float echo = 0.0f, room = 0.0f;
    juce::uint32 seed = 1;
    int number = 1;               // for the file name: sword_01, sword_02 ...

    juce::var toVar() const;
    static Sfx fromVar (const juce::var&);
};

/** A new effect of this kind. darkness 0..1: lower, darker, more room. */
Sfx generate (Kind, juce::uint32 seed, float darkness);
/** The same effect, a little different. */
Sfx mutate (const Sfx&, juce::uint32 seed);
/** Makes the sound (mono). pitch: semitones up / down; length: x0.5 .. x2. */
juce::AudioBuffer<float> render (const Sfx&, double sampleRate, float pitch = 0.0f, float length = 1.0f);
/** Saves a mono 16-bit .wav (what game engines like). */
bool saveWav (const juce::AudioBuffer<float>&, double sampleRate, const juce::File&);
} // namespace tomb
