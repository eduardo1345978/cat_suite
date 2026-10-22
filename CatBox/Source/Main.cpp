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

            // the hardware MIDI map: knob 1 (tempo) five clicks right, then the Instrument button
            const float tempo0 = box.getData().globals[GTempo];
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 20, 5));
            check (std::abs (box.getData().globals[GTempo] - (tempo0 + 5.0f)) < 0.01f, "knob CC 20 turns the tempo",
                   juce::String (box.getData().globals[GTempo]));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 20, 123));
            check (std::abs (box.getData().globals[GTempo] - tempo0) < 0.01f, "knob turns back (two's complement)", {});
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 106, 127));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 106, 0));
            check (box.getPage() == Arp, "Instrument button goes to the Arp page", juce::String (box.getPage()));

            // the arp: hold a chord with Hold on, the pads arrive as notes 36.. on channel 16
            box.buttonDown (BtnHold); box.buttonUp (BtnHold);
            for (int pad : { 0, 2, 4 })
            {
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (16, 36 + pad, (juce::uint8) 100));
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOff (16, 36 + pad));
            }

            // bass page: pad 6 = the chord on the 6th degree
            box.buttonDown (BtnInstrument); box.buttonUp (BtnInstrument);
            box.padDown (5, 100); box.padUp (5);
            check (box.getData().bassChordDegree == 5, "bass pad picks the chord", box.bassChordName());
            box.buttonDown (BtnInstrument); box.buttonUp (BtnInstrument);   // back to Bubny
            check (box.getPage() == Drums, "pages go round", {});

            box.buttonDown (BtnPlay); box.buttonUp (BtnPlay);

            // each instrument on its own must be heard
            for (int i = 0; i < NumInstruments; ++i)
            {
                for (int j = 0; j < NumInstruments; ++j) box.setMuted (j, j != i);
                const auto st = renderSeconds (audio, 4.0, nullptr);
                check (st.rms > 0.005f, instrumentName (i) + " plays", "rms " + juce::String (st.rms, 4));
                check (st.finite && st.peak <= 1.0f, instrumentName (i) + " stays clean", "peak " + juce::String (st.peak, 3));
            }
            for (int j = 0; j < NumInstruments; ++j) box.setMuted (j, false);

            // the whole box, written to the wav
            juce::AudioBuffer<float> song (2, (int) (sr * 16.0));
            const auto all = renderSeconds (audio, 16.0, &song);
            check (all.rms > 0.02f && all.finite && all.peak <= 1.0f, "all three together",
                   "rms " + juce::String (all.rms, 4) + " peak " + juce::String (all.peak, 3));
            writeWav (wavOut, song);

            // arp chance 0 -> the arp is silent (its knobs really reach the engine)
            box.setMuted (Drums, true); box.setMuted (Bass, true);
            box.buttonDown (BtnInstrument); box.buttonUp (BtnInstrument);   // Arp page
            box.turnKnob (slot::AProbability, -200.0f);
            renderSeconds (audio, 4.0, nullptr);   // let the last notes and echoes ring out
            const auto quiet = renderSeconds (audio, 2.0, nullptr);
            check (quiet.rms < 0.002f, "arp chance 0 % = silent", "rms " + juce::String (quiet.rms, 5));
            box.turnKnob (slot::AProbability, 200.0f);

            // presets: save to pad 5, change the sound, load pad 5 back
            box.savePreset (Arp, 4);
            check (box.presetFile (Arp, 4).existsAsFile(), "preset file written", box.presetFile (Arp, 4).getFullPathName());
            const float mode = box.getData().inst[Arp].values[slot::AMode];
            box.turnKnob (slot::AMode, 3.0f);
            check (box.getData().inst[Arp].values[slot::AMode] != mode, "knob changes the arp mode", {});
            check (box.loadPreset (Arp, 4) && box.getData().inst[Arp].values[slot::AMode] == mode, "preset loads back", {});
            check (! box.loadPreset (Arp, 9), "empty preset pad is refused", {});

            // Shift + pad loads, Shift + Rec + pad saves (the button way)
            box.buttonDown (BtnShift); box.buttonDown (BtnRec); box.buttonUp (BtnRec);
            box.padDown (11, 100); box.padUp (11);
            box.buttonUp (BtnShift);
            check (box.presetExists (Arp, 11), "Shift + Rec + pad saves a preset", {});

            // Rec on the drum page: a pad played while it runs is kept in the loop
            box.setMuted (Drums, false);
            box.buttonDown (BtnInstrument); box.buttonUp (BtnInstrument);   // Bass
            box.buttonDown (BtnInstrument); box.buttonUp (BtnInstrument);   // Bubny
            box.buttonDown (BtnRec); box.buttonUp (BtnRec);
            box.padDown (2, 110); box.padUp (2);
            renderSeconds (audio, 0.2, nullptr);
            box.pump();
            check (box.getData().inst[Drums].drumHits.size() == 1, "Rec keeps a played drum hit",
                   juce::String ((int) box.getData().inst[Drums].drumHits.size()));

            box.saveState();
        }
        {
            BoxController again (dir);
            check (again.getData().inst[Drums].drumHits.size() == 1 && again.getData().loadedPreset[Arp] == 11,
                   "the box remembers everything after a restart", {});
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
        o.dialogTitle = juce::String::fromUTF8 ("Cat Box: zvuková karta a MIDI");
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
