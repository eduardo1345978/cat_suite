#include "Tunings.h"

namespace micro
{
namespace
{
    double ratioCents (double num, double den) { return 1200.0 * std::log2 (num / den); }

    Tuning equal (int n, const juce::String& desc = {})
    {
        Tuning t;
        t.name = juce::String (n) + "-EDO";
        if (n == 12) t.name = "12-EDO (normal)";
        t.category = "Equal"; t.categoryId = CatEqual;
        t.description = desc.isNotEmpty() ? desc : juce::String (n) + " equal steps per octave, " + juce::String (1200.0 / n, 2) + " cents each";
        for (int k = 1; k <= n; ++k) t.cents.push_back (1200.0 * k / n);
        t.mapping = n == 12 ? MapTwelve : MapPerStep;
        return t;
    }

    Tuning fromRatios (const juce::String& name, int cat, const juce::String& desc, std::initializer_list<std::pair<int, int>> ratios, int mapping)
    {
        Tuning t;
        t.name = name; t.categoryId = cat; t.category = categoryNames()[cat]; t.description = desc; t.mapping = mapping;
        for (auto [a, b] : ratios) t.cents.push_back (ratioCents (a, b));
        return t;
    }

    Tuning fromCents (const juce::String& name, int cat, const juce::String& desc, std::initializer_list<double> cents, int mapping)
    {
        Tuning t;
        t.name = name; t.categoryId = cat; t.category = categoryNames()[cat]; t.description = desc; t.mapping = mapping;
        t.cents.assign (cents.begin(), cents.end());
        return t;
    }

