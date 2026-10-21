#include "Streamer.h"

namespace ocean
{
//==============================================================================
struct Streamer::Sender : public juce::Thread
{
    Sender (const juce::String& name, int maxQueued) : juce::Thread (name), maxItems (maxQueued) {}
    ~Sender() override { finish(); }

    bool listen (int& port)
    {
        juce::Random r;
        for (int tries = 0; tries < 50; ++tries)
        {
            port = 40000 + r.nextInt (20000);
            if (listener.createListener (port, "127.0.0.1")) return true;
        }
        return false;
    }

    bool hasRoom() const { const juce::ScopedLock sl (lock); return (int) queue.size() < maxItems; }
    void add (juce::MemoryBlock&& b)
    {
        { const juce::ScopedLock sl (lock); queue.push_back (std::move (b)); }
        ready.signal();
    }

    void run() override
    {
        std::unique_ptr<juce::StreamingSocket> client (listener.waitForNextConnection());
        if (client == nullptr || threadShouldExit()) return;
        {
            const juce::ScopedLock sl (lock);
            socket = client.get();
        }
        connected = true;
        while (! threadShouldExit())
        {
            juce::MemoryBlock block;
            {
                const juce::ScopedLock sl (lock);
                if (! queue.empty()) { block = std::move (queue.front()); queue.pop_front(); }
            }
            if (block.isEmpty()) { ready.wait (50); continue; }
            auto* data = static_cast<const char*> (block.getData());
            size_t left = block.getSize();
            while (left > 0 && ! threadShouldExit())
            {
                const int n = client->write (data, (int) juce::jmin<size_t> (left, 65536));
                if (n <= 0) { broken = true; break; }
                data += n; left -= (size_t) n;
            }
            if (broken) break;
        }
        {
            const juce::ScopedLock sl (lock);
            socket = nullptr;
        }
        client->close();
    }

    void finish()
    {
        signalThreadShouldExit();
        listener.close();
        {
            const juce::ScopedLock sl (lock);
            if (socket != nullptr) socket->close();
        }
        ready.signal();
        stopThread (5000);
    }

    /** Lets everything queued go out, then hangs up (so a video file gets finished properly). */
    void drainAndFinish (int timeoutMs)
    {
        const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (connected && ! broken && juce::Time::getMillisecondCounter() < until)
        {
            { const juce::ScopedLock sl (lock); if (queue.empty()) break; }
            juce::Thread::sleep (20);
        }
        finish();
    }

