#include "Phonetics.h"
#include <map>
#include <set>

namespace talkcat
{
juce::StringArray languageNames() { return { "English", "Slovak", "Japanese", "Cat" }; }

namespace
{
    using Phones = std::vector<Phone>;

    Phone mk (const char* sym, PK kind, uint8_t place, bool voiced, float dur)
    {
        Phone p;
        p.kind = kind; p.place = place; p.voiced = voiced; p.dur = dur;
        juce::String (sym).copyToUTF8 (p.sym, sizeof (p.sym));
        return p;
    }
    Phone vowel (const char* sym, float x, float y, float lips = 0.0f, bool rhotic = false)
    {
        Phone p = mk (sym, PK::Vowel, Lips, true, 0.1f);
        p.x = x; p.y = y; p.lips = lips; p.rhotic = rhotic;
        return p;
    }
    Phone plosive (const char* sym, uint8_t place, bool voiced, float colour, float dur)
    {
        Phone p = mk (sym, PK::Plosive, place, voiced, dur); p.colour = colour; return p;
    }
    Phone fric (const char* sym, uint8_t place, bool voiced, float area, float colour, float dur)
    {
        Phone p = mk (sym, PK::Fricative, place, voiced, dur); p.area = area; p.colour = colour; return p;
    }
    Phone nasal (const char* sym, uint8_t place, float dur) { return mk (sym, PK::Nasal, place, true, dur); }
    Phone glide (const char* sym, float x, float y, float lips, float dur)
    {
        Phone p = mk (sym, PK::Glide, Lips, true, dur); p.x = x; p.y = y; p.lips = lips; return p;
    }

    struct Table
    {
        std::map<juce::String, Phones> phones;       // symbol -> one or more phones (affricates are two)
        std::map<juce::String, std::pair<juce::String, juce::String>> diphthongs;
        std::set<juce::String> vowels;
    };

    const Table& table()
    {
        static const Table t = []
        {
            Table tb;
            auto add = [&tb] (const char* s, Phones p) { tb.phones[s] = std::move (p); };
            auto addV = [&tb] (const Phone& p) { tb.phones[p.sym] = { p }; tb.vowels.insert (p.sym); };

            // ---- vowels (vowel chart: x 0 front .. 1 back, y 0 close .. 1 open)
            addV (vowel ("iy", 0.06f, 0.06f));
            addV (vowel ("i",  0.06f, 0.08f));
            addV (vowel ("ih", 0.12f, 0.22f));
            addV (vowel ("e",  0.16f, 0.40f));
            addV (vowel ("eh", 0.20f, 0.58f));
            addV (vowel ("ae", 0.24f, 0.82f));
            addV (vowel ("a",  0.42f, 0.95f));
            addV (vowel ("aa", 0.62f, 0.95f));
            addV (vowel ("ah", 0.52f, 0.68f));
            addV (vowel ("ao", 0.88f, 0.66f));
            addV (vowel ("o",  0.93f, 0.40f));
            addV (vowel ("uh", 0.80f, 0.22f));
            addV (vowel ("uw", 0.95f, 0.06f, -0.2f));
            addV (vowel ("u",  0.95f, 0.06f));
            addV (vowel ("ux", 0.72f, 0.10f, 0.9f));           // Japanese u (unrounded)
            addV (vowel ("ax", 0.42f, 0.50f));                 // schwa
            addV (vowel ("er", 0.55f, 0.42f, -0.4f, true));    // r-coloured
            tb.diphthongs["ay"] = { "aa", "ih" };
            tb.diphthongs["aw"] = { "aa", "uh" };
            tb.diphthongs["ey"] = { "e", "ih" };
            tb.diphthongs["ow"] = { "o", "uh" };
            tb.diphthongs["oy"] = { "ao", "ih" };
            for (auto& [k, v] : tb.diphthongs) tb.vowels.insert (k);

            // ---- plosives
            add ("p",  { plosive ("p", Lips, false, 0.40f, 0.070f) });
            add ("b",  { plosive ("b", Lips, true, 0.40f, 0.060f) });
            add ("t",  { plosive ("t", Alveolar, false, 0.80f, 0.065f) });
            add ("d",  { plosive ("d", Alveolar, true, 0.70f, 0.055f) });
            add ("k",  { plosive ("k", Velar, false, 0.55f, 0.070f) });
            add ("g",  { plosive ("g", Velar, true, 0.55f, 0.060f) });
            add ("tj", { plosive ("tj", Palatal, false, 0.85f, 0.065f) });
            add ("dj", { plosive ("dj", Palatal, true, 0.80f, 0.060f) });
            add ("q",  { mk ("q", PK::Glottal, GlottalPlace, false, 0.05f) });

            // ---- fricatives
            add ("f",  { fric ("f", Lips, false, 0.10f, 0.30f, 0.090f) });
            add ("v",  { fric ("v", Lips, true, 0.12f, 0.30f, 0.070f) });
            add ("th", { fric ("th", Alveolar, false, 0.15f, 0.35f, 0.080f) });
            add ("dh", { fric ("dh", Alveolar, true, 0.15f, 0.35f, 0.055f) });
            add ("s",  { fric ("s", Alveolar, false, 0.06f, 1.00f, 0.100f) });
            add ("z",  { fric ("z", Alveolar, true, 0.07f, 0.90f, 0.080f) });
            add ("sh", { fric ("sh", Palatal, false, 0.12f, 0.70f, 0.100f) });
            add ("zh", { fric ("zh", Palatal, true, 0.13f, 0.65f, 0.080f) });
            add ("x",  { fric ("x", Velar, false, 0.10f, 0.45f, 0.090f) });
            add ("gh", { fric ("gh", Velar, true, 0.12f, 0.45f, 0.070f) });
            add ("hh", { mk ("hh", PK::Aspirate, GlottalPlace, false, 0.070f) });
            add ("hv", { mk ("hv", PK::Aspirate, GlottalPlace, true, 0.060f) });

            // ---- affricates (closure + fricative)
            add ("ch", { plosive ("ch", Palatal, false, 0.70f, 0.050f), fric ("", Palatal, false, 0.12f, 0.70f, 0.060f) });
            add ("jh", { plosive ("jh", Palatal, true, 0.65f, 0.045f), fric ("", Palatal, true, 0.13f, 0.65f, 0.050f) });
            add ("ts", { plosive ("ts", Alveolar, false, 0.80f, 0.045f), fric ("", Alveolar, false, 0.06f, 1.00f, 0.060f) });
            add ("dz", { plosive ("dz", Alveolar, true, 0.70f, 0.040f), fric ("", Alveolar, true, 0.07f, 0.90f, 0.050f) });

            // ---- nasals
            add ("m",  { nasal ("m", Lips, 0.070f) });
            add ("n",  { nasal ("n", Alveolar, 0.065f) });
            add ("ng", { nasal ("ng", Velar, 0.070f) });
            add ("nj", { nasal ("nj", Palatal, 0.070f) });
            add ("N",  { nasal ("N", Velar, 0.080f) });

            // ---- liquids, r sounds
            { auto l = mk ("l", PK::Lateral, Alveolar, true, 0.060f); l.area = 0.30f; add ("l", { l }); }
            { auto l = mk ("lj", PK::Lateral, Palatal, true, 0.060f); l.area = 0.25f; add ("lj", { l }); }
            { auto r = mk ("r", PK::Rhotic, Palatal, true, 0.065f); r.area = 0.55f; r.x = 0.55f; r.y = 0.45f; r.lips = -0.5f; add ("r", { r }); }
            { auto r = mk ("rr", PK::Trill, Alveolar, true, 0.075f); r.area = 0.25f; add ("rr", { r }); }
            { auto r = mk ("dx", PK::Tap, Alveolar, true, 0.030f); r.area = 0.25f; add ("dx", { r }); }

            // ---- glides
            add ("w",  { glide ("w", 0.95f, 0.06f, -0.5f, 0.060f) });
            add ("y",  { glide ("y", 0.06f, 0.06f, 0.2f, 0.055f) });
            add ("wu", { glide ("wu", 0.90f, 0.12f, -0.3f, 0.050f) });   // Slovak u-glide ("krv", "prav-da")
            return tb;
        }();
        return t;
    }

