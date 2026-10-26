#include "Engine.h"
#include "ChoirEngine.h"
#include <cmath>

/**
    CHOIR: Sing-Cat's choir - physically modelled singers (people and cats), each with its own
    glottis and vocal tract, on a stage in a room. Up to 16 singers in the box.
*/
namespace catbox
{
namespace
{
ParamDef plain (const char* id, const char* name, float lo, float hi, float def, const char* suffix, float skew = 1.0f)
{
    ParamDef d = defs::num (id, name, 0, 1, 0);
    d.unit = ParamDef::Plain; d.lo = lo; d.hi = hi; d.def = def; d.suffix = suffix; d.skew = skew;
    return d;
}
ParamDef ms (const char* id, const char* name, float lo, float hi, float def)
{
    ParamDef d = defs::num (id, name, 0, 1, 0);
    d.unit = ParamDef::Ms; d.lo = lo; d.hi = hi; d.def = def; d.suffix = " ms"; d.skew = 0.4f;
    return d;
}
enum { cMode = 0, cSingers, cUnison, cCast, cStyle, cOnset, cVX, cVY, cVX2, cVY2, cMove, cMoveRate, cSpread, cAttack, cRelease,
       cVibrato, cBreath, cRasp, cGlide, cDrift, cHuman, cPurr, cNeck, cJaw, cLips, cNose, cChest, cRoom, cRoomMix, cMicWidth, cMicDist,
       cVolume, cSeed, cDiction, cCount };
}

EngineDef makeChoirDef()
{
    using namespace defs;
    EngineDef e;
    e.name = "Choir";
    e.params = {
        choice ("mode", "Mode", singcat::modeNames(), 1), num ("singers", "Singers", 1, 16, 8), num ("unison", "Per note", 1, 4, 1),
        choice ("cast", "Cast", singcat::castNames(), 0), choice ("style", "Style", singcat::styleNames(), 0), choice ("onset", "Onset", singcat::onsetNames(), 0),
        plain ("vowelX", "Front-back", 0.0f, 1.0f, 0.55f, ""), plain ("vowelY", "Close-open", 0.0f, 1.0f, 0.95f, ""),
        plain ("vowelX2", "Front-back 2", 0.0f, 1.0f, 0.95f, ""), plain ("vowelY2", "Close-open 2", 0.0f, 1.0f, 0.06f, ""),
        choice ("move", "Vowel move", singcat::moveNames(), 0), hz ("moveRate", "Move rate", 0.05f, 8.0f, 0.5f),
        plain ("spread", "Spread", 0.0f, 100.0f, 30.0f, "%"), ms ("attack", "Attack", 5.0f, 3000.0f, 120.0f), ms ("release", "Release", 20.0f, 5000.0f, 600.0f),
        plain ("vibrato", "Vibrato", 0.0f, 200.0f, 100.0f, "%"), plain ("breath", "Breath", 0.0f, 100.0f, 20.0f, "%"), plain ("rasp", "Rasp", 0.0f, 100.0f, 0.0f, "%"),
        ms ("glide", "Glide", 0.0f, 1000.0f, 120.0f), plain ("drift", "Drift", 0.0f, 100.0f, 30.0f, "%"), plain ("human", "Looseness", 0.0f, 100.0f, 30.0f, "%"),
        plain ("purr", "Purr", 0.0f, 100.0f, 0.0f, "%"),
        plain ("neck", "Neck", -100.0f, 100.0f, 0.0f, "%"), plain ("jaw", "Jaw", -100.0f, 100.0f, 0.0f, "%"), plain ("lips", "Lips", -100.0f, 100.0f, 0.0f, "%"),
        plain ("nose", "Nose", -100.0f, 100.0f, 0.0f, "%"), plain ("chest", "Chest", -100.0f, 100.0f, 0.0f, "%"),
        choice ("room", "Room", singcat::roomNames(), 1), plain ("roomMix", "Room mix", 0.0f, 100.0f, 30.0f, "%"),
        plain ("micWidth", "Mic width", 0.0f, 100.0f, 70.0f, "%"), plain ("micDist", "Mic distance", 0.0f, 100.0f, 40.0f, "%"),
        db ("volume", "Volume", -36.0f, 6.0f, 0.0f), num ("seed", "Singers seed", 1, 9999, 1), plain ("diction", "Diction", 0.0f, 100.0f, 50.0f, "%"),
    };
    jassert ((int) e.params.size() == cCount);
    e.pages = {
        page ("SING",  { cMode, cSingers, cCast, cStyle, cVX, cVY, cMove, cMoveRate, cAttack, cRelease, cRoomMix, cVolume,
                         cUnison, cOnset, cVX2, cVY2, cSpread, cVibrato, cBreath, cRasp, cGlide, cDrift, cHuman, cPurr }),
        page ("VOICE", { cNeck, cJaw, cLips, cNose, cChest, cRoom, cMicWidth, cMicDist, cDiction, cSeed, cBreath, cVibrato }),
    };
    using V = std::vector<std::pair<juce::String, float>>;
    auto add = [&e] (const char* name, const char* cat, V v) { e.presets.push_back ({ name, cat, std::move (v) }); };
    add ("Chamber Choir", "Choir", { { "mode", 1.0f }, { "singers", 12.0f }, { "unison", 2.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "attack", 260.0f }, { "release", 900.0f }, { "room", 2.0f }, { "roomMix", 35.0f } });
    add ("Gospel Mass", "Choir", { { "mode", 2.0f }, { "singers", 24.0f }, { "cast", 0.0f }, { "style", 1.0f }, { "vowelX", 0.8f }, { "vowelY", 0.6f }, { "attack", 120.0f }, { "release", 700.0f }, { "rasp", 10.0f }, { "room", 2.0f }, { "roomMix", 40.0f } });
    add ("Pop Duo", "Choir", { { "mode", 1.0f }, { "singers", 2.0f }, { "cast", 1.0f }, { "style", 2.0f }, { "vowelX", 0.55f }, { "vowelY", 0.5f }, { "move", 2.0f }, { "vowelX2", 0.16f }, { "vowelY2", 0.4f }, { "moveRate", 1.5f }, { "breath", 35.0f }, { "room", 0.0f }, { "roomMix", 20.0f }, { "attack", 60.0f }, { "release", 350.0f } });
    add ("Folk Trio", "Choir", { { "mode", 1.0f }, { "singers", 3.0f }, { "cast", 0.0f }, { "style", 3.0f }, { "vowelX", 0.3f }, { "vowelY", 0.7f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 80.0f }, { "release", 400.0f }, { "nose", 15.0f } });
    add ("Cathedral Ooh", "Choir", { { "mode", 2.0f }, { "singers", 32.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "move", 1.0f }, { "vowelX2", 0.93f }, { "vowelY2", 0.4f }, { "moveRate", 0.08f }, { "spread", 60.0f }, { "attack", 700.0f }, { "release", 2500.0f }, { "room", 3.0f }, { "roomMix", 60.0f }, { "micDist", 70.0f } });
    add ("Boy Soprano", "Solo", { { "volume", -5.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 3.0f }, { "style", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "vibrato", 45.0f }, { "glide", 90.0f }, { "room", 3.0f }, { "roomMix", 45.0f }, { "attack", 140.0f }, { "release", 900.0f } });
    add ("Old Monks", "Choir", { { "mode", 2.0f }, { "singers", 8.0f }, { "cast", 2.0f }, { "style", 7.0f }, { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "onset", 2.0f }, { "neck", 30.0f }, { "chest", 40.0f }, { "drift", 50.0f }, { "room", 3.0f }, { "roomMix", 55.0f }, { "attack", 400.0f }, { "release", 1800.0f } });
    add ("Pansori Solo", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 1.0f }, { "style", 6.0f }, { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "rasp", 40.0f }, { "glide", 180.0f }, { "room", 1.0f }, { "roomMix", 20.0f }, { "attack", 40.0f }, { "release", 500.0f } });
    add ("Breathy Ahh", "Choir", { { "mode", 1.0f }, { "singers", 6.0f }, { "unison", 2.0f }, { "cast", 1.0f }, { "style", 5.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "breath", 70.0f }, { "room", 2.0f }, { "roomMix", 35.0f }, { "attack", 300.0f }, { "release", 1200.0f } });
    add ("Formant Sweep", "Solo", { { "volume", -5.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 2.0f }, { "style", 2.0f }, { "vowelX", 0.06f }, { "vowelY", 0.06f }, { "move", 1.0f }, { "vowelX2", 0.95f }, { "vowelY2", 0.06f }, { "moveRate", 0.5f }, { "room", 0.0f }, { "roomMix", 15.0f } });
    add ("Kitten Choir", "Cats", { { "mode", 1.0f }, { "singers", 8.0f }, { "unison", 2.0f }, { "cast", 5.0f }, { "style", 3.0f }, { "onset", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 30.0f }, { "release", 300.0f } });
    add ("Tomcat Serenade", "Cats", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 6.0f }, { "style", 4.0f }, { "onset", 4.0f }, { "rasp", 30.0f }, { "glide", 200.0f }, { "vowelX", 0.8f }, { "vowelY", 0.6f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "release", 500.0f } });
    add ("Purring Pad", "Cats", { { "mode", 2.0f }, { "singers", 6.0f }, { "cast", 7.0f }, { "style", 7.0f }, { "purr", 80.0f }, { "vowelX", 0.55f }, { "vowelY", 0.5f }, { "attack", 800.0f }, { "release", 2000.0f }, { "room", 2.0f }, { "roomMix", 40.0f } });
    add ("Meow Lead", "Cats", { { "volume", -5.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 7.0f }, { "style", 2.0f }, { "onset", 4.0f }, { "glide", 60.0f }, { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 0.0f }, { "roomMix", 15.0f }, { "attack", 15.0f }, { "release", 250.0f } });
    add ("Cats & Humans", "Cats", { { "mode", 2.0f }, { "singers", 16.0f }, { "cast", 8.0f }, { "style", 1.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "room", 2.0f }, { "roomMix", 35.0f }, { "attack", 150.0f }, { "release", 800.0f } });
    add ("Vowel Rider", "Choir", { { "mode", 1.0f }, { "singers", 8.0f }, { "cast", 0.0f }, { "style", 2.0f }, { "move", 3.0f }, { "moveRate", 2.0f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 40.0f }, { "release", 400.0f } });
    add ("Madrigal Quartet", "Choir", { { "mode", 1.0f }, { "singers", 4.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "vowelX", 0.3f }, { "vowelY", 0.6f }, { "vibrato", 60.0f }, { "room", 1.0f }, { "roomMix", 30.0f }, { "attack", 90.0f }, { "release", 500.0f } });
    add ("Lullaby", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 1.0f }, { "style", 3.0f }, { "vowelX", 0.55f }, { "vowelY", 0.5f }, { "breath", 40.0f }, { "vibrato", 40.0f }, { "glide", 160.0f }, { "room", 1.0f }, { "roomMix", 30.0f }, { "attack", 180.0f }, { "release", 800.0f } });
    add ("Soul Diva", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 1.0f }, { "style", 1.0f }, { "vowelX", 0.5f }, { "vowelY", 0.85f }, { "glide", 150.0f }, { "rasp", 15.0f }, { "chest", 30.0f }, { "room", 0.0f }, { "roomMix", 20.0f }, { "attack", 40.0f }, { "release", 450.0f } });
    add ("Barbershop", "Choir", { { "mode", 1.0f }, { "singers", 4.0f }, { "cast", 2.0f }, { "style", 4.0f }, { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "vibrato", 50.0f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 70.0f }, { "release", 400.0f } });
    add ("Children's Choir", "Choir", { { "mode", 2.0f }, { "singers", 16.0f }, { "cast", 3.0f }, { "style", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "vibrato", 35.0f }, { "room", 2.0f }, { "roomMix", 40.0f }, { "attack", 200.0f }, { "release", 900.0f } });
    add ("Grandparents", "Choir", { { "mode", 1.0f }, { "singers", 8.0f }, { "unison", 2.0f }, { "cast", 4.0f }, { "style", 3.0f }, { "vowelX", 0.3f }, { "vowelY", 0.7f }, { "drift", 60.0f }, { "human", 60.0f }, { "room", 1.0f }, { "roomMix", 30.0f }, { "attack", 150.0f }, { "release", 700.0f } });
    add ("Whisper Choir", "Choir", { { "mode", 1.0f }, { "singers", 8.0f }, { "unison", 2.0f }, { "cast", 0.0f }, { "style", 5.0f }, { "vowelX", 0.55f }, { "vowelY", 0.7f }, { "breath", 100.0f }, { "vibrato", 20.0f }, { "room", 2.0f }, { "roomMix", 45.0f }, { "attack", 350.0f }, { "release", 1500.0f } });
    add ("Throat Drone", "Choir", { { "mode", 2.0f }, { "singers", 4.0f }, { "cast", 2.0f }, { "style", 7.0f }, { "onset", 2.0f }, { "neck", 60.0f }, { "chest", 60.0f }, { "move", 1.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "vowelX2", 0.93f }, { "vowelY2", 0.4f }, { "moveRate", 0.12f }, { "room", 3.0f }, { "roomMix", 50.0f }, { "attack", 600.0f }, { "release", 2500.0f } });
    add ("Nasal Folk", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 2.0f }, { "style", 3.0f }, { "nose", 80.0f }, { "vowelX", 0.24f }, { "vowelY", 0.8f }, { "glide", 80.0f }, { "room", 1.0f }, { "roomMix", 20.0f } });
    add ("Jazz Scat", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 2.0f }, { "style", 4.0f }, { "move", 5.0f }, { "attack", 20.0f }, { "release", 200.0f }, { "glide", 60.0f }, { "room", 0.0f }, { "roomMix", 15.0f } });
    add ("Wah Choir", "Choir", { { "mode", 1.0f }, { "singers", 6.0f }, { "cast", 1.0f }, { "style", 2.0f }, { "move", 1.0f }, { "moveRate", 2.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "spread", 10.0f }, { "room", 0.0f }, { "roomMix", 20.0f }, { "attack", 30.0f }, { "release", 300.0f } });
    add ("Vowel Steps", "Choir", { { "mode", 1.0f }, { "singers", 8.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "move", 3.0f }, { "moveRate", 1.0f }, { "room", 2.0f }, { "roomMix", 35.0f }, { "attack", 120.0f }, { "release", 700.0f } });
    add ("Big Hall Tutti", "Choir", { { "mode", 2.0f }, { "singers", 64.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "room", 2.0f }, { "roomMix", 45.0f }, { "micDist", 60.0f }, { "attack", 400.0f }, { "release", 1500.0f } });
    add ("Alley Cat Fight", "Cats", { { "mode", 1.0f }, { "singers", 6.0f }, { "cast", 7.0f }, { "style", 6.0f }, { "onset", 4.0f }, { "rasp", 50.0f }, { "vowelX", 0.6f }, { "vowelY", 0.9f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 20.0f }, { "release", 300.0f } });
    add ("Kitten Lullaby", "Cats", { { "volume", -5.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 5.0f }, { "style", 5.0f }, { "onset", 4.0f }, { "glide", 120.0f }, { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 1.0f }, { "roomMix", 30.0f }, { "release", 500.0f } });
    add ("Purring Tomcats", "Cats", { { "mode", 2.0f }, { "singers", 4.0f }, { "cast", 6.0f }, { "style", 7.0f }, { "purr", 60.0f }, { "vowelX", 0.55f }, { "vowelY", 0.5f }, { "room", 1.0f }, { "roomMix", 30.0f }, { "attack", 500.0f }, { "release", 1500.0f } });
    add ("Meow Pluck", "Cats", { { "mode", 1.0f }, { "singers", 4.0f }, { "cast", 7.0f }, { "style", 2.0f }, { "onset", 4.0f }, { "attack", 5.0f }, { "release", 150.0f }, { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 0.0f }, { "roomMix", 15.0f } });
    add ("Close Mic Solo", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 1.0f }, { "style", 2.0f }, { "breath", 30.0f }, { "micDist", 0.0f }, { "room", 0.0f }, { "roomMix", 5.0f }, { "vowelX", 0.55f }, { "vowelY", 0.6f }, { "attack", 30.0f }, { "release", 300.0f } });
    add ("Far Away Choir", "Choir", { { "mode", 2.0f }, { "singers", 24.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "micDist", 100.0f }, { "roomMix", 70.0f }, { "room", 3.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "attack", 600.0f }, { "release", 2500.0f } });
    add ("Requiem", "Choir", { { "mode", 2.0f }, { "singers", 40.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "move", 2.0f }, { "vowelX2", 0.95f }, { "vowelY2", 0.06f }, { "moveRate", 0.25f }, { "room", 3.0f }, { "roomMix", 55.0f }, { "attack", 900.0f }, { "release", 3000.0f }, { "chest", 20.0f } });
    add ("Choir Stabs", "Choir", { { "mode", 2.0f }, { "singers", 16.0f }, { "cast", 0.0f }, { "style", 1.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "attack", 10.0f }, { "release", 180.0f }, { "room", 2.0f }, { "roomMix", 30.0f } });
    add ("Gospel Lead", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 2.0f }, { "style", 1.0f }, { "vowelX", 0.8f }, { "vowelY", 0.6f }, { "glide", 140.0f }, { "rasp", 25.0f }, { "chest", 40.0f }, { "room", 1.0f }, { "roomMix", 25.0f } });
    add ("Folk Harmony", "Choir", { { "mode", 1.0f }, { "singers", 2.0f }, { "cast", 0.0f }, { "style", 3.0f }, { "vowelX", 0.3f }, { "vowelY", 0.7f }, { "nose", 20.0f }, { "room", 1.0f }, { "roomMix", 20.0f } });
    add ("Soprano Aria", "Solo", { { "volume", -5.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 1.0f }, { "style", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "vibrato", 120.0f }, { "glide", 110.0f }, { "room", 2.0f }, { "roomMix", 40.0f }, { "attack", 120.0f }, { "release", 900.0f } });
    add ("Bass Profundo", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 2.0f }, { "style", 7.0f }, { "neck", 50.0f }, { "chest", 70.0f }, { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "room", 3.0f }, { "roomMix", 45.0f } });
    add ("Doo-Wop", "Choir", { { "mode", 1.0f }, { "singers", 4.0f }, { "cast", 2.0f }, { "style", 4.0f }, { "onset", 3.0f }, { "move", 2.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "moveRate", 4.0f }, { "attack", 20.0f }, { "release", 250.0f }, { "room", 1.0f }, { "roomMix", 25.0f } });
    add ("Pansori Duet", "Choir", { { "mode", 1.0f }, { "singers", 2.0f }, { "cast", 0.0f }, { "style", 6.0f }, { "rasp", 35.0f }, { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 1.0f }, { "roomMix", 25.0f } });
    add ("Nursery Rhyme", "Choir", { { "mode", 1.0f }, { "singers", 6.0f }, { "cast", 3.0f }, { "style", 3.0f }, { "vowelX", 0.3f }, { "vowelY", 0.7f }, { "human", 70.0f }, { "drift", 60.0f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 40.0f }, { "release", 350.0f } });
    add ("Cat Opera", "Cats", { { "volume", -5.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 7.0f }, { "style", 0.0f }, { "onset", 4.0f }, { "vibrato", 150.0f }, { "glide", 150.0f }, { "room", 3.0f }, { "roomMix", 45.0f }, { "release", 700.0f } });
    add ("Midnight Alley", "Cats", { { "mode", 1.0f }, { "singers", 8.0f }, { "cast", 7.0f }, { "style", 4.0f }, { "onset", 4.0f }, { "rasp", 20.0f }, { "human", 80.0f }, { "room", 2.0f }, { "roomMix", 40.0f }, { "micDist", 70.0f }, { "release", 600.0f } });
    add ("Purr & Hum", "Cats", { { "mode", 2.0f }, { "singers", 8.0f }, { "cast", 8.0f }, { "style", 7.0f }, { "onset", 2.0f }, { "purr", 45.0f }, { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "room", 2.0f }, { "roomMix", 40.0f }, { "attack", 600.0f }, { "release", 2000.0f } });
    add ("Opera Duet", "Choir", { { "mode", 1.0f }, { "singers", 2.0f }, { "cast", 9.0f }, { "style", 8.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "glide", 120.0f }, { "room", 2.0f }, { "roomMix", 40.0f }, { "attack", 90.0f }, { "release", 900.0f } });
    add ("Grand Opera", "Choir", { { "mode", 2.0f }, { "singers", 24.0f }, { "cast", 0.0f }, { "style", 8.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "room", 3.0f }, { "roomMix", 45.0f }, { "attack", 200.0f }, { "release", 1500.0f } });
    add ("Bulgarian Voices", "Choir", { { "mode", 1.0f }, { "singers", 6.0f }, { "cast", 1.0f }, { "style", 9.0f }, { "vowelX", 0.24f }, { "vowelY", 0.82f }, { "nose", 30.0f }, { "room", 2.0f }, { "roomMix", 30.0f }, { "attack", 40.0f }, { "release", 600.0f } });
    add ("Soul Choir", "Choir", { { "mode", 2.0f }, { "singers", 16.0f }, { "cast", 0.0f }, { "style", 10.0f }, { "vowelX", 0.8f }, { "vowelY", 0.6f }, { "rasp", 12.0f }, { "room", 2.0f }, { "roomMix", 35.0f }, { "attack", 80.0f }, { "release", 700.0f } });
    add ("Soul Singer", "Solo", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 1.0f }, { "style", 10.0f }, { "vowelX", 0.55f }, { "vowelY", 0.85f }, { "glide", 160.0f }, { "chest", 25.0f }, { "room", 4.0f }, { "roomMix", 22.0f }, { "attack", 30.0f }, { "release", 450.0f } });
    add ("Robot Choir", "Choir", { { "mode", 1.0f }, { "singers", 8.0f }, { "cast", 2.0f }, { "style", 11.0f }, { "vowelX", 0.55f }, { "vowelY", 0.5f }, { "human", 0.0f }, { "drift", 0.0f }, { "spread", 0.0f }, { "room", 4.0f }, { "roomMix", 25.0f }, { "attack", 10.0f }, { "release", 200.0f } });
    add ("Vocoder Pad", "Choir", { { "mode", 2.0f }, { "singers", 6.0f }, { "cast", 10.0f }, { "style", 11.0f }, { "move", 1.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "vowelX2", 0.06f }, { "vowelY2", 0.06f }, { "moveRate", 0.3f }, { "room", 2.0f }, { "roomMix", 35.0f }, { "attack", 400.0f }, { "release", 1500.0f } });
    add ("Bedtime Lullaby", "Solo", { { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 1.0f }, { "style", 12.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "glide", 180.0f }, { "room", 1.0f }, { "roomMix", 30.0f }, { "attack", 220.0f }, { "release", 1000.0f } });
    add ("Family Singalong", "Choir", { { "mode", 1.0f }, { "singers", 6.0f }, { "cast", 12.0f }, { "style", 3.0f }, { "vowelX", 0.3f }, { "vowelY", 0.7f }, { "human", 60.0f }, { "drift", 50.0f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 60.0f }, { "release", 400.0f } });
    add ("Cat Family", "Cats", { { "mode", 1.0f }, { "singers", 4.0f }, { "cast", 13.0f }, { "style", 3.0f }, { "onset", 4.0f }, { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 1.0f }, { "roomMix", 25.0f }, { "attack", 20.0f }, { "release", 300.0f } });
    add ("Deep Cave", "Choir", { { "mode", 2.0f }, { "singers", 8.0f }, { "cast", 10.0f }, { "style", 7.0f }, { "onset", 2.0f }, { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "neck", 40.0f }, { "chest", 50.0f }, { "room", 5.0f }, { "roomMix", 60.0f }, { "attack", 500.0f }, { "release", 2500.0f } });
    add ("Forest Spirits", "Choir", { { "mode", 1.0f }, { "singers", 6.0f }, { "cast", 11.0f }, { "style", 5.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "breath", 60.0f }, { "move", 1.0f }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "moveRate", 0.15f }, { "room", 6.0f }, { "roomMix", 45.0f }, { "micDist", 70.0f }, { "attack", 500.0f }, { "release", 2000.0f } });
    add ("Angel Voices", "Choir", { { "volume", -5.0f }, { "mode", 2.0f }, { "singers", 16.0f }, { "cast", 11.0f }, { "style", 0.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "vibrato", 50.0f }, { "room", 3.0f }, { "roomMix", 60.0f }, { "micDist", 80.0f }, { "attack", 600.0f }, { "release", 2500.0f } });
    add ("Plate Harmonies", "Choir", { { "mode", 1.0f }, { "singers", 3.0f }, { "cast", 1.0f }, { "style", 2.0f }, { "vowelX", 0.55f }, { "vowelY", 0.6f }, { "breath", 30.0f }, { "room", 4.0f }, { "roomMix", 35.0f }, { "attack", 50.0f }, { "release", 500.0f } });
    add ("Basso Chorus", "Choir", { { "mode", 2.0f }, { "singers", 12.0f }, { "cast", 10.0f }, { "style", 8.0f }, { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "chest", 40.0f }, { "room", 3.0f }, { "roomMix", 45.0f }, { "attack", 250.0f }, { "release", 1200.0f } });
    add ("Yowl Opera", "Cats", { { "volume", -10.0f }, { "mode", 0.0f }, { "singers", 1.0f }, { "cast", 6.0f }, { "style", 8.0f }, { "onset", 4.0f }, { "glide", 220.0f }, { "room", 3.0f }, { "roomMix", 40.0f }, { "release", 800.0f } });
    add ("Aero Choir", "Choir", { { "mode", 2.0f }, { "singers", 12.0f }, { "cast", 11.0f }, { "style", 5.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "breath", 45.0f }, { "move", 1.0f }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "moveRate", 0.1f }, { "spread", 70.0f }, { "room", 4.0f }, { "roomMix", 50.0f }, { "micDist", 60.0f }, { "attack", 500.0f }, { "release", 2200.0f } });
    add ("Ocean Ooh", "Choir", { { "mode", 2.0f }, { "singers", 16.0f }, { "unison", 2.0f }, { "cast", 0.0f }, { "style", 0.0f }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "move", 1.0f }, { "vowelX2", 0.93f }, { "vowelY2", 0.4f }, { "moveRate", 0.06f }, { "drift", 40.0f }, { "room", 3.0f }, { "roomMix", 60.0f }, { "micDist", 85.0f }, { "attack", 900.0f }, { "release", 3000.0f } });
    add ("Glass Ahh", "Choir", { { "mode", 1.0f }, { "singers", 6.0f }, { "cast", 11.0f }, { "style", 2.0f }, { "vowelX", 0.55f }, { "vowelY", 0.95f }, { "breath", 25.0f }, { "vibrato", 30.0f }, { "room", 4.0f }, { "roomMix", 40.0f }, { "attack", 200.0f }, { "release", 1200.0f } });
    add ("Sky Whisper", "Choir", { { "mode", 1.0f }, { "singers", 8.0f }, { "unison", 2.0f }, { "cast", 11.0f }, { "style", 5.0f }, { "vowelX", 0.16f }, { "vowelY", 0.4f }, { "breath", 90.0f }, { "spread", 80.0f }, { "room", 6.0f }, { "roomMix", 45.0f }, { "attack", 400.0f }, { "release", 1800.0f } });
    add ("Init", "Init", {});
    return e;
}

