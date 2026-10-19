// Cat Ocean: an endless pixel-art ocean for YouTube live streams. Viewers get their own fish
// through the chat; the fish have babies, mutate, and their genes steer a never-ending song.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "OceanEngine.h"
#include "OceanFilter.h"
#include "CatLookAndFeel.h"

using namespace ocean;

int runSelfTest (const juce::File& outDir);   // OceanTests.cpp

namespace
{
juce::File appFolder() { return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Cat Ocean"); }

/** The speakers: the engine makes 48 kHz, the sound card may want something else. */
class MonitorSource : public juce::AudioSource
{
public:
    explicit MonitorSource (Engine& e) : engine (e) {}
    void prepareToPlay (int block, double sr) override
    {
        inner.engine = &engine;
        resampler = std::make_unique<juce::ResamplingAudioSource> (&inner, false, 2);
        resampler->setResamplingRatio ((double) sampleRate / sr);
        resampler->prepareToPlay (block, sr);
    }
    void releaseResources() override { if (resampler) resampler->releaseResources(); }
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (resampler == nullptr || info.buffer->getNumChannels() < 2) { info.clearActiveBufferRegion(); return; }
        resampler->getNextAudioBlock (info);
    }
private:
    struct Inner : juce::AudioSource
    {
        Engine* engine = nullptr;
        void prepareToPlay (int, double) override {}
        void releaseResources() override {}
        void getNextAudioBlock (const juce::AudioSourceChannelInfo& i) override
        {
            if (engine == nullptr || i.buffer->getNumChannels() < 2) { i.clearActiveBufferRegion(); return; }
            engine->readMonitor (i.buffer->getWritePointer (0, i.startSample), i.buffer->getWritePointer (1, i.startSample), i.numSamples);
        }
    } inner;
    Engine& engine;
    std::unique_ptr<juce::ResamplingAudioSource> resampler;
};

//==============================================================================
class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent (Engine& e, juce::PropertiesFile& p) : engine (e), props (p), monitor (e)
    {
        setLookAndFeel (&lnf);
        auto label = [this] (juce::Label& l, const juce::String& text, float size = 14.0f, bool bold = false)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (cat::catFont (size, bold));
            l.setColour (juce::Label::textColourId, cat::Colours::text);
            addAndMakeVisible (l);
        };
        label (title, "Cat Ocean", 24.0f, true);
        label (keyLabel, "1.  YouTube stream key");
        label (keyHint, "YouTube Studio > Go live > Stream > copy the Stream key", 12.0f);
        keyHint.setColour (juce::Label::textColourId, cat::Colours::textDim);
        label (channelLabel, "2.  Your channel (@name) or the live video link");
        label (streamStatus, "", 13.0f);
        label (chatStatus, "", 13.0f);
        label (ffmpegStatus, "", 13.0f);
        label (hostLabel, "Chat as the host (try !fish Miro blue stripes):", 13.0f);
        label (volumeLabel, "Speakers", 13.0f);
        label (statsLabel, "", 13.0f);
        statsLabel.setJustificationType (juce::Justification::topLeft);
        for (auto* l : { &streamStatus, &chatStatus, &ffmpegStatus }) l->setJustificationType (juce::Justification::topLeft);

        keyEditor.setPasswordCharacter ((juce::juce_wchar) 0x2022);
        keyEditor.setText (props.getValue ("streamKey"), false);
        channelEditor.setText (props.getValue ("channel"), false);
        channelEditor.setTextToShowWhenEmpty ("@yourchannel", cat::Colours::textDim);
        hostEditor.setTextToShowWhenEmpty ("!fish Miro blue stripes", cat::Colours::textDim);
        for (auto* ed : { &keyEditor, &channelEditor, &hostEditor })
        {
            ed->setFont (cat::catFont (14.0f));
            addAndMakeVisible (ed);
        }
        hostEditor.onReturnKey = [this]
        {
            const auto text = hostEditor.getText().trim();
            if (text.isNotEmpty()) engine.say ("host", "Host", text);
            hostEditor.clear();
        };

