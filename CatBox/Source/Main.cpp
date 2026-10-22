// Cat Box: the Cat Suite groovebox (drums, arp, bass) - on the PC as an app, on the
// Raspberry Pi inside the box as the whole instrument.
//
//   Cat-Box                     window with the screen + a clickable front panel
//   Cat-Box --kiosk             only the 800 x 480 screen, full screen (the Pi in the box)
//   Cat-Box --data <folder>     where presets and the last state live
//   Cat-Box --selftest <out.wav>  plays everything offline, checks it, writes <out>.txt (exit code 0 = ok)

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "BoxController.h"
#include "BoxView.h"
#include <cmath>
#include <cstdio>

namespace catbox
{
//==============================================================================
/** Offline check of the whole box: the controller map, every instrument, Rec, presets, memory. */
class SelfTest
{
public:
    int run (const juce::File& wavOut)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("catbox-selftest-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
        {
            BoxController box (dir);
            auto& audio = box.getAudio();
            audio.prepare (sr, block);
            auto press = [&box] (int b) { box.buttonDown (b); box.buttonUp (b); };
            auto tap = [&box] (int pad) { box.padDown (pad, 100); box.padUp (pad); };
            auto shiftPress = [&box, &press] (int b) { box.buttonDown (BtnShift); press (b); box.buttonUp (BtnShift); };
            auto soloOnly = [&box] (int inst) { for (int j = 0; j < NumInstruments; ++j) box.setMuted (j, inst >= 0 && j != inst); };

            // the hardware MIDI map: knob 1 (tempo) five clicks right and back, then the Instr button
            const float tempo0 = box.getData().globals[GTempo];
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 20, 5));
            check (std::abs (box.getData().globals[GTempo] - (tempo0 + 5.0f)) < 0.01f, "knob CC 20 turns the tempo",
                   juce::String (box.getData().globals[GTempo]));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 20, 123));
            check (std::abs (box.getData().globals[GTempo] - tempo0) < 0.01f, "knob turns back (two's complement)", {});
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 106, 127));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 106, 0));
            check (box.getPage() == Arp, "Instr button goes to the Arp page", juce::String (box.getPage()));
            press (BtnInstrument); press (BtnInstrument);
            check (box.getPage() == PageFx, "the fourth page is FX", juce::String (box.getPage()));
            press (BtnInstrument);
            check (box.getPage() == Drums, "pages go round", {});

            // Steps is per page: Steps on the drums does not turn the Arp pads into steps
            press (BtnSteps);
            box.setPage (Arp);
            check (! box.isStepView(), "Steps on Drums leaves the Arp pads playing", {});

            // the arp sounds at once, even for a short tap while it runs
            box.setPage (Drums);
            press (BtnPlay);
            soloOnly (Arp);
            renderSeconds (audio, 0.31, nullptr);
            box.setPage (Arp);
            box.padDown (0, 100);
            const auto shortTap = renderSeconds (audio, 0.04, nullptr);
            box.padUp (0);
            renderSeconds (audio, 0.5, nullptr);
            check (shortTap.rms > 0.002f, "a short tap on an Arp pad sounds at once", "rms " + juce::String (shortTap.rms, 4));

            // Arp: Rec = Hold, then a chord on the pads (notes 36.. on channel 16)
            press (BtnRec);
            check (box.isLatched(), "Rec holds the arp", {});
            for (int pad : { 0, 2, 4 })
            {
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (16, 36 + pad, (juce::uint8) 100));
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOff (16, 36 + pad));
            }
            for (int snd : { arpSoundSupersaw, arpSoundAcid })
            {
                box.buttonDown (BtnShift);
                box.turnKnob (slot::ASound - numKnobs, (float) (snd - juce::roundToInt (box.getData().inst[Arp].values[slot::ASound])));
                box.buttonUp (BtnShift);
                const auto st = renderSeconds (audio, 2.0, nullptr);
                check (st.rms > 0.005f && st.finite && st.peak <= 1.0f, juce::String ("Arp sound ") + arpSoundNames()[snd] + " plays",
                       "rms " + juce::String (st.rms, 4));
            }
            box.buttonDown (BtnShift);
            box.turnKnob (slot::ASound - numKnobs, (float) (2 - juce::roundToInt (box.getData().inst[Arp].values[slot::ASound])));
            box.buttonUp (BtnShift);

            // Bass steps: tap an empty step, raise its note with knob 2, slide with knob 3
            box.setPage (Bass);
            press (BtnSteps);
            tap (1);
            const int n0 = box.getData().inst[Bass].bassSteps[1].note;
            check (n0 >= 0 && box.selectedBassStep() == 1, "tapping an empty bass step sets it", noteName (n0));
            box.turnKnob (1, 2.0f);
            const int n1 = box.getData().inst[Bass].bassSteps[1].note;
            check (n1 > n0, "knob 2 moves the step's note up the scale", noteName (n0) + " -> " + noteName (n1));
            box.turnKnob (2, 1.0f);
            check (box.getData().inst[Bass].bassSteps[1].slide, "knob 3 sets the slide", {});
            tap (1);
            check (box.getData().inst[Bass].bassSteps[1].note < 0, "tapping the picked step clears it", {});
            tap (1);
            press (BtnSteps);

            // Drums steps: the kick gets step 3, then an accent
            box.setPage (Drums);
            if (! box.isStepView()) press (BtnSteps);
            tap (2);
            check (box.getData().inst[Drums].drumSteps[0][2] == 100, "a drum step goes on", {});
            tap (2);
            check (box.getData().inst[Drums].drumSteps[0][2] == 127, "tapped again it is an accent", {});
            press (BtnSteps);

            // each instrument on its own must be heard
            for (int i = 0; i < NumInstruments; ++i)
            {
                soloOnly (i);
                const auto st = renderSeconds (audio, 4.0, nullptr);
                check (st.rms > 0.005f, instrumentName (i) + " plays", "rms " + juce::String (st.rms, 4));
                check (st.finite && st.peak <= 1.0f, instrumentName (i) + " stays clean", "peak " + juce::String (st.peak, 3));
            }
            soloOnly (-1);

            // the whole box, written to the wav
            juce::AudioBuffer<float> song (2, (int) (sr * 16.0));
            const auto all = renderSeconds (audio, 16.0, &song);
            check (all.rms > 0.02f && all.finite && all.peak <= 1.0f, "all three together",
                   "rms " + juce::String (all.rms, 4) + " peak " + juce::String (all.peak, 3));
            writeWav (wavOut, song);

            // FX page: every knob up, every performance pad held - all clean
            box.setPage (PageFx);
            for (int k = 1; k <= 10; ++k) box.turnKnob (k, 30.0f);
            const auto fxAll = renderSeconds (audio, 2.0, nullptr);
            check (fxAll.rms > 0.01f && fxAll.finite && fxAll.peak <= 1.0f, "every FX knob turned up stays clean",
                   "rms " + juce::String (fxAll.rms, 4) + " peak " + juce::String (fxAll.peak, 3));
            shiftPress (BtnSteps);   // Shift + Steps on the FX page = FX back to the start
            check (std::abs (box.getData().fx[slot::FDjFilter] - 0.5f) < 0.001f, "Shift + Steps resets the FX page", {});
            for (int p = 0; p < NumPerf; ++p)
            {
                box.padDown (p, 100);
                const auto st = renderSeconds (audio, 1.0, nullptr);
                box.padUp (p);
                renderSeconds (audio, 0.1, nullptr);
                const bool mayBeQuiet = p == PerfTapeStop || p == PerfKillDrums || p == PerfKillArp || p == PerfKillBass;
                check (st.finite && st.peak <= 1.0f && (mayBeQuiet || st.rms > 0.005f), juce::String ("pad ") + perfName (p),
                       "rms " + juce::String (st.rms, 4));
            }
            soloOnly (Drums);
            box.padDown (PerfKillDrums, 100);
            renderSeconds (audio, 0.2, nullptr);
            const auto killed = renderSeconds (audio, 1.0, nullptr);
            box.padUp (PerfKillDrums);
            check (killed.rms < 0.02f, "Kill drums silences the drums (the reverb tail may ring)", "rms " + juce::String (killed.rms, 4));
            soloOnly (-1);

            // the bass follows the arp: hold F (C minor) and the line moves up a fourth
            box.setPage (Bass);
            box.buttonDown (BtnShift);
            box.turnKnob (slot::BFollow - numKnobs, 1.0f);
            box.buttonUp (BtnShift);
            box.setPage (Arp);
            shiftPress (BtnSteps);   // release the held notes
            box.padDown (3, 100);    // degree 4 of C minor = F
            box.padUp (3);
            bool moved = false;
            for (int i = 0; i < 400 && ! moved; ++i)
            {
                renderSeconds (audio, 0.01, nullptr);
                moved = audio.bassNoteNow.load() == 41;   // C2 + 5
            }
            check (moved, "Follow arp moves the bass line to the arp's chord", {});

            // Chaos: new sound settings on the page, the pattern stays; Shift + Chaos takes them back
            box.setPage (Bass);
            const auto bassBefore = box.getData().inst[Bass];
            press (BtnChaos);
            check (box.getData().inst[Bass].values != bassBefore.values, "Chaos changes the bass sound", {});
            check (box.getData().inst[Bass].bassSteps == bassBefore.bassSteps, "Chaos keeps the bass line", {});
            check (box.getData().inst[Bass].values[slot::BVolume] == bassBefore.values[slot::BVolume]
                   && box.getData().globals[GTempo] == tempo0, "Chaos keeps volume and tempo", {});
            const auto chaosSound = renderSeconds (audio, 2.0, nullptr);
            check (chaosSound.finite && chaosSound.peak <= 1.0f, "the chaos sound stays clean", "peak " + juce::String (chaosSound.peak, 3));
            shiftPress (BtnChaos);
            check (box.getData().inst[Bass].values == bassBefore.values, "Shift + Chaos takes it back", {});
            box.setPage (PageFx);
            for (int i = 0; i < 20; ++i) { press (BtnChaos); renderSeconds (audio, 0.15, nullptr); }
            const auto fxChaos = renderSeconds (audio, 1.0, nullptr);
            check (fxChaos.finite && fxChaos.peak <= 1.0f && fxChaos.rms > 0.005f, "20 x Chaos on the FX page stays clean",
                   "rms " + juce::String (fxChaos.rms, 4));
            box.setPage (Arp);

            // presets: save to pad 5, change the sound, load pad 5 back
            box.savePreset (Arp, 4);
            check (box.presetFile (Arp, 4).existsAsFile(), "preset file written", {});
            const float mode = box.getData().inst[Arp].values[slot::AMode];
            box.turnKnob (slot::AMode, 3.0f);
            check (box.getData().inst[Arp].values[slot::AMode] != mode, "knob changes the arp mode", {});
            check (box.loadPreset (Arp, 4) && box.getData().inst[Arp].values[slot::AMode] == mode, "preset loads back", {});
            check (! box.loadPreset (Arp, 9), "empty preset pad is refused", {});
            box.buttonDown (BtnShift); press (BtnRec);
            tap (11);
            box.buttonUp (BtnShift);
            check (box.presetExists (Arp, 11), "Shift + Rec + pad saves a preset", {});
            check (box.isLatched(), "Shift + Rec does not touch Hold", {});

            // scenes: Scene, Rec + pad saves everything; change things; recall waits for the bar
            press (BtnScene);
            press (BtnRec);
            tap (6);
            check (box.sceneExists (6), "Scene + Rec + pad saves a scene", {});
            const auto drums0 = box.getData().inst[Drums].drumSteps;
            box.setPage (Drums);
            shiftPress (BtnSteps);   // clear all drums
            check (box.getData().inst[Drums].drumSteps != drums0, "the drums were cleared", {});
            tap (6);
            check (box.getData().inst[Drums].drumSteps == drums0 && box.getData().loadedScene == 6, "the scene comes back", {});
            check (audio.sceneWaiting.load(), "while playing it waits for the next bar", {});
            renderSeconds (audio, 2.3, nullptr);
            check (! audio.sceneWaiting.load(), "and then it plays", {});
            press (BtnScene);

            // Rec on the drum page: a pad played while it runs is written into its steps
            box.setPage (Drums);
            press (BtnRec);
            check (audio.isRecording(), "Rec arms recording", {});
            box.padDown (2, 115); box.padUp (2);
            renderSeconds (audio, 0.2, nullptr);
            box.pump();
            int clapSteps = 0;
            for (auto v : box.getData().inst[Drums].drumSteps[2]) if (v > 0) ++clapSteps;
            check (clapSteps == 1, "Rec writes a played drum into the steps", juce::String (clapSteps));

            // Rec on the bass page: two pads held together = a slide
            box.setPage (Bass);
            shiftPress (BtnSteps);   // clear the line
            box.padDown (0, 100);
            renderSeconds (audio, 0.3, nullptr);
            box.padDown (4, 120);
            renderSeconds (audio, 0.1, nullptr);
            box.padUp (0); box.padUp (4);
            box.pump();
            bool slid = false, accented = false;
            for (const auto& st : box.getData().inst[Bass].bassSteps)
            {
                slid = slid || st.slide;
                accented = accented || (st.note == box.bassPadNote (4) && st.accent);
            }
            check (slid && accented, "Rec writes the bass line (legato = slide, hard = accent)", {});
            box.setPage (Drums);
            press (BtnRec);

            box.saveState();
        }
        {
            BoxController again (dir);
            int clapSteps = 0;
            for (auto v : again.getData().inst[Drums].drumSteps[2]) if (v > 0) ++clapSteps;
            int bassNotes = 0;
            for (const auto& st : again.getData().inst[Bass].bassSteps) if (st.note >= 0) ++bassNotes;
            check (clapSteps == 1, "after a restart: the recorded drum", juce::String (clapSteps));
            check (again.getData().loadedScene == 6, "after a restart: the scene number", juce::String (again.getData().loadedScene));
            check (again.getData().inst[Drums].drumSteps[0][2] == 127, "after a restart: the drum steps", {});
            check (bassNotes == 2, "after a restart: the recorded bass line", juce::String (bassNotes));
        }
        dir.deleteRecursively();

        const auto report = wavOut.withFileExtension ("txt");
        report.replaceWithText (log.joinIntoString ("\n") + "\n");
        std::printf ("%s\n", log.joinIntoString ("\n").toRawUTF8());
        return failures == 0 ? 0 : 1;
    }