    bool isVowelSym (const juce::String& s) { return table().vowels.count (s) > 0; }

    /** Puts a vowel symbol (or diphthong) into the nucleus of a syllable. */
    void setNucleus (Syllable& syl, const juce::String& sym)
    {
        const auto& tb = table();
        if (auto d = tb.diphthongs.find (sym); d != tb.diphthongs.end())
        {
            syl.nucleus = tb.phones.at (d->second.first)[0];
            syl.nucleus2 = tb.phones.at (d->second.second)[0];
            syl.diphthong = true;
            juce::String (sym).copyToUTF8 (syl.nucleus.sym, sizeof (syl.nucleus.sym));
            return;
        }
        if (auto p = tb.phones.find (sym); p != tb.phones.end())
            syl.nucleus = p->second[0];
    }

    void addPhones (Syllable& syl, const juce::String& sym, bool coda)
    {
        const auto& tb = table();
        auto it = tb.phones.find (sym);
        if (it == tb.phones.end()) return;
        for (const auto& p : it->second)
            coda ? syl.addCoda (p) : syl.addOnset (p);
    }

    bool isSonorant (const juce::String& s)
    {
        return s == "m" || s == "n" || s == "ng" || s == "nj" || s == "N" || s == "l" || s == "lj" || s == "r" || s == "rr";
    }

    /** Splits the spelling of a word into n display parts: hel|lo, twin|kle, a|bove, won|der. */
    juce::StringArray splitSpelling (const juce::String& word, int n)
    {
        juce::StringArray parts;
        if (n <= 1) { parts.add (word); return parts; }
        auto vowel = [] (juce::juce_wchar c) { return juce::String (juce::CharPointer_UTF8 ("aeiouy" "\xc3\xa1" "" "\xc3\xa4" "" "\xc3\xa9" "" "\xc3\xad" "" "\xc3\xb3" "" "\xc3\xb4" "" "\xc3\xba" "" "\xc3\xbd" "" "\xc4\x81" "" "\xc4\xab" "" "\xc5\xab" "" "\xc4\x93" "" "\xc5\x8d" "")).containsChar (juce::CharacterFunctions::toLowerCase (c)); };
        // vowel groups (a final silent e after a consonant does not count)
        std::vector<std::pair<int, int>> groups;
        const int len = word.length();
        for (int i = 0; i < len;)
        {
            if (vowel (word[i]) && ! (i == 0 && (word[i] == 'y' || word[i] == 'Y') && len > 1))
            {
                int j = i;
                while (j < len && vowel (word[j])) ++j;
                groups.push_back ({ i, j });
                i = j;
            }
            else ++i;
        }
        if ((int) groups.size() > n && len >= 2 && (word.getLastCharacter() == 'e') && ! vowel (word[len - 2])) groups.pop_back();
        if ((int) groups.size() == n)
        {
            int start = 0;
            for (int g = 0; g + 1 < n; ++g)
            {
                const int c0 = groups[(size_t) g].second, c1 = groups[(size_t) g + 1].first;   // consonant run [c0, c1)
                const int cut = c1 - c0 >= 2 ? c0 + 1 : c0;
                parts.add (word.substring (start, cut));
                start = cut;
            }
            parts.add (word.substring (start));
            return parts;
        }
        // fallback: equal shares
        int start = 0;
        for (int k = 0; k < n; ++k)
        {
            const int end = k + 1 == n ? len : juce::jmax (start + 1, len * (k + 1) / n);
            parts.add (word.substring (start, end));
            start = end;
        }
        return parts;
    }

    /** Phone symbols of one word (already split into syllables by '.'), -> syllables. */
    void symbolsToSyllables (const juce::StringArray& syms, const std::vector<int>& nucleusIdx, std::vector<int> splits,
                             const juce::String& word, std::vector<Syllable>& out, bool aspirate)
    {
        // splits[k] = first symbol index of syllable k (k > 0)
        std::vector<std::pair<int, int>> ranges;
        int start = 0;
        for (size_t k = 0; k < nucleusIdx.size(); ++k)
        {
            const int end = k + 1 < nucleusIdx.size() ? splits[k + 1] : syms.size();
            ranges.push_back ({ start, end });
            start = end;
        }
        const auto textParts = splitSpelling (word, (int) ranges.size());
        for (size_t k = 0; k < ranges.size(); ++k)
        {
            Syllable syl;
            const int nuc = nucleusIdx[k];
            for (int i = ranges[k].first; i < nuc; ++i) addPhones (syl, syms[i], false);
            if (isVowelSym (syms[nuc])) setNucleus (syl, syms[nuc]);
            else syl.nucleus = table().phones.at (syms[nuc])[0];   // syllabic consonant
            // a second vowel straight after the nucleus = diphthong (ia -> handled as glide onset)
            int i = nuc + 1;
            if (i < ranges[k].second && isVowelSym (syms[i]) && ! syl.diphthong)
            {
                Syllable tmp;
                setNucleus (tmp, syms[i]);
                syl.nucleus2 = tmp.nucleus;
                syl.diphthong = true;
                ++i;
            }
            for (; i < ranges[k].second; ++i) addPhones (syl, syms[i], true);

            // English: p / t / k right before the vowel are aspirated (not after s)
            if (aspirate && syl.numOnset > 0)
            {
                auto& last = syl.onset[(size_t) syl.numOnset - 1];
                const bool afterS = syl.numOnset >= 2 && juce::String (syl.onset[(size_t) syl.numOnset - 2].sym) == "s";
                if (last.kind == PK::Plosive && ! last.voiced && ! afterS) last.aspirated = true;
            }

            const auto part = k < textParts.size() ? textParts[(int) k] : juce::String();
            part.copyToUTF8 (syl.text, sizeof (syl.text));
            out.push_back (syl);
        }
    }

