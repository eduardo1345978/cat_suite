// Cat Ocean: what people can type in the YouTube chat (and what moderators can do).
#include "OceanSim.h"
#include "OceanFilter.h"
#include <algorithm>

namespace ocean
{
namespace
{
    constexpr double commandCooldown = 3.0, feedCooldown = 45.0, xpMessageGap = 30.0;
    constexpr double emoteCooldown = 20.0, lookCooldown = 8.0, nameCooldown = 60.0, speechCooldown = 8.0, helpCooldown = 90.0, feedAnyoneCooldown = 6.0;
    constexpr int maxCommandsPerMinute = 10;
    constexpr double muteSeconds = 120.0;

    int indexIn (const juce::StringArray& names, const juce::String& w) { return names.indexOf (w, true); }

    const char* helpText = "!fish NAME  !color red blue  !body cat  !pattern stripes  !hat crown  !voice bell  !melody 1353  !dance  !feed  !vote 1  !level";
}

bool Sim::spamCheck (Viewer& v, const juce::String& text, bool isCommand)
{
    // true = let it through. Muted viewers, the same text again and again, and floods are ignored.
    if (time < v.mutedUntil) { ++ignoredCommands; return false; }
    const auto norm = text.trim().toLowerCase();
    if (! isCommand && norm == v.lastText && time - v.lastTextAt < 30.0) { ++ignoredCommands; return false; }   // commands have their own pauses
    v.lastText = norm;
    v.lastTextAt = time;
    if (! isCommand) return true;
    while (! v.recent.empty() && time - v.recent.front() > 60.0) v.recent.pop_front();
    v.recent.push_back (time);
    if ((int) v.recent.size() > maxCommandsPerMinute)
    {
        v.mutedUntil = time + muteSeconds;
        v.recent.clear();
        if (auto* f = findFish (v.fishId); f != nullptr && f->present)
            toast (*f, "too many commands - taking a 2 minute nap", 0xfff6757a);
        ++ignoredCommands;
        return false;
    }
    return true;
}

void Sim::chat (const juce::String& authorId, const juce::String& authorName, const juce::String& text, bool moderator)
{
    if (authorId.isEmpty() || banned.count (authorId) > 0) return;
    auto& v = viewers[authorId];
    if (v.name.isEmpty())
        v.name = filter::cleanName (authorName, (int) (authorId.hashCode() & 0x7fffffff));
    v.lastMessage = time;
    if (! moderator && ! spamCheck (v, text, text.trim().startsWithChar ('!'))) return;

    if (auto* f = findFish (v.fishId))
    {
        bringBack (*f);
        // chatting earns XP (at most every 30 s)
        if (time - v.lastXpMessage >= xpMessageGap) { v.lastXpMessage = time; addXp (*f, 2); }
    }

    const auto t = text.trim();
    if (t.startsWithChar ('!'))
        command (v, authorId, t, moderator);
    else if (auto* f = findFish (v.fishId); f != nullptr && f->present && ! f->leaving)
    {
        // a normal chat message: the fish says it in a speech bubble (only kind words, short)
        juce::String said;
        for (auto c : t)
        {
            c = filter::fold (c);
            if (c >= 32 && c < 127) said << juce::String::charToString (c);
        }
        said = said.trim().substring (0, 34).trim();
        // no shouting, no walls of the same letter, one bubble every few seconds, at most 6 on screen
        int upper = 0, letters = 0;
        for (auto c : said) if (juce::CharacterFunctions::isLetter (c)) { ++letters; if (juce::CharacterFunctions::isUpperCase (c)) ++upper; }
        if (letters > 6 && upper * 10 > letters * 7) said = said.toLowerCase();
        juce::String squashed;
        juce::juce_wchar prev = 0; int run = 0;
        for (auto c : said) { run = c == prev ? run + 1 : 0; prev = c; if (run < 3) squashed << juce::String::charToString (c); }
        said = squashed;
        const bool roomy = speech.size() < 6 || time - v.lastSpeech > 20.0;
        if (said.isNotEmpty() && filter::isClean (said) && time - v.lastSpeech >= speechCooldown && roomy)
        {
            speech.erase (std::remove_if (speech.begin(), speech.end(), [f] (const Speech& sp) { return sp.fishId == f->id; }), speech.end());
            speech.push_back ({ f->id, said, 6.0f });
            v.lastSpeech = time;
            if (speech.size() > 12) speech.erase (speech.begin());
        }
    }
}

bool Sim::applyWord (Fish& f, const juce::String& word, juce::String& changed, juce::String& locked)
{
    // the word decides on its own which gene it means; "!tail round" style hints are handled by command()
    auto& g = f.genes;
    const int lv = f.level();
    auto note = [&changed] (const juce::String& s) { changed << (changed.isEmpty() ? "" : ", ") << s; };
    auto allowed = [&] (int kind, int value, const juce::String& what)
    {
        const int need = unlockLevel (kind, value);
        if (need <= lv) return true;
        locked << (locked.isEmpty() ? "" : ", ") << what << " at LV" << need;
        return false;
    };
    int i;
    if ((i = indexIn (bodyNames(), word)) >= 0)           { if (allowed (KindBody, i, word)) { g.body = i; note (word); } return true; }
    if (word == "small" || word == "tiny")                 { g.size = 0; note ("small"); return true; }
    if (word == "medium")                                  { g.size = 1; note ("medium"); return true; }
    if (word == "big" || word == "huge" || word == "large"){ g.size = 2; note ("big"); return true; }
    if ((i = indexIn (patternNames(), word)) >= 0)         { if (allowed (KindPattern, i, word)) { g.pattern = i; note (word); } return true; }
    if (word == "stripe" || word == "striped")             { g.pattern = PatStripes; note ("stripes"); return true; }
    if (word == "spot" || word == "spotty" || word == "dots") { g.pattern = PatSpots; note ("spots"); return true; }
    if ((i = indexIn (accessoryNames(), word)) >= 0)       { if (allowed (KindHat, i, word)) { g.accessory = i; note (i == 0 ? "no hat" : word); } return true; }
    if (word == "nothing" || word == "none")               { g.accessory = AccNone; note ("no hat"); return true; }
    if (word == "sunglasses")                              { g.accessory = AccGlasses; note ("glasses"); return true; }
    if ((i = indexIn (voiceNames(), word)) >= 0)           { if (allowed (KindVoice, i, word)) { g.voice = i; note (word + " voice"); } return true; }
    if (word == "low" || word == "mid" || word == "high")  { g.pitch = word == "low" ? 0 : word == "mid" ? 1 : 2; note (word + " voice"); return true; }
    if ((i = indexIn (moodNames(), word)) >= 0)            { g.setMood (i); note (word); return true; }
    if (word == "sparse" || word == "busy")                { g.setRhythm (word == "sparse" ? 0 : 2); note (word + " rhythm"); return true; }
    if (word == "slow" || word == "fast")                  { g.speed = word == "slow" ? 0.15f : 0.9f; note (word); return true; }
    if (word == "fan" || word == "fork" || word == "veil") { i = indexIn (tailNames(), word); if (allowed (KindTail, i, word + " tail")) { g.tail = i; note (word + " tail"); } return true; }
    if (word == "sail" || word == "spiky" || word == "nofin") { i = indexIn (finNames(), word); if (allowed (KindFin, i, word + " fin")) { g.fin = i; note (word == "nofin" ? "no fin" : word + " fin"); } return true; }
    if (word == "dot" || word == "sleepy" || word == "star")  { i = indexIn (eyeNames(), word); if (allowed (KindEyes, i, word + " eyes")) { g.eyes = i; note (word + " eyes"); } return true; }
    return false;
}

juce::String Sim::findViewer (const juce::String& nameOrMention) const
{
    // "@Miro", "Miro" or a fish name; YouTube names may contain spaces, so compare without them
    auto key = [] (const juce::String& s) { return s.trim().trimCharactersAtStart ("@").removeCharacters (" ").toLowerCase(); };
    const auto want = key (nameOrMention);
    if (want.isEmpty()) return {};
    for (auto& v : viewers)
        if (key (v.second.name) == want) return v.first;
    for (auto& f : fish)
        if (f.viewer() && (key (f.name) == want || key (f.ownerName) == want)) return f.ownerId;
    for (auto& b : bannedNamesById)
        if (key (b.second) == want) return b.first;
    return {};
}

bool Sim::modCommand (const juce::String& cmd, const juce::StringArray& tok, const juce::StringArray& raw)
{
    // moderators: !rename @name New Name, !resetname @name, !kick @name, !ban @name, !unban @name,
    // !event storm, !place kelp, !music lofi, !endvote, !feast
    auto target = [&] { return tok.size() > 0 ? findViewer (raw[0]) : juce::String(); };
    if (cmd == "rename" && tok.size() >= 2)
    {
        const auto id = target();
        if (auto* f = fishOf (id))
        {
            juce::StringArray rest (raw);
            rest.remove (0);
            f->name = filter::cleanName (rest.joinIntoString (" "), f->id);
            if (f->familyId == f->id) f->familyName = f->name;
            toast (*f, "renamed by a moderator", 0xffc0a7f0);
        }
        return true;
    }
    if (cmd == "resetname")
    {
        if (auto* f = fishOf (target()))
        {
            f->name = "Fish" + juce::String (100 + f->id % 900);
            if (f->familyId == f->id) f->familyName = f->name;
            toast (*f, "name reset by a moderator", 0xffc0a7f0);
        }
        return true;
    }
    if (cmd == "kick")
    {
        if (auto* f = fishOf (target())) sendAway (*f);
        return true;
    }
    if (cmd == "ban")
    {
        const auto id = target();
        if (id.isEmpty()) return true;
        auto it = viewers.find (id);
        bannedNamesById[id] = it != viewers.end() ? it->second.name : raw[0].trimCharactersAtStart ("@");
        banned.insert (id);
        if (auto* f = fishOf (id))
        {
            // the fish leaves for good
            const int fid = f->id;
            fish.erase (std::remove_if (fish.begin(), fish.end(), [fid] (const Fish& x) { return x.id == fid; }), fish.end());
        }
        viewers.erase (id);
        announce (bannedNamesById[id] + " was banned from the ocean", 0xff8b9bb4);
        return true;
    }
    if (cmd == "unban")
    {
        const auto id = target();
        if (id.isNotEmpty() && banned.erase (id) > 0)
        {
            announce (bannedNamesById[id] + " may come back to the ocean", 0xff8b9bb4);
            bannedNamesById.erase (id);
        }
        return true;
    }
    if (cmd == "event" && tok.size() > 0)
    {
        const int e = findEvent (tok.joinIntoString (""));
        if (e >= 0) { announce ("A moderator called: " + eventName (e) + "!", 0xff2ce8f5); applyEvent (e); }
        return true;
    }
    if ((cmd == "place" || cmd == "goto") && tok.size() > 0)
    {
        const int p = findPlace (tok.joinIntoString (""));
        if (p >= 0) { announce ("Off to the " + placeName (p) + "!", 0xff2ce8f5); goToPlace (p); }
        return true;
    }
    if (cmd == "music" && tok.size() > 0)
    {
        const int st = findStyle (tok.joinIntoString (""));
        if (st >= 0) { styleRequest = st; announce ("Now playing: " + styleName (st), 0xff2ce8f5); }
        return true;
    }
    if (cmd == "endvote" || cmd == "skipvote")
    {
        if (vote.active) vote.left = 0.0f;
        return true;
    }
    if (cmd == "startvote")
    {
        if (! vote.active) voteTimer = 0.0f;
        return true;
    }
    return false;
}

void Sim::command (Viewer& v, const juce::String& ownerId, const juce::String& text, bool moderator)
{
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

    // moderators' commands have no cooldown
    if (moderator && modCommand (cmd, tok, raw)) return;

    if (time - v.lastCommand < commandCooldown) { ++ignoredCommands; return; }
    v.lastCommand = time;

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
        if (time - v.lastFeed < feedCooldown || time - lastFeedAnyone < feedAnyoneCooldown) { ++ignoredCommands; return; }
        v.lastFeed = time;
        lastFeedAnyone = time;
        dropFood (4, f != nullptr && f->present ? f->x : -1.0f);
        if (f != nullptr && f->present) toast (*f, "yum!", 0xfffee761);
        return;
    }