    /** A 12-key tuning: 12-EDO with some keys moved (key 0 = C .. 11 = B), in cents. */
    Tuning twelveWith (const juce::String& name, int cat, const juce::String& desc, std::initializer_list<std::pair<int, double>> moved)
    {
        Tuning t;
        t.name = name; t.categoryId = cat; t.category = categoryNames()[cat]; t.description = desc; t.mapping = MapTwelve;
        for (int k = 1; k <= 12; ++k) t.cents.push_back (100.0 * k);
        for (auto [key, c] : moved) if (key >= 1 && key <= 11) t.cents[(size_t) key - 1] = c;
        return t;
    }
}

juce::StringArray categoryNames() { return { "Equal", "Just & historical", "World", "My tunings" }; }

double Tuning::degreeCents (int degree) const noexcept
{
    if (cents.empty()) return 100.0 * degree;
    const int n = (int) cents.size();
    const int octave = (int) std::floor ((double) degree / n);
    const int d = degree - octave * n;
    return octave * period() + (d == 0 ? 0.0 : cents[(size_t) d - 1]);
}

bool Tuning::isValid (juce::String* why) const
{
    auto fail = [why] (const juce::String& w) { if (why != nullptr) *why = w; return false; };
    if (cents.empty()) return fail ("no notes");
    if ((int) cents.size() > TuningData::maxNotes) return fail ("more than 128 notes");
    double last = 0.0;
    for (auto c : cents)
    {
        if (! std::isfinite (c) || c <= last) return fail ("every note must be higher than the one before");
        last = c;
    }
    if (period() < 10.0 || period() > 4800.0) return fail ("the period must be between 10 and 4800 cents");
    if (mapping == MapTwelve && cents.size() != 12) return fail ("the 12-key layout needs exactly 12 notes");
    return true;
}

const std::vector<Tuning>& library()
{
    static const std::vector<Tuning> lib = []
    {
        std::vector<Tuning> v;
        // ---- equal
        v.push_back (equal (12, "The normal tuning: 12 equal half steps"));
        v.push_back (equal (5, "5 equal steps: a pentatonic world of its own (close to Indonesian slendro)"));
        v.push_back (equal (7, "7 equal steps: close to Thai classical tuning"));
        v.push_back (equal (10));
        v.push_back (equal (15));
        v.push_back (equal (16));
        v.push_back (equal (17, "17 steps: sharp, bright major thirds, a neutral second"));
        v.push_back (equal (19, "19 steps: sweet thirds, like meantone; C# and Db are different notes"));
        v.push_back (equal (22, "22 steps: very pure 7-limit harmony, odd and beautiful"));
        v.push_back (equal (24, "Quarter tones: every half step split in two (Arabic music)"));
        v.push_back (equal (31, "31 steps: almost pure thirds and the 7th harmonic (Huygens, Fokker)"));
        v.push_back (equal (41, "41 steps: very close to just intonation"));
        v.push_back (equal (53, "53 steps (commas): almost perfect fifths and thirds, used in Turkish theory"));
        v.push_back (equal (72, "72 steps: twelfth tones, used by Ivor Darreg and in Byzantine chant"));
        {
            Tuning bp;
            bp.name = "Bohlen-Pierce"; bp.categoryId = CatEqual; bp.category = "Equal"; bp.mapping = MapPerStep;
            bp.description = "13 equal steps of a tritave (3:1) - no octaves at all, a clarinet-like world";
            for (int k = 1; k <= 13; ++k) bp.cents.push_back (ratioCents (3, 1) * k / 13.0);
            v.push_back (bp);
        }
        v.push_back (fromCents ("Carlos Alpha", CatEqual, "Wendy Carlos: 78-cent steps that never reach an octave, perfect thirds and fifths", { 77.965 }, MapPerStep));
        // ---- just & historical (12-key layouts on C)
        v.push_back (fromRatios ("Just intonation (5-limit)", CatJust, "Pure thirds and fifths on C - glowing chords in C, out of tune far away",
                                 { {16,15},{9,8},{6,5},{5,4},{4,3},{45,32},{3,2},{8,5},{5,3},{9,5},{15,8},{2,1} }, MapTwelve));
        v.push_back (fromRatios ("Just intonation (7-limit)", CatJust, "With the 7th harmonic: bluesy 7ths (7:4) and tritone (7:5)",
                                 { {15,14},{8,7},{6,5},{5,4},{4,3},{7,5},{3,2},{8,5},{5,3},{7,4},{15,8},{2,1} }, MapTwelve));
        v.push_back (fromRatios ("Pythagorean", CatJust, "Stacked pure fifths (3:2): bright, wide thirds, medieval",
                                 { {256,243},{9,8},{32,27},{81,64},{4,3},{729,512},{3,2},{128,81},{27,16},{16,9},{243,128},{2,1} }, MapTwelve));
        v.push_back (fromCents ("Meantone (1/4 comma)", CatJust, "Renaissance keyboards: pure major thirds, a wolf fifth G#-Eb",
                                { 76.049, 193.157, 310.265, 386.314, 503.422, 579.471, 696.578, 772.627, 889.735, 1006.843, 1082.892, 1200.0 }, MapTwelve));
        v.push_back (fromCents ("Werckmeister III", CatJust, "Bach's time: every key works, each with its own colour",
                                { 90.225, 192.18, 294.135, 390.225, 498.045, 588.27, 696.09, 792.18, 888.27, 996.09, 1092.18, 1200.0 }, MapTwelve));
        v.push_back (fromCents ("Kirnberger III", CatJust, "A well temperament with a pure C major third",
                                { 90.225, 193.157, 294.135, 386.314, 498.045, 590.224, 696.578, 792.18, 889.735, 996.09, 1088.269, 1200.0 }, MapTwelve));
        v.push_back (fromCents ("Vallotti", CatJust, "A gentle well temperament, loved for baroque music",
                                { 94.135, 196.09, 298.045, 392.18, 501.955, 592.18, 698.045, 796.09, 894.135, 1000.0, 1090.225, 1200.0 }, MapTwelve));
        v.push_back (fromCents ("Young (1799)", CatJust, "Thomas Young's well temperament",
                                { 90.225, 196.09, 294.135, 392.18, 498.045, 588.27, 698.045, 792.18, 894.135, 996.09, 1090.225, 1200.0 }, MapTwelve));
        v.push_back (fromRatios ("Harmonic series 8-16", CatJust, "The overtones 8 to 16 as a scale: the sound of nature (and of throat singing)",
                                 { {9,8},{10,8},{11,8},{12,8},{13,8},{14,8},{15,8},{16,8} }, MapPerStep));
        v.push_back (fromRatios ("Ptolemy diatonic", CatJust, "A pure 7-note major scale: every next key is the next note",
                                 { {9,8},{5,4},{4,3},{3,2},{5,3},{15,8},{2,1} }, MapPerStep));
        // ---- world
        v.push_back (twelveWith ("Maqam Rast (on C)", CatWorld, "Arabic: E and B a quarter tone flat (play C D E F G A B)",
                                 { {4, 350.0}, {11, 1050.0} }));
        v.push_back (twelveWith ("Maqam Bayati (on D)", CatWorld, "Arabic: D, E half-flat, F, G, A, Bb, C (play from D)",
                                 { {4, 350.0} }));
        v.push_back (twelveWith ("Maqam Saba (on D)", CatWorld, "Arabic: D, E half-flat, F, Gb, A, Bb, C - sad and longing (play from D)",
                                 { {4, 350.0} }));
        v.push_back (twelveWith ("Maqam Sikah (on E half-flat)", CatWorld, "Arabic: starts on E half-flat (play from E); B half-flat too",
                                 { {4, 350.0}, {11, 1050.0} }));
        {
            const double c = 1200.0 / 53.0;   // a Holdrian comma
            v.push_back (fromCents ("Turkish makam (Arel-Ezgi)", CatWorld, "Turkish theory in 53 commas, on 12 keys: tones of 9 commas, small half steps of 4",
                                    { 4 * c, 9 * c, 13 * c, 17 * c, 22 * c, 26 * c, 31 * c, 35 * c, 40 * c, 44 * c, 48 * c, 1200.0 }, MapTwelve));
        }
        v.push_back (fromRatios ("Indian 22 shruti", CatWorld, "The 22 microtones of Indian classical music, one per key",
                                 { {256,243},{16,15},{10,9},{9,8},{32,27},{6,5},{5,4},{81,64},{4,3},{27,20},{45,32},{729,512},{3,2},{128,81},{8,5},{5,3},{27,16},{16,9},{9,5},{15,8},{243,128},{2,1} }, MapPerStep));
        v.push_back (fromCents ("Gamelan slendro", CatWorld, "Javanese gamelan: 5 nearly equal steps (a real set measured, approx.)",
                                { 231.0, 474.0, 717.0, 955.0, 1200.0 }, MapPerStep));
        v.push_back (fromCents ("Gamelan pelog", CatWorld, "Javanese gamelan: 7 uneven steps (a real set measured, approx.)",
                                { 120.0, 270.0, 540.0, 670.0, 785.0, 950.0, 1200.0 }, MapPerStep));
        {
            auto thai = equal (7, "Thai classical music: 7 equal steps per octave");
            thai.name = "Thai (7 equal)"; thai.categoryId = CatWorld; thai.category = categoryNames()[CatWorld];
            v.push_back (thai);
        }
        v.push_back (fromRatios ("Chinese pentatonic", CatWorld, "Gong shang jue zhi yu, from pure fifths (one per key)",
                                 { {9,8},{81,64},{3,2},{27,16},{2,1} }, MapPerStep));
        v.push_back (fromCents ("Japanese koto (hirajoshi)", CatWorld, "A koto tuning: 5 notes with small half steps (one per key)",
                                { 200.0, 300.0, 700.0, 800.0, 1200.0 }, MapPerStep));
        return v;
    }();
    return lib;
}

//==============================================================================
bool parsePitch (const juce::String& token, double& cents)
{
    const auto t = token.trim().upToFirstOccurrenceOf (" ", false, false).trim();
    if (t.isEmpty()) return false;
    if (t.containsChar ('.'))
    {
        if (! t.containsOnly ("-0123456789.")) return false;
        cents = t.getDoubleValue();
        return std::isfinite (cents);
    }
    if (! t.containsOnly ("0123456789/")) return false;
    const double num = t.upToFirstOccurrenceOf ("/", false, false).getDoubleValue();
    const double den = t.containsChar ('/') ? t.fromFirstOccurrenceOf ("/", false, false).getDoubleValue() : 1.0;
    if (num <= 0.0 || den <= 0.0) return false;
    cents = ratioCents (num, den);
    return true;
}

bool parseScl (const juce::String& text, Tuning& out, juce::String* error)
{
    juce::StringArray lines;
    lines.addLines (text);
    juce::StringArray body, abl;
    for (auto& l : lines)
    {
        const auto tl = l.trim();
        if (! tl.startsWithChar ('!')) { body.add (l); continue; }                 // ! = a comment
        const auto rest = tl.substring (1).trimStart();                              // "!! @ABL" = switched off
        if (rest.startsWith ("@ABL ")) abl.add (rest.substring (5).trim());
    }
    if (body.size() < 2) { if (error) *error = "not a Scala file"; return false; }
    Tuning t;
    t.description = body[0].trim();
    const int count = body[1].trim().getIntValue();
    if (count < 1 || count > TuningData::maxNotes) { if (error) *error = "the number of notes must be 1 - 128"; return false; }
    for (int i = 0; i < count; ++i)
    {
        double c = 0.0;
        if (! parsePitch (body[2 + i], c)) { if (error) *error = "can't read note " + juce::String (i + 1) + ": " + body[2 + i].trim(); return false; }
        t.cents.push_back (c);
    }
    t.name = t.description.isNotEmpty() ? t.description.upToFirstOccurrenceOf (" - ", false, false).substring (0, 40) : juce::String ("My tuning");
    t.category = categoryNames()[CatMine]; t.categoryId = CatMine;
    t.mapping = count == 12 && std::abs (t.period() - 1200.0) < 1e-6 ? MapTwelve : MapPerStep;
    juce::String why;
    if (! t.isValid (&why)) { if (error) *error = why; return false; }

    // Ableton .ascl directives
    for (auto& d : abl)
    {
        juce::StringArray tok;
        tok.addTokens (d, " \t", "\"");
        tok.removeEmptyStrings();
        for (auto& s : tok) s = s.unquoted();
        if (tok.isEmpty()) continue;
        if (tok[0] == "REFERENCE_PITCH" && tok.size() >= 4)
        {
            const int degree = tok[2].getIntValue();
            const double hz = tok[3].getDoubleValue();
            if (degree >= 0 && degree < count && hz > 0.0 && std::isfinite (hz))
                t.referenceHz = hz / std::pow (2.0, t.degreeCents (degree) / 1200.0);   // the first note of that octave
        }
        else if (tok[0] == "NOTE_NAMES")
        {
            tok.remove (0);
            if (tok.size() == count) t.noteNames = tok;
        }
        else if (tok[0] == "SOURCE" && t.description.isEmpty()) t.description = d.substring (6).trim();
    }
    out = t;
    return true;
}

juce::String toScl (const Tuning& t)
{
    juce::String s;
    s << "! " << juce::File::createLegalFileName (t.name) << ".scl\n";
    s << "! made with Micro-Cat (Cat Suite)\n";
    s << t.name << (t.description.isNotEmpty() ? " - " + t.description : juce::String()) << "\n";
    s << (int) t.cents.size() << "\n!\n";
    for (auto c : t.cents) s << " " << juce::String (c, 5) << "\n";
    return s;
}

bool rootFromReference (const Tuning& t, int& rootKey, double& rootHz)
{
    if (! (t.referenceHz > 0.0) || ! t.isValid()) return false;
    double hz = t.referenceHz;
    const double period = std::pow (2.0, t.period() / 1200.0);
    // the first note's octave nearest middle C (C4 = 261.63 Hz), so the root key is where you'd expect it
    while (hz < 261.6255653 / std::sqrt (period)) hz *= period;
    while (hz > 261.6255653 * std::sqrt (period)) hz /= period;
    // 12 notes to the octave: the key it's nearest to (a C stays on C); other tunings start on middle C
    const bool twelve = t.notesPerPeriod() == 12 && std::abs (t.period() - 1200.0) < 1e-6;
    rootKey = twelve ? juce::jlimit (0, 127, juce::roundToInt (69.0 + 12.0 * std::log2 (hz / 440.0))) : 60;
    rootHz = juce::jlimit (20.0, 2000.0, hz);
    return true;
}

juce::File userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("CatSuite").getChildFile ("Micro-Cat").getChildFile ("Tunings");
}

