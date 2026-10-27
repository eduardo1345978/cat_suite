#include "Samples.h"
#include <map>
#include <juce_audio_formats/juce_audio_formats.h>

namespace catbox
{
/** The steady part of a held note (strings, brass, organ ...): after the attack, while it stays loud. */
static void findLoop (Sample& s)
{
    const int block = juce::jmax (256, (int) (s.sampleRate * 0.01));
    const int n = s.length() / block;
    if (n < 30) return;   // shorter than ~0.3 s: a hit, not a held note
    std::vector<float> rms ((size_t) n);
    float peak = 0.0f;
    int peakAt = 0;
    for (int b = 0; b < n; ++b)
    {
        double sum = 0.0;
        for (int i = b * block; i < (b + 1) * block; ++i) sum += (double) s.l[(size_t) i] * s.l[(size_t) i] + (double) s.r[(size_t) i] * s.r[(size_t) i];
        rms[(size_t) b] = (float) std::sqrt (sum / (2.0 * block));
        if (rms[(size_t) b] > peak) { peak = rms[(size_t) b]; peakAt = b; }
    }
    // from the peak on, as long as it stays above half the peak (a held note), not falling much
    int end = peakAt;
    while (end + 1 < n && rms[(size_t) (end + 1)] > peak * 0.45f) ++end;
    const int run = end - peakAt;
    if (run * block < (int) (s.sampleRate * 0.35)) return;    // it dies away: a plucked / hit sound
    // a decaying sound (piano, bells) isn't held either: the end of the run much quieter than its start
    float a = 0.0f, z = 0.0f;
    for (int b = peakAt; b < peakAt + run / 4; ++b) a += rms[(size_t) b];
    for (int b = end - run / 4; b <= end; ++b) z += rms[(size_t) b];
    if (z < a * 0.55f) return;
    s.loopFrom = (peakAt + run / 4) * block;
    s.loopTo = (peakAt + run * 9 / 10) * block;
}

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
    // VSCO names its notes one octave low (C3 = middle C, the Yamaha way)
    if (found >= 0 && name.contains ("VSCO")) found = juce::jmin (127, found + 12);
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
    findLoop (*s);
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
namespace
{
/** How loud a take is, from its name: v1..v9, pp / p / mp / mf / f / ff / fff (0 = unknown). */
int dynamicOf (const juce::String& name)
{
    int d = 0;
    for (const auto& t : juce::StringArray::fromTokens (name.fromLastOccurrenceOf ("/", false, false).toLowerCase(), "_ -.", ""))
    {
        if (t.length() == 2 && t[0] == 'v' && juce::CharacterFunctions::isDigit (t[1])) d = 10 * (t[1] - '0');
        static const char* dyn[] = { "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff" };
        for (int k = 0; k < 8; ++k) if (t == dyn[k]) d = 10 * (k + 1);
    }
    return d;
}
bool isLaterTake (const juce::String& name)
{
    const auto n = name.fromLastOccurrenceOf ("/", false, false).toLowerCase();
    return n.contains ("rr2") || n.contains ("rr3") || n.contains ("rr4") || n.contains ("_2.") || n.endsWith ("_2") || n.endsWith ("_3") || n.endsWith ("_4");
}
} // namespace

bool SampleBank::buildZones (int slot)
{
    if (slot < 0 || slot >= used.load()) return false;
    if (zonesFor (slot) != nullptr) return true;
    const auto folder = folderOf (slot);
    if (folder.isEmpty()) return false;
    // the notes of the folder, each with its takes from soft to loud (first round-robin only)
    std::map<int, std::vector<std::pair<int, int>>> byNote;   // note -> (dynamic, slot)
    const auto inFolder = librarySlotsIn (folder);
    for (int s : inFolder)
    {
        const int note = noteFromName (names[(size_t) s]);
        if (note < 0 || isLaterTake (names[(size_t) s])) continue;
        byNote[note].push_back ({ dynamicOf (names[(size_t) s]), s });
    }
    if (byNote.size() < 3) return false;   // not an instrument across the keys
    auto map = std::make_unique<ZoneMap>();
    std::vector<int> notes;
    for (auto& [n, takes] : byNote) { std::sort (takes.begin(), takes.end()); notes.push_back (n); }
    for (int key = 0; key < 128; ++key)
    {
        int best = notes.front();
        for (int n : notes) if (std::abs (n - key) < std::abs (best - key)) best = n;
        const auto& takes = byNote[best];
        auto& z = map->keys[(size_t) key];
        z.root = (int8_t) best;
        const int count = juce::jmin (4, (int) takes.size());
        z.layers = (int8_t) count;
        for (int k = 0; k < count; ++k)   // spread over the takes: softest ... loudest
        {
            const int idx = count == 1 ? 0 : (int) std::lround ((double) k * (double) (takes.size() - 1) / (double) (count - 1));
            z.slot[(size_t) k] = (int16_t) takes[(size_t) idx].second;
        }
    }
    // read what the map uses
    for (const auto& z : map->keys)
        for (int k = 0; k < z.layers; ++k) ensureLoaded (z.slot[(size_t) k]);
    const ZoneMap* raw = map.get();
    ownedZones.push_back (std::move (map));
    for (int s : inFolder) zones[(size_t) s].store (raw, std::memory_order_release);
    return true;
}

} // namespace catbox