    if (cmd == "help" || cmd == "commands")
    {
        if (time - lastHelp < helpCooldown) return;   // one help line for everybody, not one each
        lastHelp = time;
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
        juce::String changed, name, locked;
        int colourSlot = 0;
        for (int i = 0; i < tok.size(); ++i)
        {
            const int c = findColour (tok[i]);
            if (c >= 0) { (colourSlot == 0 ? f->genes.colour1 : colourSlot == 1 ? f->genes.colour2 : f->genes.colour3) = c; ++colourSlot; changed << (changed.isEmpty() ? "" : ", ") << tok[i]; continue; }
            if (! applyWord (*f, tok[i], changed, locked)) name << (name.isEmpty() ? "" : " ") << raw[i];
        }
        // a new fish starts with looks everyone has (higher-level things come with levels)
        auto& g = f->genes;
        if (unlockLevel (KindBody, g.body) > 1) g.body = BodySlim;
        if (unlockLevel (KindPattern, g.pattern) > 1) g.pattern = PatStripes;
        if (unlockLevel (KindTail, g.tail) > 1) g.tail = TailFan;
        if (unlockLevel (KindFin, g.fin) > 1) g.fin = FinSmall;
        if (unlockLevel (KindEyes, g.eyes) > 1) g.eyes = EyesBig;
        if (unlockLevel (KindHat, g.accessory) > 1) g.accessory = AccNone;
        if (unlockLevel (KindVoice, g.voice) > 1) g.voice = VoiceBell;
        if (name.isNotEmpty()) f->name = f->familyName = filter::cleanName (name, f->id);
        toast (*f, locked.isNotEmpty() ? "Hi, I'm " + f->name + "! (" + locked + ")" : "Hi, I'm " + f->name + "!", 0xff9ef0c2);
        if (f->genes.rare != RareNone) announce ("Lucky! " + v.name + " got a " + rareNames()[f->genes.rare] + " fish!", 0xfffee761);
        return;
    }

