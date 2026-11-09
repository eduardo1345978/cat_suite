#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

using namespace juce;

namespace
{
AudioParameterFloatAttributes percent()
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
}
String hzText (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; }
const char* renderParams[] = { "mode", "low", "high", "length", "contrast", "soft", "invert", "key", "scale" };
}

AudioProcessorValueTreeState::ParameterLayout NegativeProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", negative::modeNames(), 0));
    NormalisableRange<float> lowR (20.0f, 18000.0f); lowR.setSkewForCentre (400.0f);
    NormalisableRange<float> highR (500.0f, 22000.0f); highR.setSkewForCentre (5000.0f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "low", 1 }, "Low", lowR, 40.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return hzText (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "high", 1 }, "High", highR, 16000.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return hzText (v); })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "length", 1 }, "Length", negative::lengthNames(), 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "contrast", 1 }, "Contrast", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "soft", 1 }, "Soft", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent()));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "invert", 1 }, "Negative (invert)", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", negative::keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", negative::scaleNames(), 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level", NormalisableRange<float> (-60.0f, 0.0f), -12.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Play with Cubase", true));
    return l;
}

NegativeProcessor::NegativeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),   // an instrument: sound out only
      Thread ("Negative render"),
      apvts (*this, nullptr, "Negative", createLayout())
{
    formats.registerBasicFormats();
    for (auto* id : renderParams) apvts.addParameterListener (id, this);
    apvts.addParameterListener ("level", this);
    // something to hear before a photo is opened: a soft starry sky
    Image start (Image::RGB, 480, 240, true);
    {
        Graphics g (start);
        Random r (7);
        for (int i = 0; i < 90; ++i)
        {
            const float s = 1.5f + r.nextFloat() * 3.0f;
            g.setColour (Colours::white.withAlpha (0.4f + 0.6f * r.nextFloat()));
            g.fillEllipse (r.nextFloat() * 480.0f, r.nextFloat() * 240.0f, s, s);
        }
        g.setColour (Colours::white);
        g.fillEllipse (330.0f, 40.0f, 46.0f, 46.0f);
    }
    photo = start;
    photoName = "stars";
    startThread (Thread::Priority::low);
}

NegativeProcessor::~NegativeProcessor()
{
    cancel = true;
    stopThread (5000);
    for (auto* id : renderParams) apvts.removeParameterListener (id, this);
    apvts.removeParameterListener ("level", this);
}

bool NegativeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void NegativeProcessor::markDirty()
{
    dirtySince = Time::currentTimeMillis();
    if (rendering) cancel = true;
    dirty = true;
}

void NegativeProcessor::parameterChanged (const String& id, float)
{
    if (id == "level") { dirtySince = Time::currentTimeMillis(); dirty = true; return; }   // only the picture needs redoing (cheap)
    markDirty();
}

void NegativeProcessor::prepareToPlay (double sampleRate, int)
{
    if (std::abs (sampleRate - sr.load()) > 0.5) { sr = sampleRate; markDirty(); }
    level.reset (sampleRate, 0.05);
    level.setCurrentAndTargetValue (Decibels::decibelsToGain (p ("level"), -60.0f));
    for (auto& v : voices) v = Voice();
}

double NegativeProcessor::renderSampleRate() const { return sr.load(); }

void NegativeProcessor::run()
{
    while (! threadShouldExit())
    {
        wait (30);
        if (! dirty.load() || Time::currentTimeMillis() - dirtySince.load() < 120) continue;
        dirty = false;
        cancel = false;
        rendering = true;

        negative::RenderSettings s;
        Image ph;
        AudioBuffer<float> sg;
        {
            const ScopedLock sl (dataLock);
            ph = photo.createCopy();
            sg.makeCopyOf (song);
        }
        s.mode = roundToInt (p ("mode"));
        s.lowHz = p ("low"); s.highHz = p ("high");
        s.key = roundToInt (p ("key")); s.scale = roundToInt (p ("scale"));
        s.sampleRate = sr.load();
        const double bpm = hostBpm.load();
        s.seconds = negative::lengthBars (roundToInt (p ("length"))) * 4.0 * 60.0 / bpm;
        const auto gray = negative::prepare (ph, p ("contrast"), p ("soft"), p ("invert") > 0.5f);
        auto snd = negative::render (gray, s, &cancel);
        if (cancel.load() || threadShouldExit()) { rendering = false; continue; }

        // the picture of what comes out (the photo at its level, in the song if there is one)
        AudioBuffer<float> look (2, snd.getNumSamples());
        look.clear();
        const float g = Decibels::decibelsToGain (p ("level"), -60.0f);
        for (int ch = 0; ch < 2; ++ch)
        {
            look.addFrom (ch, 0, snd, ch, 0, snd.getNumSamples(), g);
            if (sg.getNumSamples() > 0) look.addFrom (ch, 0, sg, jmin (ch, sg.getNumChannels() - 1), 0, jmin (sg.getNumSamples(), look.getNumSamples()));
        }
        auto pic = negative::analyse (look, s.sampleRate, s.lowHz * 0.8f, jmin ((float) (s.sampleRate * 0.5), s.highHz * 1.1f), 1200, 360);

        AudioBuffer<float> forAudio, forSong;
        forAudio.makeCopyOf (snd);
        forSong.makeCopyOf (sg);
        {
            const ScopedLock sl (dataLock);
            spectrogram = pic;
        }
        {
            const SpinLock::ScopedLockType al (audioLock);
            std::swap (sound, forAudio);
            std::swap (songAudio, forSong);
        }
        renderedBpm = bpm;
        rendering = false;
        ++renderCount;
    }
}

