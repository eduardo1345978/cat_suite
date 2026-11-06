// Glitch-Cat's check: gentle is in tune and on the beat, madness really makes rolls and glitches,
// every preset stays finite and under 0 dB (even fully mad), the low keys play drums, and a loaded voice
// plays and is kept in the saved project.
#include "PluginProcessor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

struct FakePlayHead : AudioPlayHead
{
    bool playing = false;
    double bpm = 120.0, ppq = 0.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        return info;
    }
};

struct Ev { double t; MidiMessage msg; };
constexpr double sr = 48000.0;
constexpr int block = 256;

static std::vector<float> render (GlitchCatProcessor& p, double seconds, std::vector<Ev> evs, FakePlayHead* ph = nullptr)
{
    std::vector<float> out;
    AudioBuffer<float> buf (2, block);
    MidiBuffer midi;
    const int blocks = (int) (seconds * sr / block);
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        const double t0 = b * block / sr, t1 = (b + 1) * block / sr;
        for (auto& e : evs) if (e.t >= t0 && e.t < t1) midi.addEvent (e.msg, (int) ((e.t - t0) * sr));
        buf.clear();
        p.processBlock (buf, midi);
        for (int i = 0; i < block; ++i) out.push_back (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
        if (ph != nullptr && ph->playing) ph->ppq += block / sr * ph->bpm / 60.0;
    }
    return out;
}

