#include "PattCatEngine.h"
#include "EuclideanGenerator.h"

namespace pattcat
{
juce::StringArray getDivisionNames()
{
    return { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
}

double divisionToPpq (int division)
{
    switch (division)
    {
        case DivQuarter:          return 1.0;
        case DivEighth:           return 0.5;
        case DivThirtySecond:     return 0.125;
        case DivEighthTriplet:    return 1.0 / 3.0;
        case DivSixteenthTriplet: return 1.0 / 6.0;
        case DivSixteenth:
        default:                  return 0.25;
    }
}

juce::String voiceName (int v)
{
    if (v == VoiceAll)    return "All";
    if (v == VoiceTop)    return "Top";
    if (v == VoiceBottom) return "Bottom";
    return "Voice " + juce::String (v + 1);
}

//==============================================================================
juce::ValueTree Step::toValueTree() const
{
    juce::ValueTree t ("STEP");
    t.setProperty ("a", active, nullptr);
    t.setProperty ("v", velocity, nullptr);
    t.setProperty ("g", gate, nullptr);
    t.setProperty ("o", octaveOffset, nullptr);
    t.setProperty ("vi", voiceIndex, nullptr);
    t.setProperty ("t", isTie, nullptr);
    t.setProperty ("r", isRest, nullptr);
    t.setProperty ("rt", ratchet, nullptr);
    t.setProperty ("p", probability, nullptr);
    return t;
}

Step Step::fromValueTree (const juce::ValueTree& t)
{
    Step s;
    s.active = t.getProperty ("a", true);
    s.velocity = juce::jlimit (0.0f, 1.5f, (float) t.getProperty ("v", 1.0));
    s.gate = juce::jlimit (0.1f, 1.5f, (float) t.getProperty ("g", 0.8));
    s.octaveOffset = juce::jlimit (-2, 2, (int) t.getProperty ("o", 0));
    s.voiceIndex = juce::jlimit (-3, 5, (int) t.getProperty ("vi", -1));
    s.isTie = t.getProperty ("t", false);
    s.isRest = t.getProperty ("r", false);
    s.ratchet = juce::jlimit (1, 4, (int) t.getProperty ("rt", 1));
    s.probability = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("p", 1.0));
    return s;
}

juce::ValueTree PatternLayer::toValueTree() const
{
    juce::ValueTree t ("LAYER");
    t.setProperty ("length", length, nullptr);
    t.setProperty ("division", division, nullptr);
    t.setProperty ("swing", swing, nullptr);
    t.setProperty ("velScale", velocityScale, nullptr);
    t.setProperty ("channel", midiChannel, nullptr);
    t.setProperty ("muted", muted, nullptr);
    t.setProperty ("poly", isPolyrhythmic, nullptr);
    t.setProperty ("name", name, nullptr);
    t.setProperty ("volume", volume, nullptr);
    t.setProperty ("solo", solo, nullptr);
    t.setProperty ("mpe", mpe, nullptr);
    t.setProperty ("euclidFill", euclidFill, nullptr);
    t.setProperty ("euclidRot", euclidRotation, nullptr);
    for (const auto& s : steps)
        t.appendChild (s.toValueTree(), nullptr);
    return t;
}

PatternLayer PatternLayer::fromValueTree (const juce::ValueTree& t)
{
    PatternLayer l;
    l.length = juce::jlimit (1, 32, (int) t.getProperty ("length", 16));
    l.division = juce::jlimit (0, NumDivisions - 1, (int) t.getProperty ("division", (int) DivSixteenth));
    l.swing = juce::jlimit (0, 100, (int) t.getProperty ("swing", 0));
    l.velocityScale = juce::jlimit (0.0f, 2.0f, (float) t.getProperty ("velScale", 1.0));
    l.midiChannel = juce::jlimit (1, 16, (int) t.getProperty ("channel", 1));
    l.muted = t.getProperty ("muted", false);
    l.isPolyrhythmic = t.getProperty ("poly", false);
    l.name = t.getProperty ("name", "").toString();
    l.volume = juce::jlimit (0.0f, 1.5f, (float) t.getProperty ("volume", 1.0));
    l.solo = t.getProperty ("solo", false);
    l.mpe = t.getProperty ("mpe", false);
    l.euclidFill = juce::jlimit (0, 32, (int) t.getProperty ("euclidFill", 4));
    l.euclidRotation = juce::jlimit (0, 31, (int) t.getProperty ("euclidRot", 0));

    int i = 0;
    for (const auto& child : t)
        if (child.hasType ("STEP") && i < 32)
            l.steps[(size_t) i++] = Step::fromValueTree (child);
    return l;
}

//==============================================================================
PattCatEngine::PattCatEngine()
{
    resetToDefault();
}

void PattCatEngine::resetToDefault()
{
    const char* names[] = { "Chords", "Bass", "Inner Voices", "Top Line" };
    for (int l = 0; l < 4; ++l)
    {
        auto& layer = layers[(size_t) l];
        layer = {};
        layer.name = names[l];
        clearLayer (l);
    }
    applyPatternPreset (*this, 0);
}

void PattCatEngine::clearLayer (int l)
{
    auto& layer = layers[(size_t) juce::jlimit (0, 3, l)];
    for (auto& s : layer.steps)
    {
        s = {};
        s.active = false;
    }
}

void PattCatEngine::applyEuclidean (int l, int pulses, int steps, int rotation)
{
    auto& layer = layers[(size_t) juce::jlimit (0, 3, l)];
    steps = juce::jlimit (1, 32, steps);
    const auto pattern = EuclideanGenerator::generate (pulses, steps, rotation);

    layer.length = steps;
    layer.euclidFill = pulses;
    layer.euclidRotation = rotation;
    for (int s = 0; s < 32; ++s)
    {
        auto& st = layer.steps[(size_t) s];
        st.active = s < steps && pattern[(size_t) s];
        st.isTie = false;
        st.isRest = false;
    }
}

void PattCatEngine::randomizeLayer (int l, juce::Random& rng)
{
    auto& layer = layers[(size_t) juce::jlimit (0, 3, l)];
    const int steps = layer.length;
    const int pulses = juce::jlimit (1, steps, steps / 4 + rng.nextInt (juce::jmax (1, steps / 2)));
    applyEuclidean (l, pulses, steps, rng.nextInt (juce::jmax (1, steps / 4)));

    for (int s = 0; s < steps; ++s)
    {
        auto& st = layer.steps[(size_t) s];
        if (! st.active) continue;
        st.velocity = 0.65f + rng.nextFloat() * 0.6f;
        st.gate = 0.3f + rng.nextFloat() * 0.8f;
        st.ratchet = rng.nextFloat() < 0.1f ? 2 + rng.nextInt (3) : 1;
        st.probability = rng.nextFloat() < 0.2f ? 0.5f + rng.nextFloat() * 0.4f : 1.0f;
        if (l == 0)      st.voiceIndex = VoiceAll;
        else if (l == 1) st.voiceIndex = VoiceBottom;
        else if (l == 3) st.voiceIndex = rng.nextFloat() < 0.6f ? VoiceTop : rng.nextInt (4);
        else             st.voiceIndex = 1 + rng.nextInt (3);
    }
}

bool PattCatEngine::isLayerAudible (int l) const
{
    bool anySolo = false;
    for (const auto& layer : layers) anySolo = anySolo || layer.solo;
    const auto& layer = layers[(size_t) l];
    return ! layer.muted && (! anySolo || layer.solo);
}

double PattCatEngine::getCycleLengthPpq (int l, double barLengthPpq) const
{
    const auto& layer = layers[(size_t) l];
    const double patternLen = juce::jlimit (1, 32, layer.length) * divisionToPpq (layer.division);
    if (layer.isPolyrhythmic || barLengthPpq <= 0.0)
        return patternLen;

    // Bar-locked: the pattern restarts on the next bar line after it finishes
    return std::ceil (patternLen / barLengthPpq - 1.0e-9) * barLengthPpq;
}

int PattCatEngine::getStepAt (int l, double ppq, double barLengthPpq) const
{
    const auto& layer = layers[(size_t) l];
    const double cycle = getCycleLengthPpq (l, barLengthPpq);
    const double pos = ppq - std::floor (ppq / cycle) * cycle;
    const int s = (int) std::floor (pos / divisionToPpq (layer.division));
    return s < layer.length ? s : -1;
}

void PattCatEngine::selectVoices (const std::vector<int>& chord, int voiceIndex, std::vector<int>& out)
{
    out.clear();
    if (chord.empty())
        return;

    if (voiceIndex == VoiceAll)         out = chord;
    else if (voiceIndex == VoiceTop)    out.push_back (chord.back());
    else if (voiceIndex == VoiceBottom) out.push_back (chord.front());
    else                                out.push_back (chord[(size_t) (voiceIndex % (int) chord.size())]);
}

juce::ValueTree PattCatEngine::toValueTree() const
{
    juce::ValueTree t (treeId);
    t.setProperty ("currentLayer", currentLayer, nullptr);
    for (const auto& l : layers)
        t.appendChild (l.toValueTree(), nullptr);
    return t;
}

void PattCatEngine::fromValueTree (const juce::ValueTree& t)
{
    if (! t.isValid())
        return;
    currentLayer = juce::jlimit (0, 3, (int) t.getProperty ("currentLayer", 0));
    int i = 0;
    for (const auto& child : t)
        if (child.hasType ("LAYER") && i < 4)
            layers[(size_t) i++] = PatternLayer::fromValueTree (child);
}

//==============================================================================
// Factory patterns
namespace
{
    struct LayerSpec
    {
        int layer;
        const char* steps;          // x normal, X accent, o soft, . off, - tie, 2/3/4 ratchet, ? 50 %, + octave up, r rest
        const char* voices = "";    // a all, t top, b bottom, 0-5 voice; repeats if shorter than steps
        int division = DivSixteenth;
        float gate = 0.8f;
        int octave = 0;
        int channel = 1;
        int swing = 0;
        bool poly = false;
        float velScale = 1.0f;
    };