void NegativeProcessor::waitForRender (int timeoutMs)
{
    const auto start = Time::getMillisecondCounter();
    const int before = renderCount.load();
    while ((dirty.load() || rendering.load() || renderCount.load() == before) && (int) (Time::getMillisecondCounter() - start) < timeoutMs)
    {
        if (! dirty.load() && ! rendering.load() && renderCount.load() > 0) break;
        Thread::sleep (10);
    }
}

void NegativeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();                                  // an instrument: only the photo comes out

    double ppq = -1.0, bpm = hostBpm.load();
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition(); q.hasValue()) ppq = *q;
        }
    if (std::abs (bpm - hostBpm.load()) > 0.01) { hostBpm = bpm; markDirty(); }
    level.setTargetValue (Decibels::decibelsToGain (p ("level"), -60.0f));

    const SpinLock::ScopedTryLockType al (audioLock);
    if (! al.isLocked()) return;
    const int len = sound.getNumSamples();
    if (len < 2) { midi.clear(); return; }
    const auto* sL = sound.getReadPointer (0);
    const auto* sR = sound.getReadPointer (1);
    auto read = [&] (const float* src, double pos)
    {
        const int i = (int) pos;
        if (i < 0 || i >= len - 1) return 0.0f;
        const float f = (float) (pos - i);
        return src[i] + (src[i + 1] - src[i]) * f;
    };

    const bool sync = p ("sync") > 0.5f && playing && ppq >= 0.0;
    const double beats = negative::lengthBars (roundToInt (p ("length"))) * 4.0;
    const double beatsPerSample = bpm / 60.0 / getSampleRate();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float attack = 1.0f / (0.005f * (float) getSampleRate()), release = 1.0f / (0.03f * (float) getSampleRate());

    auto it = midi.cbegin();
    float shown = -1.0f;
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())
            {
                Voice* v = nullptr;
                for (auto& vv : voices) if (! vv.on) { v = &vv; break; }
                if (v == nullptr) v = &voices[0];
                *v = Voice();
                v->on = true; v->note = m.getNoteNumber(); v->rate = std::pow (2.0, (m.getNoteNumber() - 60) / 12.0);
                v->gain = m.getFloatVelocity();
            }
            else if (m.isNoteOff())
            {
                for (auto& vv : voices) if (vv.on && vv.note == m.getNoteNumber()) vv.releasing = true;
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                for (auto& vv : voices) vv.releasing = true;
            }
            ++it;
        }

        float al2 = 0.0f, ar = 0.0f;
        if (sync)
        {
            const double b = std::fmod (ppq + i * beatsPerSample, beats);
            const double pos = (b < 0.0 ? b + beats : b) / beats * (double) len;
            al2 += read (sL, pos); ar += read (sR, pos);
            shown = (float) (pos / len);
        }
        for (auto& v : voices)
        {
            if (! v.on) continue;
            v.env = v.releasing ? v.env - release : jmin (1.0f, v.env + attack);
            if (v.env <= 0.0f && v.releasing) { v.on = false; continue; }
            al2 += read (sL, v.pos) * v.env * v.gain;
            ar += read (sR, v.pos) * v.env * v.gain;
            shown = (float) (v.pos / len);
            v.pos += v.rate;
            if (v.pos >= len - 1) v.on = false;
        }
        float songL = 0.0f, songR = 0.0f;
        if (previewing.load())
        {
            al2 += read (sL, previewPos); ar += read (sR, previewPos);
            const int si = (int) previewPos;
            if (si < songAudio.getNumSamples())
            {
                songL = songAudio.getSample (0, si);
                songR = songAudio.getSample (jmin (1, songAudio.getNumChannels() - 1), si);
            }
            const int total = jmax (len, songAudio.getNumSamples());
            shown = (float) (previewPos / jmax (1, len));
            previewPos += 1.0;
            if (previewPos >= total - 1) previewPos = 0.0;    // the preview loops (the whole song, if there is one)
        }
        else previewPos = 0.0;
        const float g = level.getNextValue();
        L[i] += al2 * g + songL;
        if (R != nullptr) R[i] += ar * g + songR;
    }
    playPos = shown;
}

