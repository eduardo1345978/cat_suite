// Cat Hub: one window that plays Chord-Cat, Bass-Cat, Drum-Cat and Life-Cat together.
//
//   Cat-Hub                                     normal app
//   Cat-Hub --selftest <catsFolder> <out.wav> [seconds]
//                                               renders a jam offline, writes the WAV and
//                                               <out.wav>.txt, exit code 0 = every Cat was heard
//   Cat-Hub --snapshot <catsFolder> <out.png>   pictures of the window (tutorial, simple, advanced,
//                                               settings, every theme)
//   Cat-Hub --uistress <catsFolder> <seconds> [seed]
//                                               the real window, random clicks / keys / tabs /
//                                               play / record / save + load while audio runs
#include "HubWindow.h"
#include <set>
#include <thread>

namespace hub
{
static int runSelfTest (const juce::StringArray& args)
{
    const int at = args.indexOf ("--selftest");
    const juce::File catsDir (args[at + 1]);
    const juce::File out (args[at + 2]);
    const double seconds = args.size() > at + 3 ? juce::jmax (12.0, args[at + 3].getDoubleValue()) : 16.0;
    juce::StringArray log;
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { log.add ((ok ? "ok    " : "FAIL  ") + what); if (! ok) ++failures; };

    HubEngine engine;
    const double sr = 48000.0;
    const int block = 480;   // not a power of two on purpose
    engine.prepare (sr, 512);
    const auto report = engine.loadCats ({ catsDir });
    if (report.isNotEmpty()) log.add (report);
    for (int id = 0; id < PattCat; ++id) check (engine.hasCat (id), juce::String (catName (id)) + " loaded");

    // Extra parts with every other Cat: MIDI effects and sound Cats
    struct Extra { int src, fx, sound; };
    const Extra extras[] = { { SrcChords, FxPatt, SndGlide }, { SrcLead, FxOrbit, SndSing }, { SrcLead, FxBend, SndTalk }, { SrcChords, FxNone, SndEPiano } };
    for (const auto& x : extras)
    {
        const bool fxOk = catForMidiFx (x.fx) < 0 || engine.catAvailable (catForMidiFx (x.fx));
        const bool sndOk = catForSound (x.sound) < 0 || engine.catAvailable (catForSound (x.sound));
        if (! fxOk || ! sndOk) { log.add ("(skipped a part: " + juce::String (midiFxName (x.fx)) + " / " + soundName (x.sound) + " not found)"); continue; }
        const bool added = engine.addTrack (x.src, x.fx, x.sound);
        auto& t = engine.track (engine.getNumTracks() - 1);
        check (added && t.midiFx.load() == x.fx && t.sound.load() == x.sound,
               "added part: " + juce::String (sourceName (x.src)) + " > " + midiFxName (x.fx) + " > " + soundName (x.sound)
               + (engine.lastLoadError.isNotEmpty() ? "  (" + engine.lastLoadError + ")" : juce::String()));
        engine.lastLoadError = {};
        t.filter = 0.3f; t.chorus = 0.4f; t.delay = 0.3f; t.drive = 0.2f; t.crush = 0.1f; t.volume = 0.6f;
    }

    engine.bpm = 100.0;
    engine.barsPerChord = 1;
    engine.setPlaying (true);
    const double life0 = engine.getLifeTime();

    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> audio (2, total);
    std::set<int> padsSeen;
    bool finite = true;
    float peak = 0.0f;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        engine.render (audio.getWritePointer (0, pos), audio.getWritePointer (1, pos), n);
        padsSeen.insert (engine.getCurrentPad());
    }
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
        {
            const float v = audio.getSample (ch, i);
            if (! std::isfinite (v)) finite = false;
            peak = juce::jmax (peak, std::abs (v));
        }

    for (int i = 0; i < engine.getNumTracks(); ++i)
    {
        auto& t = engine.track (i);
        const double rms = std::sqrt (t.energy.load() / (2.0 * total));
        check (rms > 1.0e-3, "part " + juce::String (i + 1) + " (" + sourceName (t.source.load()) + " > " + soundName (t.sound.load())
                             + ") is heard (rms " + juce::String (rms, 4) + ")");
    }
    const double life1 = engine.getLifeTime();
    check (life1 > life0 + seconds * 0.5, "Life-Cat's world is moving (" + juce::String (life0, 1) + " s -> " + juce::String (life1, 1) + " s)");
    check (finite, "no NaN / inf");
    check (peak <= 1.0f, "never clips (peak " + juce::String (peak, 3) + ")");
    padsSeen.erase (-1);
    check (padsSeen.size() >= 4, "the song stepped through " + juce::String ((int) padsSeen.size()) + " pads");

