#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace singcat;

const char* const SingCatProcessor::paramIds[SingCatProcessor::numParams] = {
    "mode", "singers", "unison", "cast", "style", "onset", "vowelX", "vowelY", "vowelX2", "vowelY2", "move", "moveRate", "spread",
    "attack", "release", "vibrato", "breath", "rasp", "glide", "drift", "human", "purr", "neck", "jaw", "lips", "nose", "chest",
    "room", "roomMix", "micWidth", "micDist", "mpe", "bendRange", "quality", "volume", "seed", "words", "diction"
};

juce::AudioProcessorValueTreeState::ParameterLayout SingCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto choice = [&] (const char* id, const String& name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto integer = [&] (const char* id, const String& name, int lo, int hi, int def)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
    };
    auto fl = [&] (const char* id, const String& name, NormalisableRange<float> r, float def, const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto pct = [&] (const char* id, const String& name, float def) { fl (id, name, { 0.0f, 100.0f, 0.1f }, def, "%"); };
    auto bipolar = [&] (const char* id, const String& name) { fl (id, name, { -100.0f, 100.0f, 0.1f }, 0.0f, "%"); };

    choice ("mode", "Mode", modeNames(), 1);
    integer ("singers", "Singers", 1, maxSingers, 8);
    integer ("unison", "Voices per Note", 1, 8, 1);
    choice ("cast", "Cast", castNames(), 0);
    choice ("style", "Style", styleNames(), 0);
    choice ("onset", "Onset", onsetNames(), 0);
    fl ("vowelX", "Vowel Front-Back", { 0.0f, 1.0f, 0.001f }, 0.55f, "");
    fl ("vowelY", "Vowel Close-Open", { 0.0f, 1.0f, 0.001f }, 0.95f, "");
    fl ("vowelX2", "Target Front-Back", { 0.0f, 1.0f, 0.001f }, 0.95f, "");
    fl ("vowelY2", "Target Close-Open", { 0.0f, 1.0f, 0.001f }, 0.06f, "");
    choice ("move", "Vowel Movement", moveNames(), 0);
    fl ("moveRate", "Movement Rate", { 0.05f, 8.0f, 0.01f, 0.4f }, 0.5f, "Hz");
    pct ("spread", "Spread", 30.0f);
    fl ("attack", "Attack", { 5.0f, 3000.0f, 1.0f, 0.4f }, 120.0f, "ms");
    fl ("release", "Release", { 20.0f, 5000.0f, 1.0f, 0.4f }, 600.0f, "ms");
    fl ("vibrato", "Vibrato", { 0.0f, 200.0f, 0.1f }, 100.0f, "%");
    pct ("breath", "Breath", 20.0f);
    pct ("rasp", "Rasp", 0.0f);
    fl ("glide", "Glide", { 0.0f, 1000.0f, 1.0f, 0.5f }, 120.0f, "ms");
    pct ("drift", "Drift", 30.0f);
    pct ("human", "Looseness", 30.0f);
    pct ("purr", "Purr", 0.0f);
    bipolar ("neck", "Neck");
    bipolar ("jaw", "Jaw");
    bipolar ("lips", "Lips");
    bipolar ("nose", "Nose");
    bipolar ("chest", "Chest");
    choice ("room", "Room", roomNames(), 1);
    pct ("roomMix", "Room Mix", 30.0f);
    pct ("micWidth", "Mic Width", 70.0f);
    pct ("micDist", "Mic Distance", 40.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "mpe", 1 }, "MPE", false));
    integer ("bendRange", "Bend Range", 1, 48, 2);
    choice ("quality", "Quality", { "Eco", "High" }, 0);
    fl ("volume", "Volume", { -36.0f, 6.0f, 0.1f }, 0.0f, "dB");
    integer ("seed", "Singer Seed", 1, 9999, 1);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "words", 1 }, "Words", false));
    pct ("diction", "Diction", 50.0f);
    return layout;
}

