// Negative's check: a picture comes back out of its own sound (in all three ways), a black picture is silent,
// the musical way only plays notes of the key, MIDI plays it higher, it follows Cubase, it hides in a track without
// touching it, it opens and saves sounds, and the photo is saved with the project.
#include "PluginProcessor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

constexpr double sr = 48000.0;
constexpr int block = 512;

struct Head : AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0; bool playing = false;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setIsPlaying (playing); p.setPpqPosition (ppq);
        return p;
    }
};

static std::unique_ptr<NegativeProcessor> make()
{
    auto p = std::make_unique<NegativeProcessor>();
    p->setPlayConfigDetails (2, 2, sr, block);
    p->prepareToPlay (sr, block);
    return p;
}

static void set (NegativeProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** Render a picture straight through the core. */
static AudioBuffer<float> sound (const Image& img, int mode, float low = 100.0f, float high = 10000.0f, double seconds = 4.0, int key = 0, int scale = 1)
{
    negative::RenderSettings s;
    s.mode = mode; s.lowHz = low; s.highHz = high; s.seconds = seconds; s.sampleRate = sr; s.key = key; s.scale = scale;
    return negative::render (negative::prepare (img, 0.5f, 0.0f, false), s);
}

/** How much two pictures look alike (correlation of their brightness, both made 64 x 32). */
static double likeness (const Image& a, const Image& b)
{
    auto ga = negative::prepare (a.rescaled (64, 32, Graphics::highResamplingQuality), 0.5f, 0.0f, false);
    auto gb = negative::prepare (b.rescaled (64, 32, Graphics::highResamplingQuality), 0.5f, 0.0f, false);
    double ma = 0, mb = 0;
    for (size_t i = 0; i < ga.v.size(); ++i) { ma += ga.v[i]; mb += gb.v[i]; }
    ma /= (double) ga.v.size(); mb /= (double) gb.v.size();
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < ga.v.size(); ++i)
    {
        const double x = ga.v[i] - ma, y = gb.v[i] - mb;
        sab += x * y; saa += x * x; sbb += y * y;
    }
    return sab / std::sqrt (saa * sbb + 1e-12);
}

static Image testPicture()
{
    // a big X and a sun: something you'd recognise
    Image img (Image::RGB, 320, 160, true);
    Graphics g (img);
    g.setColour (Colours::white);
    g.drawLine (20.0f, 20.0f, 300.0f, 140.0f, 10.0f);
    g.drawLine (20.0f, 140.0f, 300.0f, 20.0f, 10.0f);
    g.fillEllipse (130.0f, 50.0f, 60.0f, 60.0f);
    return img;
}

