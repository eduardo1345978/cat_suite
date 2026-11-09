#include "PluginEditor.h"
#include "CatTheme.h"

using namespace juce;

namespace
{
constexpr int designW = 1000, designH = 620;
const Colour bg (0xff0b0b0c), ink (0xffe8e6e1), faint (0x26ffffff);
Font uiFont (float h) { return cat::catFont (h); }
}

namespace negui
{
//==============================================================================
Line::Line (AudioProcessorValueTreeState& s, const String& id, const String& cap)
    : Slider (Slider::LinearHorizontal, Slider::NoTextBox), caption (cap), attachment (s, id, *this)
{
    setSliderSnapsToMousePosition (false);
    setDoubleClickReturnValue (true, (double) s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
}

void Line::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setFont (uiFont (11.5f));
    g.setColour (ink.withAlpha (0.5f));
    g.drawText (caption, b.withHeight (14.0f), Justification::centredLeft);
    const float y = 24.0f, w = b.getWidth() - 10.0f, p = (float) valueToProportionOfLength (getValue());
    g.setColour (faint);
    g.fillRect (5.0f, y, w, 2.0f);
    g.setColour (ink);
    g.fillRect (5.0f, y, w * p, 2.0f);
    g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre ({ 5.0f + w * p, y + 1.0f }));
    g.setFont (uiFont (13.5f));
    g.drawText (getTextFromValue (getValue()), b.withTop (36.0f).withHeight (18.0f), Justification::centredLeft);
}

//==============================================================================
void Chip::paintButton (Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float r = b.getHeight() * 0.5f;
    const bool on = getToggleState() || primary;
    if (on)
    {
        g.setColour (ink.withAlpha (down ? 0.75f : (over ? 0.95f : 0.88f)));
        g.fillRoundedRectangle (b, r);
    }
    else
    {
        g.setColour (Colours::white.withAlpha (down ? 0.12f : (over ? 0.07f : 0.0f)));
        g.fillRoundedRectangle (b, r);
        g.setColour (Colours::white.withAlpha (0.26f));
        g.drawRoundedRectangle (b, r, 1.0f);
    }
    g.setColour (on ? bg : ink.withAlpha (isEnabled() ? 0.9f : 0.35f));
    g.setFont (uiFont (12.5f));
    g.drawText (getButtonText(), b, Justification::centred);
}

//==============================================================================
PhotoView::PhotoView (NegativeProcessor& p) : processor (p) { refresh(); }

void PhotoView::refresh() { shown = processor.getPhoto(); repaint(); }

Rectangle<float> PhotoView::imageArea() const
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (! shown.isValid()) return b;
    return RectanglePlacement (RectanglePlacement::centred).appliedTo (shown.getBounds().toFloat(), b);
}

void PhotoView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colours::black);
    g.fillRect (b);
    if (shown.isValid())
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (shown, imageArea());
    }
    g.setColour (drawing ? ink : Colours::white.withAlpha (0.18f));
    g.drawRect (b, 1.0f);
    if (drawing)
    {
        g.setFont (uiFont (11.0f));
        g.setColour (ink.withAlpha (0.7f));
        g.drawText ("draw: drag  ·  erase: right-click or shift", b.reduced (8.0f).removeFromBottom (14.0f), Justification::centredLeft);
    }
}

void PhotoView::paintAt (Point<float> pos, bool erase)
{
    if (! shown.isValid()) return;
    const auto a = imageArea();
    const float sx = (float) shown.getWidth() / a.getWidth();
    const Point<float> p ((pos.x - a.getX()) * sx, (pos.y - a.getY()) * sx);
    const Point<float> q ((last.x - a.getX()) * sx, (last.y - a.getY()) * sx);
    const float r = jmax (2.0f, (float) shown.getHeight() * 0.02f);
    Graphics g (shown);
    g.setColour (erase ? Colours::black : Colours::white);
    g.drawLine (juce::Line<float> (q, p), r * 2.0f);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
    last = pos;
    repaint();
}

void PhotoView::mouseDown (const MouseEvent& e)
{
    if (! drawing) return;
    shown = shown.createCopy();
    stroking = true;
    last = e.position;
    paintAt (e.position, e.mods.isRightButtonDown() || e.mods.isShiftDown());
}

void PhotoView::mouseDrag (const MouseEvent& e)
{
    if (stroking) paintAt (e.position, e.mods.isRightButtonDown() || e.mods.isShiftDown());
}

void PhotoView::mouseUp (const MouseEvent&)
{
    if (! stroking) return;
    stroking = false;
    processor.setPhoto (shown, processor.getPhotoName().startsWith ("drawing") ? processor.getPhotoName() : "drawing");
}