namespace
{
class ChoirEngine : public Engine
{
public:
    void prepare (double sampleRate) override
    {
        engine.prepare (sampleRate, block);
        engine.arrangeStage (s.singers, s.cast);
        sr = (float) sampleRate;
    }

    void setParams (const float* v) override
    {
        s.mode = (singcat::Mode) juce::roundToInt (v[cMode]);
        s.singers = juce::jlimit (1, 16, juce::roundToInt (v[cSingers]));
        s.unison = juce::roundToInt (v[cUnison]);
        s.cast = juce::roundToInt (v[cCast]);
        s.style = juce::roundToInt (v[cStyle]);
        s.onset = (singcat::Onset) juce::roundToInt (v[cOnset]);
        s.ax = v[cVX]; s.ay = v[cVY]; s.bx = v[cVX2]; s.by = v[cVY2];
        s.move = (singcat::Move) juce::roundToInt (v[cMove]);
        if (s.move == singcat::Move::Path) s.move = singcat::Move::Wave;   // no drawn path in the box
        s.moveRate = v[cMoveRate];
        s.spread = v[cSpread] * 0.01f;
        s.attackMs = v[cAttack]; s.releaseMs = v[cRelease];
        s.vibrato = v[cVibrato] * 0.01f; s.breath = v[cBreath] * 0.01f; s.rasp = v[cRasp] * 0.01f;
        s.glideMs = v[cGlide]; s.drift = v[cDrift] * 0.01f; s.human = v[cHuman] * 0.01f; s.purr = v[cPurr] * 0.01f;
        s.neck = v[cNeck] * 0.01f; s.jaw = v[cJaw] * 0.01f; s.lips = v[cLips] * 0.01f; s.nose = v[cNose] * 0.01f; s.chest = v[cChest] * 0.01f;
        s.room = juce::roundToInt (v[cRoom]); s.roomMix = v[cRoomMix] * 0.01f;
        s.micWidth = v[cMicWidth] * 0.01f; s.micDist = v[cMicDist] * 0.01f;
        s.volume = juce::Decibels::decibelsToGain (v[cVolume], -36.0f);
        s.seed = (uint32_t) juce::roundToInt (v[cSeed]);
        s.diction = v[cDiction] * 0.01f;
        s.highQuality = false;
        s.words = false;
        if (s.cast != lastCast || s.singers != lastSingers) { engine.arrangeStage (s.singers, s.cast); lastCast = s.cast; lastSingers = s.singers; }
        engine.setSettings (s);
    }