    //==========================================================================
    // English
    const std::map<juce::String, juce::String>& englishDictionary()
    {
        static const std::map<juce::String, juce::String> dict = []
        {
            std::map<juce::String, juce::String> d;
            static const char* src =
                "a=ax|about=ax b aw t|above=ax b ah v|across=ax k r ao s|after=ae f t er|again=ax g eh n|against=ax g eh n s t|ago=ax g ow|"
                "ah=aa|aint=ey n t|air=eh r|alive=ax l ay v|all=ao l|alone=ax l ow n|along=ax l ao ng|alright=ao l r ay t|always=ao l w ey z|"
                "am=ae m|an=ae n|and=ae n d|angel=ey n jh ax l|another=ax n ah dh er|any=eh n iy|anything=eh n iy th ih ng|are=aa r|arms=aa r m z|"
                "around=ax r aw n d|as=ae z|ask=ae s k|at=ae t|away=ax w ey|awake=ax w ey k|baby=b ey b iy|back=b ae k|bad=b ae d|be=b iy|"
                "beat=b iy t|beautiful=b y uw t ax f ax l|because=b ih k ao z|bed=b eh d|been=b ih n|before=b ih f ao r|begin=b ih g ih n|"
                "behind=b ih hh ay n d|believe=b ih l iy v|best=b eh s t|better=b eh t er|between=b ih t w iy n|beyond=b ih y aa n d|big=b ih g|"
                "bird=b er d|birthday=b er th d ey|black=b l ae k|blind=b l ay n d|blood=b l ah d|blow=b l ow|blue=b l uw|body=b aa d iy|"
                "bones=b ow n z|born=b ao r n|both=b ow th|boy=b oy|brave=b r ey v|break=b r ey k|breath=b r eh th|breathe=b r iy dh|"
                "bright=b r ay t|bring=b r ih ng|broken=b r ow k ax n|brother=b r ah dh er|burn=b er n|burning=b er n ih ng|but=b ah t|by=b ay|"
                "bye=b ay|call=k ao l|came=k ey m|can=k ae n|cannot=k ae n aa t|cant=k ae n t|care=k eh r|carry=k ae r iy|cat=k ae t|cats=k ae t s|"
                "catch=k ae ch|cause=k ao z|change=ch ey n jh|chance=ch ae n s|christmas=k r ih s m ax s|city=s ih t iy|close=k l ow s|cold=k ow l d|"
                "color=k ah l er|come=k ah m|coming=k ah m ih ng|could=k uh d|couldnt=k uh d ax n t|crazy=k r ey z iy|cry=k r ay|crying=k r ay ih ng|"
                "dance=d ae n s|dancing=d ae n s ih ng|dark=d aa r k|darling=d aa r l ih ng|day=d ey|days=d ey z|dead=d eh d|dear=d ih r|deep=d iy p|"
                "diamond=d ay m ax n d|did=d ih d|die=d ay|do=d uw|does=d ah z|dont=d ow n t|done=d ah n|door=d ao r|down=d aw n|dream=d r iy m|"
                "dreams=d r iy m z|drink=d r ih ng k|drive=d r ay v|each=iy ch|earth=er th|easy=iy z iy|else=eh l s|end=eh n d|enough=ih n ah f|"
                "even=iy v ax n|ever=eh v er|every=eh v r iy|everybody=eh v r iy b aa d iy|everything=eh v r iy th ih ng|eye=ay|eyes=ay z|face=f ey s|"
                "fall=f ao l|falling=f ao l ih ng|far=f aa r|fast=f ae s t|feel=f iy l|feeling=f iy l ih ng|fight=f ay t|find=f ay n d|fine=f ay n|"
                "fire=f ay er|first=f er s t|fly=f l ay|follow=f aa l ow|for=f ao r|forever=f er eh v er|forget=f er g eh t|free=f r iy|"
                "friend=f r eh n d|friends=f r eh n d z|from=f r ah m|fun=f ah n|get=g eh t|girl=g er l|give=g ih v|go=g ow|god=g aa d|"
                "going=g ow ih ng|gold=g ow l d|golden=g ow l d ax n|gone=g ao n|gonna=g aa n ax|good=g uh d|goodbye=g uh d b ay|got=g aa t|"
                "grow=g r ow|had=hh ae d|hand=hh ae n d|hands=hh ae n d z|happy=hh ae p iy|hard=hh aa r d|has=hh ae z|have=hh ae v|he=hh iy|"
                "head=hh eh d|hear=hh ih r|heart=hh aa r t|heaven=hh eh v ax n|hello=hh ax l ow|help=hh eh l p|her=hh er|here=hh ih r|hey=hh ey|"
                "hide=hh ay d|high=hh ay|him=hh ih m|his=hh ih z|hold=hh ow l d|home=hh ow m|hope=hh ow p|hot=hh aa t|how=hh aw|hurt=hh er t|"
                "i=ay|im=ay m|ill=ay l|ive=ay v|if=ih f|in=ih n|inside=ih n s ay d|into=ih n t uw|is=ih z|it=ih t|its=ih t s|just=jh ah s t|"
                "keep=k iy p|kiss=k ih s|kitty=k ih t iy|knew=n uw|know=n ow|last=l ae s t|late=l ey t|laugh=l ae f|lay=l ey|learn=l er n|"
                "leave=l iy v|left=l eh f t|let=l eh t|lets=l eh t s|life=l ay f|light=l ay t|lights=l ay t s|like=l ay k|lips=l ih p s|"
                "listen=l ih s ax n|little=l ih t ax l|live=l ih v|lonely=l ow n l iy|long=l ao ng|look=l uh k|lose=l uw z|lost=l ao s t|"
                "love=l ah v|loved=l ah v d|lover=l ah v er|low=l ow|made=m ey d|make=m ey k|man=m ae n|many=m eh n iy|me=m iy|mean=m iy n|"
                "memory=m eh m er iy|might=m ay t|mind=m ay n d|mine=m ay n|miss=m ih s|money=m ah n iy|moon=m uw n|more=m ao r|"
                "morning=m ao r n ih ng|mother=m ah dh er|move=m uw v|much=m ah ch|music=m y uw z ih k|must=m ah s t|my=m ay|myself=m ay s eh l f|"
                "name=n ey m|need=n iy d|never=n eh v er|new=n uw|next=n eh k s t|night=n ay t|no=n ow|nobody=n ow b aa d iy|nothing=n ah th ih ng|"
                "now=n aw|ocean=ow sh ax n|of=ah v|off=ao f|oh=ow|ok=ow k ey|okay=ow k ey|old=ow l d|on=aa n|once=w ah n s|one=w ah n|only=ow n l iy|"
                "open=ow p ax n|or=ao r|other=ah dh er|our=aw er|out=aw t|over=ow v er|own=ow n|pain=p ey n|party=p aa r t iy|people=p iy p ax l|"
                "place=p l ey s|play=p l ey|please=p l iy z|pretty=p r ih t iy|promise=p r aa m ax s|put=p uh t|rain=r ey n|rainbow=r ey n b ow|"
                "reach=r iy ch|ready=r eh d iy|real=r iy l|really=r ih l iy|remember=r ih m eh m b er|rest=r eh s t|ride=r ay d|right=r ay t|"
                "river=r ih v er|road=r ow d|rock=r aa k|roll=r ow l|room=r uw m|run=r ah n|running=r ah n ih ng|said=s eh d|same=s ey m|"
                "save=s ey v|say=s ey|sea=s iy|see=s iy|seem=s iy m|set=s eh t|shadow=sh ae d ow|shake=sh ey k|she=sh iy|shine=sh ay n|"
                "should=sh uh d|show=sh ow|side=s ay d|sing=s ih ng|singing=s ih ng ih ng|sky=s k ay|sleep=s l iy p|slow=s l ow|smile=s m ay l|"
                "snow=s n ow|so=s ow|some=s ah m|somebody=s ah m b aa d iy|someone=s ah m w ah n|something=s ah m th ih ng|sometimes=s ah m t ay m z|"
                "song=s ao ng|soon=s uw n|sorry=s aa r iy|soul=s ow l|sound=s aw n d|star=s t aa r|stars=s t aa r z|start=s t aa r t|stay=s t ey|"
                "still=s t ih l|stop=s t aa p|story=s t ao r iy|street=s t r iy t|strong=s t r ao ng|such=s ah ch|summer=s ah m er|sun=s ah n|"
                "sunshine=s ah n sh ay n|sure=sh uh r|sweet=s w iy t|take=t ey k|talk=t ao k|tears=t ih r z|tell=t eh l|than=dh ae n|"
                "thank=th ae ng k|thanks=th ae ng k s|that=dh ae t|the=dh ax|their=dh eh r|them=dh eh m|then=dh eh n|there=dh eh r|"
                "these=dh iy z|they=dh ey|thing=th ih ng|things=th ih ng z|think=th ih ng k|this=dh ih s|those=dh ow z|though=dh ow|"
                "thought=th ao t|through=th r uw|till=t ih l|time=t ay m|tired=t ay er d|to=t uw|today=t ax d ey|together=t ax g eh dh er|"
                "tomorrow=t ax m aa r ow|tonight=t ax n ay t|too=t uw|took=t uh k|touch=t ah ch|town=t aw n|true=t r uw|trust=t r ah s t|"
                "try=t r ay|turn=t er n|twinkle=t w ih ng k ax l|two=t uw|under=ah n d er|until=ax n t ih l|up=ah p|us=ah s|very=v eh r iy|"
                "voice=v oy s|wait=w ey t|waiting=w ey t ih ng|wake=w ey k|walk=w ao k|wall=w ao l|want=w aa n t|wanna=w aa n ax|was=w aa z|"
                "water=w ao t er|way=w ey|we=w iy|wear=w eh r|well=w eh l|went=w eh n t|were=w er|what=w ah t|when=w eh n|where=w eh r|"
                "while=w ay l|white=w ay t|who=hh uw|whole=hh ow l|why=w ay|wild=w ay l d|will=w ih l|wind=w ih n d|wings=w ih ng z|"
                "winter=w ih n t er|wish=w ih sh|with=w ih dh|without=w ih dh aw t|woman=w uh m ax n|wonder=w ah n d er|wont=w ow n t|"
                "word=w er d|words=w er d z|world=w er l d|would=w uh d|wrong=r ao ng|yeah=y ae|year=y ih r|years=y ih r z|yes=y eh s|"
                "yesterday=y eh s t er d ey|yet=y eh t|you=y uw|young=y ah ng|your=y ao r|youre=y ao r|yourself=y ao r s eh l f|"
                "la=l aa|na=n aa|da=d aa|doo=d uw|ooh=uw|oo=uw|aah=aa|whoa=w ow|yeh=y eh|uh=ah|hallelujah=hh ae l ax l uw y ax|amen=aa m eh n|"
                "kitten=k ih t ax n|purrfect=p er f ih k t|paws=p ao z|tail=t ey l|fish=f ih sh|milk=m ih l k|nap=n ae p|whiskers=w ih s k er z";
            for (const auto& entry : juce::StringArray::fromTokens (src, "|", ""))
                d[entry.upToFirstOccurrenceOf ("=", false, false)] = entry.fromFirstOccurrenceOf ("=", false, false);
            return d;
        }();
        return dict;
    }

