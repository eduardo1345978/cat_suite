#pragma once

#include "Composer.h"
#include "CatSounds.h"
#include "SafetyLimiter.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace cat::muse
{
/** Which built-in sound plays each part (indexes into the CatSounds instruments). */
struct SoundSet
{
    int chords = 0;   // KeysSynth: 0 Soft Pad, 1 E-Piano, 2 Pluck, 3 Strings
    int bass = 0;     // BassSynth: 0 Sub, 1 Saw, 2 Reese, 3 Pluck
    int drums = 0;    // DrumKit: 0 Boom Bap, 1 808, 2 Lo-fi, 3 Live
    int melody = 2;   // KeysSynth
    int counter = 2;  // KeysSynth
};
SoundSet defaultSounds (int genre);

/**
    Plays a song with the built-in Cat sounds: a little mixer with a level per part, a shared
    room reverb and a safety limiter. Used for Muse-Cat's own sound, the Hub's WAV export and the tests.
*/
class SongRenderer
{
public:
    void prepare (double sampleRate, int maxBlock);
    void setSounds (const SoundSet&);
    void setLevels (const std::array<float, NumParts>& l) { levels = l; }
    std::array<float, NumParts> levels { 0.45f, 0.75f, 0.4f, 1.4f, 0.7f };
    std::array<bool, NumParts> mute {};
    float reverbSend = 0.18f;

    /** Feeds one part's MIDI (audio thread). */
    void handle (int part, const juce::MidiBuffer&);
    /** Renders all parts (adds nothing: overwrites l/r). */
    void render (float* l, float* r, int n);
    void allOff();
    float partMeter (int part) const noexcept { return meters[(size_t) juce::jlimit (0, NumParts - 1, part)]; }

    /** Writes the whole song (stereo 24-bit WAV), or one part only (stem) when onlyPart >= 0. */
    static bool renderToWav (const Song&, const SoundSet&, const juce::File&, double sampleRate = 44100.0,
                             int onlyPart = -1, std::function<bool (float)> progress = {});

private:
    hub::KeysSynth chords, melody, counter;
    hub::BassSynth bass;
    hub::DrumKit drums;
    juce::Reverb reverb;
    cat::SafetyLimiter limiter;
    juce::AudioBuffer<float> work, send;
    std::array<float, NumParts> meters {};
    std::array<juce::MidiBuffer, NumParts> pending;
    double sr = 44100.0;
    SoundSet sounds;
};
} // namespace cat::muse