    void setLayer (PattCatEngine& e, const LayerSpec& spec)
    {
        auto& layer = e.layers[(size_t) spec.layer];
        e.clearLayer (spec.layer);
        const juce::String steps (spec.steps), voices (spec.voices);

        layer.length = juce::jlimit (1, 32, steps.length());
        layer.division = spec.division;
        juce::ignoreUnused (spec.channel); // output channels are a user/mixer setting, presets keep them
        layer.swing = spec.swing;
        layer.isPolyrhythmic = spec.poly;
        layer.velocityScale = spec.velScale;

        for (int i = 0; i < layer.length; ++i)
        {
            auto& st = layer.steps[(size_t) i];
            const auto c = steps[i];
            st = {};
            st.gate = spec.gate;
            st.octaveOffset = spec.octave;
            st.active = c != '.';

            switch (c)
            {
                case 'X': st.velocity = 1.25f; break;
                case 'o': st.velocity = 0.7f; break;
                case '-': st.isTie = true; break;
                case '2': case '3': case '4': st.ratchet = c - '0'; st.velocity = 0.85f; break;
                case '?': st.probability = 0.5f; break;
                case '+': st.octaveOffset = spec.octave + 1; break;
                case 'r': st.isRest = true; break;
                default: break;
            }

            if (voices.isNotEmpty())
            {
                const auto v = voices[i % voices.length()];
                if (v == 't') st.voiceIndex = VoiceTop;
                else if (v == 'b') st.voiceIndex = VoiceBottom;
                else if (v >= '0' && v <= '5') st.voiceIndex = v - '0';
                else st.voiceIndex = VoiceAll;
            }
        }
    }

