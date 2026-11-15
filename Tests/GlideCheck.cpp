// Glide-Cat's check: every sound preset plays chords cleanly - no clicks (sudden jumps), no runaway level,
// no silence, no NaN - and fast enough for real time with 8 notes held (stutter = too slow).
#include "PluginProcessor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static constexpr double sr = 48000.0;
static constexpr int block = 256;

struct Head : AudioPlayHead
{
    double ppq = 0.0;
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm (120.0); p.setIsPlaying (true); p.setPpqPosition (ppq); return p; }
};

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const bool verbose = argc > 1;
    GlideCatProcessor p;
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);
    Head head;
    p.setPlayHead (&head);
    AudioBuffer<float> buf (2, block);

    // a chord progression: C - Am - F - G, 4 notes each, changing every 0.75 s (that is where gliding happens)
    const int chords[4][4] = { { 48, 60, 64, 67 }, { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 43, 55, 59, 62 } };
    int bad = 0;
    double worstLoad = 0.0;
    String worstName;
    for (int pr = 0; pr < p.getNumPrograms(); ++pr)
    {
        p.setCurrentProgram (pr);
        p.reset();
        const double seconds = 4.0;
        const int total = (int) (seconds * sr);
        float peak = 0.0f, jump = 0.0f, prev = 0.0f;
        double sum = 0.0;
        bool nan = false;
        double cpu = 0.0;
        for (int pos = 0; pos < total; pos += block)
        {
            MidiBuffer midi;
            const int chordLen = (int) (0.75 * sr);
            for (int i = 0; i < block; ++i)
            {
                const int t = pos + i;
                if (t < 4 * chordLen && t % chordLen == 0)
                {
                    const int c = t / chordLen;
                    if (c > 0) for (int n : chords[c - 1]) midi.addEvent (MidiMessage::noteOff (1, n), i);
                    for (int n : chords[c]) midi.addEvent (MidiMessage::noteOn (1, n, (uint8) 100), i);
                }
                if (t == 4 * chordLen) for (int n : chords[3]) midi.addEvent (MidiMessage::noteOff (1, n), i);
            }
            head.ppq = pos / sr * 2.0;
            buf.clear();
            const auto t0 = Time::getHighResolutionTicks();
            p.processBlock (buf, midi);
            cpu += Time::highResolutionTicksToSeconds (Time::getHighResolutionTicks() - t0);
            for (int i = 0; i < block; ++i)
            {
                const float x = buf.getSample (0, i);
                if (! std::isfinite (x)) nan = true;
                peak = jmax (peak, std::abs (x));
                jump = jmax (jump, std::abs (x - prev));
                prev = x;
                sum += x * x;
            }
        }
        const float rms = (float) std::sqrt (sum / total);
        const double load = cpu / seconds;
        if (load > worstLoad) { worstLoad = load; worstName = p.getProgramName (pr); }
        // a click: a jump bigger than most of the level allows (a sine at full level moves at most ~0.06 per sample at 440 Hz)
        const bool click = jump > 0.5f && jump > 6.0f * rms;
        const bool ok = ! nan && peak <= 1.0f && rms > 0.003f && ! click && load < 0.25;
        if (! ok || verbose)
            std::printf ("%s  %-26s peak %.2f rms %.3f jump %.2f cpu %4.1f%%\n", ok ? "    " : "BAD ", p.getProgramName (pr).toRawUTF8(), peak, rms, jump, load * 100.0);
        if (! ok) ++bad;
    }
    check (bad == 0, String (p.getNumPrograms() - bad) + " of " + String (p.getNumPrograms()) + " presets play chords cleanly");
    check (worstLoad < 0.25, "the heaviest preset uses " + String (worstLoad * 100.0, 1) + "% of one core (" + worstName + ")");
    std::printf (failures == 0 ? "ALL GLIDE CHECKS PASSED\n" : "%d GLIDE CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
