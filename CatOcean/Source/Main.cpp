// Cat Ocean: an endless pixel-art ocean for YouTube live streams. Viewers get their own fish
// through the chat; the fish have babies, mutate, and their genes steer a never-ending song.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "OceanEngine.h"
#include "OceanFilter.h"
#include "CatLookAndFeel.h"
#include "BinaryData.h"

using namespace ocean;

int runSelfTest (const juce::File& outDir);   // OceanTests.cpp
int renderPlaces (const juce::File& outDir);  // OceanTests.cpp

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
/** Frutiger Aero: sky gradient, glossy rounded buttons, white glass text boxes. */
namespace aero
{
    const juce::Colour ink { 0xff0b2f55 }, inkDim { 0xff3d6a8f }, sky { 0xff7fd0ff }, sea { 0xffe6f8ff };

    class LookAndFeel : public cat::CatLookAndFeel
    {
    public:
        LookAndFeel()
        {
            setColour (juce::TextEditor::backgroundColourId, juce::Colours::white.withAlpha (0.85f));
            setColour (juce::TextEditor::textColourId, ink);
            setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff6fc3ec));
            setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff1e90e0));
            setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff9fdcff));
            setColour (juce::CaretComponent::caretColourId, ink);
            setColour (juce::Label::textColourId, ink);
            setColour (juce::Slider::thumbColourId, juce::Colour (0xff1e90e0));
            setColour (juce::Slider::trackColourId, juce::Colour (0xff7fd0ff));
            setColour (juce::Slider::backgroundColourId, juce::Colours::white.withAlpha (0.6f));
        }
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const float c = juce::jmin (12.0f, r.getHeight() * 0.5f);
            const bool red = base == juce::Colour (0xffc0392b);
            juce::Colour top = red ? juce::Colour (0xffff8a7a) : juce::Colour (0xffbfeaff), bottom = red ? juce::Colour (0xffc0392b) : juce::Colour (0xff2b96e0);
            if (over) { top = top.brighter (0.1f); bottom = bottom.brighter (0.15f); }
            if (down) { std::swap (top, bottom); }
            g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, c);
            // the glossy shine on the top half
            auto shine = r.reduced (3.0f, 2.0f).withHeight (r.getHeight() * 0.45f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.75f), shine.getX(), shine.getY(),
                                                     juce::Colours::white.withAlpha (0.1f), shine.getX(), shine.getBottom(), false));
            g.fillRoundedRectangle (shine, c * 0.8f);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (r, c, 1.2f);
        }
        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            g.setFont (cat::catFont (juce::jmin (15.0f, b.getHeight() * 0.48f), true));
            g.setColour (juce::Colours::white);
            const auto area = b.getLocalBounds().reduced (6, 2);
            g.setColour (juce::Colour (0x660b2f55));
            g.drawFittedText (b.getButtonText(), area.translated (0, 1), juce::Justification::centred, 1);
            g.setColour (juce::Colours::white);
            g.drawFittedText (b.getButtonText(), area, juce::Justification::centred, 1);
        }
    };
}