    bool isVowelLetter (juce::juce_wchar c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

    /** English spelling rules -> phone symbols (for words that are not in the dictionary). */
    juce::StringArray englishRules (const juce::String& w)
    {
        struct Unit { bool vowel; juce::String text; bool longVowel = false; };
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
                while (j < n && (isVowelLetter (w[j]) || (w[j] == 'y' && (j + 1 >= n || ! isVowelLetter (w[j + 1])))) && j - i < 2) ++j;
                if (j < n && w[j] == 'w' && j - i == 1 && (j + 1 >= n || ! isVowelLetter (w[j + 1]))) ++j;
                u.push_back ({ true, w.substring (i, j) });
                i = j;
                continue;
            }
            bool matched = false;
            for (auto* d : { "tch", "sh", "ch", "th", "ph", "ng", "ck", "wh", "qu", "gh", "kn", "wr" })
                if (w.substring (i, i + (int) strlen (d)) == d
                    && ! (juce::String (d) == "ng" && i + 2 < n && (w[i + 2] == 'e' || w[i + 2] == 'i' || w[i + 2] == 'y')))
                {
                    juce::String s (d);
                    if (s == "tch") s = "ch";
                    else if (s == "ph") s = "f";
                    else if (s == "ck") s = "k";
                    else if (s == "wh") s = "w";
                    else if (s == "gh") s = "";
                    else if (s == "kn") s = "n";
                    else if (s == "wr") s = "r";
                    if (s.isNotEmpty()) u.push_back ({ false, s });
                    i += (int) strlen (d);
                    matched = true;
                    break;
                }
            if (matched) continue;
            if (c == 'c') u.push_back ({ false, (next == 'e' || next == 'i' || next == 'y') ? "s" : "k" });
            else if (c == 'g' && (next == 'e' || next == 'i' || next == 'y') && i > 0) u.push_back ({ false, "j" });
            else if (c >= 'a' && c <= 'z')
            {
                // double letters sound once
                if (! (u.size() > 0 && ! u.back().vowel && u.back().text == juce::String::charToString (c)))
                    u.push_back ({ false, juce::String::charToString (c) });
            }
            ++i;
        }
        if (u.size() >= 2 && ! u.back().vowel && u.back().text == "h" && u[u.size() - 2].vowel) u.pop_back();
        // "-ple", "-tle", "-kle": the final e is a schwa before the l (purple, little, twinkle)
        bool syllabicLe = false;
        if (u.size() >= 3 && u.back().vowel && u.back().text == "e" && ! u[u.size() - 2].vowel && u[u.size() - 2].text == "l"
            && ! u[u.size() - 3].vowel)
        {
            u.pop_back();
            u.pop_back();
            syllabicLe = true;
        }

        int vowels = 0;
        for (const auto& x : u) vowels += x.vowel ? 1 : 0;
        // silent final e (time, home) and -es (times)
        if (vowels >= 2 && u.size() >= 4 && ! u.back().vowel && u.back().text == "s" && u[u.size() - 2].vowel && u[u.size() - 2].text == "e" && ! u[u.size() - 3].vowel)
        {
            u.erase (u.end() - 2);
            for (int k = (int) u.size() - 1; k >= 0; --k) if (u[(size_t) k].vowel) { u[(size_t) k].longVowel = true; break; }
        }
        else if (vowels >= 2 && u.size() >= 3 && u.back().vowel && u.back().text == "e" && ! u[u.size() - 2].vowel)
        {
            u.pop_back();
            for (int k = (int) u.size() - 1; k >= 0; --k) if (u[(size_t) k].vowel) { u[(size_t) k].longVowel = true; break; }
        }

