#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

/**
    Meow-Cat's voice: every note is a cat. No samples - each meow is made from scratch: a voice (a buzzing pulse)
    through four mouth resonances that move from "m" to "i" to "a" to "o" to "u" while the pitch rises and falls.

    SIZE   kitten .. house cat .. lion (how big the mouth is - the resonances move down, a lion gets a growl)
    MOOD   sweet "mew?" .. hungry "meeoow" .. angry, rough "mrrraaooww" with a big wobble
    LENGTH a short mew .. a very long meeeeeooow
    CHOIR  1 .. 6 cats on every note, each a bit different (size, tuning, timing, left / right)
    CHAOS  every meow a bit different, the odd voice crack
    PURR   a purr under every held note (also the mod wheel)
    HISS   hit a key hard and the cat hisses instead (0 = never, 1 = from velocity 83)
    ALLEY  the echo of a back alley at night
    MODE   MEOW (one meow per note) / SING (holds the "aaa" while you hold the key, "ow" when you let go) /
           CHATTER (the "ek-ek-ek" of a cat watching birds, in time) / PURR (only purring)
*/
namespace meow
{
enum class Mode { Meow = 0, Sing, Chatter, Purr };

constexpr int maxNotes = 10, maxCats = 6, formants = 4;

struct Settings
{
    Mode mode = Mode::Meow;
    float size = 0.4f, mood = 0.45f, length = 0.4f, chaos = 0.15f, purr = 0.0f, hiss = 0.3f, alley = 0.25f;
    int choir = 1, octave = 0;
    float outDb = 0.0f;
    double bpm = 120.0;
    bool playing = false;
    double ppq = 0.0;
};

/** A mouth shape: four resonances (Hz), how loud each one is, how wide, how open the mouth looks (0..1). */
struct Vowel
{
    float f[formants], a[formants], bw[formants];
    float loud, open;
};
Vowel mix (const Vowel& x, const Vowel& y, float t) noexcept;

/** How much the resonances are moved for this SIZE (kitten 1.55 .. lion 0.7). */
float formantScale (float size) noexcept;
/** The meow's length in seconds for these settings (before CHAOS). */
float meowSeconds (float length, float mood) noexcept;

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setSettings (const Settings& s) noexcept { settings = s; }

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();
    void sustain (bool down);
    void pitchBend (float minus1to1) noexcept { bend = minus1to1; }
    void modWheel (float v) noexcept { wheel = v; }

    /** Adds the cats into l / r. */
    void render (float* l, float* r, int n);
    /** The back alley (echo + room) and the output level, over the whole block. */
    void finish (float* l, float* r, int n);

    int activeNotes() const noexcept;
    int activeCats() const noexcept;

    // for the screen
    std::atomic<float> mouth { 0.0f }, hissing { 0.0f }, purring { 0.0f }, level { 0.0f };
    std::atomic<int> meows { 0 }, lastNote { 60 };

private:
    struct Svf
    {
        float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
        void set (float hz, float q, float sr) noexcept;
        float band (float x) noexcept;    // unity gain at the centre
        void clear() noexcept { ic1 = ic2 = 0.0f; }
    };

    struct Cat
    {
        bool active = false, hiss = false;
        float delay = 0.0f;               // seconds before it starts
        float t = 0.0f;                   // 0..1 through the meow
        float seconds = 1.0f;             // how long the whole meow takes
        float sizeShift = 0.0f, detune = 0.0f, pan = 0.0f, gain = 1.0f;
        float peak = 2.0f, crackAt = -1.0f;
        float phase = 0.0f, tilt = 0.0f, jitter = 0.0f, amp = 0.0f, fade = 1.0f;
        int counter = 0, period = 0;
        Svf f[formants];
        float fa[formants] {};
        float loud = 0.0f, open = 0.0f;
        uint32_t seed = 1;
    };

    struct Note
    {
        bool active = false, held = false, sustained = false;
        int note = 60;
        float velocity = 0.8f;
        uint32_t age = 0;
        std::array<Cat, maxCats> cats;
        // purr
        float purrPhase = 0.0f, purrBreath = 0.0f, purrBurst = 0.0f, purrLevel = 0.0f, purrLp1 = 0.0f, purrLp2 = 0.0f;
        Svf purrF[2];
        // chatter
        double chatterNext = 0.0;
        int chatterCount = 0;
    };

    void startCats (Note&, bool chatter);
    float renderCat (Cat&, const Note&, float& outL, float& outR) noexcept;
    float renderPurr (Note&, float amount) noexcept;
    float noise (uint32_t& s) noexcept { s = s * 1664525u + 1013904223u; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
    float random01() noexcept { return noise (rng) * 0.5f + 0.5f; }
    bool noteBusy (const Note&) const noexcept;

    Settings settings;
    double sr = 48000.0;
    std::array<Note, maxNotes> notes;
    uint32_t clock = 0, rng = 0x1234567u;
    bool pedal = false;
    float bend = 0.0f, wheel = 0.0f;
    double samplesSeen = 0.0;

    // the alley
    juce::Reverb reverb;
    std::vector<float> echoL, echoR;
    int echoPos = 0;
    juce::SmoothedValue<float> alleySmooth, outSmooth;
    float mouthNow = 0.0f, hissNow = 0.0f, purrNow = 0.0f;
};
} // namespace meow