    // every style loads and plays
    for (int st = 0; st < HubEngine::styleNames().size(); ++st)
    {
        engine.applyStyle (st);
        for (int i = 0; i < engine.getNumTracks(); ++i) engine.track (i).energy = 0.0;
        juce::AudioBuffer<float> b (2, (int) (4.0 * sr));
        for (int pos = 0; pos < b.getNumSamples(); pos += block)
            engine.render (b.getWritePointer (0, pos), b.getWritePointer (1, pos), juce::jmin (block, b.getNumSamples() - pos));
        int heard = 0;
        for (int i = 0; i < engine.getNumTracks(); ++i) heard += engine.track (i).energy.load() > 1.0e-3 ? 1 : 0;
        const float pk = juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
        check (heard >= 3 && pk <= 1.0f && pk > 0.01f, "style " + HubEngine::styleNames()[st] + ": " + juce::String (heard) + " of "
                                                        + juce::String (engine.getNumTracks()) + " parts heard, peak " + juce::String (pk, 2));
    }
    juce::Random dice (3);
    for (int k = 0; k < 4; ++k) { engine.roll (dice); juce::AudioBuffer<float> b (2, 48000); engine.render (b.getWritePointer (0), b.getWritePointer (1), 48000); }
    check (true, "dice rolled 4 times");

    // stop: everything should fade out (no hanging notes in the built-in synths)
    engine.applyStyle (0);
    engine.setPlaying (true);
    { juce::AudioBuffer<float> b (2, 96000); engine.render (b.getWritePointer (0), b.getWritePointer (1), 96000); }
    engine.setPlaying (false);
    for (int i = 0; i < engine.getNumTracks(); ++i) if (engine.track (i).source.load() == SrcLife) engine.track (i).mute = true;   // Life lives on
    juce::AudioBuffer<float> tail (2, (int) (5.0 * sr));
    for (int pos = 0; pos < tail.getNumSamples(); pos += block)
        engine.render (tail.getWritePointer (0, pos), tail.getWritePointer (1, pos), juce::jmin (block, tail.getNumSamples() - pos));
    const float lastSecond = juce::jmax (tail.getMagnitude (0, tail.getNumSamples() - (int) sr, (int) sr), tail.getMagnitude (1, tail.getNumSamples() - (int) sr, (int) sr));
    check (lastSecond < 0.01f, "silent after Stop (" + juce::String (lastSecond, 4) + ")");

    // save + restore round trip
    const auto state = engine.saveState();
    const int parts = engine.getNumTracks();
    engine.restoreState (state);
    check (engine.getNumTracks() == parts && state.getNumChildren() >= parts + 4, "state saves every part and Cat");

    if (out != juce::File())
    {
        out.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (out.createOutputStream().release());
        if (os != nullptr)
            if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (16)))
                w->writeFromAudioSampleBuffer (audio, 0, total);
        log.add ("wrote " + out.getFullPathName());
    }
    log.add (failures == 0 ? "CAT HUB SELF TEST PASSED" : "CAT HUB SELF TEST FAILED (" + juce::String (failures) + ")");
    const auto text = log.joinIntoString ("\n");
    std::cout << text << std::endl;
    if (out != juce::File()) out.withFileExtension ("txt").replaceWithText (text);
    engine.releaseCats();
    return failures == 0 ? 0 : 1;
}

//==============================================================================
/** Pokes the real Hub window at random while another thread renders audio. */
class UiStress : private juce::Timer
{
public:
    UiStress (const juce::File& catsDir, double secondsToRun, int seed)
        : seconds (secondsToRun), rng (seed)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Cat Hub stress";
        o.filenameSuffix = "settings";
        o.folderName = "Cat Suite";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
        engine.prepare (48000.0, 512);
        const auto report = engine.loadCats ({ catsDir });
        std::cout << "cats: " << (report.isEmpty() ? juce::String ("all loaded") : report) << std::endl;
        audio = std::thread ([this]
        {
            juce::Random r (7);
            std::vector<float> l (1024), rr (1024);
            while (! stop)
            {
                const int n = 1 + r.nextInt (1024);
                engine.render (l.data(), rr.data(), n);
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (l[(size_t) i]) || std::abs (l[(size_t) i]) > 1.0f) { std::cout << "!! bad sample " << l[(size_t) i] << std::endl; std::abort(); }
                std::this_thread::sleep_for (std::chrono::milliseconds (r.nextInt (3)));
            }
        });
        openWindow();
        start = juce::Time::getMillisecondCounterHiRes();
        startTimer (15);
    }

    ~UiStress() override
    {
        stopTimer();
        engine.stopRecording();
        main.reset();
        stop = true;
        audio.join();
        engine.releaseCats();
    }

    int failures = 0;