//==============================================================================
void NegativeProcessor::setPhoto (const Image& img, const String& name)
{
    if (! img.isValid()) return;
    auto copy = img.convertedToFormat (Image::RGB);
    if (copy.getWidth() > 1600 || copy.getHeight() > 1600)
    {
        const float k = 1600.0f / (float) jmax (copy.getWidth(), copy.getHeight());
        copy = copy.rescaled (jmax (1, roundToInt ((float) copy.getWidth() * k)), jmax (1, roundToInt ((float) copy.getHeight() * k)), Graphics::highResamplingQuality);
    }
    {
        const ScopedLock sl (dataLock);
        photo = copy;
        photoName = name;
    }
    markDirty();
}

Image NegativeProcessor::getPhoto() const { const ScopedLock sl (dataLock); return photo; }
String NegativeProcessor::getPhotoName() const { const ScopedLock sl (dataLock); return photoName; }
String NegativeProcessor::getSongName() const { const ScopedLock sl (dataLock); return songName; }
Image NegativeProcessor::getSpectrogram() const { const ScopedLock sl (dataLock); return spectrogram; }

bool NegativeProcessor::loadPhotoFile (const File& f)
{
    auto img = ImageFileFormat::loadFrom (f);
    if (! img.isValid()) return false;
    setPhoto (img, f.getFileNameWithoutExtension());
    return true;
}

bool NegativeProcessor::loadSongFile (const File& f)
{
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr || reader->numChannels < 1 || reader->numChannels > 32 || reader->sampleRate < 4000.0 || reader->sampleRate > 768000.0) return false;
    const auto maxLen = (int64) (reader->sampleRate * 600.0);                 // up to 10 minutes
    const int len = (int) jmin (reader->lengthInSamples, maxLen);
    if (len <= 0) return false;
    AudioBuffer<float> in (2, len);
    reader->read (&in, 0, len, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1) in.copyFrom (1, 0, in, 0, 0, len);
    // to the sample rate the photo is made in
    const double ratio = reader->sampleRate / sr.load();
    AudioBuffer<float> out;
    if (std::abs (ratio - 1.0) < 1.0e-9) out = std::move (in);     // the same rate: exactly as it is
    else
    {
        out.setSize (2, (int) (len / ratio));
        for (int ch = 0; ch < 2; ++ch)
        {
            LagrangeInterpolator li;
            li.process (ratio, in.getReadPointer (ch), out.getWritePointer (ch), out.getNumSamples(), len, 0);
        }
    }
    {
        const ScopedLock sl (dataLock);
        song = std::move (out);
        songName = f.getFileNameWithoutExtension();
    }
    markDirty();
    return true;
}

void NegativeProcessor::clearSong()
{
    {
        const ScopedLock sl (dataLock);
        song.setSize (0, 0);
        songName.clear();
    }
    markDirty();
}

AudioBuffer<float> NegativeProcessor::getMix() const
{
    AudioBuffer<float> snd;
    {
        const SpinLock::ScopedLockType al (const_cast<SpinLock&> (audioLock));
        snd.makeCopyOf (sound);
    }
    const ScopedLock sl (dataLock);
    const int len = jmax (snd.getNumSamples(), song.getNumSamples());
    AudioBuffer<float> mix (2, jmax (1, len));
    mix.clear();
    const float g = Decibels::decibelsToGain (apvts.getRawParameterValue ("level")->load(), -60.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        if (song.getNumSamples() > 0) mix.addFrom (ch, 0, song, jmin (ch, song.getNumChannels() - 1), 0, song.getNumSamples());
        if (snd.getNumSamples() > 0) mix.addFrom (ch, 0, snd, ch, 0, snd.getNumSamples(), g);
    }
    const float peak = jmax (mix.getMagnitude (0, 0, mix.getNumSamples()), mix.getMagnitude (1, 0, mix.getNumSamples()));
    if (peak > 0.999f) mix.applyGain (0.999f / peak);                       // never clips in the file
    return mix;
}

bool NegativeProcessor::saveWav (const File& f) const
{
    const auto mix = getMix();
    f.deleteFile();
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr.load(), 2, 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (mix, 0, mix.getNumSamples());
}

File NegativeProcessor::writeTempWav() const
{
    auto name = File::createLegalFileName ("Negative - " + getPhotoName() + ".wav");
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile (name);
    return saveWav (f) ? f : File();
}

void NegativeProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    {
        const ScopedLock sl (dataLock);
        state.setProperty ("photo", negative::toBase64Png (photo.getWidth() > 1000 || photo.getHeight() > 1000
                                                               ? photo.rescaled (jmin (1000, photo.getWidth()), jmin (1000, photo.getHeight()), Graphics::highResamplingQuality)
                                                               : photo), nullptr);
        state.setProperty ("photoName", photoName, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void NegativeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        const auto img = negative::fromBase64Png (state.getProperty ("photo").toString());
        const auto name = state.getProperty ("photoName").toString();
        state.removeProperty ("photo", nullptr);
        state.removeProperty ("photoName", nullptr);
        apvts.replaceState (state);
        if (img.isValid()) setPhoto (img, name);
        markDirty();
    }
}

AudioProcessorEditor* NegativeProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<NegativeEditor> (*this), "Negative");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NegativeProcessor();
}
