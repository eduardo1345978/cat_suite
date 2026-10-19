#include "OceanEngine.h"

namespace ocean
{
Engine::Engine (const juce::File& saveFile)
    : juce::Thread ("Cat Ocean clock"), file (saveFile),
      sim ((juce::uint32) juce::Time::currentTimeMillis())
{
    left.resize ((size_t) samplesPerFrame);
    right.resize ((size_t) samplesPerFrame);
    shown.resize ((size_t) (W * H), 0xff000000);
    fifoL.resize ((size_t) sampleRate);
    fifoR.resize ((size_t) sampleRate);
    music.prepare (sampleRate);

    if (file.existsAsFile())
        if (auto xml = juce::parseXML (file))
        {
            const auto t = juce::ValueTree::fromXml (*xml);
            sim.load (t.getChildWithName ("OCEAN"));
            music.load (t.getChildWithName ("MUSIC"));
        }
}

Engine::~Engine()
{
    stop();
    chat.stop();
    streamer.stop();
    saveNow();
}

void Engine::start (bool rt)
{
    realTime = rt;
    startThread (juce::Thread::Priority::high);
}

void Engine::stop() { stopThread (5000); }

void Engine::saveNow()
{
    if (file == juce::File()) return;
    juce::ValueTree t ("CATOCEAN");
    t.addChild (sim.save(), -1, nullptr);
    t.addChild (music.save(), -1, nullptr);
    if (auto xml = t.createXml())
    {
        file.getParentDirectory().createDirectory();
        const auto tmp = file.getSiblingFile (file.getFileName() + ".tmp");
        if (xml->writeTo (tmp)) tmp.moveFileTo (file);
    }
}

void Engine::say (const juce::String& id, const juce::String& name, const juce::String& text)
{
    const juce::ScopedLock sl (chatLock);
    if (pending.size() < 5000) pending.push_back ({ id, name, text });
}

void Engine::copyFrame (juce::Image& img) const
{
    if (! img.isValid() || img.getWidth() != W || img.getHeight() != H) img = juce::Image (juce::Image::ARGB, W, H, false);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    const juce::SpinLock::ScopedLockType sl (frameLock);
    for (int y = 0; y < H; ++y)
    {
        auto* line = reinterpret_cast<juce::uint32*> (bd.getLinePointer (y));
        std::memcpy (line, shown.data() + (size_t) (y * W), (size_t) W * 4);
    }
}

juce::String Engine::stats() const { const juce::ScopedLock sl (statsLock); return statsText; }

bool Engine::goLive (const juce::File& ffmpeg, const juce::String& target)
{
    const juce::ScopedLock sl (liveLock);
    liveFFmpeg = ffmpeg;
    liveTarget = target;
    liveWanted = true;
    lastRestart = juce::Time::getMillisecondCounterHiRes() * 0.001;
    return streamer.start (ffmpeg, target, W, H, fps, sampleRate);
}

void Engine::stopLive()
{
    const juce::ScopedLock sl (liveLock);
    liveWanted = false;
    streamer.stop();
}

void Engine::readMonitor (float* l, float* r, int n)
{
    int s1, n1, s2, n2;
    fifo.prepareToRead (n, s1, n1, s2, n2);
    const float g = monitorGain.load();
    for (int i = 0; i < n1; ++i) { l[i] = fifoL[(size_t) (s1 + i)] * g; r[i] = fifoR[(size_t) (s1 + i)] * g; }
    for (int i = 0; i < n2; ++i) { l[n1 + i] = fifoL[(size_t) (s2 + i)] * g; r[n1 + i] = fifoR[(size_t) (s2 + i)] * g; }
    for (int i = n1 + n2; i < n; ++i) l[i] = r[i] = 0.0f;
    fifo.finishedRead (n1 + n2);
}

void Engine::runFrames (int count)
{
    for (int i = 0; i < count; ++i) frame();
}

void Engine::run()
{
    const double frameMs = 1000.0 / fps;
    double next = juce::Time::getMillisecondCounterHiRes();
    while (! threadShouldExit())
    {
        frame();
        if (! realTime) continue;
        next += frameMs;
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - next > 1000.0) next = now;      // far behind (the PC was busy): don't race to catch up
        const double wait = next - now;
        if (wait > 1.0) juce::Thread::sleep ((int) wait);
    }
}

void Engine::frame()
{
    // chat: YouTube + typed in the app
    for (auto& m : chat.take()) sim.chat (m.authorId, m.authorName, m.text);
    {
        std::vector<ChatLine> lines;
        { const juce::ScopedLock sl (chatLock); lines.swap (pending); }
        for (auto& l : lines) sim.chat (l.id, l.name, l.text);
    }

    const float dt = 1.0f / fps;
    sim.step (dt);
    music.render (left.data(), right.data(), samplesPerFrame, sim);
    if (++frameCount % juce::jmax (1, paintEvery) == 0)
        painter.paint (canvas, sim, music.info (sim), dt * (float) juce::jmax (1, paintEvery));

    {
        const juce::SpinLock::ScopedLockType sl (frameLock);
        std::memcpy (shown.data(), canvas.data(), (size_t) (W * H) * 4);
    }
    streamer.push (canvas.data(), left.data(), right.data(), samplesPerFrame);

    float pk = 0.0f;
    for (int i = 0; i < samplesPerFrame; ++i) pk = juce::jmax (pk, std::abs (left[(size_t) i]), std::abs (right[(size_t) i]));
    peak = pk;

    // the speakers (drops sound when nobody listens)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (samplesPerFrame, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) { fifoL[(size_t) (s1 + i)] = left[(size_t) i]; fifoR[(size_t) (s1 + i)] = right[(size_t) i]; }
        for (int i = 0; i < n2; ++i) { fifoL[(size_t) (s2 + i)] = left[(size_t) (n1 + i)]; fifoR[(size_t) (s2 + i)] = right[(size_t) (n1 + i)]; }
        fifo.finishedWrite (n1 + n2);
        if (fifo.getNumReady() > sampleRate / 2)   // nobody reads: keep only the newest sound
        {
            int r1, rn1, r2, rn2;
            fifo.prepareToRead (fifo.getNumReady() - sampleRate / 4, r1, rn1, r2, rn2);
            fifo.finishedRead (rn1 + rn2);
        }
    }

    // a stream that broke tries again every 20 s
    if (liveWanted.load() && streamer.hasFailed())
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const juce::ScopedTryLock sl (liveLock);
        if (sl.isLocked() && liveWanted.load() && now - lastRestart > 20.0)
        {
            lastRestart = now;
            streamer.start (liveFFmpeg, liveTarget, W, H, fps, sampleRate);
        }
    }

    if (sim.time - lastSave > 60.0) { lastSave = sim.time; saveNow(); }

    if (((juce::int64) (sim.time * fps)) % 15 == 0)
    {
        int present = 0, viewersHere = 0;
        for (auto& f : sim.fish) { if (f.present && ! f.dying) ++present; if (f.viewer() && f.present) ++viewersHere; }
        juce::String s;
        s << present << " fish swimming, " << viewersHere << " of them belong to viewers (" << sim.viewerCount() << " viewers ever)\n"
          << sim.births << " babies born, " << sim.mutations << " mutations\n"
          << "music: " << styleName (sim.getStyle()) << ", " << (int) std::lround (music.currentBpm()) << " bpm, key "
          << cat::muse::keyName (music.currentKey()) << ", piece " << music.pieces();
        const juce::ScopedLock sl (statsLock);
        statsText = s;
    }
}
} // namespace ocean