std::vector<Tuning> loadUserTunings()
{
    std::vector<Tuning> v;
    for (auto& f : userFolder().findChildFiles (juce::File::findFiles, false, "*.scl;*.ascl"))
    {
        Tuning t;
        if (parseScl (f.loadFileAsString(), t))
        {
            t.name = f.getFileNameWithoutExtension();
            v.push_back (t);
        }
    }
    std::sort (v.begin(), v.end(), [] (const Tuning& a, const Tuning& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    return v;
}

bool saveUserTuning (const Tuning& t)
{
    if (! t.isValid()) return false;
    userFolder().createDirectory();
    return userFolder().getChildFile (juce::File::createLegalFileName (t.name) + ".scl").replaceWithText (toScl (t));
}

//==============================================================================
TuningData TuningData::from (const Tuning& t)
{
    TuningData d;
    d.count = juce::jlimit (1, maxNotes, (int) t.cents.size());
    for (int i = 0; i < d.count; ++i) d.cents[(size_t) i] = t.cents[(size_t) i];
    d.mapping = t.mapping == MapTwelve && d.count == 12 ? MapTwelve : MapPerStep;
    return d;
}

KeyPitch pitchFor (const TuningData& d, int key, int rootKey, double rootHz) noexcept
{
    KeyPitch k;
    const int n = juce::jmax (1, d.count);
    const int steps = key - rootKey;
    const int periods = (int) std::floor ((double) steps / n);
    const int deg = steps - periods * n;
    const double period = d.cents[(size_t) n - 1];
    const double cents = periods * period + (deg == 0 ? 0.0 : d.cents[(size_t) deg - 1]);
    k.hz = rootHz * std::pow (2.0, cents / 1200.0);
    const double semis = 69.0 + 12.0 * std::log2 (k.hz / 440.0);
    k.note = (int) std::lround (semis);
    k.bend = semis - k.note;
    k.valid = k.note >= 0 && k.note <= 127 && std::isfinite (semis);
    if (! k.valid) k.note = juce::jlimit (0, 127, k.note);
    return k;
}
} // namespace micro
