#include "Lyrics.h"

namespace singcat
{
namespace
{
    // Vowel-chart positions (x 0 front .. 1 back, y 0 close .. 1 open)
    struct V { float x, y; };
    constexpr V vI { 0.06f, 0.06f }, vIh { 0.10f, 0.18f }, vE { 0.16f, 0.40f }, vA { 0.55f, 0.95f }, vAw { 0.90f, 0.70f },
                vO { 0.93f, 0.40f }, vOh { 0.90f, 0.55f }, vU { 0.95f, 0.06f }, vUh { 0.55f, 0.50f }, vEr { 0.55f, 0.45f };

    Phone plosive (Place place, bool voiced, float colour, float dur)
    {
        Phone p; p.kind = PhoneKind::Plosive; p.place = place; p.voiced = voiced; p.colour = colour; p.dur = dur; return p;
    }
    Phone fricative (Place place, bool voiced, float area, float colour, float dur)
    {
        Phone p; p.kind = PhoneKind::Fricative; p.place = place; p.voiced = voiced; p.area = area; p.colour = colour; p.dur = dur; return p;
    }
    Phone nasal (Place place, float dur)
    {
        Phone p; p.kind = PhoneKind::Nasal; p.place = place; p.dur = dur; return p;
    }
    Phone liquid (Place place, float area, float dur, V target = vUh, bool round = false)
    {
        Phone p; p.kind = PhoneKind::Liquid; p.place = place; p.area = area; p.dur = dur; p.x = target.x; p.y = target.y; p.round = round; return p;
    }
    Phone glide (V target, bool round, float dur)
    {
        Phone p; p.kind = PhoneKind::Glide; p.x = target.x; p.y = target.y; p.round = round; p.dur = dur; return p;
    }
    Phone aspirate()
    {
        Phone p; p.kind = PhoneKind::Aspirate; p.voiced = false; p.dur = 0.07f; return p;
    }

    /** Consonant spelling -> phones (some letters make two). */
    void consonantPhones (const juce::String& c, bool voicedTh, std::vector<Phone>& out)
    {
        if (c == "p")       out.push_back (plosive (Lips, false, 0.40f, 0.070f));
        else if (c == "b")  out.push_back (plosive (Lips, true, 0.40f, 0.060f));
        else if (c == "t")  out.push_back (plosive (Alveolar, false, 0.80f, 0.065f));
        else if (c == "d")  out.push_back (plosive (Alveolar, true, 0.70f, 0.055f));
        else if (c == "k" || c == "q") out.push_back (plosive (Velar, false, 0.55f, 0.070f));
        else if (c == "g")  out.push_back (plosive (Velar, true, 0.55f, 0.060f));
        else if (c == "f")  out.push_back (fricative (Lips, false, 0.10f, 0.30f, 0.090f));
        else if (c == "v")  out.push_back (fricative (Lips, true, 0.12f, 0.30f, 0.070f));
        else if (c == "th") out.push_back (fricative (Alveolar, voicedTh, 0.15f, 0.35f, voicedTh ? 0.060f : 0.080f));
        else if (c == "s")  out.push_back (fricative (Alveolar, false, 0.06f, 1.00f, 0.100f));
        else if (c == "z")  out.push_back (fricative (Alveolar, true, 0.07f, 0.90f, 0.080f));
        else if (c == "sh") out.push_back (fricative (Palatal, false, 0.12f, 0.70f, 0.100f));
        else if (c == "zh") out.push_back (fricative (Palatal, true, 0.13f, 0.65f, 0.080f));
        else if (c == "ch") { out.push_back (plosive (Palatal, false, 0.70f, 0.050f)); out.push_back (fricative (Palatal, false, 0.12f, 0.70f, 0.060f)); }
        else if (c == "j")  { out.push_back (plosive (Palatal, true, 0.65f, 0.045f)); out.push_back (fricative (Palatal, true, 0.13f, 0.65f, 0.050f)); }
        else if (c == "x")  { out.push_back (plosive (Velar, false, 0.55f, 0.060f)); out.push_back (fricative (Alveolar, false, 0.06f, 1.0f, 0.080f)); }
        else if (c == "qu") { out.push_back (plosive (Velar, false, 0.55f, 0.060f)); out.push_back (glide (vU, true, 0.050f)); }
        else if (c == "h")  out.push_back (aspirate());
        else if (c == "m")  out.push_back (nasal (Lips, 0.070f));
        else if (c == "n")  out.push_back (nasal (Alveolar, 0.065f));
        else if (c == "ng") out.push_back (nasal (Velar, 0.070f));
        else if (c == "l")  out.push_back (liquid (Alveolar, 0.30f, 0.060f));
        else if (c == "r")  out.push_back (liquid (Palatal, 0.60f, 0.065f, vEr, true));
        else if (c == "w")  out.push_back (glide (vU, true, 0.060f));
        else if (c == "y")  out.push_back (glide (vI, false, 0.055f));
    }

