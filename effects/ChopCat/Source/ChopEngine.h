#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

/**
    Chop-Cat's engine: it glitches drums (or anything) in time.

    It keeps recording the last 4 seconds into a ring buffer. A GLITCH stops passing the input through and plays
    a piece of that memory instead: the same bit again and again (REPEAT 1/8 .. 1/64), a roll that speeds up (ROLL),
    the last bit backwards (REVERSE), slowing to a stop (TAPE STOP), an octave up / down (PITCH), an earlier slice of
    the bar (SCATTER - the beat gets shuffled), a frozen smear of grains (FREEZE), or silence (MUTE).

    Three things start glitches, the strongest wins:
      LIVE  - a pad held with the mouse, or a MIDI key (C = pad 1 .. B = pad 12, any octave)
      SEQ   - the 16-step sequencer (one glitch per 1/16 step; the same glitch on neighbouring steps runs on)
      CHAOS - glitches at random, in time; FILL puts more of them at the end of the bar, LOCK repeats the same
              random glitches every 1, 2 or 4 bars (so it grooves instead of being random noise)
*/
namespace chop
{
enum Fx { None = 0, Rpt8, Rpt16, Rpt32, Rpt64, Roll, Reverse, TapeStop, PitchUp, PitchDown, Scatter, Freeze, Mute, numFx };
enum Source { SrcNone = 0, SrcSeq, SrcChaos, SrcLive };

juce::StringArray fxNames();       // "Off", "Repeat 1/8", ...
juce::StringArray fxShort();       // "", "1/8", ... "ROLL", "REV" ...
juce::StringArray lockNames();
juce::StringArray quantizeNames();

constexpr int numSteps = 16;
constexpr int numPads = 12;

struct Settings
{
    float mix = 1.0f, chaos = 0.0f, fill = 0.5f, smooth = 0.3f, outDb = 0.0f;
    int lock = 2;             // 0 free, 1 = 1 bar, 2 = 2 bars, 3 = 4 bars
    int quantize = 1;         // live pads: 0 off, 1 = 1/16, 2 = 1/8
    int seed = 1;
    bool seqOn = true;
    std::array<int, numSteps> steps {};
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
};

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int blockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer&, const Settings&);

    /** the pads held with the mouse (the screen writes, the audio reads); pad = Fx - 1 */
    std::array<std::atomic<bool>, numPads> padMouse;

    /** for the screen */
    std::atomic<int> nowFx { 0 }, nowSource { 0 }, step { -1 };
    std::array<std::atomic<int>, numSteps> barFx;          // what played on each step of this bar: fx | source << 8
    std::array<std::atomic<bool>, numPads> padLit;          // held by mouse or MIDI
    static constexpr int peakCount = 512;
    std::array<std::atomic<float>, peakCount> peaks;        // the output, one peak per 128 samples
    std::array<std::atomic<int>, peakCount> peakFx;         // and what glitch it was
    std::atomic<int> peakPos { 0 };
    std::atomic<float> level { 0.0f };

    /** CHAOS's decision for one step (for tests): fx and length in steps, or None */
    std::pair<int, int> chaosAt (const Settings&, long long bar, int stepInBar) const;
    int samplesPerStep (double bpm) const { return juce::jmax (16, (int) (sr * 60.0 / bpm / 4.0)); }

private:
    struct Event
    {
        int fx = None, source = SrcNone;
        int startW = 0;                 // where the buffer was when it started
        long long t = 0;                // samples since it started
        int length = 0;                 // planned length (samples), 0 = until released
        double pos = 0.0;               // tape stop's read position
        int seg = 0, segLen = 0, segPos = 0;   // roll
        int scatterBack = 1;            // scatter: how many steps back
        bool active () const { return fx != None; }
    };
    void startEvent (Event&, int fx, int source, int lengthSamples, const Settings&, long long salt);
    void readEvent (Event&, float& l, float& r, int stepLen, float fade);
    float rd (const std::vector<float>& b, double pos) const;

    double sr = 48000.0;
    std::vector<float> bufL, bufR;
    int size = 0, w = 0, filled = 0;
    Event seqEv, chaosEv, liveEv, lastEv;
    int livePad = -1, pendingPad = -2;
    std::array<bool, numPads> midiHeld {}, prevMouse {};
    std::array<long long, numPads> pressOrder {};
    long long pressCounter = 0;
    double beats = 0.0; long long stepIdx = -1; bool clockReset = true;
    int seqRunLeft = 0, chaosLeft = 0;
    float wet = 0.0f, crushHold[2] {}, crushCount = 0.0f;
    float peakAcc = 0.0f; int peakN = 0;
    juce::SmoothedValue<float> outGain;
};
} // namespace chop
