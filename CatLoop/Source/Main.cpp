// Cat Loop: a 5-track live looper (a program, no DAW needed).
//   Cat-Loop                      the program
//   Cat-Loop --uistress <s> <seed> the window, random presses / knobs / keys / MIDI while a fake sound card plays noise
//   Cat-Loop --snapshot <png>      a picture of the window with a demo set (for the docs)
#include "LoopView.h"
#include <thread>

using namespace juce;

namespace looper
{
/** The window's content: Cat Loop's 1180 x 720 panel, scaled to the window. */
class Scaler : public Component
{
public:
    explicit Scaler (std::unique_ptr<LoopView> v) : view (std::move (v)) { addAndMakeVisible (*view); view->setSize (1180, 720); }
    void resized() override
    {
        const float s = jmin ((float) getWidth() / 1180.0f, (float) getHeight() / 720.0f);
        view->setTransform (AffineTransform::scale (s).translated (((float) getWidth() - 1180.0f * s) * 0.5f, ((float) getHeight() - 720.0f * s) * 0.5f));
    }
    void paint (Graphics& g) override { g.fillAll (Colour (0xff16181b)); }
    std::unique_ptr<LoopView> view;
};

/** The sound card <-> the engine, and MIDI -> the window. */
class Io : public AudioIODeviceCallback, public MidiInputCallback
{
public:
    explicit Io (Engine& e) : engine (e) {}
    Component::SafePointer<LoopView> view;

    void audioDeviceAboutToStart (AudioIODevice* d) override
    {
        engine.prepare (d->getCurrentSampleRate(), d->getCurrentBufferSizeSamples(), 60.0);
    }
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n, const AudioIODeviceCallbackContext&) override
    {
        const float* l = numIn > 0 ? in[0] : nullptr;
        const float* r = numIn > 1 ? in[1] : l;
        if (numOut <= 0) return;
        float* ol = out[0];
        float* orr = numOut > 1 ? out[1] : nullptr;
        if (orr == nullptr)
        {
            tmp.resize ((size_t) n);
            orr = tmp.data();
        }
        engine.process (l, r, ol, orr, n);
        if (numOut == 1) for (int i = 0; i < n; ++i) ol[i] = 0.5f * (ol[i] + orr[i]);
        for (int ch = 2; ch < numOut; ++ch) FloatVectorOperations::clear (out[ch], n);
    }
    void handleIncomingMidiMessage (MidiInput*, const MidiMessage& m) override
    {
        auto v = view;
        MessageManager::callAsync ([v, m] { if (v != nullptr) v->handleMidi (m); });
    }

private:
    Engine& engine;
    std::vector<float> tmp;
};

/** For the tests: plays the engine like a sound card would (noise in), on its own thread. */
class FakeCard
{
public:
    explicit FakeCard (Engine& e) : engine (e) { engine.prepare (48000.0, 480, 30.0); thread = std::thread ([this] { run(); }); }
    ~FakeCard() { stop = true; thread.join(); }
    std::atomic<bool> bad { false };
    std::atomic<float> peak { 0.0f };

private:
    void run()
    {
        std::vector<float> il (480), ir (480), ol (480), orr (480);
        Random r (1);
        while (! stop)
        {
            for (int i = 0; i < 480; ++i) { il[(size_t) i] = (r.nextFloat() - 0.5f) * 0.4f; ir[(size_t) i] = (r.nextFloat() - 0.5f) * 0.4f; }
            engine.process (il.data(), ir.data(), ol.data(), orr.data(), 480);
            for (int i = 0; i < 480; ++i)
            {
                if (! std::isfinite (ol[(size_t) i]) || ! std::isfinite (orr[(size_t) i])) bad = true;
                peak = jmax (peak.load(), std::abs (ol[(size_t) i]), std::abs (orr[(size_t) i]));
            }
            std::this_thread::sleep_for (std::chrono::milliseconds (5));     // ~2x real time
        }
    }
    Engine& engine;
    std::atomic<bool> stop { false };
    std::thread thread;
};

//==============================================================================
class App : public JUCEApplication
{
public:
    const String getApplicationName() override    { return "Cat Loop"; }
    const String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override    { return true; }