private:
    struct Stats { float rms = 0.0f, peak = 0.0f; bool finite = true; };

    Stats renderSeconds (BoxAudio& audio, double seconds, juce::AudioBuffer<float>* keep)
    {
        Stats st;
        const int total = (int) (sr * seconds);
        std::vector<float> l ((size_t) block), r ((size_t) block);
        double sum = 0.0;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            std::fill (l.begin(), l.end(), 0.0f);
            std::fill (r.begin(), r.end(), 0.0f);
            audio.render (l.data(), r.data(), n);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) st.finite = false;
                st.peak = juce::jmax (st.peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]));
                sum += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
            }
            if (keep != nullptr && pos + n <= keep->getNumSamples())
            {
                keep->copyFrom (0, pos, l.data(), n);
                keep->copyFrom (1, pos, r.data(), n);
            }
        }
        st.rms = (float) std::sqrt (sum / juce::jmax (1, 2 * total));
        return st;
    }

    void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
    {
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        if (out == nullptr) { check (false, "write " + f.getFullPathName(), {}); return; }
        auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
        if (w == nullptr) { check (false, "wav writer", {}); return; }
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    void check (bool ok, const juce::String& what, const juce::String& detail)
    {
        log.add ((ok ? "ok   " : "FAIL ") + what + (detail.isNotEmpty() ? "  (" + detail + ")" : juce::String()));
        if (! ok) ++failures;
    }

    const double sr = 48000.0;
    const int block = 256;
    juce::StringArray log;
    int failures = 0;
};