//==============================================================================
const std::vector<SingCatProcessor::Preset>& SingCatProcessor::presets()
{
    // Vowel chart: x 0 = front .. 1 = back, y 0 = close .. 1 = open.
    // i (0.06, 0.06)  e (0.16, 0.40)  ae (0.24, 0.82)  a (0.55, 0.95)  aw (0.90, 0.70)  o (0.93, 0.40)  u (0.95, 0.06)  uh (0.55, 0.50)
    static const std::vector<Preset> list = {
        { "Chamber Choir",   { { "mode", 1 }, { "singers", 12 }, { "unison", 2 }, { "cast", 0 }, { "style", 0 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                               { "attack", 260 }, { "release", 900 }, { "room", 2 }, { "roomMix", 35 } }, {} },
        { "Gospel Mass",     { { "mode", 2 }, { "singers", 24 }, { "cast", 0 }, { "style", 1 }, { "vowelX", 0.8f }, { "vowelY", 0.6f },
                               { "attack", 120 }, { "release", 700 }, { "rasp", 10 }, { "room", 2 }, { "roomMix", 40 } }, {} },
        { "Pop Duo",         { { "mode", 1 }, { "singers", 2 }, { "cast", 1 }, { "style", 2 }, { "vowelX", 0.55f }, { "vowelY", 0.5f },
                               { "move", 2 }, { "vowelX2", 0.16f }, { "vowelY2", 0.4f }, { "moveRate", 1.5f }, { "breath", 35 },
                               { "room", 0 }, { "roomMix", 20 }, { "attack", 60 }, { "release", 350 } }, {} },
        { "Folk Trio",       { { "mode", 1 }, { "singers", 3 }, { "cast", 0 }, { "style", 3 }, { "vowelX", 0.3f }, { "vowelY", 0.7f },
                               { "room", 1 }, { "roomMix", 25 }, { "attack", 80 }, { "release", 400 }, { "nose", 15 } }, {} },
        { "Cathedral Ooh",   { { "mode", 2 }, { "singers", 32 }, { "cast", 0 }, { "style", 0 }, { "vowelX", 0.95f }, { "vowelY", 0.06f },
                               { "move", 1 }, { "vowelX2", 0.93f }, { "vowelY2", 0.4f }, { "moveRate", 0.08f }, { "spread", 60 },
                               { "attack", 700 }, { "release", 2500 }, { "room", 3 }, { "roomMix", 60 }, { "micDist", 70 } }, {} },
        { "Boy Soprano",     { { "volume", -5.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 3 }, { "style", 0 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                               { "vibrato", 45 }, { "glide", 90 }, { "room", 3 }, { "roomMix", 45 }, { "attack", 140 }, { "release", 900 } }, {} },
        { "Old Monks",       { { "mode", 2 }, { "singers", 8 }, { "cast", 2 }, { "style", 7 }, { "vowelX", 0.93f }, { "vowelY", 0.4f },
                               { "onset", 2 }, { "neck", 30 }, { "chest", 40 }, { "drift", 50 }, { "room", 3 }, { "roomMix", 55 },
                               { "attack", 400 }, { "release", 1800 } }, {} },
        { "Pansori Solo",    { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 6 }, { "vowelX", 0.55f }, { "vowelY", 0.9f },
                               { "rasp", 40 }, { "glide", 180 }, { "room", 1 }, { "roomMix", 20 }, { "attack", 40 }, { "release", 500 } }, {} },
        { "Breathy Ahh",     { { "mode", 1 }, { "singers", 6 }, { "unison", 2 }, { "cast", 1 }, { "style", 5 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                               { "breath", 70 }, { "room", 2 }, { "roomMix", 35 }, { "attack", 300 }, { "release", 1200 } }, {} },
        { "Formant Sweep",   { { "volume", -5.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 2 }, { "style", 2 }, { "vowelX", 0.06f }, { "vowelY", 0.06f },
                               { "move", 1 }, { "vowelX2", 0.95f }, { "vowelY2", 0.06f }, { "moveRate", 0.5f }, { "room", 0 }, { "roomMix", 15 } }, {} },
        { "Kitten Choir",    { { "mode", 1 }, { "singers", 8 }, { "unison", 2 }, { "cast", 5 }, { "style", 3 }, { "onset", 0 },
                               { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 1 }, { "roomMix", 25 }, { "attack", 30 }, { "release", 300 } }, {} },
        { "Tomcat Serenade", { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 6 }, { "style", 4 }, { "onset", 4 }, { "rasp", 30 },
                               { "glide", 200 }, { "vowelX", 0.8f }, { "vowelY", 0.6f }, { "room", 1 }, { "roomMix", 25 }, { "release", 500 } }, {} },
        { "Purring Pad",     { { "mode", 2 }, { "singers", 6 }, { "cast", 7 }, { "style", 7 }, { "purr", 80 }, { "vowelX", 0.55f }, { "vowelY", 0.5f },
                               { "attack", 800 }, { "release", 2000 }, { "room", 2 }, { "roomMix", 40 } }, {} },
        { "Meow Lead",       { { "volume", -5.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 7 }, { "style", 2 }, { "onset", 4 }, { "glide", 60 },
                               { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 0 }, { "roomMix", 15 }, { "attack", 15 }, { "release", 250 } }, {} },
        { "Cats & Humans",   { { "mode", 2 }, { "singers", 16 }, { "cast", 8 }, { "style", 1 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                               { "room", 2 }, { "roomMix", 35 }, { "attack", 150 }, { "release", 800 } }, {} },
        { "Vowel Rider",     { { "mode", 1 }, { "singers", 8 }, { "cast", 0 }, { "style", 2 }, { "move", 3 }, { "moveRate", 2.0f },
                               { "mpe", 1 }, { "bendRange", 48 }, { "room", 1 }, { "roomMix", 25 }, { "attack", 40 }, { "release", 400 } }, {} },
        { "Drawn Path",      { { "mode", 1 }, { "singers", 6 }, { "cast", 1 }, { "style", 3 }, { "move", 4 }, { "moveRate", 0.3f }, { "spread", 50 },
                               { "room", 2 }, { "roomMix", 30 }, { "attack", 200 }, { "release", 900 } },
                             { { 0.06f, 0.06f }, { 0.16f, 0.4f }, { 0.4f, 0.8f }, { 0.55f, 0.95f }, { 0.85f, 0.7f }, { 0.93f, 0.4f }, { 0.95f, 0.06f } } },
        { "Madrigal Quartet", { { "mode", 1 }, { "singers", 4 }, { "cast", 0 }, { "style", 0 }, { "vowelX", 0.3f }, { "vowelY", 0.6f },
                                { "vibrato", 60 }, { "room", 1 }, { "roomMix", 30 }, { "attack", 90 }, { "release", 500 } }, {} },
        { "Lullaby",          { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 3 }, { "vowelX", 0.55f }, { "vowelY", 0.5f },
                                { "breath", 40 }, { "vibrato", 40 }, { "glide", 160 }, { "room", 1 }, { "roomMix", 30 }, { "attack", 180 }, { "release", 800 } }, {} },
        { "Soul Diva",        { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 1 }, { "vowelX", 0.5f }, { "vowelY", 0.85f },
                                { "glide", 150 }, { "rasp", 15 }, { "chest", 30 }, { "room", 0 }, { "roomMix", 20 }, { "attack", 40 }, { "release", 450 } }, {} },
        { "Barbershop",       { { "mode", 1 }, { "singers", 4 }, { "cast", 2 }, { "style", 4 }, { "vowelX", 0.93f }, { "vowelY", 0.4f },
                                { "vibrato", 50 }, { "room", 1 }, { "roomMix", 25 }, { "attack", 70 }, { "release", 400 } }, {} },
        { "Children's Choir", { { "mode", 2 }, { "singers", 16 }, { "cast", 3 }, { "style", 0 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "vibrato", 35 }, { "room", 2 }, { "roomMix", 40 }, { "attack", 200 }, { "release", 900 } }, {} },
        { "Grandparents",     { { "mode", 1 }, { "singers", 8 }, { "unison", 2 }, { "cast", 4 }, { "style", 3 }, { "vowelX", 0.3f }, { "vowelY", 0.7f },
                                { "drift", 60 }, { "human", 60 }, { "room", 1 }, { "roomMix", 30 }, { "attack", 150 }, { "release", 700 } }, {} },
        { "Whisper Choir",    { { "mode", 1 }, { "singers", 8 }, { "unison", 2 }, { "cast", 0 }, { "style", 5 }, { "vowelX", 0.55f }, { "vowelY", 0.7f },
                                { "breath", 100 }, { "vibrato", 20 }, { "room", 2 }, { "roomMix", 45 }, { "attack", 350 }, { "release", 1500 } }, {} },
        { "Throat Drone",     { { "mode", 2 }, { "singers", 4 }, { "cast", 2 }, { "style", 7 }, { "onset", 2 }, { "neck", 60 }, { "chest", 60 },
                                { "move", 1 }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "vowelX2", 0.93f }, { "vowelY2", 0.4f }, { "moveRate", 0.12f },
                                { "room", 3 }, { "roomMix", 50 }, { "attack", 600 }, { "release", 2500 } }, {} },
        { "Nasal Folk",       { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 2 }, { "style", 3 }, { "nose", 80 }, { "vowelX", 0.24f }, { "vowelY", 0.8f },
                                { "glide", 80 }, { "room", 1 }, { "roomMix", 20 } }, {} },
        { "Jazz Scat",        { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 2 }, { "style", 4 }, { "move", 5 }, { "attack", 20 }, { "release", 200 },
                                { "glide", 60 }, { "room", 0 }, { "roomMix", 15 } }, {} },
        { "Wah Choir",        { { "mode", 1 }, { "singers", 6 }, { "cast", 1 }, { "style", 2 }, { "move", 1 }, { "moveRate", 2.0f },
                                { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "spread", 10 },
                                { "room", 0 }, { "roomMix", 20 }, { "attack", 30 }, { "release", 300 } }, {} },
        { "Vowel Steps",      { { "mode", 1 }, { "singers", 8 }, { "cast", 0 }, { "style", 0 }, { "move", 3 }, { "moveRate", 1.0f },
                                { "room", 2 }, { "roomMix", 35 }, { "attack", 120 }, { "release", 700 } }, {} },
        { "Big Hall Tutti",   { { "mode", 2 }, { "singers", 64 }, { "cast", 0 }, { "style", 0 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "room", 2 }, { "roomMix", 45 }, { "micDist", 60 }, { "attack", 400 }, { "release", 1500 } }, {} },
        { "Alley Cat Fight",  { { "mode", 1 }, { "singers", 6 }, { "cast", 7 }, { "style", 6 }, { "onset", 4 }, { "rasp", 50 },
                                { "vowelX", 0.6f }, { "vowelY", 0.9f }, { "room", 1 }, { "roomMix", 25 }, { "attack", 20 }, { "release", 300 } }, {} },
        { "Kitten Lullaby",   { { "volume", -5.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 5 }, { "style", 5 }, { "onset", 4 }, { "glide", 120 },
                                { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 1 }, { "roomMix", 30 }, { "release", 500 } }, {} },
        { "Purring Tomcats",  { { "mode", 2 }, { "singers", 4 }, { "cast", 6 }, { "style", 7 }, { "purr", 60 }, { "vowelX", 0.55f }, { "vowelY", 0.5f },
                                { "room", 1 }, { "roomMix", 30 }, { "attack", 500 }, { "release", 1500 } }, {} },
        { "Meow Pluck",       { { "mode", 1 }, { "singers", 4 }, { "cast", 7 }, { "style", 2 }, { "onset", 4 }, { "attack", 5 }, { "release", 150 },
                                { "vowelX", 0.55f }, { "vowelY", 0.9f }, { "room", 0 }, { "roomMix", 15 } }, {} },
        { "Close Mic Solo",   { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 2 }, { "breath", 30 }, { "micDist", 0 },
                                { "room", 0 }, { "roomMix", 5 }, { "vowelX", 0.55f }, { "vowelY", 0.6f }, { "attack", 30 }, { "release", 300 } }, {} },
        { "Far Away Choir",   { { "mode", 2 }, { "singers", 24 }, { "cast", 0 }, { "style", 0 }, { "micDist", 100 }, { "roomMix", 70 },
                                { "room", 3 }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "attack", 600 }, { "release", 2500 } }, {} },
        { "Requiem",          { { "mode", 2 }, { "singers", 40 }, { "cast", 0 }, { "style", 0 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "move", 2 }, { "vowelX2", 0.95f }, { "vowelY2", 0.06f }, { "moveRate", 0.25f }, { "room", 3 }, { "roomMix", 55 },
                                { "attack", 900 }, { "release", 3000 }, { "chest", 20 } }, {} },
        { "Choir Stabs",      { { "mode", 2 }, { "singers", 16 }, { "cast", 0 }, { "style", 1 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "attack", 10 }, { "release", 180 }, { "room", 2 }, { "roomMix", 30 } }, {} },
        { "Gospel Lead",      { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 2 }, { "style", 1 }, { "vowelX", 0.8f }, { "vowelY", 0.6f },
                                { "glide", 140 }, { "rasp", 25 }, { "chest", 40 }, { "room", 1 }, { "roomMix", 25 } }, {} },
        { "Folk Harmony",     { { "mode", 1 }, { "singers", 2 }, { "cast", 0 }, { "style", 3 }, { "vowelX", 0.3f }, { "vowelY", 0.7f },
                                { "nose", 20 }, { "room", 1 }, { "roomMix", 20 } }, {} },
        { "Soprano Aria",     { { "volume", -5.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 0 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "vibrato", 120 }, { "glide", 110 }, { "room", 2 }, { "roomMix", 40 }, { "attack", 120 }, { "release", 900 } }, {} },
        { "Bass Profundo",    { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 2 }, { "style", 7 }, { "neck", 50 }, { "chest", 70 },
                                { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "room", 3 }, { "roomMix", 45 } }, {} },
        { "Doo-Wop",          { { "mode", 1 }, { "singers", 4 }, { "cast", 2 }, { "style", 4 }, { "onset", 3 },
                                { "move", 2 }, { "vowelX", 0.95f }, { "vowelY", 0.06f }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "moveRate", 4.0f },
                                { "attack", 20 }, { "release", 250 }, { "room", 1 }, { "roomMix", 25 } }, {} },
        { "Pansori Duet",     { { "mode", 1 }, { "singers", 2 }, { "cast", 0 }, { "style", 6 }, { "rasp", 35 }, { "vowelX", 0.55f }, { "vowelY", 0.9f },
                                { "room", 1 }, { "roomMix", 25 } }, {} },
        { "Nursery Rhyme",    { { "mode", 1 }, { "singers", 6 }, { "cast", 3 }, { "style", 3 }, { "vowelX", 0.3f }, { "vowelY", 0.7f },
                                { "human", 70 }, { "drift", 60 }, { "room", 1 }, { "roomMix", 25 }, { "attack", 40 }, { "release", 350 } }, {} },
        { "Cat Opera",        { { "volume", -5.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 7 }, { "style", 0 }, { "onset", 4 }, { "vibrato", 150 },
                                { "glide", 150 }, { "room", 3 }, { "roomMix", 45 }, { "release", 700 } }, {} },
        { "Midnight Alley",   { { "mode", 1 }, { "singers", 8 }, { "cast", 7 }, { "style", 4 }, { "onset", 4 }, { "rasp", 20 },
                                { "human", 80 }, { "room", 2 }, { "roomMix", 40 }, { "micDist", 70 }, { "release", 600 } }, {} },
        { "Purr & Hum",       { { "mode", 2 }, { "singers", 8 }, { "cast", 8 }, { "style", 7 }, { "onset", 2 }, { "purr", 45 },
                                { "vowelX", 0.93f }, { "vowelY", 0.4f }, { "room", 2 }, { "roomMix", 40 }, { "attack", 600 }, { "release", 2000 } }, {} },
        // ---- v2: new styles (8 Opera, 9 Bulgarian, 10 Soul, 11 Robot, 12 Lullaby), casts (9 Opera Stars, 10 Low, 11 High,
        //      12 Family, 13 Cat Family) and rooms (4 Plate, 5 Cave, 6 Forest)
        { "Opera Duet",       { { "mode", 1 }, { "singers", 2 }, { "cast", 9 }, { "style", 8 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "glide", 120 }, { "room", 2 }, { "roomMix", 40 }, { "attack", 90 }, { "release", 900 } }, {} },
        { "Grand Opera",      { { "mode", 2 }, { "singers", 24 }, { "cast", 0 }, { "style", 8 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "room", 3 }, { "roomMix", 45 }, { "attack", 200 }, { "release", 1500 } }, {} },
        { "Bulgarian Voices", { { "mode", 1 }, { "singers", 6 }, { "cast", 1 }, { "style", 9 }, { "vowelX", 0.24f }, { "vowelY", 0.82f },
                                { "nose", 30 }, { "room", 2 }, { "roomMix", 30 }, { "attack", 40 }, { "release", 600 } }, {} },
        { "Soul Choir",       { { "mode", 2 }, { "singers", 16 }, { "cast", 0 }, { "style", 10 }, { "vowelX", 0.8f }, { "vowelY", 0.6f },
                                { "rasp", 12 }, { "room", 2 }, { "roomMix", 35 }, { "attack", 80 }, { "release", 700 } }, {} },
        { "Soul Singer",      { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 10 }, { "vowelX", 0.55f }, { "vowelY", 0.85f },
                                { "glide", 160 }, { "chest", 25 }, { "room", 4 }, { "roomMix", 22 }, { "attack", 30 }, { "release", 450 } }, {} },
        { "Robot Choir",      { { "mode", 1 }, { "singers", 8 }, { "cast", 2 }, { "style", 11 }, { "vowelX", 0.55f }, { "vowelY", 0.5f },
                                { "human", 0 }, { "drift", 0 }, { "spread", 0 }, { "room", 4 }, { "roomMix", 25 }, { "attack", 10 }, { "release", 200 } }, {} },
        { "Vocoder Pad",      { { "mode", 2 }, { "singers", 6 }, { "cast", 10 }, { "style", 11 }, { "move", 1 }, { "vowelX", 0.95f }, { "vowelY", 0.06f },
                                { "vowelX2", 0.06f }, { "vowelY2", 0.06f }, { "moveRate", 0.3f }, { "room", 2 }, { "roomMix", 35 }, { "attack", 400 }, { "release", 1500 } }, {} },
        { "Bedtime Lullaby",  { { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 12 }, { "vowelX", 0.95f }, { "vowelY", 0.06f },
                                { "glide", 180 }, { "room", 1 }, { "roomMix", 30 }, { "attack", 220 }, { "release", 1000 } }, {} },
        { "Family Singalong", { { "mode", 1 }, { "singers", 6 }, { "cast", 12 }, { "style", 3 }, { "vowelX", 0.3f }, { "vowelY", 0.7f },
                                { "human", 60 }, { "drift", 50 }, { "room", 1 }, { "roomMix", 25 }, { "attack", 60 }, { "release", 400 } }, {} },
        { "Cat Family",       { { "mode", 1 }, { "singers", 4 }, { "cast", 13 }, { "style", 3 }, { "onset", 4 }, { "vowelX", 0.55f }, { "vowelY", 0.9f },
                                { "room", 1 }, { "roomMix", 25 }, { "attack", 20 }, { "release", 300 } }, {} },
        { "Deep Cave",        { { "mode", 2 }, { "singers", 8 }, { "cast", 10 }, { "style", 7 }, { "onset", 2 }, { "vowelX", 0.93f }, { "vowelY", 0.4f },
                                { "neck", 40 }, { "chest", 50 }, { "room", 5 }, { "roomMix", 60 }, { "attack", 500 }, { "release", 2500 } }, {} },
        { "Forest Spirits",   { { "mode", 1 }, { "singers", 6 }, { "cast", 11 }, { "style", 5 }, { "vowelX", 0.95f }, { "vowelY", 0.06f },
                                { "breath", 60 }, { "move", 1 }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "moveRate", 0.15f },
                                { "room", 6 }, { "roomMix", 45 }, { "micDist", 70 }, { "attack", 500 }, { "release", 2000 } }, {} },
        { "Angel Voices",     { { "volume", -5.0f }, { "mode", 2 }, { "singers", 16 }, { "cast", 11 }, { "style", 0 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "vibrato", 50 }, { "room", 3 }, { "roomMix", 60 }, { "micDist", 80 }, { "attack", 600 }, { "release", 2500 } }, {} },
        { "Plate Harmonies",  { { "mode", 1 }, { "singers", 3 }, { "cast", 1 }, { "style", 2 }, { "vowelX", 0.55f }, { "vowelY", 0.6f },
                                { "breath", 30 }, { "room", 4 }, { "roomMix", 35 }, { "attack", 50 }, { "release", 500 } }, {} },
        { "Basso Chorus",     { { "mode", 2 }, { "singers", 12 }, { "cast", 10 }, { "style", 8 }, { "vowelX", 0.93f }, { "vowelY", 0.4f },
                                { "chest", 40 }, { "room", 3 }, { "roomMix", 45 }, { "attack", 250 }, { "release", 1200 } }, {} },
        { "Yowl Opera",       { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 6 }, { "style", 8 }, { "onset", 4 }, { "glide", 220 },
                                { "room", 3 }, { "roomMix", 40 }, { "release", 800 } }, {} },
        // ---- word presets (Words on: each note sings the next syllable)
        { "Amen Choir",       { { "mode", 2 }, { "singers", 24 }, { "cast", 0 }, { "style", 0 }, { "words", 1 }, { "diction", 40 },
                                { "room", 3 }, { "roomMix", 50 }, { "attack", 300 }, { "release", 1500 } }, {}, "a + men" },
        { "Hallelujah",       { { "mode", 1 }, { "singers", 12 }, { "unison", 2 }, { "cast", 0 }, { "style", 1 }, { "words", 1 },
                                { "room", 2 }, { "roomMix", 35 }, { "release", 700 } }, {}, "hal-le-lu-jah" },
        { "Doo-Wop Words",    { { "mode", 1 }, { "singers", 4 }, { "cast", 2 }, { "style", 4 }, { "words", 1 }, { "diction", 70 },
                                { "room", 1 }, { "roomMix", 25 }, { "release", 250 } }, {}, "doo bee doo wah, shoo bop shoo bop" },
        { "Pop Hook",         { { "volume", -10.0f }, { "mode", 0 }, { "singers", 1 }, { "cast", 1 }, { "style", 2 }, { "words", 1 }, { "glide", 70 },
                                { "room", 0 }, { "roomMix", 18 }, { "release", 300 } }, {}, "oh ba-by ba-by, oh my love" },
        { "Kitten Talk",      { { "mode", 1 }, { "singers", 4 }, { "cast", 5 }, { "style", 3 }, { "words", 1 }, { "diction", 60 },
                                { "room", 1 }, { "roomMix", 25 }, { "release", 300 } }, {}, "meow meow mew nyan" },
        { "Kyrie",            { { "mode", 2 }, { "singers", 8 }, { "cast", 2 }, { "style", 7 }, { "words", 1 }, { "diction", 35 },
                                { "room", 3 }, { "roomMix", 55 }, { "attack", 400 }, { "release", 2000 } }, {}, "ky-ri-e e-lei-son" },
        { "Gloria",           { { "mode", 2 }, { "singers", 16 }, { "cast", 0 }, { "style", 0 }, { "words", 1 }, { "diction", 45 },
                                { "room", 3 }, { "roomMix", 50 }, { "attack", 150 }, { "release", 1200 } }, {}, "glo-ri-a in ex-cel-sis de-o" },
        { "La La Pop",        { { "volume", -5.0f }, { "mode", 1 }, { "singers", 3 }, { "cast", 1 }, { "style", 2 }, { "words", 1 }, { "diction", 60 },
                                { "room", 4 }, { "roomMix", 22 }, { "release", 280 } }, {}, "la la la, la la la la, na na na" },
        { "Robot Words",      { { "mode", 1 }, { "singers", 4 }, { "cast", 2 }, { "style", 11 }, { "words", 1 }, { "diction", 80 },
                                { "room", 4 }, { "roomMix", 20 }, { "release", 200 } }, {}, "we are the ro-bots, beep bop beep" },
        // ---- Aero: airy, glossy, watery (Frutiger Aero)
        { "Aero Choir",       { { "mode", 2 }, { "singers", 12 }, { "cast", 11 }, { "style", 5 }, { "vowelX", 0.95f }, { "vowelY", 0.06f },
                                { "breath", 45 }, { "move", 1 }, { "vowelX2", 0.55f }, { "vowelY2", 0.95f }, { "moveRate", 0.1f }, { "spread", 70 },
                                { "room", 4 }, { "roomMix", 50 }, { "micDist", 60 }, { "attack", 500 }, { "release", 2200 } }, {} },
        { "Ocean Ooh",        { { "mode", 2 }, { "singers", 16 }, { "unison", 2 }, { "cast", 0 }, { "style", 0 }, { "vowelX", 0.95f }, { "vowelY", 0.06f },
                                { "move", 1 }, { "vowelX2", 0.93f }, { "vowelY2", 0.4f }, { "moveRate", 0.06f }, { "drift", 40 },
                                { "room", 3 }, { "roomMix", 60 }, { "micDist", 85 }, { "attack", 900 }, { "release", 3000 } }, {} },
        { "Glass Ahh",        { { "mode", 1 }, { "singers", 6 }, { "cast", 11 }, { "style", 2 }, { "vowelX", 0.55f }, { "vowelY", 0.95f },
                                { "breath", 25 }, { "vibrato", 30 }, { "room", 4 }, { "roomMix", 40 }, { "attack", 200 }, { "release", 1200 } }, {} },
        { "Sky Whisper",      { { "mode", 1 }, { "singers", 8 }, { "unison", 2 }, { "cast", 11 }, { "style", 5 }, { "vowelX", 0.16f }, { "vowelY", 0.4f },
                                { "breath", 90 }, { "spread", 80 }, { "room", 6 }, { "roomMix", 45 }, { "attack", 400 }, { "release", 1800 } }, {} },
        { "Bubble Pop Duo",   { { "mode", 1 }, { "singers", 2 }, { "cast", 1 }, { "style", 2 }, { "words", 1 }, { "diction", 55 }, { "glide", 60 },
                                { "room", 4 }, { "roomMix", 28 }, { "attack", 30 }, { "release", 350 } }, {}, "bub-ble, bub-ble, in the blue, o-cean sky" },
    };
    return list;
}

int SingCatProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String SingCatProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size()) ? presets()[(size_t) index].name : "";
}

//==============================================================================
SingCatProcessor::SingCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    {
        cat::LinkClient::Caps caps;
        caps.receiveChords = true;
        caps.receiveChordsText = "Sing Chord-Cat's chords";
        presetManager.enableLink (apvts, caps);
    }
    for (int i = 0; i < numParams; ++i)
    {
        refs[(size_t) i] = apvts.getRawParameterValue (paramIds[i]);
        jassert (refs[(size_t) i] != nullptr);
    }
    engine.setLyrics (lyricsText);
    applyPreset (0);
}

SingCatProcessor::~SingCatProcessor() = default;

bool SingCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SingCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSync.prepare (sampleRate);
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock);
    work.setSize (2, juce::jmax (512, samplesPerBlock), false, false, true);
}

singcat::Settings SingCatProcessor::readSettings() const
{
    Settings s;
    s.mode = (Mode) juce::roundToInt (p (mode));
    s.singers = juce::roundToInt (p (singers));
    s.unison = juce::roundToInt (p (unison));
    s.cast = juce::roundToInt (p (cast));
    s.style = juce::roundToInt (p (style));
    s.onset = (Onset) juce::roundToInt (p (onset));
    s.ax = p (vowelX); s.ay = p (vowelY);
    s.bx = p (vowelX2); s.by = p (vowelY2);
    s.move = (Move) juce::roundToInt (p (move));
    s.moveRate = p (moveRate);
    s.spread = p (spread) * 0.01f;
    s.attackMs = p (attack);
    s.releaseMs = p (release);
    s.vibrato = p (vibrato) * 0.01f;
    s.breath = p (breath) * 0.01f;
    s.rasp = p (rasp) * 0.01f;
    s.glideMs = p (glide);
    s.drift = p (drift) * 0.01f;
    s.human = p (human) * 0.01f;
    s.purr = p (purr) * 0.01f;
    s.neck = p (neck) * 0.01f;
    s.jaw = p (jaw) * 0.01f;
    s.lips = p (lips) * 0.01f;
    s.nose = p (nose) * 0.01f;
    s.chest = p (chest) * 0.01f;
    s.room = juce::roundToInt (p (room));
    s.roomMix = p (roomMix) * 0.01f;
    s.micWidth = p (micWidth) * 0.01f;
    s.micDist = p (micDist) * 0.01f;
    s.mpe = p (mpe) > 0.5f;
    s.bendRange = p (bendRange);
    s.highQuality = juce::roundToInt (p (quality)) == 1;
    s.volume = juce::Decibels::decibelsToGain (p (volume), -36.0f);
    s.seed = (uint32_t) juce::roundToInt (p (seed));
    s.words = p (words) > 0.5f;
    s.diction = p (diction) * 0.01f;
    return s;
}