static void set (GlitchCatProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static std::unique_ptr<GlitchCatProcessor> make()
{
    auto p = std::make_unique<GlitchCatProcessor>();
    p->setPlayConfigDetails (0, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void quiet (GlitchCatProcessor& p)   // just the synth, dry
{
    set (p, "drumLevel", 0.0f); set (p, "voiceLevel", 0.0f); set (p, "space", 0.0f); set (p, "tape", 0.0f);
}

static double pitchHz (const std::vector<float>& x, int from, int to)
{
    double best = 0.0; int bestLag = 0;
    for (int lag = (int) (sr / 1000.0); lag < (int) (sr / 150.0); ++lag)
    {
        double s = 0.0, e1 = 0.0, e2 = 0.0;
        for (int i = from; i < to - lag; ++i) { s += x[(size_t) i] * x[(size_t) (i + lag)]; e1 += x[(size_t) i] * x[(size_t) i]; e2 += x[(size_t) (i + lag)] * x[(size_t) (i + lag)]; }
        const double c = s / std::sqrt (e1 * e2 + 1e-12);
        if (c > best) { best = c; bestLag = lag; }
    }
    if (bestLag <= 0) return 0.0;
    // parabolic refinement
    auto corr = [&] (int lag)
    {
        double s = 0.0, e = 0.0;
        for (int i = from; i < to - lag; ++i) { s += x[(size_t) i] * x[(size_t) (i + lag)]; e += x[(size_t) i] * x[(size_t) i]; }
        return s / (e + 1e-12);
    };
    const double a = corr (bestLag - 1), b = corr (bestLag), c = corr (bestLag + 1);
    const double shift = 0.5 * (a - c) / (a - 2.0 * b + c);
    return sr / (bestLag + (std::isfinite (shift) ? shift : 0.0));
}

static std::vector<double> onsets (const std::vector<float>& x, double windowMs, float floor, float jump = 1.8f)
{
    const int w = (int) (sr * windowMs / 1000.0);
    std::vector<double> times;
    float prev = 0.0f;
    bool armed = true;
    for (size_t i = 0; i + (size_t) w < x.size(); i += (size_t) w)
    {
        float e = 0.0f;
        for (int k = 0; k < w; ++k) e = jmax (e, std::abs (x[i + (size_t) k]));
        if (armed && e > floor && e > prev * jump) { times.push_back ((double) i / sr); armed = false; }
        if (e < prev * 0.9f) armed = true;
        prev = e;
    }
    return times;
}

static float peak (const std::vector<float>& x, int from = 0, int to = -1)
{
    float pk = 0.0f;
    for (int i = from; i < (to < 0 ? (int) x.size() : to); ++i) pk = jmax (pk, std::abs (x[(size_t) i]));
    return pk;
}

int main()
{
    ScopedJuceInitialiser_GUI init;

    // gentle: a warm swarm, in tune
    {
        auto p = make();
        quiet (*p);
        set (*p, "madness", 0.0f);
        const auto x = render (*p, 1.6, { { 0.0, MidiMessage::noteOn (1, 69, 0.8f) } });
        const double f = pitchHz (x, (int) (0.6 * sr), (int) (1.5 * sr));
        const double cents = 1200.0 * std::log2 (f / 440.0);
        check (peak (x) > 0.05f, "gentle: the swarm sounds (peak " + String (peak (x), 3) + ")");
        check (std::abs (cents) < 20.0, "gentle: A4 is A4 (" + String (f, 1) + " Hz, " + String (cents, 1) + " cents)");
        check (p->engine.activeBugs.load() == 2 + roundToInt (0.5f * 6.0f), "a note falls apart into its bugs (" + String (p->engine.activeBugs.load()) + ")");
        check (p->engine.glitchEvents.load() == 0, "gentle: no glitches");
    }

    // every preset, played and then fully mad (mod wheel): finite, never over 0 dB
    {
        auto p = make();
        bool ok = true;
        float worst = 0.0f;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            std::vector<Ev> evs;
            for (int k : { 60, 64, 67, 71, 36, 38 }) evs.push_back ({ 0.01, MidiMessage::noteOn (1, k, 0.9f) });
            evs.push_back ({ 1.0, MidiMessage::controllerEvent (1, 1, 127) });
            evs.push_back ({ 1.2, MidiMessage::pitchWheel (1, 16000) });
            for (int k : { 60, 64, 67, 71 }) evs.push_back ({ 2.2, MidiMessage::noteOff (1, k) });
            evs.push_back ({ 2.5, MidiMessage::controllerEvent (1, 1, 0) });
            const auto x = render (*p, 3.0, evs);
            for (float v : x) ok &= std::isfinite (v);
            worst = jmax (worst, peak (x));
        }
        check (ok && worst <= 1.0f, "12 presets, fully mad too: finite and under 0 dB (peak " + String (worst, 3) + ")");
        check (p->getNumPrograms() == 12, "12 presets");
    }

    // madness makes rolls: the bugs restart (ratchets) and the level jumps about
    {
        struct R { int retrig; double wobble; };
        auto measure = [] (float madness)
        {
            auto p = make();
            quiet (*p);
            set (*p, "madness", madness);
            set (*p, "doDetune", 0.0f); set (*p, "doStutter", 0.0f); set (*p, "doOdd", 0.0f);
            set (*p, "attack", 0.001f);
            set (*p, "flight", 0.0f);
            const auto x = render (*p, 3.0, { { 0.0, MidiMessage::noteOn (1, 60, 0.9f) }, { 0.0, MidiMessage::noteOn (1, 67, 0.9f) } });
            // how much the loudness moves (20 ms windows, after the first half second)
            std::vector<double> env;
            const int w = (int) (0.02 * sr);
            for (size_t i = (size_t) (0.5 * sr); i + (size_t) w < x.size(); i += (size_t) w)
            {
                double e = 0.0;
                for (int k = 0; k < w; ++k) e += (double) x[i + (size_t) k] * x[i + (size_t) k];
                env.push_back (std::sqrt (e / w));
            }
            double mean = 0.0, var = 0.0;
            for (double e : env) mean += e;
            mean /= (double) env.size();
            for (double e : env) var += (e - mean) * (e - mean);
            return R { p->engine.retriggers.load(), std::sqrt (var / (double) env.size()) / (mean + 1e-9) };
        };
        const auto gentle = measure (0.0f), mad = measure (0.55f), insane = measure (0.95f);
        check (gentle.retrig == 0 && mad.retrig > 100, "madness: the bugs roll and ratchet (" + String (gentle.retrig) + " restarts gentle, " + String (mad.retrig) + " mad)");
        check (mad.wobble > gentle.wobble * 2.0 && insane.wobble > gentle.wobble * 2.0,
               "madness: you hear the rolls - the loudness jumps (" + String (gentle.wobble, 2) + " gentle, " + String (mad.wobble, 2) + " mad, " + String (insane.wobble, 2) + " insane)");
    }

    // the swarm's beat sits on Cubase's grid when gentle: a kick right on every beat
    {
        auto p = make();
        quiet (*p);
        set (*p, "synthLevel", 0.0f);
        set (*p, "drumLevel", 1.0f);
        set (*p, "madness", 0.0f);
        set (*p, "beatMode", (float) glitch::BeatAuto);
        set (*p, "beatStyle", (float) glitch::Rave);
        FakePlayHead ph; ph.playing = true; ph.bpm = 120.0;
        p->setPlayHead (&ph);
        const auto x = render (*p, 4.0, { { 0.0, MidiMessage::noteOn (1, 60, 0.9f) } }, &ph);
        auto energy = [&] (double from, double to)
        {
            double e = 0.0;
            for (int i = jmax (0, (int) (from * sr)); i < jmin ((int) x.size(), (int) (to * sr)); ++i) e += (double) x[(size_t) i] * x[(size_t) i];
            return e;
        };
        int onBeat = 0;
        for (int k = 1; k < 8; ++k)
        {
            const double t = k * 0.5;
            if (energy (t, t + 0.006) > 6.0 * energy (t - 0.006, t)) ++onBeat;
        }
        check (onBeat == 7, "gentle: the kick hits right on every beat at 120 BPM (" + String (onBeat) + " / 7)");
        p->setPlayHead (nullptr);
    }

    // madness glitches: stutter / reverse / tape stop / chop
    {
        auto p = make();
        set (*p, "madness", 1.0f);
        set (*p, "doRolls", 0.0f); set (*p, "doDetune", 0.0f); set (*p, "doOdd", 0.0f);
        FakePlayHead ph; ph.playing = true;
        p->setPlayHead (&ph);
        const auto x = render (*p, 4.0, { { 0.0, MidiMessage::noteOn (1, 60, 0.9f) }, { 0.0, MidiMessage::noteOn (1, 64, 0.9f) } }, &ph);
        check (p->engine.glitchEvents.load() >= 4 && peak (x) <= 1.0f, "insane: the sound glitches (" + String (p->engine.glitchEvents.load()) + " glitches in 4 s)");
        p->setPlayHead (nullptr);
    }

    // the low keys play drums
    {
        auto p = make();
        quiet (*p);
        set (*p, "drumLevel", 1.0f);
        set (*p, "beatMode", (float) glitch::BeatKeys);
        const auto x = render (*p, 0.5, { { 0.0, MidiMessage::noteOn (1, 36, 1.0f) } });
        check (peak (x, 0, (int) (0.3 * sr)) > 0.1f && p->engine.activeBugs.load() == 0, "Low keys: C1 plays the kick, no swarm");
        set (*p, "beatMode", (float) glitch::BeatOff);
        render (*p, 0.3, { { 0.0, MidiMessage::noteOn (1, 40, 1.0f) } });
        check (p->engine.activeBugs.load() > 0, "beat off: the low keys are notes again");
    }

    // my voice: load, play, keep in the project
    {
        auto file = File::getSpecialLocation (File::tempDirectory).getChildFile ("glitchcheck_voice.wav");
        {
            file.deleteFile();
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (file), 44100.0, 1, 16, {}, 0));
            std::vector<float> v (44100);
            double ph = 0.0;
            for (size_t i = 0; i < v.size(); ++i)
            {
                ph += 220.0 / 44100.0;
                const float env = (float) std::sin (MathConstants<double>::pi * (double) i / (double) v.size());
                v[i] = env * (float) (0.4 * std::sin (MathConstants<double>::twoPi * ph) + 0.2 * std::sin (MathConstants<double>::twoPi * 3 * ph));
            }
            const float* ch[] = { v.data() };
            w->writeFromFloatArrays (ch, 1, (int) v.size());
        }
        auto p = make();
        quiet (*p);
        String err;
        const bool loaded = p->loadVoice (file, err);
        set (*p, "synthLevel", 0.0f);
        set (*p, "voiceLevel", 1.0f);
        set (*p, "voiceType", (float) glitch::MyVoice);
        const auto x = render (*p, 1.0, { { 0.0, MidiMessage::noteOn (1, 60, 0.9f) } });
        check (loaded && peak (x) > 0.05f, "my voice: a recording loads and the swarm plays it (peak " + String (peak (x), 3) + ")");

        // a shorter voice arrives while the swarm is still reading the long one (crashed Cubase, 5 Nov)
        {
            auto shortFile = File::getSpecialLocation (File::tempDirectory).getChildFile ("glitchcheck_short.wav");
            shortFile.deleteFile();
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (shortFile), 44100.0, 1, 16, {}, 0));
            std::vector<float> v (6000);
            for (size_t i = 0; i < v.size(); ++i) v[i] = 0.5f * std::sin ((float) i * 0.05f) * std::sin (MathConstants<float>::pi * (float) i / (float) v.size());
            const float* ch[] = { v.data() };
            w->writeFromFloatArrays (ch, 1, (int) v.size());
            w.reset();
            set (*p, "madness", 0.0f);
            render (*p, 0.5, { { 0.0, MidiMessage::noteOn (1, 64, 0.9f) }, { 0.0, MidiMessage::noteOn (1, 67, 0.9f) } });   // bugs deep in the long voice
            String e2;
            const bool ok = p->loadVoice (shortFile, e2);
            const auto y = render (*p, 0.5, {});
            bool finite = true;
            for (float s : y) finite &= std::isfinite (s);
            check (ok && finite && peak (y) > 0.01f, "a shorter voice loaded while notes play: no crash, it keeps singing");
            render (*p, 0.2, { { 0.0, MidiMessage::noteOff (1, 64) }, { 0.0, MidiMessage::noteOff (1, 67) }, { 0.0, MidiMessage::noteOff (1, 60) } });
            shortFile.deleteFile();
            p->loadVoice (file, e2);
        }

        set (*p, "madness", 0.77f);
        MemoryBlock state;
        p->getStateInformation (state);
        auto q = make();
        q->setStateInformation (state.getData(), (int) state.getSize());
        check (q->voiceName() == p->voiceName() && std::abs (q->voiceSeconds() - p->voiceSeconds()) < 0.01f
                   && std::abs (q->apvts.getRawParameterValue ("madness")->load() - 0.77f) < 0.001f,
               "saved project: the voice (" + String (q->voiceSeconds(), 2) + " s) and the knobs come back");
        String err2;
        auto bad = File::getSpecialLocation (File::tempDirectory).getChildFile ("glitchcheck_bad.wav");
        bad.replaceWithText ("nope");
        check (! q->loadVoice (bad, err2) && err2.isNotEmpty(), "a file that isn't audio is refused");
        bad.deleteFile();
        file.deleteFile();

        // record from the side-chain input
        auto r = make();
        r->setPlayConfigDetails (2, 2, sr, block);
        r->prepareToPlay (sr, block);
        r->startRecording();
        AudioBuffer<float> buf (2, block);
        MidiBuffer midi;
        for (int b = 0; b < 100; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.5f * std::sin ((float) (b * block + i) * 0.03f);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            r->processBlock (buf, midi);
        }
        r->stopRecording();
        check (r->voiceSeconds() > 0.4f, "recording through the side-chain input makes the voice (" + String (r->voiceSeconds(), 2) + " s)");
    }

    std::printf (failures == 0 ? "GLITCH CHECK PASSED\n" : "GLITCH CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