        for (auto* b : { &liveButton, &stopButton, &chatButton, &testButton, &getFFmpegButton, &findFFmpegButton })
            addAndMakeVisible (b);
        liveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
        liveButton.onClick = [this] { goLive(); };
        stopButton.onClick = [this] { engine.stopLive(); engine.chat.stop(); };
        chatButton.onClick = [this] { save(); engine.chat.start (channelEditor.getText()); };
        testButton.onClick = [this] { recordTest(); };
        getFFmpegButton.onClick = [this] { downloadFFmpeg(); };
        findFFmpegButton.onClick = [this] { chooseFFmpeg(); };

        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (0.0, 1.0);
        volume.setValue (props.getDoubleValue ("volume", 0.7));
        volume.onValueChange = [this] { engine.monitorGain = (float) volume.getValue(); props.setValue ("volume", volume.getValue()); };
        engine.monitorGain = (float) volume.getValue();
        addAndMakeVisible (volume);

        if (devices.initialiseWithDefaultDevices (0, 2).isEmpty())
        {
            player.setSource (&monitor);
            devices.addAudioCallback (&player);
        }
        if (channelEditor.getText().isNotEmpty()) engine.chat.start (channelEditor.getText());

        setSize (1360, 640);
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        save();
        devices.removeAudioCallback (&player);
        player.setSource (nullptr);
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (cat::Colours::background);
        engine.copyFrame (frame);
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (frame, previewArea.toFloat());
        g.setColour (cat::Colours::textDim);
        g.setFont (cat::catFont (12.0f));
        g.drawText ("This is what the stream shows (1920 x 1080 on YouTube)", previewArea.withY (previewArea.getBottom() + 4).withHeight (18),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto side = r.removeFromRight (400);
        r.removeFromRight (12);
        // the biggest 16:9 picture that fits
        const int w = juce::jmin (r.getWidth(), (r.getHeight() - 24) * 16 / 9);
        previewArea = juce::Rectangle<int> (r.getX(), r.getY(), w, w * 9 / 16);

        title.setBounds (side.removeFromTop (34));
        keyLabel.setBounds (side.removeFromTop (22));
        keyEditor.setBounds (side.removeFromTop (28));
        keyHint.setBounds (side.removeFromTop (18));
        side.removeFromTop (6);
        channelLabel.setBounds (side.removeFromTop (22));
        channelEditor.setBounds (side.removeFromTop (28));
        side.removeFromTop (8);
        auto row = side.removeFromTop (34);
        liveButton.setBounds (row.removeFromLeft (130)); row.removeFromLeft (8);
        stopButton.setBounds (row.removeFromLeft (80)); row.removeFromLeft (8);
        chatButton.setBounds (row);
        side.removeFromTop (6);
        streamStatus.setBounds (side.removeFromTop (34));
        chatStatus.setBounds (side.removeFromTop (34));
        side.removeFromTop (4);
        testButton.setBounds (side.removeFromTop (30));
        side.removeFromTop (6);
        ffmpegStatus.setBounds (side.removeFromTop (34));
        row = side.removeFromTop (28);
        getFFmpegButton.setBounds (row.removeFromLeft (190)); row.removeFromLeft (8);
        findFFmpegButton.setBounds (row);
        side.removeFromTop (8);
        hostLabel.setBounds (side.removeFromTop (20));
        hostEditor.setBounds (side.removeFromTop (28));
        side.removeFromTop (6);
        row = side.removeFromTop (24);
        volumeLabel.setBounds (row.removeFromLeft (70));
        volume.setBounds (row);
        side.removeFromTop (6);
        statsLabel.setBounds (side);
    }

private:
    void timerCallback() override
    {
        repaint (previewArea);
        if (++ticks % 10 != 0) return;
        streamStatus.setText ("Stream: " + engine.streamer.getStatus(), juce::dontSendNotification);
        chatStatus.setText ("Chat: " + engine.chat.getStatus() + (engine.chat.getMessageCount() > 0 ? "  (" + juce::String (engine.chat.getMessageCount()) + " messages)" : juce::String()),
                            juce::dontSendNotification);
        const auto ff = Streamer::findFFmpeg (props.getValue ("ffmpeg"));
        if (downloading.isEmpty())
            ffmpegStatus.setText (ff.existsAsFile() ? "ffmpeg: ready (" + ff.getFullPathName() + ")" : "ffmpeg: missing - press \"Get ffmpeg\" (free, about 150 MB)",
                                  juce::dontSendNotification);
        else ffmpegStatus.setText (downloading, juce::dontSendNotification);
        statsLabel.setText (engine.stats(), juce::dontSendNotification);
    }

