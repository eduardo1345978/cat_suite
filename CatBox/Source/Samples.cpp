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
    if (slot >= 0 && slot < used.load() && slot < (int) names.size()) return names[(size_t) slot];
    return "(empty)";
}

juce::String SampleBank::shortName (int slot) const
{
    return name (slot).fromLastOccurrenceOf ("/", false, false);
}

juce::String SampleBank::folderOf (int slot) const
{
    const auto n = name (slot);
    return n.containsChar ('/') ? n.upToLastOccurrenceOf ("/", false, false) : juce::String();
}

int SampleBank::noteFromName (const juce::String& name)
{
    auto toks = juce::StringArray::fromTokens (name.fromLastOccurrenceOf ("/", false, false), "_ -.", "");
    int found = -1;
    for (const auto& t : toks)
    {
        if (t.length() < 2 || t.length() > 4) continue;
        const juce::juce_wchar c = juce::CharacterFunctions::toUpperCase (t[0]);
        static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        if (c < 'A' || c > 'G') continue;
        int i = 1, semi = base[c - 'A'];
        if (t[i] == '#') { ++semi; ++i; }
        else if (t[i] == 'b' && t.length() > 2) { --semi; ++i; }
        const auto rest = t.substring (i);
        if (rest.isEmpty() || ! rest.containsOnly ("-0123456789")) continue;
        const int oct = rest.getIntValue();
        if (oct < -1 || oct > 9) continue;
        found = juce::jlimit (0, 127, (oct + 1) * 12 + semi);
    }
    return found;
}

int SampleBank::find (const juce::String& n) const
{
    const int u = juce::jmin (used.load(), (int) names.size());
    if (n.containsChar ('@'))   // "folder@C4": the sample of that folder nearest to the note
    {
        const auto folder = n.upToFirstOccurrenceOf ("@", false, false);
        const int want = noteFromName (n.fromFirstOccurrenceOf ("@", false, false));
        int best = -1, bestD = 1000;
        std::vector<int> inFolder;
        for (int i = 0; i < u; ++i)
        {
            const auto f = folderOf (i);
            if (f != folder && ! f.endsWith ("/" + folder)) continue;
            inFolder.push_back (i);
            const int note = noteFromName (names[(size_t) i]);
            if (note < 0) continue;
            int d = std::abs (note - want) * 4;
            // prefer the first velocity / round robin of a note (v1, rr1, mf)
            if (names[(size_t) i].containsIgnoreCase ("rr2") || names[(size_t) i].containsIgnoreCase ("rr3") || names[(size_t) i].containsIgnoreCase ("_2")) d += 1;
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best < 0 && ! inFolder.empty()) best = inFolder[inFolder.size() / 2];
        return best;
    }
    for (int i = 0; i < u; ++i)
        if (names[(size_t) i] == n) return i;
    // a library sample saved with a longer / shorter path: match the end
    if (n.containsChar ('/'))
        for (int i = 0; i < u; ++i)
            if (names[(size_t) i].endsWith ("/" + n) || n.endsWith ("/" + names[(size_t) i])) return i;
    return -1;
}

juce::File SampleBank::findLibrary (const juce::File& dataDir)
{
    if (const auto env = juce::SystemStats::getEnvironmentVariable ("CATBOX_LIBRARY", {}); env.isNotEmpty() && juce::File::isAbsolutePath (env) && juce::File (env).isDirectory())
        return juce::File (env);
    if (dataDir.getChildFile ("library").isDirectory()) return dataDir.getChildFile ("library");
    // next to the app, or in the repository it was built in (CatBox/library)
    auto d = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 8 && d.exists(); ++i)
    {
        if (d.getChildFile ("library").isDirectory()) return d.getChildFile ("library");
        if (d.getChildFile ("CatBox").getChildFile ("library").isDirectory()) return d.getChildFile ("CatBox").getChildFile ("library");
        d = d.getParentDirectory();
    }
    return {};
}

