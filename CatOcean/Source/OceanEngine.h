#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "OceanSim.h"
#include "OceanArt.h"
#include "OceanMusic.h"
#include "Streamer.h"
#include "YouTubeChat.h"

/**
    Cat Ocean's heart: one thread that, 30 times a second, reads the chat, moves the ocean, makes
    1/30 s of music, paints the picture and hands picture + sound to the stream and the speakers.
*/
namespace ocean
{
constexpr int fps = 30, sampleRate = 48000, samplesPerFrame = sampleRate / fps;

class Engine : private juce::Thread
{
public:
    /** saveFile: where the ocean is kept (loaded now, saved every minute and at the end). */
    explicit Engine (const juce::File& saveFile);
    ~Engine() override;

    void start (bool realTime = true);
    void stop();

    // any thread
    void say (const juce::String& authorId, const juce::String& authorName, const juce::String& text);
    void copyFrame (juce::Image& into) const;
    juce::String stats() const;
    bool goLive (const juce::File& ffmpeg, const juce::String& target);
    void stopLive();
    bool wantsLive() const noexcept { return liveWanted.load(); }

    /** The speakers: reads what the engine made (48 kHz stereo). */
    void readMonitor (float* l, float* r, int n);
    std::atomic<float> monitorGain { 0.8f };

    Streamer streamer;
    YouTubeChat chat;

    // tests: run frames on the calling thread (the engine must not be started)
    void runFrames (int count);
    Sim& simForTests() { return sim; }
    Music& musicForTests() { return music; }
    const Canvas& canvasForTests() const { return canvas; }
    float lastPeak() const noexcept { return peak.load(); }
    void saveNow();
    int paintEvery = 1;   // tests: paint only every n-th frame

private:
    void run() override;
    void frame();

    juce::File file;
    Sim sim;
    Music music;
    Painter painter;
    Canvas canvas;
    std::vector<float> left, right;

    struct ChatLine { juce::String id, name, text; };
    juce::CriticalSection chatLock;
    std::vector<ChatLine> pending;

    mutable juce::SpinLock frameLock;
    std::vector<juce::uint32> shown;

    juce::AbstractFifo fifo { sampleRate };
    std::vector<float> fifoL, fifoR;

    mutable juce::CriticalSection statsLock;
    juce::String statsText;
    std::atomic<float> peak { 0.0f };
    std::atomic<bool> liveWanted { false };
    juce::CriticalSection liveLock;
    juce::int64 frameCount = 0;
    juce::String liveTarget;
    juce::File liveFFmpeg;
    double lastSave = 0, lastRestart = -1e9;
    bool realTime = true;
};
} // namespace ocean