private:
    void openWindow()
    {
        main = std::make_unique<HubMain> (engine, deviceManager, *settings);
        main->setTopLeftPosition (0, 0);
        main->addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar);
        main->setVisible (true);
        if (rng.nextBool()) main->finishTutorialForTests();   // sometimes click through the tutorial too
    }

    static void collect (juce::Component& c, juce::Array<juce::Component*>& out)
    {
        for (auto* ch : c.getChildren())
            if (ch->isVisible()) { out.add (ch); collect (*ch, out); }
    }

    void timerCallback() override
    {
        if (juce::Time::getMillisecondCounterHiRes() - start > seconds * 1000.0)
        {
            stopTimer();
            std::cout << "UI STRESS OK after " << actions << " actions" << std::endl;
            juce::JUCEApplication::getInstance()->setApplicationReturnValue (failures == 0 ? 0 : 1);
            juce::JUCEApplication::quit();
            return;
        }
        ++actions;
        const int what = rng.nextInt (100);
        if (what < 3)  { engine.setPlaying (! engine.isPlaying()); return; }
        if (what < 5)  { main.reset(); openWindow(); return; }                                  // close / reopen the window
        if (what < 7)  { const auto t = engine.saveState(); main->restoreStateSafely (t); return; }  // save + load while playing
        if (what < 9)  { const int st = rng.nextInt (HubEngine::styleNames().size()); main->changeCats ([this, st] { engine.applyStyle (st); }); return; }
        if (what < 10) { main->changeCats ([this] { engine.roll (rng); }); return; }
        if (what < 8)  { if (engine.isRecording()) engine.stopRecording(); else engine.startRecording (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cathub-stress.wav")); return; }
        if (what < 12) { juce::PopupMenu::dismissAllActiveMenus(); return; }
        if (what < 18)
        {
            const int pad = rng.nextInt (9);
            main->keyPressed (pad == 8 ? juce::KeyPress (juce::KeyPress::spaceKey) : juce::KeyPress ('1' + pad, juce::ModifierKeys(), (juce::juce_wchar) ('1' + pad)));
            engine.setLivePad (rng.nextBool() ? -1 : pad);
            return;
        }

        juce::Array<juce::Component*> comps;
        collect (*main, comps);
        if (comps.isEmpty()) return;
        auto* c = comps[rng.nextInt (comps.size())];
        if (auto* b = dynamic_cast<juce::Button*> (c))
        {
            const auto t = b->getButtonText();
            if (t.startsWith ("Audio") || t.startsWith ("Find") || t.startsWith ("Rec") || t.startsWith ("Stop rec")) return;  // dialogs / files
            if (t == "Test" || t.startsWith ("Control Panel") || t.startsWith ("Reset")) return;    // sound card dialogs
            b->triggerClick();
            return;
        }
        if (auto* cb = dynamic_cast<juce::ComboBox*> (c))
        {
            if (cb->getNumItems() > 0) cb->setSelectedItemIndex (rng.nextInt (cb->getNumItems()), juce::sendNotificationSync);
            return;
        }
        if (auto* sl = dynamic_cast<juce::Slider*> (c))
        {
            sl->setValue (sl->getMinimum() + rng.nextDouble() * (sl->getMaximum() - sl->getMinimum()), juce::sendNotificationSync);
            return;
        }
        // a click / drag on whatever it is (pads, cards, tab bar, the Cats' windows)
        const auto r = c->getLocalBounds();
        if (r.isEmpty()) return;
        auto pt = [&] { return juce::Point<float> ((float) rng.nextInt (juce::jmax (1, r.getWidth())), (float) rng.nextInt (juce::jmax (1, r.getHeight()))); };
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto now = juce::Time::getCurrentTime();
        const auto down = pt();
        auto ev = [&] (juce::Point<float> pos, juce::ModifierKeys m, bool dragged)
        { return juce::MouseEvent (src, pos, m, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, c, now, down, now, 1, dragged); };
        // like a real click: a component behind a modal window (e.g. the open LINK pop-up)
        // does not get it; the modal window is told instead (and usually closes)
        if (c->isCurrentlyBlockedByAnotherModalComponent())
        {
            if (auto* m = juce::Component::getCurrentlyModalComponent()) m->inputAttemptWhenModal();
            return;
        }
        juce::Component::SafePointer<juce::Component> safe (c);
        c->mouseDown (ev (down, juce::ModifierKeys::leftButtonModifier, false));
        for (int k = rng.nextInt (4); k > 0 && safe; --k) c->mouseDrag (ev (pt(), juce::ModifierKeys::leftButtonModifier, true));
        if (safe) c->mouseUp (ev (pt(), juce::ModifierKeys(), false));
    }

    HubEngine engine;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<HubMain> main;
    std::thread audio;
    std::atomic<bool> stop { false };
    const double seconds;
    juce::Random rng;
    double start = 0.0;
    int actions = 0;
};

//==============================================================================
class HubApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Cat Hub"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        for (auto& a : args) a = a.unquoted();
        if (args.contains ("--snapshot"))
        {
            // --snapshot <catsFolder> <out.png>: pictures of every tab (for checking the layout)
            const int at = args.indexOf ("--snapshot");
            juce::PropertiesFile::Options o;
            o.applicationName = "Cat Hub snapshot"; o.filenameSuffix = "settings"; o.folderName = "Cat Suite";
            juce::PropertiesFile props (o);
            HubEngine e;
            e.prepare (48000.0, 512);
            e.loadCats ({ juce::File (args[at + 1]) });
            e.setPlaying (true);
            juce::AudioDeviceManager dm;
            {
                HubMain m (e, dm, props);
                m.setSize (1280, 820);
                const juce::File out (args[at + 2]);
                std::vector<float> l (512), r (512);
                int shot = 0;
                auto snap = [&] (const juce::String& name)
                {
                    for (int i = 0; i < 20; ++i) e.render (l.data(), r.data(), 512);
                    for (int k = 0; k < 3; ++k) m.tickForSnapshot();
                    auto img = m.createComponentSnapshot (m.getLocalBounds());
                    juce::PNGImageFormat png;
                    auto f = out.getSiblingFile (out.getFileNameWithoutExtension() + "_" + juce::String (shot++) + "_" + name + ".png");
                    f.deleteFile();
                    if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
                };
                for (int i = 0; i < 200; ++i) e.render (l.data(), r.data(), 512);   // meters + pads moving
                m.startTutorial();
                snap ("tutorial");
                m.finishTutorialForTests();
                m.setAdvanced (false);
                snap ("simple");
                m.changeCats ([&e] { e.applyStyle (4); });
                m.setAdvanced (true);
                snap ("advanced");
                m.selectTab (1);
                snap ("settings");
                m.selectTab (0);
                for (int th = 1; th < (int) themes().size(); ++th) { m.applyTheme (th); snap ("theme" + juce::String (th)); }
                m.applyTheme (0);
            }
            e.releaseCats();
            quit();
            return;
        }
        if (args.contains ("--uistress"))
        {
            const int at = args.indexOf ("--uistress");
            uiStress = std::make_unique<UiStress> (juce::File (args[at + 1]), juce::jmax (5.0, args[at + 2].getDoubleValue()),
                                                   args.size() > at + 3 ? args[at + 3].getIntValue() : 1);
            return;
        }
        if (args.contains ("--selftest"))
        {
            setApplicationReturnValue (runSelfTest (args));
            quit();
            return;
        }

        juce::PropertiesFile::Options o;
        o.applicationName = "Cat Hub";
        o.filenameSuffix = "settings";
        o.folderName = "Cat Suite";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);

        engine = std::make_unique<HubEngine>();
        engine->prepare (48000.0, 512);
        engine->loadCats (HubEngine::defaultSearchFolders (juce::File (settings->getValue ("catsFolder"))));
        if (auto xml = settings->getXmlValue ("hub")) engine->restoreState (juce::ValueTree::fromXml (*xml));

        const auto audioState = settings->getXmlValue ("audio");
        deviceManager.initialise (0, 2, audioState.get(), true);
        deviceManager.addAudioCallback (engine.get());

        window = std::make_unique<MainWindow> (*engine, deviceManager, *settings);
    }

    void shutdown() override
    {
        uiStress.reset();
        if (engine == nullptr) return;
        if (auto xml = engine->saveState().createXml()) settings->setValue ("hub", xml.get());
        if (auto xml = deviceManager.createStateXml()) settings->setValue ("audio", xml.get());
        settings->saveIfNeeded();
        window.reset();                                   // Cat windows first
        deviceManager.removeAudioCallback (engine.get());
        deviceManager.closeAudioDevice();
        engine.reset();                                   // then the Cats
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (HubEngine& e, juce::AudioDeviceManager& dm, juce::PropertiesFile& s)
            : DocumentWindow ("Cat Hub", theme().bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new HubMain (e, dm, s), true);
            setResizable (true, false);
            setResizeLimits (1000, 680, 4000, 3000);
            // fit the screen, then centre it
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                const auto area = d->userArea;
                centreWithSize (juce::jmin (getWidth(), area.getWidth() - 40), juce::jmin (getHeight(), area.getHeight() - 60));
            }
            else centreWithSize (getWidth(), getHeight());
            setVisible (true);
            if (auto* c = getContentComponent()) c->grabKeyboardFocus();
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<HubEngine> engine;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<UiStress> uiStress;
};
} // namespace hub

START_JUCE_APPLICATION (hub::HubApplication)