/** The loudest frequency in the middle second (FFT peak, 1 Hz). */
static double peakHz (const AudioBuffer<float>& b, double from, double len)
{
    const int a = (int) (from * sr), n = (int) (len * sr);
    double best = 0, bestF = 0;
    for (double f = 100.0; f < 4000.0; f += 1.0)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; i += 2)
        {
            const double ph = MathConstants<double>::twoPi * f * i / sr;
            const double v = b.getSample (0, a + i);
            re += v * std::cos (ph); im += v * std::sin (ph);
        }
        if (re * re + im * im > best) { best = re * re + im * im; bestF = f; }
    }
    return bestF;
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    const auto pic = testPicture();

    // the photo comes back out of its sound
    for (int mode : { 0, 2 })
    {
        const auto snd = sound (pic, mode);
        const auto back = negative::analyse (snd, sr, 100.0f, 10000.0f, 320, 160);
        const double l = likeness (pic, back);
        check (l > (mode == 0 ? 0.75 : 0.35), String (mode == 0 ? "EXACT" : "TEXTURE") + ": the picture comes back out of its sound (likeness " + String (l, 2) + ")");
    }

    // black is silence; the sound never clips
    {
        Image black (Image::RGB, 100, 50, true);
        bool silent = true;
        for (int mode : { 0, 1, 2 }) silent &= sound (black, mode).getMagnitude (0, 0, (int) (4.0 * sr)) < 1.0e-6f;
        const auto loud = sound (pic, 0);
        check (silent && loud.getMagnitude (0, 0, loud.getNumSamples()) <= 0.9f, "a black picture is silent, a bright one stays under -1 dB");
    }

    // MUSICAL: only notes of the key (A minor): a bright band near 440 Hz plays A, not something in between
    {
        Image band (Image::RGB, 200, 200, true);
        {
            Graphics g (band);
            g.setColour (Colours::white);
            // 100 .. 10000 Hz log over 200 rows: 440 Hz is at row (1 - log(4.4)/log(100)) * 199 = 135
            g.fillRect (0, 133, 200, 4);
        }
        const auto snd = sound (band, 1, 100.0f, 10000.0f, 3.0, 9, 1);
        const double f = peakHz (snd, 1.0, 0.5);
        const double cents = 1200.0 * std::log2 (f / 440.0);
        check (std::abs (cents) < 15.0, "MUSICAL in A minor: a bright line at 440 Hz plays A (" + String (f, 1) + " Hz)");
    }

    // the processor: it renders in the background, MIDI plays it, Cubase plays it in time, the input passes untouched
    {
        auto p = make();
        Image band (Image::RGB, 200, 200, true);
        { Graphics g (band); g.setColour (Colours::white); g.fillRect (0, 133, 200, 4); }
        set (*p, "mode", 1.0f); set (*p, "low", 100.0f); set (*p, "high", 10000.0f); set (*p, "key", 9.0f); set (*p, "level", 0.0f);
        set (*p, "sync", 0.0f);
        p->setPhoto (band, "band");
        p->waitForRender();
        check (p->getRenderCount() > 0 && p->getSpectrogram().isValid(), "the photo became sound in the background (and its spectrogram is there)");

        // MIDI: C5 plays it an octave up
        AudioBuffer<float> buf (2, block);
        MidiBuffer midi;
        AudioBuffer<float> rec (2, (int) sr);
        for (int b = 0; b < (int) sr / block; ++b)
        {
            buf.clear(); midi.clear();
            if (b == 0) midi.addEvent (MidiMessage::noteOn (1, 72, (uint8) 127), 0);
            p->processBlock (buf, midi);
            rec.copyFrom (0, b * block, buf, 0, 0, block);
        }
        const double f = peakHz (rec, 0.3, 0.4);
        check (std::abs (1200.0 * std::log2 (f / 880.0)) < 20.0, "MIDI C5 plays the photo an octave higher (" + String (f, 1) + " Hz)");

        // the input goes through untouched when nothing plays (the key is let go first)
        midi.clear(); midi.addEvent (MidiMessage::noteOff (1, 72), 0);
        for (int b = 0; b < 20; ++b) { buf.clear(); p->processBlock (buf, midi); midi.clear(); }
        set (*p, "sync", 1.0f);
        Head head; head.playing = false;
        p->setPlayHead (&head);
        midi.clear();
        buf.clear();
        for (int i = 0; i < block; ++i) { buf.setSample (0, i, 0.25f); buf.setSample (1, i, -0.25f); }
        p->processBlock (buf, midi);
        bool same = true;
        for (int i = 0; i < block; ++i) same &= buf.getSample (0, i) == 0.25f && buf.getSample (1, i) == -0.25f;
        check (same, "stopped: the track's sound passes through untouched");
        p->setPlayHead (nullptr);
    }

    // in time with Cubase: at beat 8 of 16 (4 bars) it plays the middle of the photo
    {
        auto p = make();
        Image half (Image::RGB, 200, 100, true);
        { Graphics g (half); g.setColour (Colours::white); g.fillRect (100, 0, 100, 100); }   // only the second half sounds
        set (*p, "level", 0.0f); set (*p, "sync", 1.0f);
        p->setPhoto (half, "half");
        p->waitForRender();
        Head head; head.playing = true; head.bpm = 120.0;
        p->setPlayHead (&head);
        AudioBuffer<float> buf (2, block);
        MidiBuffer midi;
        auto levelAt = [&] (double ppq)
        {
            head.ppq = ppq;
            buf.clear();
            p->processBlock (buf, midi);
            return buf.getRMSLevel (0, 0, block);
        };
        const float first = levelAt (3.0), second = levelAt (12.0), wrapped = levelAt (16.0 + 12.0);
        check (first < 1.0e-4f && second > 0.01f && wrapped > 0.01f, "with Cubase: the first half is silent, the second sounds, and it starts again after 4 bars");
        p->setPlayHead (nullptr);
    }

    // hiding: a song + the photo at -40 dB, saved and read back; the song is still the song, the photo shows up high
    {
        auto p = make();
        const auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("negative-check");
        dir.createDirectory();
        // a song: a 220 Hz tone, 6 s
        const auto songFile = dir.getChildFile ("song.wav");
        {
            AudioBuffer<float> s (2, (int) (6.0 * sr));
            for (int i = 0; i < s.getNumSamples(); ++i) { const float v = 0.4f * (float) std::sin (MathConstants<double>::twoPi * 220.0 * i / sr); s.setSample (0, i, v); s.setSample (1, i, v); }
            songFile.deleteFile();
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (songFile.createOutputStream().release(), sr, 2, 24, {}, 0));
            w->writeFromAudioSampleBuffer (s, 0, s.getNumSamples());
        }
        set (*p, "low", 12000.0f); set (*p, "high", 19000.0f); set (*p, "level", -40.0f); set (*p, "length", 1.0f);
        p->setPhoto (pic, "x");
        check (p->loadSongFile (songFile), "opens a sound (.wav)");
        p->waitForRender();
        const auto out = dir.getChildFile ("hidden.wav");
        check (p->saveWav (out), "saves the song with the photo hidden in it");
        AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (out));
        AudioBuffer<float> back (2, (int) r->lengthInSamples);
        r->read (&back, 0, (int) r->lengthInSamples, 0, true, true);
        double diff = 0.0;
        for (int i = 0; i < back.getNumSamples(); ++i)
            diff = jmax (diff, (double) std::abs (back.getSample (0, i) - 0.4f * (float) std::sin (MathConstants<double>::twoPi * 220.0 * i / sr)));
        const auto look = negative::analyse (back, sr, 12000.0f, 19000.0f, 320, 160);
        const double l = likeness (pic, look.getClippedImage ({ 0, 0, (int) (320 * 4.0 / 6.0), 160 }).rescaled (320, 160));
        check ((int) r->lengthInSamples == (int) (6.0 * sr) && diff < 0.02 && l > 0.6,
               "hidden: the song changes by at most " + String (20.0 * std::log10 (diff), 1) + " dB, and its spectrogram shows the photo (likeness " + String (l, 2) + ")");
        dir.deleteRecursively();
    }

    // the photo is saved in the project
    {
        auto p = make();
        p->setPhoto (pic, "my photo");
        set (*p, "mode", 2.0f);
        MemoryBlock m;
        p->getStateInformation (m);
        auto q = make();
        q->setStateInformation (m.getData(), (int) m.getSize());
        const auto img = q->getPhoto();
        check (img.isValid() && likeness (pic, img) > 0.95 && q->getPhotoName() == "my photo" && q->apvts.getRawParameterValue ("mode")->load() == 2.0f,
               "saved project: the photo, its name and the settings come back");
    }

    std::printf (failures == 0 ? "NEGATIVE CHECK PASSED\n" : "NEGATIVE CHECK FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