//==============================================================================
SpectrumView::SpectrumView (NegativeProcessor& p) : processor (p) {}

void SpectrumView::refresh() { pic = processor.getSpectrogram(); repaint(); }

void SpectrumView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto area = b.withTrimmedLeft (44.0f).withTrimmedBottom (20.0f);
    g.setColour (Colours::black);
    g.fillRect (area);
    if (pic.isValid())
    {
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        g.drawImage (pic, area);
    }
    // Hz on the left (the picture is log frequency, low*0.8 .. high*1.1)
    const float lowHz = processor.apvts.getRawParameterValue ("low")->load() * 0.8f;
    const float highHz = jmin ((float) processor.renderSampleRate() * 0.5f, processor.apvts.getRawParameterValue ("high")->load() * 1.1f);
    g.setFont (uiFont (10.5f));
    g.setColour (ink.withAlpha (0.45f));
    for (float hz : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        if (hz < lowHz || hz > highHz) continue;
        const float y = area.getY() + (1.0f - std::log (hz / lowHz) / std::log (highHz / lowHz)) * area.getHeight();
        g.drawText (hz >= 1000.0f ? String (roundToInt (hz / 1000.0f)) + "k" : String (roundToInt (hz)), Rectangle<float> (0.0f, y - 7.0f, 38.0f, 14.0f), Justification::centredRight);
        g.fillRect (area.getX() - 4.0f, y, 3.0f, 1.0f);
    }
    // bars along the bottom
    const int bars = (int) negative::lengthBars (roundToInt (processor.apvts.getRawParameterValue ("length")->load()));
    const int step = bars > 8 ? 2 : 1;
    for (int i = 0; i <= bars; i += step)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) bars;
        g.drawText (String (i + 1), Rectangle<float> (x - 20.0f, area.getBottom() + 4.0f, 40.0f, 14.0f),
                    i == 0 ? Justification::centredLeft : (i == bars ? Justification::centredRight : Justification::centred));
    }
    const float pp = processor.playPos.load();
    if (pp >= 0.0f && pp <= 1.0f)
    {
        g.setColour (Colours::white);
        g.fillRect (area.getX() + area.getWidth() * pp, area.getY(), 1.5f, area.getHeight());
    }
    if (processor.isRendering())
    {
        g.setColour (ink.withAlpha (0.6f));
        g.drawText ("making the sound ...", area.reduced (10.0f).removeFromTop (14.0f), Justification::centredRight);
    }
}

//==============================================================================
void DragChip::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::white.withAlpha (isMouseOver() ? 0.6f : 0.35f));
    const float dash[] = { 3.0f, 3.0f };
    Path p;
    p.addRoundedRectangle (b, b.getHeight() * 0.5f);
    Path d;
    PathStrokeType (1.0f).createDashedStroke (d, p, dash, 2);
    g.fillPath (d);
    g.setColour (ink.withAlpha (0.9f));
    g.setFont (uiFont (12.5f));
    g.drawText (busy ? "..." : String::fromUTF8 ("\xe2\x87\xb2 drag .wav"), b, Justification::centred);
}

void DragChip::mouseDrag (const MouseEvent& e)
{
    if (busy || e.getDistanceFromDragStart() < 6) return;
    busy = true;
    repaint();
    const auto f = processor.writeTempWav();
    busy = false;
    repaint();
    if (f.existsAsFile()) DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}
} // namespace negui