    void euclidLayer (PattCatEngine& e, int layer, int pulses, int steps, int rotation, const char* voices, float gate, int channel = 1, int octave = 0)
    {
        setLayer (e, { layer, "................", voices, DivSixteenth, gate, octave, channel });
        e.applyEuclidean (layer, pulses, steps, rotation);
    }

    struct Preset
    {
        const char* name;
        std::vector<LayerSpec> layers;
        std::function<void (PattCatEngine&)> extra = nullptr;
    };

    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list = {
            { "Basic Quarters",     { { 0, "xxxx", "a", DivQuarter, 0.9f } } },
            { "House Stabs",        { { 0, "..X...X...X...X.", "a", DivSixteenth, 0.35f }, { 1, "x...x...x...x...", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "House Offbeat",      { { 0, "..x...x...x...x.", "a", DivSixteenth, 0.5f }, { 1, "x..x..x.x..x..x.", "b", DivSixteenth, 0.4f, -1, 2 } } },
            { "Deep House Swing",   { { 0, "x..x..x...x..x..", "a", DivSixteenth, 0.3f, 0, 1, 40 }, { 1, "x.......x.......", "b", DivSixteenth, 0.8f, -1, 2 } } },
            { "Trap Roll",          { { 0, "x.......x.......", "a", DivSixteenth, 0.9f }, { 3, "x.x.x.x.x.x2x.34", "t", DivSixteenth, 0.5f, 0, 1, 0, false, 0.8f } } },
            { "Trap Bounce",        { { 0, "x..x..x.x.....x.", "a", DivSixteenth, 0.4f }, { 1, "x.........x.....", "b", DivSixteenth, 1.4f, -1, 2 } } },
            { "Neo-Soul Keys",      { { 0, "X-..o..x-..o..x.", "a", DivSixteenth, 0.9f, 0, 1, 30 }, { 1, "x.......x.....o.", "b", DivSixteenth, 0.8f, -1, 2, 30 } } },
            { "Neo-Soul Lazy",      { { 0, "x--.....o.x--...", "a", DivSixteenth, 0.9f, 0, 1, 45 }, { 1, "x-------x---....", "b", DivSixteenth, 0.9f, -1, 2, 45 } } },
            { "Strum Down",         { { 0, "x-------", "t", DivThirtySecond, 1.0f }, { 1, ".x------", "2", DivThirtySecond, 1.0f },
                                      { 2, "..x-----", "1", DivThirtySecond, 1.0f }, { 3, "...x----", "0", DivThirtySecond, 1.0f } } },
            { "Strum Up",           { { 0, "x-------", "0", DivThirtySecond, 1.0f }, { 1, ".x------", "1", DivThirtySecond, 1.0f },
                                      { 2, "..x-----", "2", DivThirtySecond, 1.0f }, { 3, "...x----", "t", DivThirtySecond, 1.0f } } },
            { "Arp Up 16th",        { { 0, "xxxxxxxxxxxxxxxx", "0123", DivSixteenth, 0.7f } } },
            { "Arp Down 16th",      { { 0, "xxxxxxxxxxxxxxxx", "t210", DivSixteenth, 0.7f } } },
            { "Arp Up-Down",        { { 0, "xxxxxxxxxxxxxxxx", "0123t321", DivSixteenth, 0.7f } } },
            { "Arp 8th Up",         { { 0, "xxxxxxxx", "0123", DivEighth, 0.8f }, { 1, "x.......", "b", DivEighth, 1.5f, -1, 2 } } },
            { "Broken Chord",       { { 0, "x.x.x.x.x.x.x.x.", "b.1.2.t.b.2.1.t.", DivSixteenth, 0.9f } } },
            { "Alberti Bass",       { { 0, "xxxxxxxxxxxxxxxx", "bt1t", DivSixteenth, 0.6f } } },
            { "Off-Beat",           { { 0, "..x...x...x...x.", "a", DivSixteenth, 0.3f }, { 1, "x...x...x...x...", "b", DivSixteenth, 0.4f, -1, 2 } } },
            { "Skank",              { { 0, "..X...X...X...X.", "a", DivSixteenth, 0.15f }, { 1, "x..x....x..x....", "b", DivSixteenth, 0.6f, -1, 2 } } },
            { "Reggae One Drop",    { { 0, "....X.......X...", "a", DivSixteenth, 0.25f }, { 1, "........x.....x.", "b", DivSixteenth, 0.7f, -1, 2 } } },
            { "Charleston",         { { 0, "x-----x---------", "a", DivSixteenth, 0.8f } } },
            { "Bossa Nova",         { { 0, "x..x..x...x..x..", "a", DivSixteenth, 0.4f }, { 1, "x..x....x..x....", "b", DivSixteenth, 0.7f, -1, 2 } } },
            { "Samba Keys",         { { 0, "x.xx.x.xx.x.x.xx", "a", DivSixteenth, 0.3f } } },
            { "Gospel Pump",        { { 0, "x.ox.oX.ox.oX.ox", "a", DivSixteenth, 0.5f }, { 1, "x...x...x...x...", "b", DivSixteenth, 0.8f, -1, 2 } } },
            { "RnB Ballad",         { { 0, "x-------x---o---", "a", DivSixteenth, 0.9f }, { 3, ".......x.......x", "t", DivSixteenth, 0.5f, 0, 1, 0, false, 0.6f } } },
            { "LoFi Swing",         { { 0, "x--.x--.x--.x--.", "a", DivSixteenth, 0.8f, 0, 1, 50 }, { 1, "x.......x.x.....", "b", DivSixteenth, 0.8f, -1, 2, 50 } } },
            { "LoFi Dusty",         { { 0, "x---..o-x---..o-", "a", DivSixteenth, 0.9f, 0, 1, 35 }, { 2, "....x.......x...", "1", DivSixteenth, 0.6f, 0, 1, 35, false, 0.6f } } },
            { "Afrobeat",           { { 0, "x.xx.x.x.xx.x.x.", "a", DivSixteenth, 0.3f }, { 1, "x..x..x...x.....", "b", DivSixteenth, 0.6f, -1, 2 } } },
            { "Amapiano Log",       { { 0, "..x..x....x..x..", "a", DivSixteenth, 0.4f }, { 1, "x..x..x.x..x..2.", "b", DivSixteenth, 0.6f, -1, 2 } } },
            { "Garage 2-Step",      { { 0, "x..x..x...x..x..", "a", DivSixteenth, 0.3f, 0, 1, 55 }, { 1, "x.....x...x.....", "b", DivSixteenth, 0.7f, -1, 2, 55 } } },
            { "DnB Stabs",          { { 0, "x.........x.....", "a", DivSixteenth, 0.4f }, { 1, "x.........x.x...", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Synthwave Pulse",    { { 0, "x---", "a", DivQuarter, 1.0f }, { 1, "xxxxxxxx", "b", DivEighth, 0.6f, -1, 2 } } },
            { "Trance Gate",        { { 0, "x.xxx.xxx.xxx.xx", "a", DivSixteenth, 0.5f } } },
            { "Euclid 3-8",         {}, [] (PattCatEngine& e) { euclidLayer (e, 0, 3, 8, 0, "a", 0.6f); } },
            { "Euclid 5-8",         {}, [] (PattCatEngine& e) { euclidLayer (e, 0, 5, 8, 0, "a", 0.5f); } },
            { "Euclid 7-16",        {}, [] (PattCatEngine& e) { euclidLayer (e, 0, 7, 16, 0, "a", 0.5f); euclidLayer (e, 1, 3, 8, 0, "b", 0.7f, 2, -1); } },
            { "Poly 3 vs 4",        { { 0, "x..", "a", DivEighth, 0.6f, 0, 1, 0, true }, { 1, "x...", "b", DivEighth, 0.6f, -1, 2, 0, true } } },
            { "Poly 5 vs 4",        { { 0, "x.x..", "a", DivSixteenth, 0.5f, 0, 1, 0, true }, { 1, "x...", "b", DivSixteenth, 0.6f, -1, 2, 0, true } } },
            { "Poly 12 vs 16",      { { 0, "x..x..x..x..", "a", DivSixteenth, 0.5f, 0, 1, 0, true }, { 1, "x...x...x...x...", "b", DivSixteenth, 0.6f, -1, 2, 0, true } } },
            { "Ratchet Run",        { { 0, "x.......x.......", "a", DivSixteenth, 0.9f }, { 3, "x...x...2...4...", "t", DivSixteenth, 0.6f } } },
            { "Triplet Flow",       { { 0, "xxxxxx", "012t21", DivEighthTriplet, 0.7f }, { 1, "x.....", "b", DivEighthTriplet, 1.5f, -1, 2 } } },
            { "Half Time",          { { 0, "x.......x.......", "a", DivEighth, 0.9f } } },
            { "Whole Notes Pad",    { { 0, "x---", "a", DivQuarter, 1.0f } } },
            { "Dotted 8ths",        { { 0, "x..x..x..x..x...", "a", DivSixteenth, 0.7f } } },
            { "Funk 16ths",         { { 0, "X.xx.x.xX.xx.x.x", "a", DivSixteenth, 0.25f }, { 1, "x..x..x...x..x..", "b", DivSixteenth, 0.4f, -1, 2 } } },
            { "Disco Octaves",      { { 0, "..x...x...x...x.", "a", DivSixteenth, 0.4f }, { 1, "x+x+x+x+x+x+x+x+", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Piano Ballad",       { { 0, "x-------x-------", "a", DivSixteenth, 0.9f }, { 2, "..x.x.x...x.x.x.", "..1.2.t...1.2.t.", DivSixteenth, 0.6f, 0, 1, 0, false, 0.7f } } },
            { "Jazz Comp",          { { 0, "...x-.....x-....", "a", DivSixteenth, 0.6f, 0, 1, 45 }, { 1, "x...x...x...x...", "b", DivSixteenth, 0.8f, -1, 2, 45 } } },
            { "Montuno",            { { 0, "x.x.xx.x.x.xx.x.", "b.t.bt.b.t.bt.b.", DivSixteenth, 0.5f } } },
            { "Waltz Feel",         { { 0, "..x.x.", "a", DivEighth, 0.6f, 0, 1, 0, true }, { 1, "x.....", "b", DivEighth, 0.9f, -1, 2, 0, true } } },
            { "Random Sparkle",     { { 0, "x---------------", "a", DivSixteenth, 1.0f }, { 3, "x?x?x?x?x?x?x?x?", "t", DivSixteenth, 0.4f, 1, 1, 0, false, 0.7f } } },
            // ---- more patterns
            { "Pop Piano 8ths",     { { 0, "x.x.x.x.x.x.x.x.", "a", DivSixteenth, 0.45f }, { 1, "x.......x.......", "b", DivSixteenth, 0.9f, -1, 2 } } },
            { "Ballad 6/8 Arp",     { { 0, "xxxxxx", "b12t21", DivEighthTriplet, 0.8f, 0, 1, 0, true } } },
            { "Pop Strum",          { { 0, "x-x.xx.xx-x.xx.x", "a", DivSixteenth, 0.35f } } },
            { "Country Boom-Chick", { { 0, "..x...x...x...x.", "a", DivSixteenth, 0.3f }, { 1, "x...x...x...x...", "b...2...b...2...", DivSixteenth, 0.5f, -1, 2 } } },
            { "Motown Stabs",       { { 0, "....X.......X...", "a", DivSixteenth, 0.2f }, { 1, "x..x..x.x..x.x..", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Ska Upstrokes",      { { 0, "..x...x...x...x.", "a", DivSixteenth, 0.12f, 1 } } },
            { "Shuffle Blues",      { { 0, "x.xx.xx.xx.x", "a", DivEighthTriplet, 0.5f }, { 1, "x.xx.xx.xx.x", "b", DivEighthTriplet, 0.5f, -1, 2 } } },
            { "Boogie Bass",        { { 0, "..x...x.", "a", DivEighth, 0.3f }, { 1, "xxxxxxxx", "b12t21b1", DivEighth, 0.8f, -1, 2 } } },
            { "Walking Feel",       { { 0, "...x-.....x-....", "a", DivSixteenth, 0.6f, 0, 1, 45 }, { 1, "xxxx", "b12t", DivQuarter, 0.9f, -1, 2, 45 } } },
            { "Gospel Shout",       { { 0, "X.x.X.xxX.x.X.xx", "a", DivSixteenth, 0.35f }, { 1, "x..x..x.x..x..x.", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Neo-Soul Ghost",     { { 0, "x-.o..x-.o.ox-..", "a", DivSixteenth, 0.8f, 0, 1, 35 } } },
            { "RnB Triplet Hats",   { { 0, "x-------x-------", "a", DivSixteenth, 0.9f }, { 3, "xxxxxxxxxxxx", "t", DivSixteenthTriplet, 0.3f, 1, 1, 0, false, 0.5f } } },
            { "Lo-Fi Lazy Arp",     { { 0, "x.x.x.x.x.x.x.x.", "0.1.2.t.2.1.0.1.", DivSixteenth, 0.8f, 0, 1, 55 } } },
            { "Chillhop Keys",      { { 0, "x--..x-.x--..o..", "a", DivSixteenth, 0.9f, 0, 1, 40 }, { 1, "x.......x..x....", "b", DivSixteenth, 0.8f, -1, 2, 40 } } },
            { "Trap Pluck",         { { 0, "x..x..x...x..x..", "t", DivSixteenth, 0.3f }, { 1, "x.......x.......", "b", DivSixteenth, 1.5f, -1, 2 } } },
            { "Drill Slide",        { { 0, "x..x....x..x.x..", "a", DivSixteenth, 0.35f }, { 3, "..x.....x2.....x", "t", DivSixteenth, 0.3f } } },
            { "UK Garage Chop",     { { 0, "x..x.x...x.x..x.", "a", DivSixteenth, 0.25f, 0, 1, 58 } } },
            { "Tech House Stab",    { { 0, "...x..x....x..x.", "a", DivSixteenth, 0.2f } } },
            { "Deep Chord Echo",    { { 0, "x..x..x.........", "a", DivSixteenth, 0.4f }, { 1, ".x..x..x........", "a", DivSixteenth, 0.3f, 1, 1, 0, false, 0.5f } } },
            { "Trance Arp",         { { 0, "xxxxxxxxxxxxxxxx", "01230123t3210321", DivSixteenth, 0.5f } } },
            { "Synthwave Octaves",  { { 0, "x+x+x+x+x+x+x+x+", "0123", DivSixteenth, 0.5f }, { 1, "x---", "b", DivQuarter, 1.0f, -1, 2 } } },
            { "Afro House",         { { 0, "x..x..x...x.x...", "a", DivSixteenth, 0.3f }, { 1, "x...x..x..x...x.", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Dembow",             { { 0, "x..x..x.x..x..x.", "a", DivSixteenth, 0.3f }, { 1, "x...x...x...x...", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Latin Tumbao",       { { 0, "x.x.xx.x.x.xx.x.", "a", DivSixteenth, 0.3f }, { 1, "...x..x....x..x.", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Highlife Guitar",    { { 0, "x.xxx.x.xx.xx.x.", "0t1t2t1t", DivSixteenth, 0.3f } } },
            { "K-Pop Bounce",       { { 0, "x..x..x.x.x..x..", "a", DivSixteenth, 0.3f }, { 1, "x...x...x...x...", "b", DivSixteenth, 0.5f, -1, 2 } } },
            { "Cinematic Ostinato", { { 0, "xxxxxxxxxxxxxxxx", "b1b1b2b2b1b1t2t2", DivSixteenth, 0.6f } } },
            { "Reich Pulse",        { { 0, "x.x.x.x.", "0", DivSixteenth, 0.4f, 0, 1, 0, true }, { 1, "x..", "t", DivSixteenth, 0.4f, 1, 1, 0, true } } },
            { "Euclid 5-12",        {}, [] (PattCatEngine& e) { euclidLayer (e, 0, 5, 12, 0, "a", 0.5f); euclidLayer (e, 1, 2, 8, 0, "b", 0.8f, 2, -1); } },
            { "Tresillo Euclid",    {}, [] (PattCatEngine& e) { euclidLayer (e, 1, 3, 8, 0, "b", 0.7f, 2, -1); euclidLayer (e, 0, 9, 16, 0, "a", 0.35f); } },
        };
        return list;
    }
}

juce::StringArray getPatternPresetNames()
{
    juce::StringArray names;
    for (const auto& p : presets())
        names.add (p.name);
    jassert (names.size() == 80);
    return names;
}

void applyPatternPreset (PattCatEngine& e, int index)
{
    const auto& list = presets();
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;

    // Keep layer names / mixer settings, reset step data
    for (int l = 0; l < 4; ++l)
    {
        e.clearLayer (l);
        auto& layer = e.layers[(size_t) l];
        layer.length = 16;
        layer.division = DivSixteenth;
        layer.swing = 0;
        layer.isPolyrhythmic = false;
        layer.velocityScale = 1.0f;
    }

    const auto& p = list[(size_t) index];
    for (const auto& spec : p.layers)
        setLayer (e, spec);
    if (p.extra)
        p.extra (e);
}

} // namespace pattcat