    void initialise (const String& line) override
    {
        Desktop::getInstance().setDefaultLookAndFeel (&laf);
        laf.setColour (PopupMenu::backgroundColourId, Colour (0xff1e2024));
        laf.setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3a3e45));
        laf.setColour (TooltipWindow::backgroundColourId, Colour (0xff0b0b0c));
        laf.setColour (TooltipWindow::textColourId, Colour (0xffffd6aa));
        laf.setColour (TooltipWindow::outlineColourId, Colour (0xff2b2e33));
        laf.setColour (AlertWindow::backgroundColourId, Colour (0xff1e2024));
        laf.setColour (ResizableWindow::backgroundColourId, Colour (0xff1b1d21));

        const auto args = StringArray::fromTokens (line, true);
        if (args.contains ("--uistress")) { uiStress (args); return; }
        if (args.contains ("--snapshot")) { snapshot (args); return; }

        PropertiesFile::Options o;
        o.applicationName = "Cat-Loop";
        o.folderName = "Cat Suite";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<PropertiesFile> (o);

        io = std::make_unique<Io> (engine);
        auto audioState = props->getXmlValue ("audio");
        devices.initialise (2, 2, audioState.get(), true);
        if (audioState == nullptr)
            for (auto& m : MidiInput::getAvailableDevices()) devices.setMidiInputDeviceEnabled (m.identifier, true);
        devices.addMidiInputDeviceCallback ({}, io.get());
        devices.addAudioCallback (io.get());

        auto view = std::make_unique<LoopView> (engine, &devices, props.get());
        io->view = view.get();
        window = std::make_unique<Window> (std::make_unique<Scaler> (std::move (view)));
    }

    void shutdown() override
    {
        if (props != nullptr)
        {
            if (auto xml = devices.createStateXml()) props->setValue ("audio", xml.get());
            props->saveIfNeeded();
        }
        devices.removeAudioCallback (io.get());
        devices.removeMidiInputDeviceCallback ({}, io.get());
        window.reset();
        stressView.reset();
        fake.reset();
        devices.closeAudioDevice();
        Desktop::getInstance().setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    class Window : public DocumentWindow
    {
    public:
        explicit Window (std::unique_ptr<Component> content)
            : DocumentWindow ("Cat Loop", Colour (0xff16181b), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content.release(), false);
            setResizable (true, true);
            setResizeLimits (708, 432, 2360, 1440);
            centreWithSize (1180, 720);
            setVisible (true);
            if (auto* s = dynamic_cast<Scaler*> (getContentComponent())) s->view->grabKeyboardFocus();
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    void uiStress (const StringArray& args)
    {
        const int at = args.indexOf ("--uistress");
        const double seconds = jmax (5.0, args[at + 1].getDoubleValue());
        const int seed = args[at + 2].getIntValue();
        fake = std::make_unique<FakeCard> (engine);
        stressView = std::make_unique<Scaler> (std::make_unique<LoopView> (engine, nullptr, nullptr));
        stressView->setSize (1180, 720);
        stressView->addToDesktop (ComponentPeer::windowHasTitleBar);
        stressView->setVisible (true);
        auto rnd = std::make_shared<Random> (seed);
        const double end = Time::getMillisecondCounterHiRes() + seconds * 1000.0;
        auto pokes = std::make_shared<int> (0);
        auto tick = std::make_shared<std::function<void()>>();
        *tick = [this, rnd, end, pokes, tick]
        {
            if (fake->bad.load()) { std::printf ("UI STRESS FAILED: NaN / inf in the output\n"); setApplicationReturnValue (1); quit(); return; }
            if (Time::getMillisecondCounterHiRes() > end)
            {
                const float pk = fake->peak.load();
                std::printf ("%s after %d pokes (peak %.2f)\n", pk <= 1.0f ? "UI STRESS OK" : "UI STRESS FAILED: over 0 dB", *pokes, pk);
                setApplicationReturnValue (pk <= 1.0f ? 0 : 1);
                quit();
                return;
            }
            for (int k = 0; k < 3; ++k) { stressView->view->randomPoke (*rnd); ++*pokes; }
            stressView->resized();
            Timer::callAfterDelay (15, *tick);
        };
        Timer::callAfterDelay (100, *tick);
    }

    void snapshot (const StringArray& args)
    {
        const File out (args[args.indexOf ("--snapshot") + 1]);
        fake = std::make_unique<FakeCard> (engine);
        // a demo set at 96 BPM: track 1 a 4-bar chord pad, 2 a 2-bar bass, 3 records, 4 an 8-bar texture (stopped), 5 empty
        engine.setBpm (96.0);
        const double bar = 48000.0 * 60.0 * 4.0 / 96.0;
        auto tone = [] (int len, std::initializer_list<double> hz, float amp, float wob)
        {
            AudioBuffer<float> b (2, len);
            for (int i = 0; i < len; ++i)
            {
                float v = 0.0f;
                for (double h : hz) v += (float) std::sin (MathConstants<double>::twoPi * h * i / 48000.0);
                v *= amp * (0.6f + 0.4f * (float) std::sin (i * wob));
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            return b;
        };
        engine.setLoop (0, tone ((int) (bar * 4), { 261.6, 311.1, 392.0 }, 0.08f, 0.0004f));
        engine.setLoop (1, tone ((int) (bar * 2), { 65.4, 130.8 }, 0.12f, 0.002f));
        engine.setLoop (3, tone ((int) (bar * 8), { 523.3, 784.0 }, 0.05f, 0.0001f));
        engine.volume (0) = 0.82f; engine.volume (1) = 0.7f; engine.volume (2) = 0.66f; engine.volume (3) = 0.58f; engine.volume (4) = 0.5f;
        engine.setBeatPattern (0, 3);
        engine.kit = 1;
        engine.setBeat (true);
        engine.click = (int) Click::countIn;
        engine.press (0);
        engine.press (1);
        stressView = std::make_unique<Scaler> (std::make_unique<LoopView> (engine, nullptr, nullptr));
        stressView->setSize (1180, 720);
        Timer::callAfterDelay (300, [this] { engine.press (1); engine.press (2); });
        Timer::callAfterDelay (5600, [this, out]
        {
            stressView->view->resized();
            auto img = stressView->view->createComponentSnapshot ({ 0, 0, 1180, 720 }, true, 1.5f);
            out.deleteFile();
            if (auto os = out.createOutputStream()) PNGImageFormat().writeImageToStream (img, *os);
            std::printf ("snapshot %s\n", out.getFullPathName().toRawUTF8());
            quit();
        });
    }

    LookAndFeel_V4 laf { LookAndFeel_V4::getMidnightColourScheme() };
    Engine engine;
    AudioDeviceManager devices;
    std::unique_ptr<PropertiesFile> props;
    std::unique_ptr<Io> io;
    std::unique_ptr<Window> window;
    std::unique_ptr<FakeCard> fake;
    std::unique_ptr<Scaler> stressView;
};
} // namespace looper

START_JUCE_APPLICATION (looper::App)
