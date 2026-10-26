#include "Samples.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace catbox
{
SampleBank& SampleBank::get()
{
    static SampleBank bank;
    return bank;
}

juce::String SampleBank::name (int slot) const
{
    if (auto* s = at (slot)) return s->name;
    return "(empty)";
}

int SampleBank::find (const juce::String& n) const
{
    for (int i = 0; i < used.load(); ++i)
        if (auto* s = at (i); s != nullptr && s->name == n) return i;
    return -1;
}

int SampleBank::add (const juce::String& n, std::vector<float> l, std::vector<float> r, double sampleRate, bool writeFile)
{
    const int slot = used.load();
    if (slot >= maxSamples || l.empty()) return -1;
    if (r.size() != l.size()) r = l;
    auto s = std::make_unique<Sample>();
    s->name = n;
    s->sampleRate = sampleRate;
    s->l = std::move (l);
    s->r = std::move (r);
    if (writeFile && dir != juce::File())
    {
        dir.createDirectory();
        const auto f = dir.getChildFile (n + ".wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        if (out != nullptr)
            if (auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24)))
            {
                const float* ch[2] = { s->l.data(), s->r.data() };
                w->writeFromFloatArrays (ch, 2, s->length());
            }
    }
    slots[(size_t) slot].store (s.get(), std::memory_order_release);
    owned.push_back (std::move (s));
    used = slot + 1;
    return slot;
}

void SampleBank::loadFolder (const juce::File& folder, const juce::StringArray& starterNames, const std::function<void (SampleBank&)>& makeStarters)
{
    if (dir != juce::File()) return;
    dir = folder;
    dir.createDirectory();
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    auto readFile = [this, &fm] (const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr || reader->lengthInSamples <= 0) return false;
        const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));   // up to a minute
        juce::AudioBuffer<float> b ((int) juce::jmin (2u, reader->numChannels), len);
        reader->read (&b, 0, len, 0, true, true);
        std::vector<float> l (b.getReadPointer (0), b.getReadPointer (0) + len);
        std::vector<float> r (b.getReadPointer (b.getNumChannels() - 1), b.getReadPointer (b.getNumChannels() - 1) + len);
        return add (f.getFileNameWithoutExtension(), std::move (l), std::move (r), reader->sampleRate, false) >= 0;
    };
    // the starter samples always come first, in their order (the factory sounds point at them)
    bool all = true;
    for (const auto& n : starterNames) all = all && dir.getChildFile (n + ".wav").existsAsFile();
    if (all) for (const auto& n : starterNames) all = all && readFile (dir.getChildFile (n + ".wav"));
    if (! all)
    {
        for (int i = 0; i < maxSamples; ++i) slots[(size_t) i].store (nullptr);
        used = 0;
        if (makeStarters) makeStarters (*this);
    }
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
    files.sort();
    for (const auto& f : files)
    {
        if (find (f.getFileNameWithoutExtension()) >= 0) continue;
        readFile (f);
    }
}
} // namespace catbox
