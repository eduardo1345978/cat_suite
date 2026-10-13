#pragma once

#include <vector>
#include "LifeWorld.h"

/**
    Life-Cat conductor: decides when each creature sings and which note.

    Rhythm   each creature's Rhythm gene picks how often it may sing (2 bars .. 1/16); its Density
             gene and energy decide whether it actually does. Quantized to the host grid (with swing)
             or free-floating on its own clock.
    Pitch    Position (height in the world), Species (chord tones per role) or Held Chord (the notes
             you hold / Chord-Cat sends) - always inside the chosen key and scale.
*/
namespace lifecat
{
juce::StringArray scaleNames();
juce::StringArray pitchModeNames();
juce::StringArray gridNames();
double gridBeats (int index);

struct MusicSettings
{
    int pitchMode = 0;          // 0 position, 1 species, 2 held chord
    int key = 0, scale = 0;
    int lowNote = 48, highNote = 84;
    bool quantize = true;
    int grid = 2;               // index into gridNames()
    float swing = 0.0f;         // 0..100 %
    float density = 0.6f;       // 0..1
    int maxNotes = 4;           // per grid step
    float lengthScale = 1.0f;
};

struct SungNote
{
    int sample = 0;
    int creature = -1;          // index in World::creatures
    float midiNote = 60.0f;
    float velocity = 0.7f;
    float lengthBeats = 1.0f;
    float pan = 0.0f;
};

class Conductor
{
public:
    void seed (uint32_t s) { rng.seed (s * 7919u + 3u); }

    /** Notes to sing between ppqStart and ppqStart + ppqPerSample * numSamples. */
    void process (World&, const MusicSettings&, double ppqStart, double ppqPerSample, int numSamples,
                  const int* heldNotes, int numHeld, std::vector<SungNote>& out);

    float pitchFor (const Creature&, const MusicSettings&, const int* heldNotes, int numHeld) const;
    static double divisionFor (const Creature&);

private:
    Rng rng;
    std::vector<int> candidates;
};

} // namespace lifecat
