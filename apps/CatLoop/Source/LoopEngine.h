#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>
#include "CatSounds.h"
#include "DrumCatEngine.h"
#include "SafetyLimiter.h"

/**
    Cat Loop's engine: a 5-track live looper (like a Boss RC-505), with a tempo, a click and a beat.

    Every track has one big button: EMPTY -> REC -> PLAY -> DUB -> PLAY ... STOP stops it, UNDO takes the last overdub
    back (and REDO brings it again), CLEAR empties it.

    Time: the first loop sets the tempo (the length becomes 1, 2, 4, 8 or 16 bars between 70 and 140 BPM), or the
    tempo is set by hand / the beat / the click. From then on loops start and end on the bar (QUANTIZE = Bar, or Beat,
    or Off) so they stay together. With the click on, an empty transport counts one bar in first.

    The ends of every loop are cross-faded (5 ms), overdubs fade in and out, stops fade out: no clicks. LATENCY moves
    what is recorded back by the sound card's in + out delay, so overdubs land where you heard them.

    Threads: the message thread calls the commands (press / stop / clear / undo ...), they go through a FIFO to the
    audio thread. Undo copies are made on the message thread while the track only plays (the audio thread then only
    reads it).
*/
namespace looper
{
constexpr int numTracks = 5;

enum class State : int { empty = 0, armed, recording, playing, overdubbing, stopped };
juce::String stateName (State);

enum class Quantize : int { bar = 0, beat, off };
enum class Click : int { off = 0, countIn, always };
enum class Input : int { stereo = 0, left, right };

juce::StringArray quantizeNames();
juce::StringArray clickNames();
juce::StringArray inputNames();
juce::StringArray beatNames();
juce::StringArray kitNames();

class Engine
{
public:
    Engine();

    /** maxSeconds: the longest loop per track (memory: 2 layers x 2 channels per track). */
    void prepare (double sampleRate, int maxBlock, double maxSeconds = 60.0);
    double getSampleRate() const noexcept { return sr; }

    /** The audio callback. in may be null (no input). */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);

    //==== commands (message thread) ===========================================================================
    void press (int track);            // the big button
    void stop (int track);
    void clear (int track);
    bool undo (int track);             // the last overdub off (again = redo); false if there is nothing to undo
    void playAll();                    // every stopped loop starts (at the next bar)
    void stopAll();
    void togglePlayAll();              // stop all if anything plays, else play all
    void clearAll();
    void setBeat (bool on);
    bool isBeatOn() const noexcept { return beatWanted.load(); }
    void newBeat();                    // another pattern in the same style
    void setBeatStyle (int style);
    bool setBpm (double bpm);          // only while every track is empty (else false)
    void tapTempo();

    //==== settings (any thread) ===============================================================================
    std::atomic<int> quantize { (int) Quantize::bar }, click { (int) Click::countIn }, input { (int) Input::stereo };
    std::atomic<int> kit { 0 }, beatsPerBar { 4 }, latency { 0 };          // latency in samples
    std::atomic<float> master { 0.8f }, monitor { 0.0f }, clickLevel { 0.5f }, beatLevel { 0.6f };
    std::atomic<bool> recThenDub { false }, safety { true };   // safety: the output limiter (tests switch it off)
    void resetTempo();                 // back to "the first loop sets the tempo" (only while every track is empty)

    //==== what the screen shows ===============================================================================
    State getState (int t) const noexcept       { return (State) tracks[(size_t) t].state.load(); }
    int getLength (int t) const noexcept        { return tracks[(size_t) t].length.load(); }
    float getPosition (int t) const noexcept    { return tracks[(size_t) t].pos.load(); }          // 0..1 (recording: seconds)
    float getLevel (int t) const noexcept       { return tracks[(size_t) t].level.load(); }
    bool canUndo (int t) const noexcept         { return tracks[(size_t) t].hasUndo.load(); }
    bool isUndone (int t) const noexcept        { return tracks[(size_t) t].undone.load(); }
    std::atomic<float>& volume (int t) noexcept { return tracks[(size_t) t].vol; }
    int getBars (int t) const noexcept;         // whole bars, or 0 when free
    bool isTempoKnown() const noexcept          { return tempoKnown.load(); }
    double getBpm() const noexcept              { return bpm.load(); }
    bool isRunning() const noexcept             { return runningView.load(); }
    double getBarPosition() const noexcept      { return barView.load(); }   // bars since the start (negative = count-in)
    float getInputLevel() const noexcept        { return inLevel.load(); }
    float getOutputLevel() const noexcept       { return outLevel.load(); }
    bool anyLoop() const noexcept;
    int lastTouched() const noexcept            { return lastTrack.load(); }