        int numV = 0;
        for (const auto& x : u) numV += x.vowel ? 1 : 0;
        juce::StringArray out;
        auto finish = [&] (juce::StringArray& o)
        {
            if (syllabicLe) { o.add ("ax"); o.add ("l"); }
            // plural / verb s after a voiced sound is z (dragons, dreams)
            if (o.size() >= 2 && o[o.size() - 1] == "s" && w.endsWith ("s") && ! w.endsWith ("ss")
                && ! juce::StringArray ({ "p", "t", "k", "f", "th", "s", "sh", "ch" }).contains (o[o.size() - 2]))
                o.set (o.size() - 1, "z");
            return o;
        };
        int vIndex = 0;
        for (size_t k = 0; k < u.size(); ++k)
        {
            const auto& x = u[k];
            if (! x.vowel)
            {
                const auto& t = x.text;
                if (t == "x") { out.add ("k"); out.add ("s"); }
                else if (t == "qu") { out.add ("k"); out.add ("w"); }
                else if (t == "j") out.add ("jh");
                else if (t == "th") out.add (w.startsWith ("th") && (w == "thy" || w == "thou" || w == "thee") ? "dh" : "th");
                else if (t == "y") out.add ("y");
                else if (t == "q") out.add ("k");
                else if (t == "r" && k > 0 && u[k - 1].vowel) continue;      // r-coloured vowel already
                else out.add (t);
                continue;
            }
            ++vIndex;
            const bool wordFinal = k + 1 == u.size();
            const bool openSyl = k + 1 >= u.size() || (k + 2 < u.size() && ! u[k + 1].vowel && u[k + 2].vowel);
            const bool rNext = k + 1 < u.size() && ! u[k + 1].vowel && u[k + 1].text == "r";
            const auto& g = x.text;
            const auto c = g[0];
            if (rNext)
            {
                if (g == "a") { out.add ("aa"); out.add ("r"); }
                else if (g == "o" || g == "oo" || g == "ou") { out.add ("ao"); out.add ("r"); }
                else if (g == "ea" || g == "ee") { out.add ("ih"); out.add ("r"); }
                else out.add ("er");
                continue;
            }
            if (g == "ee" || g == "ea") out.add ("iy");
            else if (g == "ie") out.add (wordFinal && numV == 1 ? "ay" : "iy");
            else if (g == "igh") out.add ("ay");
            else if (g == "ai" || g == "ay" || g == "ei" || g == "ey") out.add ("ey");
            else if (g == "oo" || g == "ue" || g == "ew" || g == "ui") out.add ("uw");
            else if (g == "ou" || g == "ow") out.add ("aw");
            else if (g == "oa" || g == "oe") out.add ("ow");
            else if (g == "oi" || g == "oy") out.add ("oy");
            else if (g == "au" || g == "aw") out.add ("ao");
            else if (c == 'a') out.add (x.longVowel ? "ey" : (vIndex > 1 && wordFinal ? "ax" : "ae"));
            else if (c == 'e') out.add ((x.longVowel || (wordFinal && numV == 1)) ? "iy" : (vIndex > 1 && ! openSyl ? "ax" : "eh"));
            else if (c == 'i') out.add (x.longVowel ? "ay" : "ih");
            else if (c == 'o') out.add ((x.longVowel || (openSyl && wordFinal)) ? "ow" : (openSyl ? "ow" : "aa"));
            else if (c == 'u') out.add ((x.longVowel || openSyl) ? "uw" : "ah");
            else if (c == 'y') out.add (wordFinal && numV == 1 ? "ay" : "iy");
            else out.add ("ax");
        }
        return finish (out);
    }

    bool legalEnglishOnset (const juce::StringArray& c, int from, int to)
    {
        const int n = to - from;
        if (n <= 0) return true;
        if (n == 1) return c[from] != "ng";
        juce::String s;
        for (int i = from; i < to; ++i) s << (i > from ? " " : "") << c[i];
        static const std::set<juce::String> ok = {
            "p l", "p r", "b l", "b r", "t r", "d r", "k l", "k r", "g l", "g r", "f l", "f r", "th r", "sh r", "s l", "s m", "s n",
            "s p", "s t", "s k", "s w", "t w", "d w", "k w", "g w", "th w", "s f", "p y", "b y", "k y", "f y", "m y", "v y", "hh y", "n y",
            "s k w", "s p l", "s p r", "s t r", "s k r", "s k y", "s p y" };
        return ok.count (s) > 0;
    }

    void englishWord (const juce::String& word, std::vector<Syllable>& out)
    {
        juce::StringArray syms;
        const auto& dict = englishDictionary();
        if (auto it = dict.find (word); it != dict.end())
            syms = juce::StringArray::fromTokens (it->second, " ", "");
        else
        {
            syms = englishRules (word);
            // plural / past forms of dictionary words
            for (auto suffix : { "s", "es", "ed", "ing", "ly" })
            {
                if (! word.endsWith (suffix) || word.length() <= (int) strlen (suffix) + 1) continue;
                const auto stem = word.dropLastCharacters ((int) strlen (suffix));
                auto st = dict.find (stem);
                if (st == dict.end() && juce::String (suffix) == "ing") st = dict.find (stem + "e");
                if (st == dict.end()) continue;
                syms = juce::StringArray::fromTokens (st->second, " ", "");
                const juce::String sfx (suffix);
                if (sfx == "s") syms.add (syms[syms.size() - 1] == "t" || syms[syms.size() - 1] == "k" || syms[syms.size() - 1] == "p" || syms[syms.size() - 1] == "f" ? "s" : "z");
                else if (sfx == "es") { syms.add ("ax"); syms.add ("z"); }
                else if (sfx == "ed") { if (syms[syms.size() - 1] == "t" || syms[syms.size() - 1] == "d") syms.add ("ax"); syms.add ("d"); }
                else if (sfx == "ing") { syms.add ("ih"); syms.add ("ng"); }
                else if (sfx == "ly") { syms.add ("l"); syms.add ("iy"); }
                break;
            }
        }

        std::vector<int> nuclei;
        for (int i = 0; i < syms.size(); ++i)
            if (isVowelSym (syms[i]) && ! (i > 0 && isVowelSym (syms[i - 1]) && ! nuclei.empty() && nuclei.back() == i - 1 && false))
                nuclei.push_back (i);
        if (nuclei.empty())
        {
            // "hmm", "shh": the first sonorant carries the note
            for (int i = 0; i < syms.size(); ++i) if (isSonorant (syms[i])) { nuclei.push_back (i); break; }
            if (nuclei.empty()) { syms.add ("ax"); nuclei.push_back (syms.size() - 1); }
        }
        // maximal legal onset between nuclei
        std::vector<int> splits (nuclei.size(), 0);
        for (size_t k = 1; k < nuclei.size(); ++k)
        {
            const int a = nuclei[k - 1] + 1, b = nuclei[k];
            int s = b;
            while (s > a && legalEnglishOnset (syms, s - 1, b)) --s;
            splits[k] = s;
        }
        symbolsToSyllables (syms, nuclei, splits, word, out, true);
    }

