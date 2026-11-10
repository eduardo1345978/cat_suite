// Cat Loop's engine check: a loop plays back exactly what was played, the first loop sets the tempo, overdub + undo /
// redo, the next loops start and end on the bar, count-in, seamless loop ends, latency, beat, stop / play all, clear,
// a full track, sessions (get / set loop, stems, mix), a sample-rate change and a fuzz of random presses.
#include "LoopEngine.h"
#include <cstdio>
#include <functional>

using namespace juce;
using namespace looper;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

constexpr double sr = 48000.0;
constexpr int block = 480;

/** Runs the engine for n samples (a multiple of the block), input from f(sample), output appended. */
struct Rig
{
    Engine e;
    juce::int64 clock = 0;
    std::vector<float> outL, outR;
    std::function<std::pair<float, float> (juce::int64)> in = [] (juce::int64) { return std::pair { 0.0f, 0.0f }; };

    explicit Rig (double maxSeconds = 20.0)
    {
        e.prepare (sr, 512, maxSeconds);
        e.safety = false;
        e.master = 1.0f;
        e.click = (int) Click::off;
        for (int t = 0; t < numTracks; ++t) e.volume (t) = 1.0f;
    }
    void run (int samples)
    {
        std::vector<float> il ((size_t) block), ir ((size_t) block), ol ((size_t) block), orr ((size_t) block);
        for (int done = 0; done < samples; done += block)
        {
            for (int i = 0; i < block; ++i) { auto [l, r] = in (clock + i); il[(size_t) i] = l; ir[(size_t) i] = r; }
            e.process (il.data(), ir.data(), ol.data(), orr.data(), block);
            outL.insert (outL.end(), ol.begin(), ol.end());
            outR.insert (outR.end(), orr.begin(), orr.end());
            clock += block;
        }
    }
};

static float noiseAt (juce::int64 i)
{
    juce::uint32 x = (juce::uint32) (i * 2654435761u + 12345u);
    x ^= x >> 13; x *= 0x5bd1e995; x ^= x >> 15;
    return ((float) (x & 0xffff) / 32768.0f - 1.0f) * 0.25f;
}