//==============================================================================
NegativeEditor::NegativeEditor (NegativeProcessor& p) : processor (p)
{
    for (int i = 0; i < modes.size(); ++i)
    {
        auto* t = modeTabs.add (new negui::Chip (modes[i]));
        t->setClickingTogglesState (false);
        t->onClick = [this, i] { if (auto* prm = processor.apvts.getParameter ("mode")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); };
        addAndMakeVisible (t);
    }
    modeTabs[0]->setTooltip ("EXACT: every pixel is the loudness of one frequency - the photo comes out sharp in a spectrogram (it sounds like air)");
    modeTabs[1]->setTooltip ("MUSICAL: the photo plays notes of a key - bright spots are notes, bright areas chords");
    modeTabs[2]->setTooltip ("TEXTURE: the photo as a soft pad");
    addAndMakeVisible (key);
    addAndMakeVisible (scale);
    addAndMakeVisible (photo);
    addAndMakeVisible (spectrum);
    const std::tuple<const char*, const char*, const char*> list[] = {
        { "low", "LOW", "The lowest frequency the photo uses (its bottom edge)" },
        { "high", "HIGH", "The highest frequency (its top edge). Put LOW and HIGH up high (e.g. 12 - 18 kHz) to hide the photo" },
        { "length", "LENGTH", "How long the photo plays (in bars, at Cubase's tempo)" },
        { "contrast", "CONTRAST", "More contrast: the dark parts go quiet, only the bright ones sound" },
        { "soft", "SOFT", "Blurs the photo: smoother, less hiss" },
        { "level", "LEVEL", "How loud the photo is added to the track (low = hidden: you only see it in a spectrogram)" } };
    for (auto& [id, cap, tip] : list)
    {
        auto l = std::make_unique<negui::Line> (processor.apvts, id, cap);
        l->setTooltip (tip);
        addAndMakeVisible (*l);
        lines.push_back (std::move (l));
    }
    openPhotoB.onClick = [this] { openPhoto(); };
    openSoundB.onClick = [this] { openSound(); };
    openPhotoB.setTooltip ("Open a photo (jpg, png, gif) - or drop one on the window");
    openSoundB.setTooltip ("Open a sound (wav, mp3, aiff, flac): the photo is hidden in it (PLAY and Save .wav)");
    drawB.setClickingTogglesState (true);
    drawB.onClick = [this] { photo.drawing = drawB.getToggleState(); clearB.setVisible (photo.drawing); photo.repaint(); };
    drawB.setTooltip ("Draw on the photo with the mouse (white = sound, right-click = erase)");
    clearB.onClick = [this]
    {
        Image black (Image::RGB, 800, 300, true);
        processor.setPhoto (black, "drawing");
        photo.refresh();
    };
    clearB.setTooltip ("A black page to draw on");
    addChildComponent (clearB);
    songX.onClick = [this] { processor.clearSong(); };
    songX.setTooltip ("Take the sound out");
    addChildComponent (songX);
    invertB.setClickingTogglesState (true);
    invertB.setTooltip ("Play the photo's negative (dark = loud)");
    syncB.setClickingTogglesState (true);
    syncB.setTooltip ("Play the photo in time with Cubase, again every LENGTH (MIDI notes play it too: C4 = as it is)");
    playB.setClickingTogglesState (true);
    playB.onClick = [this] { processor.previewing = playB.getToggleState(); playB.setButtonText (playB.getToggleState() ? "Stop" : "Play"); };
    saveWavB.onClick = [this] { saveWav(); };
    savePicB.onClick = [this] { savePicture(); };
    saveWavB.setTooltip ("Save the sound (with the opened sound, if there is one) as a .wav");
    savePicB.setTooltip ("Save the spectrogram as a picture (.png)");
    dragChip.setTooltip ("Drag the sound straight onto a Cubase track");
    for (auto* c : std::initializer_list<Component*> { &openPhotoB, &openSoundB, &drawB, &invertB, &playB, &saveWavB, &savePicB, &dragChip })
        addAndMakeVisible (c);
    if (! processor.isStandalone()) addAndMakeVisible (syncB);
    setSize (designW, designH);
    startTimerHz (30);
}

NegativeEditor::~NegativeEditor()
{
    stopTimer();
    processor.previewing = false;
}

void NegativeEditor::paint (Graphics& g)
{
    g.fillAll (bg);
    g.setColour (ink);
    g.setFont (uiFont (14.0f).boldened().withExtraKerningFactor (0.5f));
    g.drawText ("NEGATIVE", 32, 22, 160, 22, Justification::centredLeft);
    g.setFont (uiFont (12.0f));
    g.setColour (ink.withAlpha (0.45f));
    g.drawText ("a photo you can hear", 32, 42, 200, 16, Justification::centredLeft);

    // under the photo: its name, and the sound it hides in
    auto cap = String (processor.getPhotoName());
    const auto song = processor.getSongName();
    if (song.isNotEmpty()) cap << "  in  " << song;
    g.drawText (cap.isEmpty() ? "drop a photo or a sound here" : cap, 32, 290, 300, 16, Justification::centredLeft);
    if (status.isNotEmpty())
    {
        g.setColour (ink.withAlpha (0.7f));
        g.drawText (status, getWidth() - 432, 290, 400, 16, Justification::centredRight);
    }
}