    void save()
    {
        props.setValue ("streamKey", keyEditor.getText().trim());
        props.setValue ("channel", channelEditor.getText().trim());
        props.saveIfNeeded();
    }

    void goLive()
    {
        save();
        const auto key = keyEditor.getText().trim();
        if (key.isEmpty()) { streamStatus.setText ("Stream: paste your stream key first (step 1)", juce::dontSendNotification); return; }
        const auto ff = Streamer::findFFmpeg (props.getValue ("ffmpeg"));
        if (! ff.existsAsFile()) { streamStatus.setText ("Stream: ffmpeg is missing - press \"Get ffmpeg\" first", juce::dontSendNotification); return; }
        engine.goLive (ff, "rtmp://a.rtmp.youtube.com/live2/" + key);
        if (channelEditor.getText().isNotEmpty() && ! engine.chat.isActive()) engine.chat.start (channelEditor.getText());
    }

    void recordTest()
    {
        const auto ff = Streamer::findFFmpeg (props.getValue ("ffmpeg"));
        if (! ff.existsAsFile()) { streamStatus.setText ("Stream: ffmpeg is missing - press \"Get ffmpeg\" first", juce::dontSendNotification); return; }
        const auto file = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getNonexistentChildFile ("Cat Ocean test", ".mp4");
        engine.goLive (ff, file.getFullPathName());
        juce::Timer::callAfterDelay (30000, [safe = juce::Component::SafePointer<MainComponent> (this), file]
        {
            if (safe == nullptr) return;
            safe->engine.stopLive();
            safe->streamStatus.setText ("Stream: test video saved on your Desktop: " + file.getFileName(), juce::dontSendNotification);
            file.revealToUser();
        });
        streamStatus.setText ("Stream: recording a 30 s test video to your Desktop ...", juce::dontSendNotification);
    }

