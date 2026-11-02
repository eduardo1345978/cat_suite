// Cat Link tuning check for one follower Cat (Glide-, Sing- or Talk-Cat): it plays E4, then a fake
// Micro-Cat sends a tuning over Cat Link (every key a quarter tone up, E4 a further +20 cents) and the
// same note must come out that much higher, with no MIDI routing; when Micro-Cat stops sending, the
// pitch goes back. The pitch is measured by autocorrelation, compared as a ratio (within 12 cents: the voices have vibrato and chorus; so the timbre
// doesn't matter).
#include <juce_audio_processors/juce_audio_processors.h>
#include "LinkClient.h"
#include <cstdio>
#include <thread>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

struct PH : AudioPlayHead
{
    double ppq = 0.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i;
        i.setBpm (120.0); i.setIsPlaying (true); i.setPpqPosition (ppq);
        i.setTimeSignature (TimeSignature { 4, 4 });
        return i;
    }
};

/** A stand-in for Micro-Cat: just enough of a plug-in to own a LinkClient that sends a tuning. */
struct FakeMicro : AudioProcessor
{
    FakeMicro() : apvts (*this, nullptr, "PARAMS", {}) {}
    const String getName() const override { return "Micro-Cat"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}
    void getStateInformation (MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    AudioProcessorValueTreeState apvts;
};

static void pump (int ms) { Thread::sleep (ms); }   // the message thread (main) runs the Cat Link timers meanwhile
/** Runs f on the message thread and waits for it. */
static void onMessageThread (std::function<void()> f)
{
    WaitableEvent done;
    MessageManager::callAsync ([&] { f(); done.signal(); });
    done.wait();
}

/** Plays E4 for 1.2 s and returns the fundamental (Hz) measured over the last half second. */
static double playAndMeasure (AudioProcessor& p, PH& ph, double sr, int bs)
{
    // from the same start every time (LFOs, vibrato, random seeds), so only the tuning differs
    p.releaseResources();
    p.prepareToPlay (sr, bs);
    p.reset();
    ph.ppq = 0.0;
    const int total = (int) (1.2 * sr), keep = (int) (0.5 * sr);
    std::vector<float> rec;
    AudioBuffer<float> buf (jmax (2, p.getTotalNumOutputChannels()), bs);
    int done = 0;
    while (done < total)
    {
        MidiBuffer m;
        if (done == 0) m.addEvent (MidiMessage::noteOn (1, 64, (uint8) 100), 0);
        buf.clear();
        p.processBlock (buf, m);
        ph.ppq += bs / sr * 2.0;
        for (int i = 0; i < bs; ++i)
            if (done + i >= total - keep) rec.push_back (buf.getSample (0, i) + (buf.getNumChannels() > 1 ? buf.getSample (1, i) : 0.0f));
        done += bs;
    }
    MidiBuffer off; off.addEvent (MidiMessage::noteOff (1, 64), 0);
    for (int b = 0; b < (int) (1.5 * sr / bs); ++b) { buf.clear(); p.processBlock (buf, off); off.clear(); }   // let it ring out

    // autocorrelation over 70 .. 1000 Hz, the first strong peak, refined with a parabola
    const int n = (int) rec.size() - 2000;
    const int minLag = (int) (sr / 1000.0), maxLag = (int) (sr / 70.0);
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    double e0 = 0.0;
    for (int i = 0; i < n; ++i) e0 += (double) rec[(size_t) i] * rec[(size_t) i];
    if (e0 < 1e-6) return 0.0;
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double s = 0.0;
        for (int i = 0; i < n; ++i) s += (double) rec[(size_t) i] * rec[(size_t) (i + lag)];
        ac[(size_t) lag] = s / e0;
    }
    double best = 0.0;
    for (int lag = minLag + 1; lag <= maxLag; ++lag) best = jmax (best, ac[(size_t) lag]);
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] >= 0.9 * best && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1])
        {
            const double a = ac[(size_t) lag - 1], b = ac[(size_t) lag], c = ac[(size_t) lag + 1];
            const double d = (a - 2 * b + c) != 0.0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0.0;
            std::printf ("      measured %.3f Hz\n", sr / (lag + d));
            return sr / (lag + d);
        }
    return 0.0;
}

static void run()
{
    const double sr = 48000.0;
    const int bs = 256;
    std::unique_ptr<AudioProcessor> p;
    onMessageThread ([&] { p.reset (createPluginFilter()); });
    p->setPlayConfigDetails (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels(), sr, bs);
    PH ph; p->setPlayHead (&ph);
    p->prepareToPlay (sr, bs);
    pump (300);

    const double plain = playAndMeasure (*p, ph, sr, bs);
    check (plain > 100.0 && plain < 1000.0, p->getName() + ": plays E4 (" + String (plain, 2) + " Hz measured)");

    // Micro-Cat sends: every key +50 cents, E4 +70 cents
    FakeMicro micro;
    cat::LinkClient::Caps caps; caps.sendTuning = true;
    std::unique_ptr<cat::LinkClient> sender;
    onMessageThread ([&] { sender = std::make_unique<cat::LinkClient> (micro, micro.apvts, "Micro-Cat", caps); });
    std::array<float, 128> table {};
    for (int k = 0; k < 128; ++k) table[(size_t) k] = (float) k + 0.5f;
    table[64] = 64.7f;
    onMessageThread ([&] { sender->publishTuning (table, true); });
    pump (500);
    const double tuned = playAndMeasure (*p, ph, sr, bs);
    const double cents = 1200.0 * std::log2 (tuned / plain);
    check (std::abs (cents - 70.0) < 12.0, p->getName() + ": in Micro-Cat's tuning E4 is 70 cents higher (" + String (cents, 1) + ")");

    // Micro-Cat bypassed: back to normal
    onMessageThread ([&] { sender->publishTuning (table, false); });
    pump (500);
    const double back = playAndMeasure (*p, ph, sr, bs);
    check (std::abs (1200.0 * std::log2 (back / plain)) < 12.0, p->getName() + ": Micro-Cat bypassed, back to normal (" + String (1200.0 * std::log2 (back / plain), 1) + " cents)");

    // sends again, then Micro-Cat is removed from the project: back to normal too
    onMessageThread ([&] { sender->publishTuning (table, true); });
    pump (500);
    const double again = playAndMeasure (*p, ph, sr, bs);
    onMessageThread ([&] { sender.reset(); });
    pump (500);
    const double gone = playAndMeasure (*p, ph, sr, bs);
    check (std::abs (1200.0 * std::log2 (again / plain) - 70.0) < 12.0 && std::abs (1200.0 * std::log2 (gone / plain)) < 12.0,
           p->getName() + ": Micro-Cat removed, back to normal");

    p->releaseResources();
    onMessageThread ([&] { p.reset(); });
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    std::thread t ([] { run(); MessageManager::getInstance()->stopDispatchLoop(); });
    MessageManager::getInstance()->runDispatchLoop();
    t.join();
    std::printf (failures == 0 ? "TUNE LINK CHECK PASSED\n" : "TUNE LINK CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