//==============================================================================
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (BoxController& box, juce::AudioDeviceManager& dm, bool kiosk)
        : DocumentWindow ("Cat Box", juce::Colour (0xff0b0d12), kiosk ? 0 : DocumentWindow::allButtons),
          deviceManager (dm)
    {
        auto* view = new BoxView (box, kiosk);
        view->onSettings = [this] { showSettings(); };
        view->onToggleFullScreen = [this] { setFullScreen (! isFullScreen()); };
        setContentOwned (view, false);

        const auto lb = view->logicalBounds();
        if (kiosk)
        {
            setTitleBarHeight (0);
            setResizable (false, false);
            setFullScreen (true);
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                setBounds (d->totalArea);
        }
        else
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setResizeLimits (400, 465, 1600, 1860);   // also creates the constrainer
            if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) lb.getWidth() / lb.getHeight());
            centreWithSize (juce::roundToInt (lb.getWidth() * 0.82), juce::roundToInt (lb.getHeight() * 0.82));
        }
        setVisible (true);
        view->grabKeyboardFocus();
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    void showSettings()
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 2, true, false, true, false);
        sel->setSize (520, 420);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.dialogTitle = "Cat Box: sound card and MIDI";
        o.dialogBackgroundColour = juce::Colour (0xff1b1e26);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