void SingCatProcessor::handleMidi (const juce::MidiMessage& m)
{
    const int ch = m.getChannel();
    if (m.isNoteOn())
    {
        engine.noteOn (ch, m.getNoteNumber(), m.getFloatVelocity());
        noteMonitor.noteOn (ch, m.getNoteNumber(), m.getVelocity());
    }
    else if (m.isNoteOff())
    {
        engine.noteOff (ch, m.getNoteNumber());
        noteMonitor.noteOff (m.getNoteNumber());
    }
    else if (m.isPitchWheel())
        engine.pitchBend (ch, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.pressure (ch, (float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())
        engine.pressure (ch, (float) m.getAfterTouchValue() / 127.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float v = (float) m.getControllerValue() / 127.0f;
        if (cc == 1)       engine.modWheel (v);
        else if (cc == 74) engine.timbre (ch, v);
        else if (cc == 64) engine.sustain (m.getControllerValue() >= 64);
        else if (cc == 120 || cc == 123)
        {
            engine.allNotesOff();
            noteMonitor.clear();
        }
    }
}

void SingCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    presetManager.linkClient->processInput (midi, buffer.getNumSamples());     // Cat Link: Chord-Cat's chord
    const int numSamples = buffer.getNumSamples();
    mergeComputerKeyboard (midi, numSamples);      // standalone app: notes from the computer keyboard
    buffer.clear();

    hostSync.beginBlock (presetManager.linkClient->playHead (getPlayHead(), numSamples, getSampleRate()), numSamples);   // Cat Link: shared tempo in the apps
    midiLearn.processMidi (midi);
    if (hostSync.get().hostPlaying && hostSync.get().jumped)
        engine.restartLyrics();          // song (re)starts or loops: lyrics from the top

    const auto settings = readSettings();
    // New cast or choir size (or a preset): stand them in the default choir arrangement
    const bool reset = stageResetPending.exchange (false);
    if (reset || (lastCast >= 0 && (settings.cast != lastCast || settings.singers != lastSingers)))
        engine.arrangeStage (settings.singers, settings.cast);
    lastCast = settings.cast;
    lastSingers = settings.singers;
    engine.setSettings (settings);

    if (work.getNumSamples() < numSamples)
        work.setSize (2, numSamples, false, false, true);   // only if the host breaks its promise
    work.clear();

    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        if (at > pos)
        {
            engine.render (l + pos, r + pos, at - pos);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < numSamples)
        engine.render (l + pos, r + pos, numSamples - pos);
    midi.clear();

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.copyFrom (1, 0, work, 1, 0, numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, numSamples);
        buffer.addFrom (0, 0, work, 1, 0, numSamples);
        buffer.applyGain (0, 0, numSamples, 0.5f);
    }

    uiPpq = hostSync.ppqAt (0);
    uiTransportRunning = hostSync.isRunning();
    hostSync.endBlock();
}

