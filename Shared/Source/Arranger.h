#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Composer.h"

/**
    The song arranger menu, shared by Cat Hub's Compose tab and Muse-Cat: click a section of the
    song's map, pick a change, and apply() gives the new list of sections for cat::muse::arrange().
*/
namespace cat::muse::arranger
{
constexpr int Renew = 1, Louder = 2, Softer = 3, Duplicate = 4, Earlier = 5, Later = 6, Delete = 7, Undo = 8,
              ChangeTo = 100, AddAfter = 200, Length = 300, PartToggle = 400;
constexpr int maxSections = 40;
inline const int* lengths() { static const int l[] = { 4, 8, 12, 16 }; return l; }

/** The menu for one section. */
inline juce::PopupMenu menu (const Song& song, int idx, bool canUndo)
{
    juce::PopupMenu m;
    if (! juce::isPositiveAndBelow (idx, (int) song.sections.size())) return m;
    const auto& sec = song.sections[(size_t) idx];
    m.addSectionHeader (sectionName (sec.type) + " (" + juce::String (sec.bars) + " bars)");
    if (song.settings.endless)
    {
        m.addItem (1, "Endless songs arrange themselves - turn Endless off to arrange", false);
        return m;
    }
    m.addItem (Renew, "Other chords for this section");
    juce::PopupMenu change, add, length, parts;
    for (int t = 0; t < NumSectionTypes; ++t)
    {
        change.addItem (ChangeTo + t, sectionName (t), true, t == sec.type);
        add.addItem (AddAfter + t, sectionName (t));
    }
    for (int k = 0; k < 4; ++k) length.addItem (Length + k, juce::String (lengths()[k]) + " bars", true, sec.bars == lengths()[k]);
    for (int p = 0; p < NumParts; ++p) parts.addItem (PartToggle + p, partNames()[p], true, sec.on[(size_t) p]);
    m.addSubMenu ("Make it a...", change);
    m.addSubMenu ("Length", length);
    m.addSubMenu ("Parts that play", parts);
    m.addItem (Louder, "More energy", sec.energy < 0.99f);
    m.addItem (Softer, "Less energy", sec.energy > 0.11f);
    m.addSeparator();
    const bool room = song.sections.size() < (size_t) maxSections;
    m.addSubMenu ("Add a section after it", add, room);
    m.addItem (Duplicate, "Play it twice", room);
    m.addItem (Earlier, "Move earlier", idx > 0);
    m.addItem (Later, "Move later", idx < (int) song.sections.size() - 1);
    m.addItem (Delete, "Remove it", song.sections.size() > 1);
    m.addSeparator();
    m.addItem (Undo, "Undo the last change", canUndo);
    return m;
}

/** Applies a menu choice (not Undo) to a copy of the song's sections. False = nothing to change. */
inline bool apply (const Song& song, int idx, int result, juce::uint32 seed, std::vector<Section>& form)
{
    if (song.settings.endless || ! juce::isPositiveAndBelow (idx, (int) song.sections.size())) return false;
    form = song.sections;
    auto& sec = form[(size_t) idx];
    const int numProgressions = juce::jmax (1, (int) song.progressions.size());
    const bool room = form.size() < (size_t) maxSections;
    if (result == Renew)
    {
        if (numProgressions < 2) return false;
        sec.progression = (sec.progression + 1 + (int) (seed % (juce::uint32) (numProgressions - 1))) % numProgressions;
    }
    else if (result == Louder) sec.energy = juce::jmin (1.0f, sec.energy + 0.15f);
    else if (result == Softer) sec.energy = juce::jmax (0.1f, sec.energy - 0.15f);
    else if (result == Duplicate) { if (! room) return false; const auto copy = sec; form.insert (form.begin() + idx + 1, copy); }
    else if (result == Earlier) { if (idx == 0) return false; std::swap (form[(size_t) idx], form[(size_t) idx - 1]); }
    else if (result == Later) { if (idx + 1 >= (int) form.size()) return false; std::swap (form[(size_t) idx], form[(size_t) idx + 1]); }
    else if (result == Delete) { if (form.size() <= 1) return false; form.erase (form.begin() + idx); }
    else if (result >= PartToggle && result < PartToggle + NumParts) { auto& on = sec.on[(size_t) (result - PartToggle)]; on = ! on; }
    else if (result >= Length && result < Length + 4) sec.bars = lengths()[result - Length];
    else if (result >= ChangeTo && result < ChangeTo + NumSectionTypes) sec = makeSection (song, result - ChangeTo, sec.bars, seed);
    else if (result >= AddAfter && result < AddAfter + NumSectionTypes)
    {
        if (! room) return false;
        const int type = result - AddAfter;
        form.insert (form.begin() + idx + 1, makeSection (song, type, type == PreChorus || type == Build ? 4 : 8, seed));
    }
    else return false;
    return true;
}

/** Which song an undo belongs to. */
inline juce::uint32 songKey (const Song& s)
{
    return s.settings.seed * 131u + (juce::uint32) s.settings.genre * 7u + (juce::uint32) s.settings.mood;
}
} // namespace cat::muse::arranger
