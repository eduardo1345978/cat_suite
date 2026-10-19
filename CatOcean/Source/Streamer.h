#pragma once

#include <juce_core/juce_core.h>
#include <deque>
#include <vector>

/**
    Sends the ocean to YouTube (or to a video file): starts ffmpeg, which connects to two local
    sockets, one for the pixels (480 x 270, scaled up 4x by ffmpeg) and one for the sound.
    Video and sound each have their own queue and sending thread, so neither can block the other
    while ffmpeg starts up; a frame and its sound are always queued (or dropped) together.
*/
namespace ocean
{
class Streamer
{
public:
    Streamer();
    ~Streamer();

    /** target: an rtmp(s):// address (YouTube) or a file path (.mp4 / .flv) for a test video. */
    bool start (const juce::File& ffmpeg, const juce::String& target, int width, int height, int fps, int sampleRate);
    void stop();
    bool isRunning() const;
    bool hasFailed() const noexcept { return failed.load(); }
    juce::String getStatus() const;
    juce::String getLog() const;

    /** One frame (BGRA pixels) and its sound (interleaved later to 16-bit). Any thread, never blocks. */
    void push (const juce::uint32* pixels, const float* left, const float* right, int numSamples);
    int droppedFrames() const noexcept { return dropped.load(); }
    juce::int64 sentFrames() const noexcept { return sent.load(); }

    /** Where ffmpeg is: the saved path, the app folder, or the PATH. */
    static juce::File findFFmpeg (const juce::String& savedPath);
    static juce::File defaultFFmpegFolder();
    static juce::StringArray buildArgs (const juce::File& ffmpeg, const juce::String& target, int w, int h, int fps, int sr, int videoPort, int audioPort);

private:
    struct Sender;
    class Watcher;
    void setStatus (const juce::String&);

    std::unique_ptr<Sender> video, audio;
    std::unique_ptr<Watcher> watcher;
    std::unique_ptr<juce::ChildProcess> process;
    mutable juce::CriticalSection lock;
    juce::CriticalSection pushLock;   // push() vs start() / stop()
    juce::String status { "not streaming" }, log;
    std::atomic<bool> failed { false }, running { false };
    std::atomic<int> dropped { 0 };
    std::atomic<juce::int64> sent { 0 };
    int frameBytes = 0;
    friend class Watcher;
};
} // namespace ocean