//==============================================================================
void SingCatProcessor::applyPreset (int index)
{
    const auto& list = presets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    currentPreset = index;

    // Defaults first (volume and quality are kept), then the preset's own values
    for (int i = 0; i < numParams; ++i)
    {
        if (i == volume || i == quality) continue;
        if (auto* param = apvts.getParameter (paramIds[i]))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->getDefaultValue());
            param->endChangeGesture();
        }
    }
    for (const auto& [id, value] : list[(size_t) index].values)
        cat::SurpriseControls::setParam (*this, id, value);

    if (list[(size_t) index].lyrics != nullptr)
        setLyricsText (list[(size_t) index].lyrics);
    engine.restartLyrics();

    if (! list[(size_t) index].path.empty())
        engine.setPath (list[(size_t) index].path);
    else if (engine.getPath().empty())
        engine.setPath ({ { 0.06f, 0.06f }, { 0.55f, 0.95f }, { 0.95f, 0.06f } });
    stageResetPending = true;
}

void SingCatProcessor::surprise()
{
    auto& rnd = juce::Random::getSystemRandom();
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    const auto& anchors = vowelAnchors();
    const auto& a = anchors[(size_t) rnd.nextInt (8)];
    const auto& b = anchors[(size_t) rnd.nextInt (8)];

    set ("mode", (float) rnd.nextInt (3));
    set ("singers", (float) juce::jlimit (1, 32, 1 + (int) (std::pow (rnd.nextFloat(), 2.0f) * 24.0f)));
    set ("unison", (float) (1 + rnd.nextInt (3)));
    set ("cast", (float) rnd.nextInt (castNames().size()));
    set ("style", (float) rnd.nextInt (8));
    set ("onset", rnd.nextFloat() < 0.6f ? 0.0f : (float) rnd.nextInt (5));
    set ("vowelX", a.x); set ("vowelY", a.y);
    set ("vowelX2", b.x); set ("vowelY2", b.y);
    set ("move", rnd.nextFloat() < 0.35f ? 0.0f : (float) rnd.nextInt (6));
    set ("moveRate", 0.05f + std::pow (rnd.nextFloat(), 2.0f) * 3.0f);
    set ("spread", rnd.nextFloat() * 80.0f);
    set ("attack", 10.0f + std::pow (rnd.nextFloat(), 2.0f) * 800.0f);
    set ("release", 150.0f + std::pow (rnd.nextFloat(), 2.0f) * 2000.0f);
    set ("vibrato", 30.0f + rnd.nextFloat() * 120.0f);
    set ("breath", rnd.nextFloat() * 60.0f);
    set ("rasp", rnd.nextFloat() < 0.7f ? 0.0f : rnd.nextFloat() * 50.0f);
    set ("drift", rnd.nextFloat() * 60.0f);
    set ("purr", rnd.nextFloat() < 0.8f ? 0.0f : rnd.nextFloat() * 80.0f);
    for (auto* id : { "neck", "jaw", "lips", "nose", "chest" })
        set (id, rnd.nextFloat() < 0.5f ? 0.0f : (rnd.nextFloat() * 2.0f - 1.0f) * 50.0f);
    set ("room", (float) rnd.nextInt (4));
    set ("roomMix", 10.0f + rnd.nextFloat() * 50.0f);
    set ("seed", (float) (1 + rnd.nextInt (9998)));
    stageResetPending = true;
}