private:
    juce::AudioDeviceManager& deviceManager;
};

//==============================================================================
class CatBoxApplication : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "Cat-Box"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        const auto args = juce::StringArray::fromTokens (commandLine, true);
        if (const int at = args.indexOf ("--selftest"); at >= 0)
        {
            const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 1].unquoted().isNotEmpty()
                                                                                      ? args[at + 1].unquoted() : "catbox-selftest.wav");
            setApplicationReturnValue (SelfTest().run (out));
            quit();
            return;
        }

        const bool kiosk = args.contains ("--kiosk");
        juce::File dataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                 .getChildFile ("CatSuite").getChildFile ("CatBox");
        if (const int at = args.indexOf ("--data"); at >= 0 && args[at + 1].isNotEmpty())
            dataDir = juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 1].unquoted());
        dataDir.createDirectory();

        box = std::make_unique<BoxController> (dataDir);
        startAudio (dataDir);
        window = std::make_unique<MainWindow> (*box, deviceManager, kiosk);
        startTimer (2000);   // a controller plugged in later (the Pico) is switched on by itself
    }

    void shutdown() override
    {
        stopTimer();
        window = nullptr;
        if (box != nullptr)
        {
            if (auto xml = deviceManager.createStateXml())
                xml->writeTo (box->getDataDirectory().getChildFile ("audio-settings.xml"));
            deviceManager.removeAudioCallback (&box->getAudio());
            deviceManager.removeMidiInputDeviceCallback ({}, box.get());
            deviceManager.closeAudioDevice();
            box->saveState();
            box = nullptr;
        }
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    void startAudio (const juce::File& dataDir)
    {
        const auto settingsFile = dataDir.getChildFile ("audio-settings.xml");
        std::unique_ptr<juce::XmlElement> saved;
        if (settingsFile.existsAsFile()) saved = juce::XmlDocument::parse (settingsFile);
        deviceManager.initialise (0, 2, saved.get(), true);

        if (saved == nullptr)
        {
            // first start: prefer a HiFiBerry card and a short buffer (the Pi in the box)
            auto setup = deviceManager.getAudioDeviceSetup();
            if (auto* type = deviceManager.getCurrentDeviceTypeObject())
                for (const auto& name : type->getDeviceNames (false))
                    if (name.containsIgnoreCase ("hifiberry")) setup.outputDeviceName = name;
           #if JUCE_LINUX
            setup.bufferSize = 256;
           #endif
            deviceManager.setAudioDeviceSetup (setup, true);
        }

        deviceManager.addAudioCallback (&box->getAudio());
        enableMidiInputs();
        deviceManager.addMidiInputDeviceCallback ({}, box.get());
    }

    void enableMidiInputs()
    {
        // only devices seen for the first time: one the user switched off in the settings stays off
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (! seenMidi.contains (d.identifier))
            {
                seenMidi.add (d.identifier);
                deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
            }
    }

    void timerCallback() override { enableMidiInputs(); }

    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<BoxController> box;
    std::unique_ptr<MainWindow> window;
    juce::StringArray seenMidi;
};
} // namespace catbox

START_JUCE_APPLICATION (catbox::CatBoxApplication)