    //==========================================================================
    // Slovak
    void slovakWord (const juce::String& wordIn, std::vector<Syllable>& out)
    {
        const auto word = wordIn.toLowerCase();
        juce::StringArray s;    // symbols
        std::vector<bool> isV;
        const int n = word.length();
        auto peek = [&] (int i) -> juce::juce_wchar { return i < n ? word[i] : 0; };
        auto vowelLetter = [] (juce::juce_wchar c)
        {
            return juce::String (juce::CharPointer_UTF8 ("a" "\xc3\xa1" "" "\xc3\xa4" "e" "\xc3\xa9" "i" "\xc3\xad" "o" "\xc3\xb3" "" "\xc3\xb4" "u" "\xc3\xba" "y" "\xc3\xbd" "")).containsChar (c);
        };
        static const std::set<juce::String> hardExceptions = { "ten", "tento", "jeden", juce::String (juce::CharPointer_UTF8 ("jeden" "\xc3\xa1" "s" "\xc5\xa5" "")), "teda", "potom" };
        const bool hard = hardExceptions.count (word) > 0;

        for (int i = 0; i < n; ++i)
        {
            const auto c = word[i];
            const auto nx = peek (i + 1);
            auto softNext = [&] { return ! hard && (nx == 'e' || nx == 'i' || nx == (juce::juce_wchar) 0x00ed || nx == (juce::juce_wchar) 0x00e9); };
            juce::String sym;
            bool vowelSym = false;
            switch (c)
            {
                case 'a': case (juce::juce_wchar) 0x00e1: sym = "a"; vowelSym = true; break;
                case (juce::juce_wchar) 0x00e4: sym = "ae"; vowelSym = true; break;
                case 'e': case (juce::juce_wchar) 0x00e9: sym = "e"; vowelSym = true; break;
                case 'i': case (juce::juce_wchar) 0x00ed: case 'y': case (juce::juce_wchar) 0x00fd:
                    if (c == 'i' && (nx == 'a' || nx == 'e' || nx == 'u') && i + 1 < n) { s.add ("y"); isV.push_back (false); continue; }   // ia ie iu
                    sym = "i"; vowelSym = true; break;
                case 'o': case (juce::juce_wchar) 0x00f3: sym = "o"; vowelSym = true; break;
                case (juce::juce_wchar) 0x00f4: s.add ("wu"); isV.push_back (false); sym = "o"; vowelSym = true; break;
                case 'u': case (juce::juce_wchar) 0x00fa: sym = "u"; vowelSym = true; break;
                case 'b': sym = "b"; break;
                case 'c': if (nx == 'h') { sym = "x"; ++i; } else sym = "ts"; break;
                case (juce::juce_wchar) 0x010d: sym = "ch"; break;
                case 'd':
                    if (nx == 'z') { sym = "dz"; ++i; }
                    else if (nx == (juce::juce_wchar) 0x017e) { sym = "jh"; ++i; }
                    else sym = softNext() ? "dj" : "d";
                    break;
                case (juce::juce_wchar) 0x010f: sym = "dj"; break;
                case 'f': sym = "f"; break;
                case 'g': sym = "g"; break;
                case 'h': sym = "hv"; break;
                case 'j': sym = "y"; break;
                case 'k': case 'q': sym = "k"; break;
                case 'l': sym = softNext() ? "lj" : "l"; break;
                case (juce::juce_wchar) 0x013a: sym = "l"; break;
                case (juce::juce_wchar) 0x013e: sym = "lj"; break;
                case 'm': sym = "m"; break;
                case 'n': sym = softNext() ? "nj" : (nx == 'k' || nx == 'g' ? "ng" : "n"); break;
                case (juce::juce_wchar) 0x0148: sym = "nj"; break;
                case 'p': sym = "p"; break;
                case 'r': case (juce::juce_wchar) 0x0155: sym = "rr"; break;
                case 's': sym = "s"; break;
                case (juce::juce_wchar) 0x0161: sym = "sh"; break;
                case 't': sym = softNext() ? "tj" : "t"; break;
                case (juce::juce_wchar) 0x0165: sym = "tj"; break;
                case 'v': case 'w': sym = "v"; break;
                case 'x': s.add ("k"); isV.push_back (false); sym = "s"; break;
                case 'z': sym = "z"; break;
                case (juce::juce_wchar) 0x017e: sym = "zh"; break;
                default: break;
            }
            if (sym.isNotEmpty()) { s.add (sym); isV.push_back (vowelSym); }
            juce::ignoreUnused (vowelLetter);
        }
        if (s.isEmpty()) return;

        // v after a vowel and before a consonant / at the end -> u-glide ("pravda", "krv")
        for (int i = 0; i < s.size(); ++i)
            if (s[i] == "v" && i > 0 && (i + 1 >= s.size() || ! isV[(size_t) i + 1]) && (isV[(size_t) i - 1] || s[i - 1] == "rr" || s[i - 1] == "l"))
                s.set (i, "wu");

        // final devoicing
        static const std::map<juce::String, juce::String> devoice = { { "b", "p" }, { "d", "t" }, { "dj", "tj" }, { "g", "k" }, { "z", "s" },
                                                                      { "zh", "sh" }, { "dz", "ts" }, { "jh", "ch" }, { "hv", "x" } };
        if (auto d = devoice.find (s[s.size() - 1]); d != devoice.end()) s.set (s.size() - 1, d->second);

        // syllabic r / l between consonants (vlk, krk, slnko, prst)
        for (int i = 0; i < s.size(); ++i)
            if ((s[i] == "rr" || s[i] == "l") && ! isV[(size_t) i])
            {
                const bool prevC = i == 0 ? false : ! isV[(size_t) i - 1];
                const bool nextC = i + 1 >= s.size() || ! isV[(size_t) i + 1];
                if (i > 0 && prevC && nextC) isV[(size_t) i] = true;
            }

        std::vector<int> nuclei;
        for (int i = 0; i < s.size(); ++i) if (isV[(size_t) i]) nuclei.push_back (i);
        if (nuclei.empty())
        {
            // prepositions "v", "k", "s", "z": sung together with nothing - give them a short schwa
            s.add ("ax"); isV.push_back (true); nuclei.push_back (s.size() - 1);
        }
        std::vector<int> splits (nuclei.size(), 0);
        for (size_t k = 1; k < nuclei.size(); ++k)
        {
            const int a = nuclei[k - 1] + 1, b = nuclei[k];
            const int cons = b - a;
            // V.CV, VC.CV, VC.CCV ; a glide (j / u-glide from ia, o) stays with its vowel
            int split = b - (cons >= 3 ? 2 : (cons >= 1 ? 1 : 0));
            if (split < b && split > a && (s[split - 1] == "y" || s[split - 1] == "wu") && cons >= 2) --split;
            if (cons >= 1 && (s[b - 1] == "y" || s[b - 1] == "wu") && cons >= 2) split = juce::jmin (split, b - 2);
            splits[k] = juce::jmax (a, split);
        }
        symbolsToSyllables (s, nuclei, splits, wordIn, out, false);
        // a syllabic r / l nucleus is sung as the liquid on a neutral vowel: give it schwa colour
        for (size_t k = out.size() - nuclei.size(); k < out.size(); ++k)
            if (out[k].nucleus.kind == PK::Trill || out[k].nucleus.kind == PK::Lateral)
            {
                out[k].nucleus.x = 0.55f; out[k].nucleus.y = 0.5f;
            }
    }