void SingCatProcessor::setLyricsText (const juce::String& text)
{
    lyricsText = text.substring (0, 2000);
    engine.setLyrics (lyricsText);
}

void SingCatProcessor::rollDice()
{
    const int newSeed = 1 + juce::Random::getSystemRandom().nextInt (9998);
    cat::SurpriseControls::setParam (*this, "seed", (float) newSeed);
    engine.shuffleStage ((uint32_t) newSeed, juce::roundToInt (p (singers)));
}

void SingCatProcessor::resetStage()
{
    stageResetPending = true;
}

//==============================================================================
void SingCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("SINGCAT");
    root.setProperty ("preset", currentPreset.load(), nullptr);

    juce::StringArray stage, pathText;
    for (int i = 0; i < maxSingers; ++i)
        stage.add (juce::String (engine.stageX[(size_t) i].load(), 3) + "," + juce::String (engine.stageY[(size_t) i].load(), 3));
    for (const auto& pt : engine.getPath())
        pathText.add (juce::String (pt.x, 3) + "," + juce::String (pt.y, 3));
    root.setProperty ("stage", stage.joinIntoString (";"), nullptr);
    root.setProperty ("micX", engine.micX.load(), nullptr);
    root.setProperty ("micY", engine.micY.load(), nullptr);
    root.setProperty ("faceView", faceView.load(), nullptr);
    root.setProperty ("path", pathText.joinIntoString (";"), nullptr);
    root.setProperty ("lyrics", lyricsText, nullptr);

    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void SingCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("SINGCAT") && ! root.hasType ("TALKCAT")) return;   // TALKCAT = saved before the rename

    currentPreset = (int) root.getProperty ("preset", 0);
    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));

    auto parsePoints = [] (const juce::String& text)
    {
        std::vector<juce::Point<float>> pts;
        for (const auto& item : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto xy = juce::StringArray::fromTokens (item, ",", "");
            if (xy.size() == 2)
                pts.push_back ({ xy[0].getFloatValue(), xy[1].getFloatValue() });
        }
        return pts;
    };

    const auto stage = parsePoints (root.getProperty ("stage").toString());
    for (size_t i = 0; i < stage.size() && i < (size_t) maxSingers; ++i)
    {
        engine.stageX[i] = juce::jlimit (-1.0f, 1.0f, stage[i].x);
        engine.stageY[i] = juce::jlimit (0.0f, 1.0f, stage[i].y);
    }
    engine.setPath (parsePoints (root.getProperty ("path").toString()));
    engine.micX = juce::jlimit (-1.0f, 1.0f, (float) root.getProperty ("micX", 0.0f));
    engine.micY = juce::jlimit (-0.22f, 1.0f, (float) root.getProperty ("micY", -0.2f));
    faceView = juce::jlimit (0, 2, (int) root.getProperty ("faceView", 0));
    if (root.hasProperty ("lyrics"))
        setLyricsText (root.getProperty ("lyrics").toString());

    // The loaded stage positions win over the default arrangement
    stageResetPending = stage.empty();
    lastCast = juce::roundToInt (p (cast));
    lastSingers = juce::roundToInt (p (singers));
}

juce::AudioProcessorEditor* SingCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<SingCatEditor> (*this), "Sing-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SingCatProcessor();
}