    if (! f->present) bringBack (*f);
    juce::String changed, locked;

    if (cmd == "dance" || cmd == "spin" || cmd == "jump")
    {
        if (time - v.lastEmote < emoteCooldown) { ++ignoredCommands; return; }
        v.lastEmote = time;
        f->emote = cmd == "dance" ? EmoteDance : cmd == "spin" ? EmoteSpin : EmoteJump;
        f->emoteTime = 0.0f;
        return;
    }
    if (cmd == "level" || cmd == "lvl" || cmd == "xp")
    {
        const int lv = f->level();
        toast (*f, "LV" + juce::String (lv) + "  " + juce::String (f->xp) + "/" + juce::String (xpForLevel (lv + 1)) + " XP", 0xfffee761);
        return;
    }
    if (cmd == "name" || cmd == "rename")
    {
        if (time - v.lastName < nameCooldown) { ++ignoredCommands; return; }
        v.lastName = time;
        const auto name = filter::cleanName (raw.joinIntoString (" "), f->id);
        f->name = name;
        if (f->familyName.isEmpty() || f->familyId == f->id) f->familyName = name;
        toast (*f, "I'm " + name + " now", 0xff9ef0c2);
        return;
    }
    const bool changesLook = cmd != "home" && cmd != "bye" && cmd != "sleep" && cmd != "release" && cmd != "info" && cmd != "me";
    if (changesLook)
    {
        if (time - v.lastLook < lookCooldown) { ++ignoredCommands; return; }
        v.lastLook = time;
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
        const auto keep = f->genes;
        f->genes = Genes::random (rng);
        auto& g = f->genes;
        g.rare = keep.rare; g.tempo = keep.tempo; g.key = keep.key; g.species = keep.species; g.trait = keep.trait;
        // only what the level allows
        const int lv = f->level();
        if (unlockLevel (KindBody, g.body) > lv) g.body = keep.body;
        if (unlockLevel (KindPattern, g.pattern) > lv) g.pattern = keep.pattern;
        if (unlockLevel (KindTail, g.tail) > lv) g.tail = keep.tail;
        if (unlockLevel (KindFin, g.fin) > lv) g.fin = keep.fin;
        if (unlockLevel (KindEyes, g.eyes) > lv) g.eyes = keep.eyes;
        if (unlockLevel (KindHat, g.accessory) > lv) g.accessory = keep.accessory;
        if (unlockLevel (KindVoice, g.voice) > lv) g.voice = keep.voice;
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
        juce::String s = "LV" + juce::String (f->level()) + " " + voiceNames()[f->genes.voice] + " " + moodNames()[f->genes.mood()]
                       + " gen " + juce::String (f->generation) + " babies " + juce::String (f->babies);
        if (auto* sp = species (f->genes.species)) s << " " << sp->name;
        toast (*f, s, 0xff2ce8f5);
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
        const int lv = f->level();
        for (auto& w : tok)
        {
            auto& g = f->genes;
            auto note = [&changed] (const juce::String& s) { changed << (changed.isEmpty() ? "" : ", ") << s; };
            auto ok = [&] (int kind, int value, const juce::String& what)
            {
                const int need = unlockLevel (kind, value);
                if (need <= lv) return true;
                locked << (locked.isEmpty() ? "" : ", ") << what << " at LV" << need;
                return false;
            };
            int i;
            if (cmd == "tail" && (i = indexIn (tailNames(), w)) >= 0)                      { if (ok (KindTail, i, w + " tail")) { g.tail = i; note (w + " tail"); } continue; }
            if ((cmd == "fin" || cmd == "fins") && (w == "none" || w == "no"))             { g.fin = FinNone; note ("no fin"); continue; }
            if ((cmd == "fin" || cmd == "fins") && (i = indexIn (finNames(), w)) >= 0)     { if (ok (KindFin, i, w + " fin")) { g.fin = i; note (w + " fin"); } continue; }
            if ((cmd == "eyes" || cmd == "eye") && (i = indexIn (eyeNames(), w)) >= 0)     { if (ok (KindEyes, i, w + " eyes")) { g.eyes = i; note (w + " eyes"); } continue; }
            if (cmd == "notes" && (w == "short" || w == "long"))                           { g.noteLength = w == "long" ? 1 : 0; note (w + " notes"); continue; }
            if (cmd == "speed" && w == "normal")                                           { g.speed = 0.5f; note ("normal speed"); continue; }
            if (cmd == "rhythm" && w == "normal")                                          { g.setRhythm (1); note ("normal rhythm"); continue; }
            const int c = findColour (w);
            if (c >= 0 && colourSlot < 3) { (colourSlot == 0 ? g.colour1 : colourSlot == 1 ? g.colour2 : g.colour3) = c; ++colourSlot; note (w); continue; }
            applyWord (*f, w, changed, locked);
        }
    }

    if (changed.isNotEmpty() || locked.isNotEmpty())
        toast (*f, changed + (locked.isNotEmpty() ? (changed.isNotEmpty() ? "  " : "") + juce::String ("(") + locked + ")" : juce::String()),
               locked.isNotEmpty() && changed.isEmpty() ? 0xfff6757a : 0xffffffff);
    else ++ignoredCommands;
}
} // namespace ocean
