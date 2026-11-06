#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Echo-Cat's sound: a tape echo (tempo-synced or free), ping-pong between the speakers, a darker /
    thinner / wobblier / warmer sound each time round, ducking while you sing or play, freeze, and a
    shimmer space (a reverb whose tail is fed back an octave higher).

        in -> [echo line L/R: time, feedback, ping-pong, tone, low cut, wobble, drive]
           -> [duck] -> mix
        in + echoes -> [space: reverb + shimmer (octave up, fed back)] -> mix

    Echo modes: Normal; Reverse (each echo plays backwards: the last echo-time of sound, read from its
    end to its start); Steps (8 echoes in a row, each with its own level: a rhythm of echoes, the pattern
    repeating with the feedback). Pitch: every echo is that many semitones higher / lower than the last.
*/
namespace echocat
{
/** Note lengths for the synced echo time (in beats). */
juce::StringArray divisionNames();
double divisionBeats (int index);
constexpr int numDivisions = 11;
enum Mode { ModeNormal = 0, ModeReverse, ModeSteps, ModeResonator };
constexpr int numSteps = 8;
constexpr int maxStrings = 8;
constexpr int chordFollow = 9;     // "Chord-Cat's chord"
juce::StringArray modeNames();
juce::StringArray chordNames();
/** A resonator chord shape (semitones above the root note); returns how many. */
int chordShape (int index, std::array<int, maxStrings>& out);

struct Settings
{
    bool sync = true;
    int division = 5;              // 1/8 dotted
    float timeMs = 350.0f;         // when not synced
    double bpm = 120.0;
    float feedback = 0.4f;         // 0 .. 1 (shown as %, kept below runaway)
    float mix = 0.35f, dry = 1.0f;
    float pingpong = 0.0f, tone = 0.7f, lowcut = 0.15f, wobble = 0.0f, drive = 0.0f;
    float space = 0.0f, shimmer = 0.0f, duck = 0.0f;
    bool freeze = false;
    float outputDb = 0.0f;
    int mode = ModeNormal;
    float pitch = 0.0f;            // semitones per echo (-12 .. +12)
    std::array<float, numSteps> steps { 1.0f, 0.0f, 0.8f, 0.0f, 0.6f, 0.6f, 0.0f, 0.4f };
    // Resonator: the strings' pitches (MIDI semitones, may be fractional: Micro-Cat's tuning)
    std::array<float, maxStrings> stringPitch {};
    int numStrings = 0;

    double delaySeconds() const;
};

/** A little granular pitch shifter (two overlapping grains read faster or slower). */
class PitchShifter
{
public:
    void prepare (double sampleRate);
    void reset();
    /** 2 = an octave up, 0.5 = an octave down. */
    void setRatio (float r) noexcept { ratio = juce::jlimit (0.25f, 4.0f, r); }
    float process (float x) noexcept;
private:
    std::vector<float> buf;
    int w = 0, grain = 2048;
    float phase = 0.0f, ratio = 2.0f;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const Settings&);

    // for the screen (any thread)
    std::atomic<float> inLevel { 0.0f }, echoLevel { 0.0f }, spaceLevel { 0.0f }, duckAmount { 0.0f };
    std::atomic<double> shownDelay { 0.3 };

private:
    double sr = 48000.0;
    std::vector<float> lineL, lineR;
    int w = 0;
    float delaySmooth = -1.0f;
    float lpL = 0, lpR = 0, hpL = 0, hpR = 0;
    double wobPhase1 = 0.0, wobPhase2 = 0.0;
    float env = 0.0f, freezeMix = 0.0f;
    juce::Reverb reverb;
    std::array<PitchShifter, 2> shifters, loopShifters;
    double revPhase = 0.0;
    struct ResString { std::vector<float> buf; int w = 0; float lp = 0.0f, delay = -1.0f, targetDelay = 100.0f; };
    std::array<ResString, maxStrings> strings;
    float fbLpL = 0, fbLpR = 0, fbHpL = 0, fbHpR = 0;      // the feedback tap's colour (Steps mode)
    std::vector<float> revL, revR, shimL, shimR;
    float shimLp = 0.0f, shimLpR = 0.0f;
    float outGain = 1.0f;
};
} // namespace echocat