void NegativeEditor::resized()
{
    // the top bar
    int x = 260;
    for (auto* t : modeTabs) { t->setBounds (x, 24, 84, 30); x += 90; }
    key.setBounds (x + 10, 26, 74, 26);
    scale.setBounds (x + 90, 26, 120, 26);
    // the photo and the sliders
    photo.setBounds (32, 76, 300, 206);
    for (size_t i = 0; i < lines.size(); ++i)
        lines[i]->setBounds (368 + (int) (i % 3) * 206, 80 + (int) (i / 3) * 70, 186, 56);
    int bx = 368;
    for (auto* c : std::initializer_list<Component*> { &openPhotoB, &openSoundB, &drawB })
    {
        const int w = c == &drawB ? 64 : 104;
        c->setBounds (bx, 230, w, 30); bx += w + 8;
    }
    clearB.setBounds (bx, 230, 64, 30);
    songX.setBounds (32 + 300 - 26, 76 + 6, 20, 20);
    invertB.setBounds (getWidth() - 32 - 92 - (processor.isStandalone() ? 0 : 140), 230, 92, 30);
    syncB.setBounds (getWidth() - 32 - 132, 230, 132, 30);
    // the spectrogram and the transport
    spectrum.setBounds (32 - 44 + 12, 320, getWidth() - 32 - (32 - 44 + 12), 236);
    playB.setBounds (32, 570, 96, 32);
    dragChip.setBounds (136, 570, 120, 32);
    saveWavB.setBounds (getWidth() - 32 - 230, 570, 110, 32);
    savePicB.setBounds (getWidth() - 32 - 112, 570, 112, 32);
}

void NegativeEditor::timerCallback()
{
    const int mode = roundToInt (processor.apvts.getRawParameterValue ("mode")->load());
    for (int i = 0; i < modeTabs.size(); ++i)
        if (modeTabs[i]->getToggleState() != (i == mode)) modeTabs[i]->setToggleState (i == mode, dontSendNotification);
    key.setEnabled (mode == 1);
    scale.setEnabled (mode == 1);
    key.setAlpha (mode == 1 ? 1.0f : 0.35f);
    scale.setAlpha (mode == 1 ? 1.0f : 0.35f);
    songX.setVisible (processor.getSongName().isNotEmpty());
    if (playB.getToggleState() != processor.previewing.load())
    {
        playB.setToggleState (processor.previewing.load(), dontSendNotification);
        playB.setButtonText (processor.previewing.load() ? "Stop" : "Play");
    }
    const int rc = processor.getRenderCount();
    if (rc != lastRender) { lastRender = rc; spectrum.refresh(); if (! photo.drawing) photo.refresh(); repaint (0, 286, getWidth(), 24); }
    else spectrum.repaint();
}

bool NegativeEditor::loadAny (const File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif")
    {
        if (! processor.loadPhotoFile (f)) return false;
        photo.refresh();
        return true;
    }
    return processor.loadSongFile (f);
}

bool NegativeEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("png;jpg;jpeg;gif;wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void NegativeEditor::filesDropped (const StringArray& files, int, int)
{
    for (auto& f : files) loadAny (File (f));
    repaint();
}

void NegativeEditor::openPhoto()
{
    chooser = std::make_unique<FileChooser> ("Open a photo", File(), "*.png;*.jpg;*.jpeg;*.gif");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        if (fc.getResult().existsAsFile()) status = loadAny (fc.getResult()) ? String() : "couldn't open that photo";
        repaint();
    });
}

void NegativeEditor::openSound()
{
    chooser = std::make_unique<FileChooser> ("Open a sound to hide the photo in", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        if (fc.getResult().existsAsFile()) status = processor.loadSongFile (fc.getResult()) ? String() : "couldn't open that sound";
        repaint();
    });
}

void NegativeEditor::saveWav()
{
    chooser = std::make_unique<FileChooser> ("Save the sound", File::getSpecialLocation (File::userMusicDirectory)
                                                                  .getChildFile (File::createLegalFileName ("Negative - " + processor.getPhotoName() + ".wav")), "*.wav");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == File()) return;
        if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
        status = processor.saveWav (f) ? "saved " + f.getFileName() : "couldn't save";
        repaint();
    });
}

void NegativeEditor::savePicture()
{
    chooser = std::make_unique<FileChooser> ("Save the spectrogram", File::getSpecialLocation (File::userPicturesDirectory)
                                                                        .getChildFile (File::createLegalFileName ("Negative - " + processor.getPhotoName() + ".png")), "*.png");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == File()) return;
        if (! f.hasFileExtension ("png")) f = f.withFileExtension ("png");
        const auto img = processor.getSpectrogram();
        f.deleteFile();
        bool ok = false;
        if (auto os = f.createOutputStream()) { PNGImageFormat png; ok = img.isValid() && png.writeImageToStream (img, *os); }
        status = ok ? "saved " + f.getFileName() : "couldn't save";
        repaint();
    });
}