int main()
{
    ScopedJuceInitialiser_GUI juce;

    // ---- 1. the first loop: free, it sets the tempo, and it plays back exactly ----
    {
        Rig r;
        r.in = [] (juce::int64 i) { return std::pair { noiseAt (i), noiseAt (i + 7777) }; };
        r.e.press (0);
        r.run (96000);                                      // 2.0 s
        r.in = [] (juce::int64) { return std::pair { 0.0f, 0.0f }; };
        r.e.press (0);                                      // close
        r.run (96000 * 2);
        check (r.e.getLength (0) == 96000, "the first loop is exactly as long as it was played (" + String (r.e.getLength (0)) + " samples)");
        check (std::abs (r.e.getBpm() - 120.0) < 0.01 && r.e.isTempoKnown() && r.e.getBars (0) == 1,
               "it set the tempo: 2 s = 1 bar of 120 BPM (" + String (r.e.getBpm(), 2) + " BPM, " + String (r.e.getBars (0)) + " bar)");
        float worst = 0.0f;
        for (int pass = 0; pass < 2; ++pass)
            for (int k = 300; k < 96000; ++k)
                worst = jmax (worst, std::abs (r.outL[(size_t) (96000 * (pass + 1) + k)] - noiseAt (k)), std::abs (r.outR[(size_t) (96000 * (pass + 1) + k)] - noiseAt (k + 7777)));
        check (worst < 1.0e-5f, "it plays back exactly what was played, twice (largest difference " + String (worst, 7) + ")");
        check (r.e.getState (0) == State::playing, "after REC it plays");

        // ---- 2. overdub, undo, redo ----
        auto sine = [] (juce::int64 i) { const float v = 0.2f * (float) std::sin (2.0 * MathConstants<double>::pi * 330.0 * (double) i / sr); return std::pair { v, v }; };
        r.e.press (0);                                       // DUB, at the loop's start (transport 288000 = 3 loops)
        r.in = [&] (juce::int64 i) { return sine (i); };
        r.run (96000);
        r.in = [] (juce::int64) { return std::pair { 0.0f, 0.0f }; };
        r.e.press (0);                                       // back to PLAY
        r.run (96000);
        const auto start = r.outL.size() - 96000;
        worst = 0.0f;
        for (int k = 600; k < 95000; ++k)
            worst = jmax (worst, std::abs (r.outL[start + (size_t) k] - (noiseAt (k) + sine (288000 + k).first)));
        check (r.e.getState (0) == State::playing && worst < 1.0e-4f, "DUB adds what was played on top (largest difference " + String (worst, 6) + ")");
        check (! r.e.undo (1), "UNDO on an empty track does nothing");
        check (r.e.undo (0), "UNDO takes the overdub back");
        r.run (96000);
        worst = 0.0f;
        for (int k = 300; k < 96000; ++k) worst = jmax (worst, std::abs (r.outL[r.outL.size() - 96000 + (size_t) k] - noiseAt (k)));
        check (worst < 1.0e-5f && r.e.isUndone (0), "after UNDO: the loop as it was before (" + String (worst, 7) + ")");
        r.e.undo (0);
        r.run (96000);
        worst = 0.0f;
        for (int k = 600; k < 95000; ++k)
            worst = jmax (worst, std::abs (r.outL[r.outL.size() - 96000 + (size_t) k] - (noiseAt (k) + sine (288000 + k).first)));
        check (worst < 1.0e-4f && ! r.e.isUndone (0), "UNDO again = REDO: the overdub is back (" + String (worst, 6) + ")");

        // ---- 3. the next loop starts on the next bar and ends on a bar ----
        r.run (24000);                                        // a quarter into a bar
        r.e.press (1);
        r.run (480);
        check (r.e.getState (1) == State::armed, "track 2 waits for the next bar");
        r.in = [] (juce::int64 i) { return std::pair { noiseAt (i + 3), noiseAt (i + 5) }; };
        r.run (72000 - 480);                                  // ... the bar line
        r.run (480);
        check (r.e.getState (1) == State::recording, "and records from the bar line");
        r.run (96000 + 28800 - 480);                         // 1.3 bars recorded
        r.e.press (1);
        r.run (480);
        check (r.e.getLength (1) == 96000 && r.e.getBars (1) == 1 && r.e.getState (1) == State::playing,
               "pressed at 1.3 bars: the loop is 1 bar (" + String (r.e.getLength (1)) + " samples)");
        check (std::abs (r.e.getPosition (1) - r.e.getPosition (0)) < 0.01f, "and it runs in step with track 1");

        r.e.press (2);
        r.run (66720);                                        // waits for the bar line ...
        r.run (163200);                                       // ... then 1.7 bars
        r.e.press (2);
        r.run (480);
        check (r.e.getState (2) == State::recording, "pressed at 1.7 bars: it records on to the 2nd bar line ...");
        r.run (96000);
        check (r.e.getLength (2) == 192000 && r.e.getBars (2) == 2 && r.e.getState (2) == State::playing, "... and is then 2 bars long");

        // ---- 4. stop all, play all, clear ----
        r.e.stopAll();
        r.run (4800);
        r.run (9600);
        float tail = 0.0f;
        for (size_t k = r.outL.size() - 9600; k < r.outL.size(); ++k) tail = jmax (tail, std::abs (r.outL[k]));
        check (r.e.getState (0) == State::stopped && r.e.getState (1) == State::stopped && ! r.e.isRunning() && tail == 0.0f,
               "STOP ALL: everything stops (fading out) and the clock stops");
        r.e.playAll();
        r.run (480);
        check (r.e.getState (0) == State::playing && r.e.getState (2) == State::playing && r.e.getPosition (0) < 0.02f,
               "PLAY ALL: everything starts again from the beginning");
        r.e.clear (1);
        r.run (480);
        check (r.e.getState (1) == State::empty && r.e.getLength (1) == 0, "CLEAR empties a track");

        // ---- 5. sessions: get / set a loop, stems, the mix ----
        auto loop = r.e.getLoop (0);
        check (loop.getNumSamples() == 96000, "a loop can be saved (" + String (loop.getNumSamples()) + " samples)");
        check (r.e.setLoop (1, loop) && r.e.getState (1) == State::stopped && r.e.getLength (1) == 96000, "and loaded onto an empty track");
        check (! r.e.setLoop (0, loop), "but not onto a track that has a loop");
        const auto st = r.e.stem (0);
        const auto mix = r.e.mixdown (true);
        check (st.getNumSamples() == 192000 && mix.getNumSamples() == 192000 && mix.getMagnitude (0, mix.getNumSamples()) <= 0.98f,
               "stems and the mix are as long as the longest loop (2 bars) and never clip");
    }

    // ---- 6. count-in: one bar of clicks, then REC ----
    {
        Rig r;
        r.e.click = (int) Click::countIn;
        r.e.setBpm (120.0);
        r.in = [] (juce::int64 i) { return std::pair { noiseAt (i), noiseAt (i) }; };
        r.e.press (0);
        r.run (480);
        check (r.e.getState (0) == State::armed && r.e.getBarPosition() < 0.0, "with the click: it counts one bar in first");
        float clickPeak = 0.0f;
        for (int k = 0; k < 480; ++k) clickPeak = jmax (clickPeak, std::abs (r.outL[(size_t) k]));
        r.run (96000);
        check (r.e.getState (0) == State::recording && clickPeak > 0.05f, "the count-in clicks, and REC starts on the bar line");
        r.run (96000 * 2);
        r.e.press (0);
        r.run (480);
        check (r.e.getLength (0) == 192000, "pressed right on the bar line: exactly 2 bars (" + String (r.e.getLength (0)) + ")");
    }

    // ---- 7. seamless loop ends (a sine that doesn't fit the loop) ----
    {
        Rig r;
        r.in = [] (juce::int64 i) { const float v = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.0 * (double) i / sr); return std::pair { v, v }; };
        r.e.press (0);
        r.run (91200);                                        // 1.9 s: the sine is cut in the middle of a wave
        r.e.press (0);
        r.run (91200 * 3);
        float jump = 0.0f;
        for (size_t k = 91200 + 600; k + 1 < r.outL.size(); ++k) jump = jmax (jump, std::abs (r.outL[k + 1] - r.outL[k]));
        check (jump < 0.03f, "no click where the loop starts again (largest step " + String (jump, 4) + ", a sine step is 0.009)");
    }

    // ---- 8. latency: overdubs land where they were heard ----
    {
        Rig r;
        r.e.latency = 480;
        r.e.setBpm (120.0);
        r.e.press (0);
        r.run (96000);
        r.e.press (0);                                        // a silent 1-bar loop
        r.run (480);
        r.e.press (0);                                        // DUB
        const juce::int64 hitAt = r.clock + 48000;          // half a bar later
        r.in = [hitAt] (juce::int64 i) { return std::pair { i == hitAt ? 1.0f : 0.0f, i == hitAt ? 1.0f : 0.0f }; };
        r.run (96000);
        r.e.press (0);
        r.run (4800);
        auto loop = r.e.getLoop (0);
        int at = -1;
        for (int k = 0; k < loop.getNumSamples(); ++k) if (loop.getSample (0, k) > 0.5f) at = k;
        const int heard = (int) ((hitAt - 96000) % 96000);
        check (at == heard - 480, "LATENCY 10 ms: the overdub lands 480 samples earlier, where it was heard (" + String (at) + " vs " + String (heard - 480) + ")");
    }

    // ---- 9. the beat ----
    {
        Rig r;
        r.e.setBeatStyle (0);
        r.e.setBeat (true);
        r.run (96000 * 2);
        float pk = 0.0f;
        for (auto v : r.outL) pk = jmax (pk, std::abs (v));
        check (r.e.isRunning() && pk > 0.1f, "BEAT: the drums play (peak " + String (pk, 2) + ")");
        r.run (24000);
        r.e.press (0);
        r.run (480);
        check (r.e.getState (0) == State::armed, "with the beat on, a new loop waits for the bar");
        r.e.setBeat (false);
        r.e.stop (0);
        r.run (96000 * 2);
        float late = 0.0f;
        for (size_t k = r.outL.size() - 48000; k < r.outL.size(); ++k) late = jmax (late, std::abs (r.outL[k]));
        check (! r.e.isRunning() && late < 1.0e-3f, "BEAT off: it stops at the bar and the clock stops");
        const auto b = r.e.beatStem();
        check (b.getNumSamples() == 192000 && b.getMagnitude (0, b.getNumSamples()) > 0.05f, "the beat can be exported (2 bars)");
    }

    // ---- 10. a full track closes by itself ----
    {
        Rig r (3.0);
        r.in = [] (juce::int64 i) { return std::pair { noiseAt (i), noiseAt (i) }; };
        r.e.press (0);
        r.run (48000 * 5);
        check (r.e.getState (0) == State::playing && r.e.getLength (0) > 48000 * 2 && r.e.getLength (0) <= (int) (48000 * 3.2),
               "a track that is full (3 s here) closes by itself (" + String (r.e.getLength (0) / sr, 2) + " s)");
    }

    // ---- 11. a new sample rate ----
    {
        Rig r;
        r.e.press (0);
        r.run (48000);
        r.e.press (0);
        r.run (4800);
        r.e.prepare (44100.0, 512, 20.0);
        check (r.e.getState (0) == State::empty, "a new sample rate: the loops go (they would play at the wrong speed)");
        r.e.prepare (44100.0, 1024, 20.0);
        r.e.press (0);
        r.run (44160);
        r.e.press (0);
        r.run (480);
        r.e.prepare (44100.0, 256, 20.0);
        r.run (4800);
        check (r.e.getLength (0) == 44160 && r.e.getState (0) == State::playing, "the same sample rate again (a new buffer size): the loops stay");
    }

    // ---- 12. fuzz: random presses for a minute, noise in, everything on ----
    {
        Rig r;
        r.e.safety = true;
        r.e.master = 0.8f;
        r.e.click = (int) Click::always;
        r.e.latency = 256;
        r.e.monitor = 0.5f;
        r.in = [] (juce::int64 i) { return std::pair { noiseAt (i) * 2.0f, noiseAt (i + 99) * 2.0f }; };
        Random rnd (42);
        bool finite = true;
        float pk = 0.0f;
        for (int k = 0; k < 600; ++k)
        {
            const int t = rnd.nextInt (numTracks);
            switch (rnd.nextInt (12))
            {
                case 0: case 1: case 2: case 3: r.e.press (t); break;
                case 4: r.e.stop (t); break;
                case 5: r.e.clear (t); break;
                case 6: r.e.undo (t); break;
                case 7: r.e.setBeat (! r.e.isBeatOn()); break;
                case 8: r.e.togglePlayAll(); break;
                case 9: r.e.quantize = rnd.nextInt (3); break;
                case 10: r.e.newBeat(); r.e.kit = rnd.nextInt (4); break;
                default: if (rnd.nextInt (8) == 0) r.e.clearAll(); break;
            }
            const size_t from = r.outL.size();
            r.run (block * (1 + rnd.nextInt (20)));
            for (size_t i = from; i < r.outL.size(); ++i)
            {
                finite &= std::isfinite (r.outL[i]) && std::isfinite (r.outR[i]);
                pk = jmax (pk, std::abs (r.outL[i]), std::abs (r.outR[i]));
            }
        }
        check (finite && pk <= 1.0f, "600 random presses over " + String ((double) r.outL.size() / sr, 0) + " s: always finite, never over 0 dB (peak "
                                     + String (pk, 2) + ")");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CAT LOOP CHECKS PASSED" : (String (failures) + " FAILED").toRawUTF8());
    return failures == 0 ? 0 : 1;
}