//==============================================================================
class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent (Engine& e, juce::PropertiesFile& p, bool resumeLive) : engine (e), props (p), monitor (e)
    {
        setLookAndFeel (&lnf);
        auto label = [this] (juce::Label& l, const juce::String& text, float size = 14.0f, bool bold = false)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (cat::catFont (size, bold));
            l.setColour (juce::Label::textColourId, juce::Colours::black);
            addAndMakeVisible (l);
        };
        label (title, "Cat Ocean", 24.0f, true);
        label (keyLabel, "1.  YouTube stream key");
        label (keyHint, "YouTube Studio > Go live > Stream > copy the Stream key", 12.0f);
        keyHint.setColour (juce::Label::textColourId, aero::inkDim);
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
        channelEditor.setTextToShowWhenEmpty ("@yourchannel", aero::inkDim);
        hostEditor.setTextToShowWhenEmpty ("!fish Miro blue stripes", aero::inkDim);
        for (auto* ed : { &keyEditor, &channelEditor, &hostEditor })
        {
            // black text on the white box (set here: text put in before the box joins the window would take the default white)
            ed->setColour (juce::TextEditor::textColourId, juce::Colours::black);
            ed->setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
            ed->setColour (juce::CaretComponent::caretColourId, juce::Colours::black);
            ed->setFont (cat::catFont (14.0f));
            ed->applyFontToAllText (cat::catFont (14.0f));
            ed->applyColourToAllText (juce::Colours::black);
            addAndMakeVisible (ed);
        }
        // the admin console
        consoleLog.setMultiLine (true);
        consoleLog.setReadOnly (true);
        consoleLog.setScrollbarsShown (true);
        consoleLog.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
        consoleLog.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xdd0b2f55));
        consoleLog.setColour (juce::TextEditor::textColourId, juce::Colour (0xffd8f6ff));
        consoleInput.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain)));
        consoleInput.setColour (juce::TextEditor::textColourId, juce::Colours::black);
        consoleInput.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        consoleInput.setColour (juce::CaretComponent::caretColourId, juce::Colours::black);
        consoleInput.setTextToShowWhenEmpty ("/help   /stats   /time night   /place lagoon   /give @name 50   !fish Miro", aero::inkDim);
        consoleInput.onReturnKey = [this]
        {
            const auto t = consoleInput.getText().trim();
            if (t.isNotEmpty()) { engine.console (t.startsWithChar ('!') || t.startsWithChar ('/') ? t : "/" + t); history.add (t); historyPos = history.size(); }
            consoleInput.clear();
        };
        addAndMakeVisible (consoleLog);
        addAndMakeVisible (consoleInput);
        label (consoleLabel, "Admin console  (type /help - chat commands like !fish work too)", 13.0f, true);
        engine.console ("/help");

        if (resumeLive && props.getBoolValue ("wasLive", false))
            juce::Timer::callAfterDelay (3000, [safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe != nullptr) safe->goLive(); });

        // one-click pickers: music, place, time of day
        for (int st = 0; st < NumStyles; ++st) musicBox.addItem (styleName (st), st + 1);
        for (int pl = 0; pl < NumPlaces; ++pl) placeBox.addItem (placeName (pl), pl + 1);
        timeBox.addItemList ({ "Real clock", "Day", "Sunset", "Night", "Sunrise" }, 1);
        lookBox.addItemList ({ "Smooth (rounded edges)", "Pixel (sharp blocks)" }, 1);
        lookBox.setSelectedId (props.getBoolValue ("smoothLook", true) ? 1 : 2, juce::dontSendNotification);
        lookBox.setTooltip ("How the stream draws the pixels: smooth rounds the edges (needs a bit more CPU), pixel keeps sharp blocks."
                            " A change shows here now and on YouTube the next time you go live.");
        engine.streamer.smooth = lookBox.getSelectedId() == 1;
        lookBox.onChange = [this]
        {
            const bool smooth = lookBox.getSelectedId() == 1;
            engine.streamer.smooth = smooth;
            props.setValue ("smoothLook", smooth);
            props.saveIfNeeded();
            repaint (previewArea);
        };
        timeBox.setSelectedId (1, juce::dontSendNotification);
        musicBox.onChange = [this] { if (! updatingPickers) engine.console ("/music " + juce::String (styleInfo (musicBox.getSelectedId() - 1).word)); };
        placeBox.onChange = [this]
        {
            if (updatingPickers) return;
            static const char* words[NumPlaces] = { "reef", "kelp", "crystal", "ship", "cave", "ice", "lagoon", "ruins", "volcano", "jelly", "open",
                                                    "city", "trench", "castle", "sky" };
            engine.console (juce::String ("/place ") + words[juce::jlimit (0, NumPlaces - 1, placeBox.getSelectedId() - 1)]);
        };
        timeBox.onChange = [this]
        {
            static const char* words[] = { "auto", "day", "sunset", "night", "sunrise" };
            engine.console (juce::String ("/time ") + words[juce::jlimit (0, 4, timeBox.getSelectedId() - 1)]);
        };
        for (auto* b : { &musicBox, &placeBox, &timeBox, &lookBox }) addAndMakeVisible (b);
        label (lookLabel, "Look", 13.0f, true);
        label (musicLabel, "Music", 13.0f, true);
        label (placeLabel, "Place", 13.0f, true);
        label (timeLabel, "Time", 13.0f, true);

        hostEditor.onReturnKey = [this]
        {
            const auto text = hostEditor.getText().trim();
            if (text.isNotEmpty()) engine.say ("host", "Host", text, true);   // the host may use the mod commands too
            hostEditor.clear();
        };

        descriptionButton.setTooltip ("Copies a description for your stream (all the commands for viewers and moderators). Paste it into YouTube Studio.");
        descriptionButton.onClick = [this]
        {
            juce::SystemClipboard::copyTextToClipboard (juce::String::fromUTF8 (BinaryData::CatOceanYouTubeDescription_txt,
                                                                                BinaryData::CatOceanYouTubeDescription_txtSize));
            descriptionButton.setButtonText ("Copied! Paste it in YouTube Studio");
            juce::Timer::callAfterDelay (3000, [safe = juce::Component::SafePointer<MainComponent> (this)]
                                         { if (safe != nullptr) safe->descriptionButton.setButtonText ("Copy the YouTube description (all commands)"); });
        };
        for (auto* b : { &liveButton, &stopButton, &chatButton, &testButton, &getFFmpegButton, &findFFmpegButton, &descriptionButton })
            addAndMakeVisible (b);
        liveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));
        liveButton.onClick = [this] { goLive(); };
        stopButton.onClick = [this] { engine.stopLive(); engine.chat.stop(); props.setValue ("wasLive", false); props.saveIfNeeded(); };
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
        reverb.setSliderStyle (juce::Slider::LinearHorizontal);
        reverb.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        reverb.setRange (0.0, 1.0);
        reverb.setValue (props.getDoubleValue ("reverb", 0.35));
        reverb.setTooltip ("The underwater space: a big soft reverb over the music and the fish (on the stream too). Console: /reverb 0-100");
        reverb.onValueChange = [this] { engine.setReverb ((float) reverb.getValue()); props.setValue ("reverb", reverb.getValue()); };
        engine.setReverb ((float) reverb.getValue());
        addAndMakeVisible (reverb);
        label (reverbLabel, "Reverb", 13.0f);

        if (devices.initialiseWithDefaultDevices (0, 2).isEmpty())
        {
            player.setSource (&monitor);
            devices.addAudioCallback (&player);
        }
        if (channelEditor.getText().isNotEmpty()) engine.chat.start (channelEditor.getText());

        setSize (1400, 860);
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
        // a bright sky to sea gradient with a soft glow (Frutiger Aero)
        g.setGradientFill (juce::ColourGradient (aero::sky, 0.0f, 0.0f, aero::sea, 0.0f, (float) getHeight(), false));
        g.fillAll();
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse ((float) getWidth() - 360.0f, -180.0f, 520.0f, 360.0f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawRect (previewArea.expanded (2), 2);
        engine.copyFrame (frame);
        if (lookBox.getSelectedId() == 1 && frame.isValid())
        {
            // smooth: rounded pixel edges (Scale2x), then a soft scale to the preview
            scale2x (frame, smoothFrame);
            g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
            g.drawImage (smoothFrame, previewArea.toFloat());
        }
        else
        {
            g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
            g.drawImage (frame, previewArea.toFloat());
        }
        g.setColour (aero::inkDim);
        g.setFont (cat::catFont (12.0f));
        g.drawText ("This is what the stream shows (1920 x 1080 on YouTube)", previewArea.withY (previewArea.getBottom() + 4).withHeight (18),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto side = r.removeFromRight (400);
        r.removeFromRight (12);
        // the console under the picture, the biggest 16:9 picture that fits above it
        auto consoleArea = r.removeFromBottom (juce::jlimit (130, 260, r.getHeight() / 4));
        consoleLabel.setBounds (consoleArea.removeFromTop (20));
        consoleInput.setBounds (consoleArea.removeFromBottom (26));
        consoleArea.removeFromBottom (3);
        consoleLog.setBounds (consoleArea);
        r.removeFromBottom (6);
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
        streamStatus.setBounds (side.removeFromTop (64));
        chatStatus.setBounds (side.removeFromTop (34));
        side.removeFromTop (4);
        testButton.setBounds (side.removeFromTop (30));
        side.removeFromTop (4);
        descriptionButton.setBounds (side.removeFromTop (30));
        side.removeFromTop (6);
        ffmpegStatus.setBounds (side.removeFromTop (34));
        row = side.removeFromTop (28);
        getFFmpegButton.setBounds (row.removeFromLeft (190)); row.removeFromLeft (8);
        findFFmpegButton.setBounds (row);
        side.removeFromTop (8);
        auto pickRow = [&] (juce::Label& l, juce::ComboBox& b)
        {
            auto row2 = side.removeFromTop (28);
            l.setBounds (row2.removeFromLeft (60));
            b.setBounds (row2);
            side.removeFromTop (4);
        };
        pickRow (musicLabel, musicBox);
        pickRow (placeLabel, placeBox);
        pickRow (timeLabel, timeBox);
        pickRow (lookLabel, lookBox);
        side.removeFromTop (4);
        hostLabel.setBounds (side.removeFromTop (20));
        hostEditor.setBounds (side.removeFromTop (28));
        side.removeFromTop (6);
        row = side.removeFromTop (24);
        volumeLabel.setBounds (row.removeFromLeft (70));
        volume.setBounds (row);
        side.removeFromTop (4);
        row = side.removeFromTop (24);
        reverbLabel.setBounds (row.removeFromLeft (70));
        reverb.setBounds (row);
        side.removeFromTop (6);
        statsLabel.setBounds (side);
    }

private:
    void timerCallback() override
    {
        repaint (previewArea);
        // the pickers follow votes and the console
        updatingPickers = true;
        if (musicBox.getSelectedId() != engine.currentStyle.load() + 1) musicBox.setSelectedId (engine.currentStyle.load() + 1, juce::dontSendNotification);
        if (placeBox.getSelectedId() != engine.currentPlace.load() + 1) placeBox.setSelectedId (engine.currentPlace.load() + 1, juce::dontSendNotification);
        updatingPickers = false;
        if (engine.consoleVersion() != shownConsole)
        {
            shownConsole = engine.consoleVersion();
            consoleLog.setText (engine.consoleText(), false);
            consoleLog.moveCaretToEnd();
        }
        // health: if the ocean stopped ticking for 20 s, start a fresh copy of the app (it carries on live) and quit
        if (engine.lastFrameMs() > 0.0 && juce::Time::getMillisecondCounterHiRes() - engine.lastFrameMs() > 20000.0 && ! restarting)
        {
            restarting = true;
            props.saveIfNeeded();
            juce::ChildProcess fresh;
            if (fresh.start (juce::StringArray { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName(), "--resume" }, 0))
                std::_Exit (3);
        }
        if (++ticks % 10 != 0) return;
        {
            juce::String st = "Stream: " + engine.streamer.getStatus();
            // sending for 2 minutes, but YouTube still shows no live video on the channel: the usual YouTube-side reasons
            const bool sending = engine.streamer.getStatus().startsWith ("LIVE");
            if (sending && engine.streamer.sentFrames() > 30 * 120 && engine.chat.getStatus().contains ("not live yet"))
                st << "\nYouTube gets the video but isn't showing it: in YouTube Studio > Go live > Stream, set Dual stream to Off or Auto,"
                      " and paste the stream key shown on THAT page (a key from another stream goes nowhere).";
            streamStatus.setText (st, juce::dontSendNotification);
        }
        chatStatus.setText ("Chat: " + engine.chat.getStatus() + (engine.chat.getMessageCount() > 0 ? "  (" + juce::String (engine.chat.getMessageCount()) + " messages)" : juce::String()),
                            juce::dontSendNotification);
        const auto ff = Streamer::findFFmpeg (props.getValue ("ffmpeg"));
        if (downloading.isEmpty())
            ffmpegStatus.setText (ff.existsAsFile() ? "ffmpeg: ready (" + ff.getFullPathName() + ")" : "ffmpeg: missing - press \"Get ffmpeg\" (free, about 150 MB)",
                                  juce::dontSendNotification);
        else ffmpegStatus.setText (downloading, juce::dontSendNotification);
        statsLabel.setText (engine.stats(), juce::dontSendNotification);
    }

    /** Scale2x (EPX): doubles a pixel picture and rounds its diagonal edges - close to what the stream's xBR does. */
    static void scale2x (const juce::Image& src, juce::Image& dst)
    {
        const int w = src.getWidth(), h = src.getHeight();
        if (! dst.isValid() || dst.getWidth() != w * 2 || dst.getHeight() != h * 2) dst = juce::Image (juce::Image::ARGB, w * 2, h * 2, false);
        const juce::Image::BitmapData in (src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData out (dst, juce::Image::BitmapData::writeOnly);
        auto px = [&] (int x, int y) { x = juce::jlimit (0, w - 1, x); y = juce::jlimit (0, h - 1, y);
                                       return *reinterpret_cast<const juce::uint32*> (in.getPixelPointer (x, y)); };
        for (int y = 0; y < h; ++y)
        {
            auto* o0 = reinterpret_cast<juce::uint32*> (out.getLinePointer (y * 2));
            auto* o1 = reinterpret_cast<juce::uint32*> (out.getLinePointer (y * 2 + 1));
            for (int x = 0; x < w; ++x)
            {
                const auto p = px (x, y), a = px (x, y - 1), b = px (x + 1, y), c = px (x - 1, y), d = px (x, y + 1);
                juce::uint32 e0 = p, e1 = p, e2 = p, e3 = p;
                if (c == a && c != d && a != b) e0 = a;
                if (a == b && a != c && b != d) e1 = b;
                if (d == c && d != b && c != a) e2 = c;
                if (b == d && b != a && d != c) e3 = d;
                o0[x * 2] = e0; o0[x * 2 + 1] = e1;
                o1[x * 2] = e2; o1[x * 2 + 1] = e3;
            }
        }
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
        const auto key = keyEditor.getText().removeCharacters (" \t\r\n");
        if (key.isEmpty()) { streamStatus.setText ("Stream: paste your stream key first (step 1)", juce::dontSendNotification); return; }
        if (key.startsWithIgnoreCase ("rtmp") || key.containsChar ('/'))
        {
            streamStatus.setText ("Stream: paste only the Stream key (like abcd-efgh-ijkl-mnop-qrst), not the Stream URL", juce::dontSendNotification);
            return;
        }
        keyEditor.setText (key, false);
        const auto ff = Streamer::findFFmpeg (props.getValue ("ffmpeg"));
        if (! ff.existsAsFile()) { streamStatus.setText ("Stream: ffmpeg is missing - press \"Get ffmpeg\" first", juce::dontSendNotification); return; }
        engine.goLive (ff, "rtmp://a.rtmp.youtube.com/live2/" + key);
        props.setValue ("wasLive", true);
        props.saveIfNeeded();
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
    aero::LookAndFeel lnf;
    juce::Image frame, smoothFrame;
    juce::Rectangle<int> previewArea;
    juce::Label title, keyLabel, keyHint, channelLabel, streamStatus, chatStatus, ffmpegStatus, hostLabel, volumeLabel, statsLabel;
    juce::TextEditor keyEditor, channelEditor, hostEditor;
    juce::TextButton liveButton { "Go live" }, stopButton { "Stop" }, chatButton { "Connect chat only" },
                     testButton { "Record a 30 s test video (to check it works)" },
                     descriptionButton { "Copy the YouTube description (all commands)" }, getFFmpegButton { "Get ffmpeg" }, findFFmpegButton { "Find ffmpeg..." };
    juce::Slider volume, reverb;
    juce::AudioDeviceManager devices;
    juce::AudioSourcePlayer player;
    MonitorSource monitor;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::URL::DownloadTask> download;
    std::unique_ptr<juce::URL::DownloadTaskListener> listener;
    juce::String downloading;
    int ticks = 0, shownConsole = -1, historyPos = 0;
    bool restarting = false;
    juce::Label consoleLabel, musicLabel, placeLabel, timeLabel, lookLabel, reverbLabel;
    juce::ComboBox musicBox, placeBox, timeBox, lookBox;
    bool updatingPickers = false;
    juce::TextEditor consoleLog, consoleInput;
    juce::StringArray history;
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
        if (args.contains ("--places"))
        {
            const int i = args.indexOf ("--places");
            renderPlaces (juce::File::getCurrentWorkingDirectory().getChildFile (i + 1 < args.size() ? args[i + 1] : "ocean-places"));
            quit();
            return;
        }
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
        // "--resume": started again by the health check (or after a crash): go live again if it was live
        window = std::make_unique<Window> (*engine, *props, args.contains ("--resume"));
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
        Window (Engine& e, juce::PropertiesFile& p, bool resume) : juce::DocumentWindow ("Cat Ocean", aero::sky, allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (e, p, resume), true);
            setResizable (true, true);
            setResizeLimits (1000, 640, 4000, 3000);
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
