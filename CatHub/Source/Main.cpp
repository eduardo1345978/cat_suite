// Cat Hub: one window that plays Chord-Cat, Bass-Cat, Drum-Cat and Life-Cat together.
//
//   Cat-Hub                                     normal app
//   Cat-Hub --selftest <catsFolder> <out.wav> [seconds]
//                                               renders a jam offline, writes the WAV and
//                                               <out.wav>.txt, exit code 0 = every Cat was heard
#include "HubWindow.h"
#include <set>

namespace hub
{
static int runSelfTest (const juce::StringArray& args)
{
    const int at = args.indexOf ("--selftest");
    const juce::File catsDir (args[at + 1]);
    const juce::File out (args[at + 2]);
    const double seconds = args.size() > at + 3 ? juce::jmax (4.0, args[at + 3].getDoubleValue()) : 16.0;
    juce::StringArray log;
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { log.add ((ok ? "ok    " : "FAIL  ") + what); if (! ok) ++failures; };

    HubEngine engine;
    const double sr = 48000.0;
    const int block = 480;   // not a power of two on purpose
    engine.prepare (sr, 512);
    const auto report = engine.loadCats ({ catsDir });
    if (report.isNotEmpty()) log.add (report);
    for (int id = 0; id < GlideCat; ++id) check (engine.hasCat (id), juce::String (catName (id)) + " loaded");

    engine.bpm = 100.0;
    engine.barsPerChord = 1;
    engine.setPlaying (true);

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
        // halfway: switch the chords to Glide-Cat if it is there
        if (pos < total / 2 && pos + block >= total / 2 && engine.glideAvailable())
            engine.parts[ChordPart].sound = engine.keysGlideIndex();
    }
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
        {
            const float v = audio.getSample (ch, i);
            if (! std::isfinite (v)) finite = false;
            peak = juce::jmax (peak, std::abs (v));
        }

    for (int p = 0; p < NumParts; ++p)
    {
        const double rms = std::sqrt (engine.parts[(size_t) p].energy.load() / (2.0 * total));
        check (rms > 1.0e-3, juce::String (partName (p)) + " is heard (rms " + juce::String (rms, 4) + ")");
    }
    check (finite, "no NaN / inf");
    check (peak <= 1.0f, "never clips (peak " + juce::String (peak, 3) + ")");
    padsSeen.erase (-1);
    check (padsSeen.size() >= 4, "the song stepped through " + juce::String ((int) padsSeen.size()) + " pads");

    // stop: everything should fade out (no hanging notes in the built-in synths)
    engine.setPlaying (false);
    juce::AudioBuffer<float> tail (2, (int) (4.0 * sr));
    for (int pos = 0; pos < tail.getNumSamples(); pos += block)
        engine.render (tail.getWritePointer (0, pos), tail.getWritePointer (1, pos), juce::jmin (block, tail.getNumSamples() - pos));
    engine.parts[LifePart].mute = true;   // Life-Cat keeps living when stopped; only check the others
    engine.parts[ChordPart].meter = engine.parts[BassPart].meter = engine.parts[DrumPart].meter = 0.0f;
    for (int pos = 0; pos < tail.getNumSamples(); pos += block)
        engine.render (tail.getWritePointer (0, pos), tail.getWritePointer (1, pos), juce::jmin (block, tail.getNumSamples() - pos));
    const float lastSecond = juce::jmax (tail.getMagnitude (0, tail.getNumSamples() - (int) sr, (int) sr), tail.getMagnitude (1, tail.getNumSamples() - (int) sr, (int) sr));
    check (lastSecond < 0.01f, "silent after Stop (" + juce::String (lastSecond, 4) + ")");

    // save + restore round trip
    const auto state = engine.saveState();
    engine.restoreState (state);
    check (state.getNumChildren() >= NumParts + 4, "state saves every Cat");

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
class HubApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Cat Hub"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        for (auto& a : args) a = a.unquoted();
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
            : DocumentWindow ("Cat Hub", cat::Colours::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new HubMain (e, dm, s), true);
            setResizable (true, false);
            setResizeLimits (900, 640, 3000, 2000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            if (auto* c = getContentComponent()) c->grabKeyboardFocus();
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<HubEngine> engine;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<MainWindow> window;
};
} // namespace hub

START_JUCE_APPLICATION (hub::HubApplication)
