#include "OceanGenes.h"

namespace ocean
{
namespace
{
    struct NamedColour { const char* name; juce::uint32 argb; };
    const NamedColour colours[] = {
        { "red", 0xffe43b44 }, { "orange", 0xfff77622 }, { "yellow", 0xfffee761 }, { "lime", 0xff63c74d },
        { "green", 0xff3e8948 }, { "teal", 0xff1f8a8a }, { "cyan", 0xff2ce8f5 }, { "blue", 0xff0099db },
        { "navy", 0xff124e89 }, { "purple", 0xff7a3f9e }, { "violet", 0xffb55088 }, { "pink", 0xfff6757a },
        { "magenta", 0xffd6338a }, { "coral", 0xffff8a65 }, { "white", 0xfff4f4f4 }, { "gray", 0xff8b9bb4 },
        { "black", 0xff2a2f4e }, { "brown", 0xff8f563b }, { "peach", 0xffe8b796 }, { "mint", 0xff9ef0c2 },
        { "lavender", 0xffc0a7f0 },
    };
    constexpr int numCols = (int) (sizeof (colours) / sizeof (colours[0]));

    int pick (juce::Random& r, int n) { return r.nextInt (n); }
    float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
    float gauss (juce::Random& r) { return (r.nextFloat() + r.nextFloat() + r.nextFloat() - 1.5f) * 0.8f; }

    juce::uint32 makeRhythm (int density, juce::Random& r)
    {
        static const int strong[] = { 0, 8, 4, 12 }, eighths[] = { 2, 6, 10, 14 }, sixteenths[] = { 1, 3, 5, 7, 9, 11, 13, 15 };
        juce::uint32 p = 0;
        const int hits = density == 0 ? 2 + r.nextInt (2) : density == 1 ? 4 + r.nextInt (3) : 7 + r.nextInt (4);
        int placed = 0, guard = 0;
        while (placed < hits && ++guard < 200)
        {
            int step;
            const float u = r.nextFloat();
            if (placed == 0) step = 0;
            else if (u < (density == 0 ? 0.75f : 0.45f)) step = strong[r.nextInt (4)];
            else if (u < (density == 2 ? 0.75f : 0.9f)) step = eighths[r.nextInt (4)];
            else step = density == 0 ? eighths[r.nextInt (4)] : sixteenths[r.nextInt (8)];
            if ((p & (1u << step)) == 0) { p |= 1u << step; ++placed; }
        }
        return p;
    }