    void setBend (float semis) override { engine.pitchBend (1, juce::jlimit (-1.0f, 1.0f, semis / juce::jmax (0.01f, s.bendRange))); }
    void setMod (float m) override { engine.modWheel (m); }
    void noteOn (int note, float vel, bool, bool accent) override
    {
        engine.noteOn (1, note, juce::jlimit (0.01f, 1.0f, vel * (accent ? 1.2f : 1.0f)));
        tail = (int) (sr * 8.0f);
    }
    void noteOff (int note) override { engine.noteOff (1, note); }
    void allOff() override { engine.allNotesOff(); }

    void render (float* outL, float* outR, int n) override
    {
        if (tail <= 0) return;
        if (engine.numActiveSingers() == 0) tail -= n;
        for (int s0 = 0; s0 < n; s0 += block)
        {
            const int m = juce::jmin (block, n - s0);
            std::fill (bl, bl + m, 0.0f);
            std::fill (br, br + m, 0.0f);
            engine.render (bl, br, m);
            for (int i = 0; i < m; ++i) { outL[s0 + i] += bl[i] * 0.55f; outR[s0 + i] += br[i] * 0.55f; }
        }
    }

private:
    static constexpr int block = 256;
    singcat::Engine engine;
    singcat::Settings s;
    int lastCast = -1, lastSingers = -1, tail = 0;
    float sr = 48000.0f;
    float bl[block] {}, br[block] {};
};
} // namespace

std::unique_ptr<Engine> makeChoirEngine() { return std::make_unique<ChoirEngine>(); }
} // namespace catbox
