// Cat Ocean: what people can type in the YouTube chat.
#include "OceanSim.h"
#include "OceanFilter.h"

namespace ocean
{
namespace
{
    constexpr double commandCooldown = 2.5, feedCooldown = 30.0;

    int indexIn (const juce::StringArray& names, const juce::String& w) { return names.indexOf (w, true); }

    /** The help line shown when someone types !help (the screen also shows tips). */
    const char* helpText = "!fish NAME  !color red blue  !body cat  !pattern stripes  !hat crown  !voice bell  !melody 1353  !feed  !vote 1";
}

void Sim::chat (const juce::String& authorId, const juce::String& authorName, const juce::String& text)
{
    if (authorId.isEmpty()) return;
    auto& v = viewers[authorId];
    if (v.name.isEmpty())
        v.name = filter::cleanName (authorName, (int) (authorId.hashCode() & 0x7fffffff));
    v.lastMessage = time;

    if (auto* f = findFish (v.fishId))
        bringBack (*f);

    const auto t = text.trim();
    if (t.startsWithChar ('!'))
        command (v, authorId, t);
}

bool Sim::applyWord (Fish& f, const juce::String& word, juce::String& changed)
{
    // the word decides on its own which gene it means; "!tail round" style hints are handled by command()
    auto& g = f.genes;
    auto note = [&changed] (const juce::String& s) { changed << (changed.isEmpty() ? "" : ", ") << s; };
    int i;
    if ((i = indexIn (bodyNames(), word)) >= 0)           { g.body = i; note (word); return true; }
    if (word == "small" || word == "tiny")                 { g.size = 0; note ("small"); return true; }
    if (word == "medium")                                  { g.size = 1; note ("medium"); return true; }
    if (word == "big" || word == "huge" || word == "large"){ g.size = 2; note ("big"); return true; }
    if ((i = indexIn (patternNames(), word)) >= 0)         { g.pattern = i; note (word); return true; }
    if (word == "stripe" || word == "striped")             { g.pattern = PatStripes; note ("stripes"); return true; }
    if (word == "spot" || word == "spotty" || word == "dots") { g.pattern = PatSpots; note ("spots"); return true; }
    if ((i = indexIn (accessoryNames(), word)) >= 0)       { g.accessory = i; note (i == 0 ? "no hat" : word); return true; }
    if (word == "nothing" || word == "none")               { g.accessory = AccNone; note ("no hat"); return true; }
    if (word == "sunglasses")                              { g.accessory = AccGlasses; note ("glasses"); return true; }
    if ((i = indexIn (voiceNames(), word)) >= 0)           { g.voice = i; note (word + " voice"); return true; }
    if (word == "low" || word == "mid" || word == "high")  { g.pitch = word == "low" ? 0 : word == "mid" ? 1 : 2; note (word + " voice"); return true; }
    if ((i = indexIn (moodNames(), word)) >= 0)            { g.setMood (i); note (word); return true; }
    if (word == "sparse" || word == "busy")                { g.setRhythm (word == "sparse" ? 0 : 2); note (word + " rhythm"); return true; }
    if (word == "slow" || word == "fast")                  { g.speed = word == "slow" ? 0.15f : 0.9f; note (word); return true; }
    if (word == "fan" || word == "fork" || word == "veil") { g.tail = indexIn (tailNames(), word); note (word + " tail"); return true; }
    if (word == "sail" || word == "spiky" || word == "nofin") { g.fin = indexIn (finNames(), word); note (word == "nofin" ? "no fin" : word + " fin"); return true; }
    if (word == "dot" || word == "sleepy" || word == "star")  { g.eyes = indexIn (eyeNames(), word); note (word + " eyes"); return true; }
    return false;
}

void Sim::command (Viewer& v, const juce::String& ownerId, const juce::String& text)
{
    if (time - v.lastCommand < commandCooldown) { ++ignoredCommands; return; }
    v.lastCommand = time;

    juce::StringArray tok;
    tok.addTokens (text.substring (1).toLowerCase(), " ,", "");
    tok.removeEmptyStrings();
    if (tok.isEmpty()) return;
    const auto cmd = tok[0];
    tok.remove (0);

    // the original spelling of the words after the command (for names)
    juce::StringArray raw;
    raw.addTokens (text.substring (1), " ,", "");
    raw.removeEmptyStrings();
    raw.remove (0);

    Fish* f = findFish (v.fishId);

    if (cmd == "vote")
    {
        if (! vote.active || tok.isEmpty()) { ++ignoredCommands; return; }
        const int choice = tok[0].getIntValue() - 1;
        if (! juce::isPositiveAndBelow (choice, (int) vote.options.size())) { ++ignoredCommands; return; }
        auto it = vote.voters.find (ownerId);
        if (it != vote.voters.end()) --vote.counts[(size_t) it->second];
        vote.voters[ownerId] = choice;
        ++vote.counts[(size_t) choice];
        if (f != nullptr && f->present) toast (*f, "voted " + juce::String (choice + 1), 0xff2ce8f5);
        return;
    }

    if (cmd == "feed")
    {
        if (time - v.lastFeed < feedCooldown) { ++ignoredCommands; return; }
        v.lastFeed = time;
        dropFood (4, f != nullptr && f->present ? f->x : -1.0f);
        if (f != nullptr && f->present) toast (*f, "yum!", 0xfffee761);
        return;
    }

    if (cmd == "help" || cmd == "commands")
    {
        announce (helpText, 0xffffffff);
        return;
    }

    const bool create = cmd == "fish" || cmd == "join" || cmd == "claim";
    if (f == nullptr)
    {
        if (! create) { ++ignoredCommands; return; }   // other commands need a fish first
        auto& nf = newViewerFish (ownerId, v.name);
        v.fishId = nf.id;
        f = &nf;
        bringBack (*f);
        // !fish Miro blue stripes: known words change the fish, the rest is its name
        juce::String changed, name;
        int colourSlot = 0;
        for (int i = 0; i < tok.size(); ++i)
        {
            const int c = findColour (tok[i]);
            if (c >= 0) { (colourSlot == 0 ? f->genes.colour1 : colourSlot == 1 ? f->genes.colour2 : f->genes.colour3) = c; ++colourSlot; changed << (changed.isEmpty() ? "" : ", ") << tok[i]; continue; }
            if (! applyWord (*f, tok[i], changed)) name << (name.isEmpty() ? "" : " ") << raw[i];
        }
        if (name.isNotEmpty()) f->name = f->familyName = filter::cleanName (name, f->id);
        toast (*f, "Hi, I'm " + f->name + "!", 0xff9ef0c2);
        if (f->genes.rare != RareNone) announce ("Lucky! " + v.name + " got a " + rareNames()[f->genes.rare] + " fish!", 0xfffee761);
        return;
    }

    if (! f->present) bringBack (*f);
    juce::String changed;

    if (cmd == "name" || cmd == "rename")
    {
        const auto name = filter::cleanName (raw.joinIntoString (" "), f->id);
        f->name = name;
        if (f->familyName.isEmpty() || f->familyId == f->id) f->familyName = name;
        toast (*f, "I'm " + name + " now", 0xff9ef0c2);
        return;
    }
    if (cmd == "color" || cmd == "colour" || cmd == "colors" || cmd == "colours")
    {
        int slot = 0;
        for (auto& w : tok)
        {
            const int c = findColour (w);
            if (c < 0 || slot > 2) continue;
            (slot == 0 ? f->genes.colour1 : slot == 1 ? f->genes.colour2 : f->genes.colour3) = c;
            changed << (changed.isEmpty() ? "" : ", ") << w;
            ++slot;
        }
        if (slot == 1) f->genes.colour3 = f->genes.colour3 == f->genes.colour1 ? f->genes.colour2 : f->genes.colour3;
    }
    else if (cmd == "melody" || cmd == "tune" || cmd == "song")
    {
        const auto digits = tok.joinIntoString ("");
        if (digits.containsOnly ("12345678") && digits.isNotEmpty())
        {
            f->genes.setMelody (digits.substring (0, 8));
            changed = "melody " + f->genes.melodyString();
        }
    }
    else if (cmd == "random" || cmd == "reroll")
    {
        const auto keepRare = f->genes.rare;
        const auto keepTempo = f->genes.tempo; const auto keepKey = f->genes.key;
        f->genes = Genes::random (rng);
        f->genes.rare = keepRare; f->genes.tempo = keepTempo; f->genes.key = keepKey;
        changed = "a new look";
    }
    else if (cmd == "home" || cmd == "bye" || cmd == "sleep")
    {
        toast (*f, "bye!", 0xffc0a7f0);
        sendAway (*f);
        v.lastMessage = time - viewerIdleSeconds + 1.0;   // stays home until the next message
        return;
    }
    else if (cmd == "release")
    {
        // the fish becomes a wild fish (it keeps its name and family)
        toast (*f, "free!", 0xffc0a7f0);
        f->ownerId = {}; f->ownerName = {};
        f->lifespan = f->age + 1200.0f;
        v.fishId = 0;
        return;
    }
    else if (cmd == "info" || cmd == "me")
    {
        toast (*f, voiceNames()[f->genes.voice] + " " + moodNames()[f->genes.mood()] + " gen " + juce::String (f->generation)
                     + " babies " + juce::String (f->babies), 0xff2ce8f5);
        return;
    }
    else
    {
        // !fish ..., !set ..., or a category like !tail round / !eyes big / !notes long / !speed normal
        const bool category = cmd == "tail" || cmd == "fin" || cmd == "fins" || cmd == "eyes" || cmd == "eye"
                           || cmd == "notes" || cmd == "speed" || cmd == "rhythm" || cmd == "size" || cmd == "hat"
                           || cmd == "voice" || cmd == "pitch" || cmd == "mood" || cmd == "body" || cmd == "pattern";
        if (! create && ! category && cmd != "set" && cmd != "look") { ++ignoredCommands; return; }
        int colourSlot = 0;
        for (auto& w : tok)
        {
            auto& g = f->genes;
            auto note = [&changed] (const juce::String& s) { changed << (changed.isEmpty() ? "" : ", ") << s; };
            int i;
            if (cmd == "tail" && (i = indexIn (tailNames(), w)) >= 0)                      { g.tail = i; note (w + " tail"); continue; }
            if ((cmd == "fin" || cmd == "fins") && (w == "none" || w == "no"))             { g.fin = FinNone; note ("no fin"); continue; }
            if ((cmd == "fin" || cmd == "fins") && (i = indexIn (finNames(), w)) >= 0)     { g.fin = i; note (w + " fin"); continue; }
            if ((cmd == "eyes" || cmd == "eye") && (i = indexIn (eyeNames(), w)) >= 0)     { g.eyes = i; note (w + " eyes"); continue; }
            if (cmd == "notes" && (w == "short" || w == "long"))                           { g.noteLength = w == "long" ? 1 : 0; note (w + " notes"); continue; }
            if (cmd == "speed" && w == "normal")                                           { g.speed = 0.5f; note ("normal speed"); continue; }
            if (cmd == "rhythm" && w == "normal")                                          { g.setRhythm (1); note ("normal rhythm"); continue; }
            const int c = findColour (w);
            if (c >= 0 && colourSlot < 3) { (colourSlot == 0 ? g.colour1 : colourSlot == 1 ? g.colour2 : g.colour3) = c; ++colourSlot; note (w); continue; }
            applyWord (*f, w, changed);
        }
    }

    if (changed.isNotEmpty()) toast (*f, changed, 0xffffffff);
    else ++ignoredCommands;
}
} // namespace ocean