    void randomMelody (Genes& g, juce::Random& r)
    {
        g.melodyLength = 4 + r.nextInt (5);
        int v = r.nextInt (5);
        for (int i = 0; i < 8; ++i)
        {
            g.melody[(size_t) i] = v;
            const int step = r.nextFloat() < 0.15f ? (r.nextBool() ? 3 : -3) : r.nextInt (5) - 2;
            v = juce::jlimit (0, 7, v + step);
        }
    }
}

int numColours() { return numCols; }
juce::String colourName (int i) { return colours[juce::jlimit (0, numCols - 1, i)].name; }
juce::uint32 colourARGB (int i) { return colours[juce::jlimit (0, numCols - 1, i)].argb; }
int findColour (const juce::String& word)
{
    auto w = word.toLowerCase().trim();
    if (w == "grey") w = "gray";
    for (int i = 0; i < numCols; ++i) if (w == colours[i].name) return i;
    return -1;
}

juce::StringArray bodyNames()      { return { "slim", "round", "long", "puffer", "cat", "flat" }; }
juce::StringArray tailNames()      { return { "fan", "fork", "round", "veil" }; }
juce::StringArray finNames()       { return { "nofin", "small", "sail", "spiky" }; }
juce::StringArray eyeNames()       { return { "dot", "big", "sleepy", "star" }; }
juce::StringArray patternNames()   { return { "plain", "stripes", "spots", "gradient", "checker", "belly" }; }
juce::StringArray accessoryNames() { return { "nohat", "hat", "glasses", "crown", "scarf", "bow", "lamp", "headphones" }; }
juce::StringArray voiceNames()     { return { "bubble", "pluck", "bell", "flute", "chip", "pad", "marimba", "whistle" }; }
juce::StringArray rareNames()      { return { "", "Golden", "Rainbow", "Ghost", "Glowing", "Crystal" }; }
juce::StringArray moodNames()      { return { "calm", "shy", "social", "hyper", "lazy" }; }

//==============================================================================
Genes Genes::random (juce::Random& r)
{
    Genes g;
    const float b = r.nextFloat();
    g.body = b < 0.28f ? BodySlim : b < 0.48f ? BodyRound : b < 0.62f ? BodyLong : b < 0.74f ? BodyPuffer : b < 0.84f ? BodyCat : BodyFlat;
    g.size = r.nextFloat() < 0.5f ? 1 : (r.nextBool() ? 0 : 2);
    g.tail = pick (r, NumTails);
    g.fin = pick (r, NumFins);
    g.eyes = r.nextFloat() < 0.5f ? EyesDot : pick (r, NumEyes);
    g.pattern = pick (r, NumPatterns);
    g.accessory = r.nextFloat() < 0.85f ? AccNone : 1 + pick (r, NumAccessories - 1);
    g.colour1 = pick (r, numCols);
    do g.colour2 = pick (r, numCols); while (g.colour2 == g.colour1);
    g.colour3 = r.nextBool() ? g.colour2 : pick (r, numCols);
    g.rare = RareNone;
    g.setMood (pick (r, NumMoods));
    g.speed = clamp01 (g.speed + gauss (r) * 0.2f);
    g.social = clamp01 (g.social + gauss (r) * 0.2f);
    g.voice = pick (r, NumVoices);
    g.pitch = pick (r, 3);
    g.rhythm = r.nextFloat() < 0.5f ? 1 : (r.nextBool() ? 0 : 2);
    g.noteLength = r.nextFloat() < 0.6f ? 0 : 1;
    g.rhythmPattern = makeRhythm (g.rhythm, r);
    randomMelody (g, r);
    g.tempo = juce::jlimit (60.0f, 140.0f, 92.0f + gauss (r) * 30.0f);
    g.key = r.nextInt (12);
    return g;
}

Genes Genes::breed (const Genes& a, const Genes& b, juce::Random& r, bool* mutated)
{
    auto either = [&r] (int x, int y) { return r.nextBool() ? x : y; };
    auto blend = [&r] (float x, float y) { return clamp01 ((x + y) * 0.5f + gauss (r) * 0.06f); };
    Genes g;
    g.body = either (a.body, b.body);       g.size = either (a.size, b.size);
    g.tail = either (a.tail, b.tail);       g.fin = either (a.fin, b.fin);
    g.eyes = either (a.eyes, b.eyes);       g.pattern = either (a.pattern, b.pattern);
    g.accessory = r.nextFloat() < 0.3f ? either (a.accessory, b.accessory) : AccNone;
    // colours: the body from one parent, the pattern colour from the other
    const bool aFirst = r.nextBool();
    g.colour1 = aFirst ? a.colour1 : b.colour1;
    g.colour2 = aFirst ? b.colour2 : a.colour2;
    g.colour3 = either (a.colour3, b.colour3);
    g.speed = blend (a.speed, b.speed);     g.social = blend (a.social, b.social);
    g.shy = blend (a.shy, b.shy);           g.bounce = blend (a.bounce, b.bounce);
    g.voice = either (a.voice, b.voice);    g.pitch = either (a.pitch, b.pitch);
    g.rhythm = either (a.rhythm, b.rhythm); g.noteLength = either (a.noteLength, b.noteLength);
    const juce::uint32 mask = (juce::uint32) r.nextInt (0x10000);
    g.rhythmPattern = ((a.rhythmPattern & mask) | (b.rhythmPattern & ~mask)) & 0xffffu;
    if (g.rhythmPattern == 0) g.rhythmPattern = 1;
    // melody: the start of one parent's tune, the end of the other's
    g.melodyLength = either (a.melodyLength, b.melodyLength);
    const int cut = 1 + r.nextInt (juce::jmax (1, g.melodyLength - 1));
    for (int i = 0; i < 8; ++i) g.melody[(size_t) i] = i < cut ? a.melody[(size_t) i] : b.melody[(size_t) i];
    g.tempo = juce::jlimit (60.0f, 140.0f, (a.tempo + b.tempo) * 0.5f + gauss (r) * 4.0f);
    g.key = either (a.key, b.key);

    // mutations
    bool mut = false;
    auto maybe = [&] (int& gene, int n) { if (r.nextFloat() < 0.04f) { gene = r.nextInt (n); mut = true; } };
    maybe (g.body, NumBodies); maybe (g.tail, NumTails); maybe (g.fin, NumFins); maybe (g.eyes, NumEyes);
    maybe (g.pattern, NumPatterns); maybe (g.colour1, numCols); maybe (g.colour2, numCols); maybe (g.colour3, numCols);
    maybe (g.voice, NumVoices); maybe (g.pitch, 3);
    if (r.nextFloat() < 0.05f) { g.melody[(size_t) r.nextInt (8)] = r.nextInt (8); mut = true; }
    if (r.nextFloat() < 0.04f) { g.rhythmPattern ^= 1u << r.nextInt (16); if (g.rhythmPattern == 0) g.rhythmPattern = 1; mut = true; }
    if (r.nextFloat() < 0.06f) { g.key = (g.key + (r.nextBool() ? 7 : 5)) % 12; mut = true; }   // a fifth away
    if (r.nextFloat() < 0.05f) { g.tempo = juce::jlimit (60.0f, 140.0f, g.tempo + gauss (r) * 25.0f); mut = true; }

    // rare fish: rare parents often pass it on; anyone can get lucky
    if ((a.rare != RareNone || b.rare != RareNone) && r.nextFloat() < 0.25f)
        g.rare = a.rare != RareNone && (b.rare == RareNone || r.nextBool()) ? a.rare : b.rare;
    else if (r.nextFloat() < 0.012f)
        g.rare = 1 + r.nextInt (NumRares - 1);
    if (g.rare != RareNone && g.rare != a.rare && g.rare != b.rare) mut = true;
    if (mutated != nullptr) *mutated = mut;
    return g;
}

void Genes::setMood (int m)
{
    switch (m)
    {
        case MoodShy:    speed = 0.4f;  social = 0.2f; shy = 0.9f;  bounce = 0.1f; break;
        case MoodSocial: speed = 0.5f;  social = 0.9f; shy = 0.1f;  bounce = 0.3f; break;
        case MoodHyper:  speed = 0.95f; social = 0.5f; shy = 0.2f;  bounce = 0.9f; break;
        case MoodLazy:   speed = 0.15f; social = 0.4f; shy = 0.3f;  bounce = 0.0f; break;
        default:         speed = 0.5f;  social = 0.5f; shy = 0.3f;  bounce = 0.2f; break;
    }
}

int Genes::mood() const
{
    if (bounce > 0.65f && speed > 0.75f) return MoodHyper;
    if (speed < 0.28f) return MoodLazy;
    if (shy > 0.7f) return MoodShy;
    if (social > 0.75f) return MoodSocial;
    return MoodCalm;
}

void Genes::setMelody (const juce::String& digits)
{
    int n = 0;
    for (auto c : digits)
        if (c >= '1' && c <= '8' && n < 8)
            melody[(size_t) n++] = (int) (c - '1');
    if (n > 0) melodyLength = n;
    for (int i = n; i < 8 && n > 0; ++i) melody[(size_t) i] = melody[(size_t) (i % n)];
}

juce::String Genes::melodyString() const
{
    juce::String s;
    for (int i = 0; i < melodyLength; ++i) s << (melody[(size_t) i] + 1);
    return s;
}

void Genes::setRhythm (int density)
{
    rhythm = juce::jlimit (0, 2, density);
    juce::Random r ((juce::int64) (rhythmPattern * 7919u + (juce::uint32) melodyLength));
    rhythmPattern = makeRhythm (rhythm, r);
}

float Genes::lengthPixels() const
{
    static const float base[NumBodies] = { 18.0f, 14.0f, 24.0f, 14.0f, 17.0f, 16.0f };
    static const float scale[3] = { 0.75f, 1.0f, 1.35f };
    return base[juce::jlimit (0, NumBodies - 1, body)] * scale[juce::jlimit (0, 2, size)];
}

//==============================================================================
juce::ValueTree Genes::toTree() const
{
    juce::ValueTree t ("GENES");
    t.setProperty ("body", body, nullptr);       t.setProperty ("size", size, nullptr);
    t.setProperty ("tail", tail, nullptr);       t.setProperty ("fin", fin, nullptr);
    t.setProperty ("eyes", eyes, nullptr);       t.setProperty ("pattern", pattern, nullptr);
    t.setProperty ("acc", accessory, nullptr);   t.setProperty ("c1", colour1, nullptr);
    t.setProperty ("c2", colour2, nullptr);      t.setProperty ("c3", colour3, nullptr);
    t.setProperty ("rare", rare, nullptr);       t.setProperty ("speed", speed, nullptr);
    t.setProperty ("social", social, nullptr);   t.setProperty ("shy", shy, nullptr);
    t.setProperty ("bounce", bounce, nullptr);   t.setProperty ("voice", voice, nullptr);
    t.setProperty ("pitch", pitch, nullptr);     t.setProperty ("rhythm", rhythm, nullptr);
    t.setProperty ("len", noteLength, nullptr);  t.setProperty ("pat", (int) rhythmPattern, nullptr);
    juce::String m;
    for (auto v : melody) m << v;
    t.setProperty ("melody", m, nullptr);        t.setProperty ("mlen", melodyLength, nullptr);
    t.setProperty ("tempo", tempo, nullptr);     t.setProperty ("key", key, nullptr);
    return t;
}

Genes Genes::fromTree (const juce::ValueTree& t)
{
    Genes g;
    auto i = [&t] (const char* id, int def, int n) { return juce::jlimit (0, n - 1, (int) t.getProperty (id, def)); };
    auto f = [&t] (const char* id, float def) { return clamp01 ((float) t.getProperty (id, def)); };
    g.body = i ("body", 0, NumBodies);        g.size = i ("size", 1, 3);
    g.tail = i ("tail", 0, NumTails);         g.fin = i ("fin", 1, NumFins);
    g.eyes = i ("eyes", 0, NumEyes);          g.pattern = i ("pattern", 0, NumPatterns);
    g.accessory = i ("acc", 0, NumAccessories);
    g.colour1 = i ("c1", 0, numCols);         g.colour2 = i ("c2", 1, numCols);   g.colour3 = i ("c3", 2, numCols);
    g.rare = i ("rare", 0, NumRares);
    g.speed = f ("speed", 0.5f);  g.social = f ("social", 0.5f);  g.shy = f ("shy", 0.3f);  g.bounce = f ("bounce", 0.2f);
    g.voice = i ("voice", 0, NumVoices);      g.pitch = i ("pitch", 1, 3);
    g.rhythm = i ("rhythm", 1, 3);            g.noteLength = i ("len", 0, 2);
    g.rhythmPattern = (juce::uint32) (int) t.getProperty ("pat", 0x1111) & 0xffffu;
    if (g.rhythmPattern == 0) g.rhythmPattern = 1;
    const auto m = t.getProperty ("melody", "02420000").toString();
    for (int k = 0; k < 8; ++k) g.melody[(size_t) k] = k < m.length() ? juce::jlimit (0, 7, (int) (m[k] - '0')) : 0;
    g.melodyLength = i ("mlen", 4, 9);
    if (g.melodyLength < 1) g.melodyLength = 1;
    g.tempo = juce::jlimit (60.0f, 140.0f, (float) t.getProperty ("tempo", 90.0f));
    g.key = i ("key", 0, 12);
    return g;
}
} // namespace ocean