    juce::StreamingSocket listener;
    juce::StreamingSocket* socket = nullptr;
    juce::CriticalSection lock;
    std::deque<juce::MemoryBlock> queue;
    juce::WaitableEvent ready;
    const int maxItems;
    std::atomic<bool> connected { false }, broken { false };
};

//==============================================================================
class Streamer::Watcher : public juce::Thread
{
public:
    explicit Watcher (Streamer& s) : juce::Thread ("ffmpeg output"), owner (s) {}
    void run() override
    {
        char buffer[512];
        while (! threadShouldExit())
        {
            auto* p = owner.process.get();
            if (p == nullptr) return;
            const int n = p->readProcessOutput (buffer, sizeof (buffer));
            if (n > 0)
            {
                const juce::String text (juce::CharPointer_UTF8 (buffer), (size_t) n);
                pending << text;
                // ffmpeg's progress report (-progress pipe:1): key=value lines every couple of seconds
                juce::StringArray lines;
                lines.addLines (pending);
                pending = pending.endsWithChar ('\n') ? juce::String() : lines[lines.size() - 1];
                if (pending.isNotEmpty()) lines.remove (lines.size() - 1);
                const juce::ScopedLock sl (owner.lock);
                for (auto& l : lines)
                {
                    if (l.startsWith ("total_size=")) { owner.outBytes = l.fromFirstOccurrenceOf ("=", false, false).getLargeIntValue(); continue; }
                    if (l.startsWith ("speed="))
                    {
                        const auto v = l.fromFirstOccurrenceOf ("=", false, false).trim().upToFirstOccurrenceOf ("x", false, false).getFloatValue();
                        if (v > 0.0f)
                        {
                            owner.speed = v;
                            owner.slowReports = v < 0.93f ? owner.slowReports.load() + 1 : 0;
                        }
                        continue;
                    }
                    if (l.containsChar ('=') && ! l.containsChar (' ')) continue;   // other progress keys
                    if (l.trim().isEmpty()) continue;
                    owner.log << l << "\n";
                }
                if (owner.log.length() > 6000) owner.log = owner.log.getLastCharacters (4000);
                updateStatus();
                continue;
            }
            if (! p->isRunning())
            {
                if (owner.running.load())
                {
                    owner.failed = true;
                    juce::String last;
                    {
                        const juce::ScopedLock sl (owner.lock);
                        juce::StringArray lines;
                        lines.addLines (owner.log);
                        lines.removeEmptyStrings();
                        last = lines.isEmpty() ? juce::String ("no message") : lines[lines.size() - 1];
                    }
                    owner.setStatus ("ffmpeg stopped: " + last.substring (0, 160));
                }
                return;
            }
            updateStatus();
            juce::Thread::sleep (200);
        }
    }
    void updateStatus()
    {
        if (owner.failed.load() || ! owner.running.load()) return;
        const auto bytes = owner.outBytes.load();
        const bool live = owner.target.startsWithIgnoreCase ("rtmp");
        const juce::String secs = juce::String (owner.sent.load() / 30) + " s";
        if (bytes > 0)
        {
            juce::String s ((live ? "LIVE - sending to YouTube (" : "recording (") + juce::String ((double) bytes / 1048576.0, 1) + " MB, " + secs + ")");
            // ffmpeg can't keep up for a while: the smooth look costs the most, so suggest the sharp one
            if (owner.slowReports.load() >= 3)
                s << " - your PC is too slow" << (owner.smooth.load() ? ": pick Look: Pixel" : "") << " (speed " << juce::String (owner.speed.load(), 2) << "x)";
            owner.setStatus (s);
        }
        else if (owner.sent.load() > 0)
            owner.setStatus (live ? "ffmpeg is running, connecting to YouTube ..." : "recording ...");
    }
private:
    Streamer& owner;
    juce::String pending;
};

//==============================================================================
Streamer::Streamer() = default;
Streamer::~Streamer() { stop(); }

void Streamer::setStatus (const juce::String& s) { const juce::ScopedLock sl (lock); status = s; }
juce::String Streamer::getStatus() const { const juce::ScopedLock sl (lock); return status; }
juce::String Streamer::getLog() const { const juce::ScopedLock sl (lock); return log; }
bool Streamer::isRunning() const { return running.load(); }

juce::File Streamer::defaultFFmpegFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Cat Ocean").getChildFile ("ffmpeg");
}

juce::File Streamer::findFFmpeg (const juce::String& savedPath)
{
#if JUCE_WINDOWS
    const juce::String exe = "ffmpeg.exe";
    const char sep = ';';
#else
    const juce::String exe = "ffmpeg";
    const char sep = ':';
#endif
    if (savedPath.isNotEmpty() && juce::File::isAbsolutePath (savedPath) && juce::File (savedPath).existsAsFile()) return juce::File (savedPath);
    const auto local = defaultFFmpegFolder().getChildFile (exe);
    if (local.existsAsFile()) return local;
    const auto besideApp = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile (exe);
    if (besideApp.existsAsFile()) return besideApp;
    juce::StringArray dirs;
    dirs.addTokens (juce::SystemStats::getEnvironmentVariable ("PATH", {}), juce::String::charToString (sep), "");
    for (auto& d : dirs)
        if (d.isNotEmpty() && juce::File::isAbsolutePath (d))
            if (auto f = juce::File (d).getChildFile (exe); f.existsAsFile()) return f;
    return {};
}