    //==== sessions and export (message thread) =================================================================
    /** The loop of a track (its length), empty buffer if none. */
    juce::AudioBuffer<float> getLoop (int t) const;
    /** A quick picture of a loop for the screen: the loudest sample in each of `bins` slices (a sparse look). */
    void overview (int t, float* peaks, int bins) const;
    /** Put a loop on an empty track (from a saved session); it is stopped and ready to play. */
    bool setLoop (int t, const juce::AudioBuffer<float>&);
    /** Everything mixed: the longest loop once (the shorter ones repeat), with the beat if it is on. */
    juce::AudioBuffer<float> mixdown (bool withBeat) const;
    /** One track for a stem, repeated to the longest loop's length so all stems line up. */
    juce::AudioBuffer<float> stem (int t) const;
    juce::AudioBuffer<float> beatStem() const;
    int getBeatStyle() const noexcept { return beatStyle.load(); }
    juce::int64 getBeatSeed() const noexcept { return beatSeed.load(); }
    void setBeatPattern (int style, juce::int64 seed);
    double getBarLength() const noexcept { return barLen.load(); }     // samples
    /** For a session: the tempo exactly as the loops were made. */
    void setTempoFromSession (double barLengthSamples, int beats);

private:
    struct Track
    {
        std::vector<float> data[2][2];                   // [layer][channel]
        std::atomic<int> active { 0 }, state { 0 }, length { 0 };
        std::atomic<bool> hasUndo { false }, undone { false };
        std::atomic<float> vol { 0.8f }, level { 0.0f }, pos { 0.0f };

        // audio thread only
        juce::int64 startAt = 0;                         // where (transport) the loop's first sample sits
        int recPos = 0;
        juce::int64 actionAt = -1;                       // a waiting change at this transport sample
        int action = 0;                                  // what changes then (see LoopEngine.cpp)
        int tailIdx = 0, tailEnd = 0;                    // after REC: still writing the end + the cross-fade
        bool tail = false;
        float playGain = 0.0f, dubGain = 0.0f, volS = 0.8f;
        int dubHold = 0;                                 // keep writing the overdub a little (latency) after DUB ends
        bool stopping = false;
        float peak = 0.0f;
    };

    enum class Cmd : int { press, stop, clear, playAll, stopAll, clearAll };
    struct Command { Cmd cmd; int track; };
    void push (Cmd, int track = 0);
    void apply (const Command&);
    void doPress (int t);
    void doStop (int t);
    void doClear (int t);
    void startRecording (Track&, juce::int64 at);
    void closeLoop (Track&, int len);
    void requestClose (Track&);
    void startPlaying (Track&);
    void ensureRunning (bool countIn);
    void deriveTempo (int len);
    juce::int64 nextBoundary (juce::int64 from, bool barOnly) const;
    double unitLength (bool barOnly) const;
    void renderBeatAndClick (float* l, float* r, int n);
    void copyToUndo (int t);
    void tailSample (Track&, float l, float r);
    void loadPendingPattern();

    std::array<Track, numTracks> tracks;
    double sr = 48000.0;
    int capacity = 0, fade = 240;

    juce::AbstractFifo fifo { 256 };
    std::array<Command, 256> commands {};

    // time
    std::atomic<double> bpm { 120.0 }, barLen { 96000.0 };   // barLen in samples (exact, also when the first loop set it)
    std::atomic<bool> tempoKnown { false }, tempoManual { false }, runningView { false };
    std::atomic<double> barView { 0.0 };
    juce::int64 transport = 0;
    bool running = false;
    std::atomic<int> lastTrack { 0 };
    juce::Array<double> taps;

    // beat
    std::atomic<bool> beatWanted { false };
    bool beatPlaying = false;
    juce::int64 beatStart = 0, beatChangeAt = -1;
    std::atomic<int> beatStyle { 0 };
    std::atomic<juce::int64> beatSeed { 1 };
    mutable juce::SpinLock patternLock;
    drumcat::Pattern pattern, pendingPattern;
    bool patternPending = false;
    hub::DrumKit drums;
    int drumKitNow = -1;
    std::vector<float> scratchL, scratchR;

    // click
    float clickEnv = 0.0f, clickPhase = 0.0f, clickHz = 1000.0f;

    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };
    cat::SafetyLimiter limiter;
};
} // namespace looper