    void chooseFFmpeg()
    {
        chooser = std::make_unique<juce::FileChooser> ("Where is ffmpeg.exe?", juce::File(), "ffmpeg*");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile()) { props.setValue ("ffmpeg", fc.getResult().getFullPathName()); props.saveIfNeeded(); }
                              });
    }

    void downloadFFmpeg()
    {
       #if JUCE_WINDOWS
        if (download != nullptr) return;
        const auto folder = Streamer::defaultFFmpegFolder();
        folder.createDirectory();
        const auto zip = folder.getChildFile ("ffmpeg-download.zip");
        downloading = "ffmpeg: downloading ...";
        struct Listener : juce::URL::DownloadTaskListener
        {
            MainComponent* owner;
            juce::File zip, folder;
            void progress (juce::URL::DownloadTask*, juce::int64 done, juce::int64 total) override
            {
                const auto text = "ffmpeg: downloading " + juce::String (done / 1048576) + (total > 0 ? " / " + juce::String (total / 1048576) : juce::String()) + " MB";
                juce::MessageManager::callAsync ([o = juce::Component::SafePointer<MainComponent> (owner), text] { if (o != nullptr) o->downloading = text; });
            }
            void finished (juce::URL::DownloadTask*, bool ok) override
            {
                bool unpacked = false;
                if (ok)
                {
                    juce::ZipFile z (zip);
                    for (int i = 0; i < z.getNumEntries(); ++i)
                        if (auto* e = z.getEntry (i); e != nullptr && e->filename.endsWithIgnoreCase ("bin/ffmpeg.exe"))
                        {
                            std::unique_ptr<juce::InputStream> in (z.createStreamForEntry (i));
                            juce::FileOutputStream out (folder.getChildFile ("ffmpeg.exe"));
                            if (in != nullptr && out.openedOk()) { out.setPosition (0); out.truncate(); out.writeFromInputStream (*in, -1); unpacked = true; }
                            break;
                        }
                }
                zip.deleteFile();
                juce::MessageManager::callAsync ([o = juce::Component::SafePointer<MainComponent> (owner), unpacked]
                {
                    if (o == nullptr) return;
                    o->downloading = unpacked ? juce::String() : "ffmpeg: the download failed - try again, or use \"Find ffmpeg\"";
                    o->download.reset();
                });
            }
        };
        listener = std::make_unique<Listener>();
        auto* l = static_cast<Listener*> (listener.get());
        l->owner = this; l->zip = zip; l->folder = folder;
        download = juce::URL ("https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip")
                       .downloadToFile (zip, juce::URL::DownloadTaskOptions().withListener (listener.get()));
        if (download == nullptr) downloading = "ffmpeg: couldn't start the download";
       #else
        downloading = "ffmpeg: install it with your package manager (it is found on the PATH)";
       #endif
    }

    Engine& engine;
    juce::PropertiesFile& props;
    cat::CatLookAndFeel lnf;
    juce::Image frame;
    juce::Rectangle<int> previewArea;
    juce::Label title, keyLabel, keyHint, channelLabel, streamStatus, chatStatus, ffmpegStatus, hostLabel, volumeLabel, statsLabel;
    juce::TextEditor keyEditor, channelEditor, hostEditor;
    juce::TextButton liveButton { "Go live" }, stopButton { "Stop" }, chatButton { "Connect chat only" },
                     testButton { "Record a 30 s test video (to check it works)" }, getFFmpegButton { "Get ffmpeg" }, findFFmpegButton { "Find ffmpeg..." };
    juce::Slider volume;
    juce::AudioDeviceManager devices;
    juce::AudioSourcePlayer player;
    MonitorSource monitor;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::URL::DownloadTask> download;
    std::unique_ptr<juce::URL::DownloadTaskListener> listener;
    juce::String downloading;
    int ticks = 0;
};

//==============================================================================
class OceanApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Cat Ocean"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }   // (the self test runs beside the app)

    void initialise (const juce::String& commandLine) override
    {
        juce::StringArray args;
        args.addTokens (commandLine, true);
        args.trim();
        args.removeEmptyStrings();
        for (auto& a : args) a = a.unquoted();
        if (args.contains ("--selftest"))
        {
            const int i = args.indexOf ("--selftest");
            const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (i + 1 < args.size() ? args[i + 1] : "ocean-test");
            const int failures = runSelfTest (out);
            setApplicationReturnValue (failures == 0 ? 0 : 1);
            quit();
            return;
        }

        juce::PropertiesFile::Options o;
        o.applicationName = "settings";
        o.filenameSuffix = ".xml";
        o.folderName = "Cat Ocean";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);

        engine = std::make_unique<Engine> (appFolder().getChildFile ("ocean.xml"));
        engine->start();
        window = std::make_unique<Window> (*engine, *props);
    }

    void shutdown() override
    {
        window.reset();
        engine.reset();   // saves the ocean
        props.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    struct Window : juce::DocumentWindow
    {
        Window (Engine& e, juce::PropertiesFile& p) : juce::DocumentWindow ("Cat Ocean", cat::Colours::background, allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (e, p), true);
            setResizable (true, true);
            setResizeLimits (900, 520, 4000, 3000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<juce::PropertiesFile> props;
    std::unique_ptr<Engine> engine;
    std::unique_ptr<Window> window;
};
} // namespace

START_JUCE_APPLICATION (OceanApp)