juce::StringArray Streamer::buildArgs (const juce::File& ffmpeg, const juce::String& target, int w, int h, int fps, int sr, int vp, int ap, bool smoothLook)
{
    juce::StringArray a;
    a.add (ffmpeg.getFullPathName());
    // -progress pipe:1: ffmpeg reports what it has sent every 2 s (so the app can show it's really live)
    a.addArray ({ "-hide_banner", "-loglevel", "warning", "-nostats", "-nostdin", "-progress", "pipe:1", "-stats_period", "2" });
    // (no -thread_queue_size on the inputs: new ffmpeg versions only accept it on outputs and refuse to start)
    a.addArray ({ "-f", "rawvideo", "-pixel_format", "bgra",
                  "-video_size", juce::String (w) + "x" + juce::String (h), "-framerate", juce::String (fps),
                  "-i", "tcp://127.0.0.1:" + juce::String (vp) });
    a.addArray ({ "-f", "s16le", "-ar", juce::String (sr), "-ac", "2",
                  "-i", "tcp://127.0.0.1:" + juce::String (ap) });
    a.addArray ({ "-map", "0:v", "-map", "1:a",
                  // smooth: xBR rounds the pixel edges (4x: 480 x 270 -> 1920 x 1080); else plain 4 x 4 blocks
                  "-vf", smoothLook ? juce::String ("xbr=n=4,format=yuv420p")
                                    : "scale=" + juce::String (w * 4) + ":" + juce::String (h * 4) + ":flags=neighbor,format=yuv420p",
                  "-c:v", "libx264", "-preset", "veryfast", "-b:v", "6000k", "-maxrate", "6000k", "-bufsize", "12000k",
                  "-g", juce::String (fps * 2), "-keyint_min", juce::String (fps * 2), "-sc_threshold", "0", "-r", juce::String (fps),
                  "-c:a", "aac", "-b:a", "160k", "-ar", juce::String (sr), "-ac", "2" });
    const bool network = target.startsWithIgnoreCase ("rtmp");
    if (network) a.addArray ({ "-f", "flv", target });
    else if (target.endsWithIgnoreCase (".mp4")) a.addArray ({ "-movflags", "+faststart", "-y", target });
    else a.addArray ({ "-y", target });
    return a;
}

bool Streamer::start (const juce::File& ffmpeg, const juce::String& target, int width, int height, int fps, int sampleRate)
{
    stop();
    failed = false;
    dropped = 0;
    sent = 0;
    outBytes = -1;
    speed = 0.0f;
    slowReports = 0;
    this->target = target;
    { const juce::ScopedLock sl (lock); log.clear(); }
    if (! ffmpeg.existsAsFile()) { setStatus ("ffmpeg is missing - press \"Get ffmpeg\" first"); failed = true; return false; }

    frameBytes = width * height * 4;
    video = std::make_unique<Sender> ("video to ffmpeg", fps * 3);
    audio = std::make_unique<Sender> ("sound to ffmpeg", fps * 10);
    int vp = 0, ap = 0;
    if (! video->listen (vp) || ! audio->listen (ap)) { setStatus ("couldn't open a local port for ffmpeg"); failed = true; return false; }

    process = std::make_unique<juce::ChildProcess>();
    if (! process->start (buildArgs (ffmpeg, target, width, height, fps, sampleRate, vp, ap, smooth.load()),
                          juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        setStatus ("couldn't start ffmpeg");
        failed = true;
        process.reset();
        return false;
    }
    video->startThread();
    audio->startThread();
    {
        const juce::ScopedLock pl (pushLock);
        running = true;
    }
    setStatus (target.startsWithIgnoreCase ("rtmp") ? "connecting to YouTube ..." : "recording ...");
    watcher = std::make_unique<Watcher> (*this);
    watcher->startThread();
    return true;
}

void Streamer::stop()
{
    const bool was = running.exchange (false);
    { const juce::ScopedLock pl (pushLock); }   // a push() in progress has finished
    if (video != nullptr) video->drainAndFinish (was ? 3000 : 0);
    if (audio != nullptr) audio->drainAndFinish (was ? 3000 : 0);
    if (process != nullptr)
    {
        if (! process->waitForProcessToFinish (15000)) process->kill();
    }
    if (watcher != nullptr) { watcher->signalThreadShouldExit(); watcher->stopThread (3000); }
    watcher.reset();
    video.reset();
    audio.reset();
    process.reset();
    if (was && ! failed) setStatus ("not streaming");
}

void Streamer::push (const juce::uint32* pixels, const float* left, const float* right, int n)
{
    const juce::ScopedLock pl (pushLock);
    if (! running.load() || video == nullptr || audio == nullptr) return;
    if (! video->hasRoom() || ! audio->hasRoom()) { ++dropped; return; }   // the picture and its sound go (or not) together
    juce::MemoryBlock v (pixels, (size_t) frameBytes);
    juce::MemoryBlock a ((size_t) n * 4);
    auto* s = static_cast<juce::int16*> (a.getData());
    for (int i = 0; i < n; ++i)
    {
        s[2 * i]     = (juce::int16) juce::jlimit (-32767, 32767, (int) std::lround (left[i] * 32767.0f));
        s[2 * i + 1] = (juce::int16) juce::jlimit (-32767, 32767, (int) std::lround (right[i] * 32767.0f));
    }
    video->add (std::move (v));
    audio->add (std::move (a));
    ++sent;
}
} // namespace ocean