int SampleBank::addLibrary (const juce::File& root)
{
    if (! root.isDirectory() || firstLibrary >= 0) return 0;
    juce::Array<juce::File> found;
    for (const auto& e : juce::RangedDirectoryIterator (root, true, "*.wav;*.WAV;*.aif;*.aiff;*.flac", juce::File::findFiles))
    {
        const auto f = e.getFile();
        if (f.getFullPathName().contains ("/.") || f.getFullPathName().contains ("\\.")) continue;   // .git and other hidden folders
        found.add (f);
    }
    struct ByPath { static int compareElements (const juce::File& a, const juce::File& b) { return a.getFullPathName().compareNatural (b.getFullPathName()); } };
    ByPath cmp;
    found.sort (cmp);
    firstLibrary = used.load();
    int n = 0;
    for (const auto& f : found)
    {
        const int slot = used.load();
        if (slot >= maxSamples) break;
        names[(size_t) slot] = f.getRelativePathFrom (root).replaceCharacter ('\\', '/').upToLastOccurrenceOf (".", false, false);
        files[(size_t) slot] = f;
        used = slot + 1;
        ++n;
    }
    libraryCount = n;
    return n;
}

juce::StringArray SampleBank::libraryFolders() const
{
    juce::StringArray out;
    juce::String last;
    for (int i = firstLibrary; i >= 0 && i < firstLibrary + libraryCount; ++i)
    {
        const auto f = folderOf (i);
        if (f != last) { out.addIfNotAlreadyThere (f); last = f; }
    }
    return out;
}

std::vector<int> SampleBank::librarySlotsIn (const juce::String& folder) const
{
    std::vector<int> out;
    for (int i = firstLibrary; i >= 0 && i < firstLibrary + libraryCount; ++i)
        if (folderOf (i) == folder) out.push_back (i);
    return out;
}

bool SampleBank::ensureLoaded (int slot)
{
    if (slot < 0 || slot >= used.load()) return false;
    if (at (slot) != nullptr) return true;
    if (files[(size_t) slot] == juce::File()) return false;
    return readInto (slot, files[(size_t) slot]);
}

bool SampleBank::readInto (int slot, const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    juce::AudioBuffer<float> b ((int) juce::jmax (1u, juce::jmin (2u, reader->numChannels)), len);
    reader->read (&b, 0, len, 0, true, true);
    auto s = std::make_unique<Sample>();
    s->name = names[(size_t) slot];
    s->sampleRate = reader->sampleRate;
    // libraries are recorded at all kinds of levels, often with silence first: even them out
    const float* L = b.getReadPointer (0);
    const float* R = b.getReadPointer (b.getNumChannels() - 1);
    float peak = 0.0f;
    for (int i = 0; i < len; ++i) peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
    int start = 0;
    if (peak > 1.0e-5f)
    {
        while (start < len - 1 && std::abs (L[start]) < peak * 0.003f && std::abs (R[start]) < peak * 0.003f) ++start;
        start = juce::jmax (0, start - 64);
    }
    const float gain = peak > 1.0e-5f ? 0.89f / peak : 1.0f;
    s->l.resize ((size_t) (len - start));
    s->r.resize ((size_t) (len - start));
    for (int i = start; i < len; ++i) { s->l[(size_t) (i - start)] = L[i] * gain; s->r[(size_t) (i - start)] = R[i] * gain; }
    slots[(size_t) slot].store (s.get(), std::memory_order_release);
    owned.push_back (std::move (s));
    return true;
}

int SampleBank::add (const juce::String& n, std::vector<float> l, std::vector<float> r, double sampleRate, bool writeFile)
{
    const int slot = used.load();
    if (slot >= maxSamples || l.empty()) return -1;
    if (r.size() != l.size()) r = l;
    auto s = std::make_unique<Sample>();
    s->name = n;
    names[(size_t) slot] = n;
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