    bool isVowelLetter (juce::juce_wchar c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

    struct Unit { bool vowel; juce::String text; bool longVowel = false; };

    /** Splits a word into consonant units (digraph aware) and vowel groups. */
    std::vector<Unit> unitsOf (const juce::String& w)
    {
        std::vector<Unit> u;
        const int n = w.length();
        int i = 0;
        while (i < n)
        {
            const auto c = w[i];
            const auto next = i + 1 < n ? w[i + 1] : 0;
            const bool yIsVowel = c == 'y' && i > 0 && (i + 1 >= n || ! isVowelLetter (next));

            if (isVowelLetter (c) || yIsVowel)
            {
                if (w.substring (i, i + 3) == "igh") { u.push_back ({ true, "igh" }); i += 3; continue; }
                int j = i + 1;
                while (j < n && (isVowelLetter (w[j]) || (w[j] == 'y' && (j + 1 >= n || ! isVowelLetter (w[j + 1]))))
                       && j - i < 2)
                    ++j;
                // "w" after a vowel is part of the vowel (ow, aw, ew)
                if (j < n && w[j] == 'w' && j - i == 1 && (j + 1 >= n || ! isVowelLetter (w[j + 1])))
                    ++j;
                u.push_back ({ true, w.substring (i, j) });
                i = j;
                continue;
            }

            for (auto* d : { "tch", "sh", "ch", "th", "ph", "ng", "ck", "wh", "qu", "gh" })
                if (w.substring (i, i + (int) strlen (d)) == d)
                {
                    juce::String s (d);
                    if (s == "tch") s = "ch";
                    else if (s == "ph") s = "f";
                    else if (s == "ck") s = "k";
                    else if (s == "wh") s = "w";
                    else if (s == "gh") s = "";               // silent in "light", "night"
                    if (s.isNotEmpty()) u.push_back ({ false, s });
                    i += (int) strlen (d);
                    goto nextLetter;
                }

            if (c == 'c')
                u.push_back ({ false, (next == 'e' || next == 'i' || next == 'y') ? "s" : "k" });
            else if (c >= 'a' && c <= 'z')
                u.push_back ({ false, juce::String::charToString (c) });
            ++i;
        nextLetter:;
        }

        // Trailing h after a vowel ("ah", "oh", "uh") just colours the vowel
        if (u.size() >= 2 && ! u.back().vowel && u.back().text == "h" && u[u.size() - 2].vowel)
            u.pop_back();

        // Silent final e: makes the previous vowel long (time, home, make, tune; also "shines", "times")
        int vowels = 0;
        for (const auto& x : u) vowels += x.vowel ? 1 : 0;
        if (vowels >= 2 && u.size() >= 4 && ! u.back().vowel && u.back().text == "s"
            && u[u.size() - 2].vowel && u[u.size() - 2].text == "e" && ! u[u.size() - 3].vowel)
        {
            u.erase (u.end() - 2);
            for (int k = (int) u.size() - 1; k >= 0; --k)
                if (u[(size_t) k].vowel) { u[(size_t) k].longVowel = true; break; }
            return u;
        }
        if (vowels >= 2 && u.size() >= 3 && u.back().vowel && u.back().text == "e" && ! u[u.size() - 2].vowel)
        {
            u.pop_back();
            for (int k = (int) u.size() - 1; k >= 0; --k)
                if (u[(size_t) k].vowel) { u[(size_t) k].longVowel = true; break; }
        }
        return u;
    }

    void setVowel (Syllable& s, const juce::String& g, bool longV, bool wordFinal, bool openSyllable, bool singleVowelWord, bool rColoured, const juce::String& word)
    {
        auto mono = [&s] (V v) { s.vx = v.x; s.vy = v.y; s.diphthong = false; };
        auto di = [&s] (V a, V b) { s.vx = a.x; s.vy = a.y; s.v2x = b.x; s.v2y = b.y; s.diphthong = true; };

        if (rColoured)
        {
            if (g == "a") mono (vA);
            else if (g == "o" || g == "oo") mono (vO);
            else mono (vEr);
            return;
        }
        if (word == "you" || word == "your") { mono (vU); return; }
        if (word == "the" || word == "a") { mono (vUh); return; }

        if (g == "ee" || g == "ea") mono (vI);
        else if (g == "ie") wordFinal && singleVowelWord ? di (vA, vI) : mono (vI);
        else if (g == "igh") di (vA, vI);
        else if (g == "ai" || g == "ay" || g == "ei" || g == "ey") di (vE, vI);
        else if (g == "oo" || g == "ue" || g == "ew" || g == "ui") mono (vU);
        else if (g == "ou" || g == "ow") di (vA, vU);
        else if (g == "oa" || g == "oe") di (vO, vU);
        else if (g == "oi" || g == "oy") di (vO, vI);
        else if (g == "au" || g == "aw") mono (vAw);
        else
        {
            const auto c = g[0];
            if (c == 'a') longV ? di (vE, vI) : mono (vA);
            else if (c == 'e') (longV || (wordFinal && singleVowelWord)) ? mono (vI) : mono (vE);
            else if (c == 'i') longV ? di (vA, vI) : mono (vIh);
            else if (c == 'o') (longV || openSyllable) ? di (vO, vU) : mono (vOh);
            else if (c == 'u') (longV || openSyllable) ? mono (vU) : mono (vUh);
            else if (c == 'y') (wordFinal && singleVowelWord) ? di (vA, vI) : mono (vI);
            else mono (vA);
        }
    }

    bool voicedThWord (const juce::String& w)
    {
        for (auto* x : { "the", "this", "that", "there", "they", "them", "then", "than", "those", "these", "though", "thee", "thy", "thou", "with" })
            if (w == x) return true;
        return false;
    }

    void addPhones (std::array<Phone, 4>& dest, int& count, const std::vector<Phone>& phones)
    {
        for (const auto& p : phones)
            if (count < 4) dest[(size_t) count++] = p;
    }

    void addSyllablesForWord (const juce::String& word, std::vector<Syllable>& out, bool fragment, bool lastPart)
    {
        auto units = unitsOf (word);
        std::vector<int> nuclei;
        for (int i = 0; i < (int) units.size(); ++i)
            if (units[(size_t) i].vowel) nuclei.push_back (i);

        const bool voicedTh = voicedThWord (word);
        for (auto* m : { "meow", "miaow", "miau", "miao", "mew", "meo" })
            if (word == m)
            {
                // the cat word: m - ee - aaa - ow
                Syllable s;
                s.onset[0] = nasal (Lips, 0.06f);
                s.onset[1] = glide (vI, false, 0.09f);
                s.numOnset = 2;
                s.vx = vA.x; s.vy = vA.y;
                s.diphthong = true; s.v2x = vU.x; s.v2y = vU.y;
                word.copyToUTF8 (s.text, sizeof (s.text));
                out.push_back (s);
                return;
            }

        if (nuclei.empty())
        {
            // No vowel ("hmm", "shh", "mm"): hum on the consonants with a neutral vowel
            Syllable s;
            std::vector<Phone> ph;
            for (const auto& u : units) consonantPhones (u.text, voicedTh, ph);
            const auto nasalIt = std::find_if (ph.begin(), ph.end(), [] (const Phone& p) { return p.kind == PhoneKind::Nasal; });
            if (nasalIt != ph.end())
            {
                // "hmm", "mm", "nn": the nasal itself is held (closed-mouth hum)
                s.hum = true;
                s.humPhone = *nasalIt;
                s.vx = vUh.x; s.vy = vUh.y;
                std::vector<Phone> before (ph.begin(), nasalIt);
                addPhones (s.onset, s.numOnset, before);
            }
            else
                addPhones (s.onset, s.numOnset, ph);
            word.copyToUTF8 (s.text, sizeof (s.text));
            out.push_back (s);
            return;
        }

        const bool singleVowelWord = nuclei.size() == 1 && ! fragment;
        // Consonants between nuclei: one goes to the next syllable, two or more are split
        std::vector<std::pair<int, int>> ranges;   // [start, end) units per syllable
        int start = 0;
        for (size_t k = 0; k < nuclei.size(); ++k)
        {
            int end;
            if (k + 1 < nuclei.size())
            {
                const int between = nuclei[k + 1] - nuclei[k] - 1;
                end = nuclei[k] + 1 + (between >= 2 ? 1 : 0);
            }
            else
                end = (int) units.size();
            ranges.push_back ({ start, end });
            start = end;
        }

        for (size_t k = 0; k < ranges.size(); ++k)
        {
            Syllable s;
            const int nucleus = nuclei[k];
            juce::String text;
            for (int i = ranges[k].first; i < ranges[k].second; ++i) text << units[(size_t) i].text;
            text.copyToUTF8 (s.text, sizeof (s.text));

            // r right after the vowel colours it
            bool rColoured = false;
            for (int i = nucleus + 1; i < ranges[k].second; ++i)
                if (units[(size_t) i].text == "r") { rColoured = true; break; }

            const bool wordFinal = lastPart && k + 1 == ranges.size() && nucleus + 1 >= ranges[k].second;
            const bool openSyllable = nucleus + 1 >= ranges[k].second;
            setVowel (s, units[(size_t) nucleus].text, units[(size_t) nucleus].longVowel, wordFinal, openSyllable, singleVowelWord, rColoured, word);

            std::vector<Phone> onset, coda;
            for (int i = ranges[k].first; i < nucleus; ++i) consonantPhones (units[(size_t) i].text, voicedTh, onset);
            for (int i = nucleus + 1; i < ranges[k].second; ++i) consonantPhones (units[(size_t) i].text, voicedTh, coda);
            addPhones (s.onset, s.numOnset, onset);
            addPhones (s.coda, s.numCoda, coda);
            out.push_back (s);
        }
    }
}

std::vector<Syllable> parseLyrics (const juce::String& text)
{
    std::vector<Syllable> out;
    juce::String clean;
    for (auto c : text.toLowerCase())
    {
        if ((c >= 'a' && c <= 'z') || c == '+' || c == '-') clean << juce::String::charToString (c);
        else if (c == '\'') continue;
        else clean << " ";
    }

    for (auto token : juce::StringArray::fromTokens (clean.replace ("+", " + "), " ", ""))
    {
        token = token.trim();
        if (token.isEmpty()) continue;
        if (token == "+")
        {
            Syllable s;
            s.hold = true;
            juce::String ("+").copyToUTF8 (s.text, sizeof (s.text));
            out.push_back (s);
            continue;
        }
        juce::StringArray parts;
        parts.addTokens (token, "-", "");
        parts.removeEmptyStrings();
        for (int i = 0; i < parts.size(); ++i)
            addSyllablesForWord (parts[i], out, parts.size() > 1, i == parts.size() - 1);
        if (out.size() > 512) break;
    }
    return out;
}

} // namespace singcat