    //==========================================================================
    // Japanese (romaji morae)
    void japaneseWord (const juce::String& wordIn, std::vector<Syllable>& out)
    {
        const auto w = wordIn.toLowerCase()
                           .replace (juce::String::charToString ((juce::juce_wchar) 0x0101), "aa").replace (juce::String::charToString ((juce::juce_wchar) 0x012b), "ii")
                           .replace (juce::String::charToString ((juce::juce_wchar) 0x016b), "uu").replace (juce::String::charToString ((juce::juce_wchar) 0x0113), "ee")
                           .replace (juce::String::charToString ((juce::juce_wchar) 0x014d), "ou");
        const int n = w.length();
        auto isV = [] (juce::juce_wchar c) { return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o'; };
        int i = 0;
        while (i < n)
        {
            const auto c = w[i];
            if (c == '\'' || c == '-') { ++i; continue; }

            // small tsu: double consonant -> a held closure at the end of the previous mora
            if (i + 1 < n && c == w[i + 1] && c != 'n' && ! isV (c))
            {
                if (! out.empty())
                {
                    juce::String closure = (c == 's') ? "s" : (c == 'p' ? "p" : (c == 'k' ? "k" : "t"));
                    addPhones (out.back(), closure, true);
                }
                ++i;
                continue;
            }
            // moraic n
            if (c == 'n' && (i + 1 >= n || (! isV (w[i + 1]) && w[i + 1] != 'y')))
            {
                Syllable syl;
                syl.nucleus = table().phones.at ("N")[0];
                juce::String ("n").copyToUTF8 (syl.text, sizeof (syl.text));
                out.push_back (syl);
                ++i;
                if (i < n && w[i] == '\'') ++i;
                continue;
            }

            // consonant part (longest match)
            static const char* cons[] = { "sh", "ch", "ts", "ky", "gy", "ny", "hy", "by", "py", "my", "ry", "dy", "j", "f", "k", "g", "s", "z",
                                          "t", "d", "n", "h", "b", "p", "m", "y", "r", "w", "v" };
            juce::String cs;
            for (auto* cc : cons)
                if (w.substring (i, i + (int) strlen (cc)) == cc && i + (int) strlen (cc) < n && isV (w[i + (int) strlen (cc)]))
                { cs = cc; break; }
            const int vi = i + cs.length();
            if (vi >= n || ! isV (w[vi])) { ++i; continue; }   // stray letter
            const auto v = w[vi];

            Syllable syl;
            juce::String vSym = v == 'a' ? "a" : v == 'i' ? "i" : v == 'u' ? "ux" : v == 'e' ? "e" : "o";
            // Hepburn / Kunrei spellings: si -> shi, ti -> chi, tu -> tsu, hu -> fu, zi / di -> ji
            juce::String c1 = cs;
            if (cs == "s" && v == 'i') c1 = "sh";
            if (cs == "t" && v == 'i') c1 = "ch";
            if (cs == "t" && v == 'u') c1 = "ts";
            if (cs == "h" && v == 'u') c1 = "f";
            if ((cs == "z" || cs == "d") && v == 'i') c1 = "j";

            auto onset = [&] (const juce::String& sym) { addPhones (syl, sym, false); };
            if (c1 == "sh") onset ("sh");
            else if (c1 == "ch") onset ("ch");
            else if (c1 == "ts") onset ("ts");
            else if (c1 == "j") onset ("jh");
            else if (c1 == "f") onset ("f");
            else if (c1 == "r") onset ("dx");
            else if (c1 == "n" && v == 'i') onset ("nj");
            else if (c1 == "h" && v == 'i') onset ("x");
            else if (c1 == "w") onset ("w");
            else if (c1 == "y") onset ("y");
            else if (c1.length() == 2 && c1[1] == 'y')
            {
                const auto base = c1.substring (0, 1);
                onset (base == "r" ? "dx" : base == "n" ? "nj" : base == "h" ? "x" : base);
                onset ("y");
            }
            else if (c1.isNotEmpty()) onset (c1);
            setNucleus (syl, vSym);
            w.substring (i, vi + 1).copyToUTF8 (syl.text, sizeof (syl.text));
            out.push_back (syl);
            i = vi + 1;
        }
    }

    //==========================================================================
    // Cat language
    bool catWord (const juce::String& w, std::vector<Syllable>& out)
    {
        auto starts = [&w] (std::initializer_list<const char*> l) { for (auto* x : l) if (w.startsWith (x)) return true; return false; };
        auto is = [&w] (std::initializer_list<const char*> l) { for (auto* x : l) if (w == x) return true; return false; };
        Syllable s;
        if (is ({ "meow", "miau", "miaow", "mjau", "meo", "mieu", "miaou", "maow" }) || (w.startsWith ("me") && w.endsWith ("ow")))
        {
            addPhones (s, "m", false);
            auto g = table().phones.at ("y")[0]; g.dur = 0.09f; s.addOnset (g);
            setNucleus (s, "aa");
            s.diphthong = true; s.nucleus2 = table().phones.at ("uw")[0];
            s.gesture = CatGesture::Meow;
        }
        else if (is ({ "mew", "mju", "myu", "mi", "mii" }))
        {
            addPhones (s, "m", false); addPhones (s, "y", false); setNucleus (s, "uw"); s.gesture = CatGesture::Mew;
        }
        else if (is ({ "nya", "nyan", "nyaa", "nyah", "nya~" }))
        {
            addPhones (s, "nj", false); setNucleus (s, "aa");
            if (w.endsWith ("n")) addPhones (s, "N", true);
            s.gesture = CatGesture::Mew;
        }
        else if (starts ({ "mrr", "mrow", "mraow", "mrrp", "mrp" }))
        {
            addPhones (s, "m", false);
            auto r = table().phones.at ("rr")[0]; r.dur = 0.14f; s.addOnset (r);
            setNucleus (s, w.contains ("ow") ? "aa" : "ax");
            if (w.contains ("ow")) { s.diphthong = true; s.nucleus2 = table().phones.at ("uw")[0]; }
            if (w.endsWith ("p")) addPhones (s, "p", true);
            s.gesture = CatGesture::Mrrp;
        }
        else if (starts ({ "prr", "purr", "pur" }))
        {
            addPhones (s, "p", false);
            setNucleus (s, "ax");
            s.gesture = CatGesture::Purr;
        }
        else if (starts ({ "hiss", "hsss", "hss", "khh", "pfft", "sss", "shh" }))
        {
            addPhones (s, "hh", false);
            setNucleus (s, "ae");
            s.gesture = CatGesture::Hiss;
        }
        else if (starts ({ "yowl", "yow", "rawr", "rowr", "raow" }))
        {
            if (w.startsWith ("y")) addPhones (s, "y", false); else addPhones (s, "rr", false);
            setNucleus (s, "aa");
            s.diphthong = true; s.nucleus2 = table().phones.at ("uw")[0];
            s.gesture = CatGesture::Yowl;
        }
        else if (starts ({ "chirp", "eep", "ek", "ack", "chrr" }))
        {
            if (w.startsWith ("ch")) addPhones (s, "ch", false);
            setNucleus (s, w.startsWith ("a") ? "ae" : "ih");
            if (w.endsWith ("p")) addPhones (s, "p", true);
            if (w.endsWith ("k")) addPhones (s, "k", true);
            s.gesture = CatGesture::Chirp;
        }
        else if (starts ({ "brr", "trill", "rrr", "brrp" }))
        {
            if (w.startsWith ("b")) addPhones (s, "b", false);
            auto r = table().phones.at ("rr")[0]; r.dur = 0.2f; s.addOnset (r);
            setNucleus (s, "uw");
            s.gesture = CatGesture::Trill;
        }
        else
            return false;
        w.copyToUTF8 (s.text, sizeof (s.text));
        out.push_back (s);
        return true;
    }

    juce::String cleanWord (const juce::String& raw, Language lang)
    {
        juce::String w;
        for (auto c : raw.toLowerCase())
        {
            if (c == '\'' || c == (juce::juce_wchar) 0x2019) continue;
            if (lang == Language::Slovak || lang == Language::Japanese)
            {
                if ((c >= 'a' && c <= 'z') || c > 127) w << juce::String::charToString (c);
            }
            else if ((c >= 'a' && c <= 'z')) w << juce::String::charToString (c);
        }
        return w;
    }
}

//==============================================================================
const Phone* phoneFor (const juce::String& symbol)
{
    const auto& tb = table();
    if (auto it = tb.phones.find (symbol); it != tb.phones.end() && ! it->second.empty()) return &it->second[0];
    return nullptr;
}

juce::StringArray phonemeHelp()
{
    return {
        "Vowels:  a  aa (father)  ae (cat)  ah (love)  ao (thought)  e  eh (bed)  i / iy  ih (sit)  o  u / uw  uh (book)  ux (Japanese u)  ax (schwa)  er (bird)",
        "Diphthongs:  ay (my)  aw (now)  ey (day)  ow (go)  oy (boy)",
        "Stops:  p b t d k g  tj dj (Slovak t' d')  q (glottal stop)",
        "Fricatives:  f v  th (thin)  dh (this)  s z  sh zh  x (Slovak ch)  hh (h)  hv (Slovak h)",
        "Affricates:  ch  jh  ts  dz",
        "Nasals:  m n ng  nj (Slovak n')  N (Japanese n)",
        "Liquids:  l  lj (Slovak l')  r (English r)  rr (rolled r)  dx (tap, Japanese r)",
        "Glides:  w  y  wu",
        "Example:  [l ah v]  [sh i]  [m y uw]",
    };
}

Syllable syllableFromPhonemes (const juce::String& symbolsIn)
{
    auto syms = juce::StringArray::fromTokens (symbolsIn.removeCharacters ("[]"), " ,", "");
    syms.removeEmptyStrings();
    for (int i = syms.size(); --i >= 0;)
        if (! table().phones.count (syms[i]) && ! table().diphthongs.count (syms[i])) syms.remove (i);

    Syllable s;
    int nuc = -1;
    for (int i = 0; i < syms.size(); ++i) if (isVowelSym (syms[i])) { nuc = i; break; }
    if (nuc < 0)
        for (int i = 0; i < syms.size(); ++i) if (isSonorant (syms[i])) { nuc = i; break; }
    if (nuc < 0) { syms.add ("ax"); nuc = syms.size() - 1; }
    std::vector<Syllable> out;
    symbolsToSyllables (syms, { nuc }, { 0 }, symbolsIn, out, false);
    s = out.empty() ? Syllable() : out[0];
    ("[" + symbolsIn.removeCharacters ("[]").trim() + "]").copyToUTF8 (s.text, sizeof (s.text));
    return s;
}

std::vector<Syllable> textToSyllables (const juce::String& text, Language language)
{
    std::vector<Syllable> out;
    // brackets first: "[l ah v] [y uw]"
    juce::String rest = text;
    juce::StringArray tokens;
    while (rest.isNotEmpty())
    {
        const int b = rest.indexOfChar ('[');
        if (b < 0) { tokens.addTokens (rest, " \t\n", ""); break; }
        tokens.addTokens (rest.substring (0, b), " \t\n", "");
        const int e = rest.indexOfChar (b, ']');
        if (e < 0) { tokens.add (rest.substring (b)); break; }
        tokens.add (rest.substring (b, e + 1));
        rest = rest.substring (e + 1);
    }
    tokens.removeEmptyStrings();

    for (auto token : tokens)
    {
        token = token.trim();
        if (token.startsWith ("["))
        {
            out.push_back (syllableFromPhonemes (token));
            continue;
        }
        if (token == "-" || token == "+" || token == "~")
        {
            Syllable s; s.hold = true; juce::String ("-").copyToUTF8 (s.text, sizeof (s.text));
            out.push_back (s);
            continue;
        }
        if (token.equalsIgnoreCase ("br") || token == "*")
        {
            Syllable s; s.breath = true; setNucleus (s, "a"); juce::String ("br").copyToUTF8 (s.text, sizeof (s.text));
            out.push_back (s);
            continue;
        }
        // "hel-lo" splits a word into notes explicitly
        auto parts = juce::StringArray::fromTokens (token, "-", "");
        parts.removeEmptyStrings();
        for (const auto& part : parts)
        {
            const auto w = cleanWord (part, language);
            if (w.isEmpty()) continue;
            switch (language)
            {
                case Language::Slovak:   slovakWord (w, out); break;
                case Language::Japanese: japaneseWord (w, out); break;
                case Language::Cat:      if (! catWord (w, out)) englishWord (w, out); break;
                case Language::English:
                default:                 if (! (w == "meow" || w == "purr" || w == "mrrp") || ! catWord (w, out)) englishWord (w, out); break;
            }
        }
        if (out.size() > 2000) break;
    }
    return out;
}

juce::String phonemeString (const Syllable& s)
{
    if (s.hold) return "-";
    if (s.breath) return "br";
    juce::StringArray parts;
    for (int i = 0; i < s.numOnset; ++i) if (s.onset[(size_t) i].sym[0] != 0) parts.add (s.onset[(size_t) i].sym);
    if (s.diphthong && table().diphthongs.count (s.nucleus.sym) == 0)
        parts.add (juce::String (s.nucleus.sym) + " " + s.nucleus2.sym);
    else
        parts.add (s.nucleus.sym);
    for (int i = 0; i < s.numCoda; ++i) if (s.coda[(size_t) i].sym[0] != 0) parts.add (s.coda[(size_t) i].sym);
    return parts.joinIntoString (" ");
}

float onsetSeconds (const Syllable& s, float scale)
{
    if (s.hold) return 0.0f;
    float t = 0.0f;
    for (int i = 0; i < s.numOnset; ++i)
        t += s.onset[(size_t) i].dur * scale + (s.onset[(size_t) i].aspirated ? 0.035f * scale : 0.0f);
    return t;
}

float codaSeconds (const Syllable& s, float scale)
{
    if (s.hold) return 0.0f;
    float t = s.diphthong ? 0.11f * scale : 0.0f;
    for (int i = 0; i < s.numCoda; ++i) t += s.coda[(size_t) i].dur * scale;
    return t;
}

} // namespace talkcat
