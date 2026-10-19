#pragma once

#include <juce_core/juce_core.h>

/** Keeps fish names friendly: no bad words (English and Slovak, also written with 0 / 1 / 3 / 4 / 5 / @ / $), short, simple characters. */
namespace ocean::filter
{
/** Slovak / Czech / German letters to plain ones (the pixel font only has ASCII). */
inline juce::juce_wchar fold (juce::juce_wchar c)
{
    static const juce::juce_wchar lower[] = { 0xe1, 0xe4, 0x10d, 0x10f, 0xe9, 0x11b, 0xed, 0x13e, 0x13a, 0x148, 0xf3, 0xf4, 0xf6, 0x155, 0x159,
                                              0x161, 0x165, 0xfa, 0x16f, 0xfc, 0xfd, 0x17e, 0xdf };
    static const juce::juce_wchar upper[] = { 0xc1, 0xc4, 0x10c, 0x10e, 0xc9, 0x11a, 0xcd, 0x13d, 0x139, 0x147, 0xd3, 0xd4, 0xd6, 0x154, 0x158,
                                              0x160, 0x164, 0xda, 0x16e, 0xdc, 0xdd, 0x17d, 0x1e9e };
    static const char plain[] = "aacdeeillnooorrstuuuyzs";
    for (size_t i = 0; i < sizeof (lower) / sizeof (lower[0]); ++i)
    {
        if (c == lower[i]) return (juce::juce_wchar) plain[i];
        if (c == upper[i]) return (juce::juce_wchar) (plain[i] - 'a' + 'A');
    }
    return c;
}

inline juce::String normalise (const juce::String& s)
{
    juce::String out;
    for (auto c : s)
    {
        c = fold (c);
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        switch (c)
        {
            case '0': c = 'o'; break;   case '1': c = 'i'; break;   case '!': c = 'i'; break;
            case '3': c = 'e'; break;   case '4': c = 'a'; break;   case '@': c = 'a'; break;
            case '5': c = 's'; break;   case '$': c = 's'; break;   case '7': c = 't'; break;
            case 0x10d: c = 'c'; break; case 0x161: c = 's'; break; case 0x17e: c = 'z'; break;   // č š ž
            case 0x165: c = 't'; break; case 0x148: c = 'n'; break; case 0x10f: c = 'd'; break;   // ť ň ď
            case 0x13e: c = 'l'; break; case 0xe1: c = 'a'; break;  case 0xe9: c = 'e'; break;    // ľ á é
            case 0xed: c = 'i'; break;  case 0xf3: c = 'o'; break;  case 0xfa: c = 'u'; break;    // í ó ú
            case 0xfd: c = 'y'; break;  case 0xe4: c = 'a'; break;  case 0xf4: c = 'o'; break;    // ý ä ô
            default: break;
        }
        if (c >= 'a' && c <= 'z') out << juce::String::charToString (c);
    }
    return out;
}

inline bool isClean (const juce::String& text)
{
    // anywhere inside a word
    static const char* bad[] = {
        "fuck", "shit", "bitch", "cunt", "dick", "cock", "pussy", "nigg", "slut", "whore", "rape", "nazi", "hitler",
        "retard", "porn", "penis", "vagina", "asshole", "bastard", "boob", "kurva", "kurv", "kokot", "jebat", "jeban",
        "chuj", "hovno", "zmrd", "sukin", "cigan", "buzerant", "buzna", "skurv", "prdel", "kunda", "pici", "picus"
    };
    // only as a whole word (they hide inside harmless words: "spice", "Picasso", "document")
    static const char* badWords[] = { "fuk", "fag", "niga", "tits", "cum", "kkt", "pica", "pice", "picu", "jebe",
                                      "debil", "mrd", "srac", "negr", "sex", "anal" };
    const auto n = normalise (text);
    for (auto* b : bad)
        if (n.contains (b)) return false;
    juce::StringArray words;
    words.addTokens (text, " -_.'", "");
    for (auto& w : words)
    {
        const auto nw = normalise (w);
        for (auto* b : badWords)
            if (nw == b) return false;
    }
    return true;
}

/** A name that may be shown: simple characters, at most 16 long, nothing rude (else "Fish123"). */
inline juce::String cleanName (const juce::String& raw, int fallbackNumber)
{
    juce::String s;
    for (auto c : raw.trim())
    {
        c = fold (c);
        if ((c < 128 && juce::CharacterFunctions::isLetterOrDigit (c)) || c == ' ' || c == '-' || c == '_' || c == '\'' || c == '.')
            s << juce::String::charToString (c);
    }
    s = s.trim().substring (0, 16).trim();
    if (s.isEmpty() || ! isClean (s)) return "Fish" + juce::String (100 + std::abs (fallbackNumber) % 900);
    return s;
}
} // namespace ocean::filter
