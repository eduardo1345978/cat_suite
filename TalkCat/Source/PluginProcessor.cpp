#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace talkcat;

namespace
{
    juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    juce::StringArray delayNames() { return { "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "1/8 triplet" }; }
    double delayBeats (int i) { static const double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 1.0 / 3.0 }; return b[juce::jlimit (0, 6, i)]; }

    // Parameters a voice preset / surprise may change (everything that is "the voice", not the song)
    const juce::StringArray& voiceParamIds()
    {
        static const juce::StringArray ids { "style", "size", "gender", "age", "cat", "breath", "tension", "rasp", "jitter", "nasal", "ring",
                                             "jaw", "lips", "tongue", "throat", "bright", "vibDepth", "vibRate", "vibDelay", "porta", "overshoot",
                                             "scoop", "fall", "drift", "prep", "consLength", "consStrength", "attack", "release", "autoBreath",
                                             "h1", "h2", "h3", "h1Level", "h2Level", "h3Level", "h1Pan", "h2Pan", "h3Pan", "dblLevel", "dblDetune",
                                             "dblDelay", "width", "lowCut", "presence", "air", "comp", "drive", "delayMix", "delayTime", "delayFb",
                                             "reverb", "reverbMix", "reverbSize" };
        return ids;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TalkCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto fl = [&] (const String& id, const String& name, float lo, float hi, float def, float skew = 1.0f, const char* unit = "")
    {
        const String u (unit);
        auto toText = [u] (float v, int) -> String
        {
            if (u == "%")    return String (roundToInt (v * 100.0f)) + "%";
            if (u == "pm")   return (v > 0.005f ? "+" : "") + String (roundToInt (v * 100.0f));
            if (u == "Hz")   return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : (v < 20.0f ? String (v, 1) : String (roundToInt (v))) + " Hz";
            if (u == "ms")   return String (roundToInt (v)) + " ms";
            if (u == "s")    return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s";
            if (u == "dB")   return String (v, 1) + " dB";
            if (u == "c")    return String (roundToInt (v)) + " ct";
            if (u == "cm")   return String (v, 1) + " cm";
            if (u == "x")    return String (roundToInt (v * 100.0f)) + "%";
            return String (v, 2);
        };
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, skew), def,
                                                           AudioParameterFloatAttributes().withLabel (u == "%" || u == "pm" || u == "x" ? String() : u)
                                                               .withStringFromValueFunction (toText)));
    };
    auto pct = [&] (const String& id, const String& name, float def) { fl (id, name, 0.0f, 1.0f, def, 1.0f, "%"); };
    auto bip = [&] (const String& id, const String& name, float def) { fl (id, name, -1.0f, 1.0f, def, 1.0f, "pm"); };
    auto ch = [&] (const String& id, const String& name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    ch ("mode", "Play Mode", { "Score", "Live MIDI", "Score + Live" }, 2);
    ch ("language", "Language", languageNames(), 0);
    ch ("style", "Singing Style", styleNames(), 1);
    fl ("volume", "Volume", -36.0f, 6.0f, 0.0f, 1.0f, "dB");

    // Voice designer
    fl ("size", "Voice Size", 8.0f, 20.0f, 16.5f, 1.0f, "cm");
    bip ("gender", "Gender", 0.0f);
    pct ("age", "Age", 0.5f);
    pct ("cat", "Cat", 0.0f);
    pct ("breath", "Breath", 0.15f);
    bip ("tension", "Tension", 0.0f);
    pct ("rasp", "Rasp", 0.0f);
    pct ("jitter", "Human Roughness", 0.3f);
    pct ("nasal", "Nasal", 0.0f);
    pct ("ring", "Ring", 0.3f);
    bip ("jaw", "Jaw", 0.0f);
    bip ("lips", "Lips", 0.0f);
    bip ("tongue", "Tongue", 0.0f);
    bip ("throat", "Throat", 0.0f);
    bip ("bright", "Brightness", 0.0f);

    // Pitch expression
    fl ("vibDepth", "Vibrato Depth", 0.0f, 150.0f, 30.0f, 1.0f, "c");
    fl ("vibRate", "Vibrato Rate", 3.0f, 9.0f, 5.4f, 1.0f, "Hz");
    fl ("vibDelay", "Vibrato Delay", 0.0f, 1.5f, 0.35f, 1.0f, "s");
    fl ("porta", "Portamento", 0.0f, 400.0f, 90.0f, 0.6f, "ms");
    pct ("overshoot", "Overshoot", 0.3f);
    pct ("scoop", "Scoop", 0.3f);
    pct ("fall", "Phrase Fall", 0.0f);
    pct ("drift", "Pitch Drift", 0.3f);
    pct ("prep", "Preparation", 0.2f);

    // Diction
    fl ("consLength", "Consonant Length", 0.4f, 2.0f, 1.0f, 1.0f, "x");
    pct ("consStrength", "Consonant Strength", 0.6f);
    ch ("attack", "Attack", attackNames(), 1);
    fl ("release", "Release", 20.0f, 800.0f, 140.0f, 0.5f, "ms");
    pct ("autoBreath", "Auto Breath", 0.4f);

    // Harmony + double
    ch ("key", "Key", keyNames(), 0);
    ch ("scale", "Scale", scaleNames(), 0);
    for (int h = 1; h <= 3; ++h)
    {
        ch ("h" + String (h), "Harmony " + String (h), harmonyNames(), 0);
        pct ("h" + String (h) + "Level", "Harmony " + String (h) + " Level", 0.6f);
        bip ("h" + String (h) + "Pan", "Harmony " + String (h) + " Pan", h == 1 ? -0.5f : (h == 2 ? 0.5f : 0.0f));
    }
    pct ("dblLevel", "Double", 0.0f);
    fl ("dblDetune", "Double Detune", 0.0f, 40.0f, 9.0f, 1.0f, "c");
    fl ("dblDelay", "Double Delay", 0.0f, 60.0f, 18.0f, 1.0f, "ms");
    pct ("width", "Width", 0.5f);

    // FX
    fl ("lowCut", "Low Cut", 20.0f, 400.0f, 90.0f, 0.5f, "Hz");
    fl ("presence", "Presence", -6.0f, 9.0f, 2.0f, 1.0f, "dB");
    fl ("air", "Air", -6.0f, 9.0f, 2.0f, 1.0f, "dB");
    pct ("comp", "Compressor", 0.4f);
    pct ("drive", "Drive", 0.0f);
    pct ("delayMix", "Delay Mix", 0.0f);
    ch ("delayTime", "Delay Time", delayNames(), 2);
    fl ("delayFb", "Delay Feedback", 0.0f, 0.9f, 0.35f, 1.0f, "%");
    ch ("reverb", "Reverb", reverbNames(), 2);
    pct ("reverbMix", "Reverb Mix", 0.14f);
    pct ("reverbSize", "Reverb Size", 0.6f);

    // Live
    fl ("bendRange", "Bend Range", 1.0f, 24.0f, 2.0f, 1.0f, "");
    return layout;
}

//==============================================================================
const std::vector<TalkCatProcessor::VoicePreset>& TalkCatProcessor::voicePresets()
{
    // style index: 0 Natural 1 Pop 2 Ballad 3 R&B 4 Opera 5 Anime Idol 6 Rock 7 Metal Fry 8 Jazz 9 Folk 10 Gospel 11 Whisper 12 Country 13 Robot 14 Cat Diva
    static const std::vector<VoicePreset> list = {
        { "Pop Diva", "Human", { { "style", 1 }, { "size", 14.6f }, { "gender", 0.6f }, { "breath", 0.15f }, { "vibDepth", 28 }, { "dblLevel", 0.35f }, { "reverbMix", 0.25f } } },
        { "Pop Boy", "Human", { { "style", 1 }, { "size", 16.8f }, { "gender", -0.3f }, { "breath", 0.18f }, { "vibDepth", 22 }, { "dblLevel", 0.3f } } },
        { "Soft Indie", "Human", { { "style", 2 }, { "size", 15.0f }, { "gender", 0.4f }, { "breath", 0.45f }, { "tension", -0.3f }, { "vibDepth", 12 }, { "comp", 0.55f }, { "reverb", 3 }, { "reverbMix", 0.3f } } },
        { "Soul Man", "Human", { { "style", 3 }, { "size", 17.2f }, { "gender", -0.5f }, { "rasp", 0.12f }, { "vibDepth", 35 }, { "scoop", 0.6f }, { "fall", 0.5f } } },
        { "R&B Smooth", "Human", { { "style", 3 }, { "size", 15.2f }, { "gender", 0.3f }, { "breath", 0.25f }, { "overshoot", 0.6f }, { "scoop", 0.6f }, { "dblLevel", 0.25f }, { "delayMix", 0.15f } } },
        { "Crooner", "Human", { { "style", 8 }, { "size", 17.5f }, { "gender", -0.6f }, { "breath", 0.2f }, { "vibDepth", 38 }, { "vibRate", 5.0f }, { "porta", 140 }, { "reverb", 1 } } },
        { "Opera Tenor", "Human", { { "style", 4 }, { "size", 17.0f }, { "gender", -0.3f }, { "ring", 0.8f }, { "jaw", 0.3f }, { "vibDepth", 70 }, { "vibRate", 5.8f }, { "reverb", 4 }, { "reverbMix", 0.35f } } },
        { "Opera Soprano", "Human", { { "style", 4 }, { "size", 14.2f }, { "gender", 0.7f }, { "ring", 0.8f }, { "jaw", 0.4f }, { "vibDepth", 80 }, { "vibRate", 5.9f }, { "reverb", 4 }, { "reverbMix", 0.35f } } },
        { "Anime Idol", "Human", { { "style", 5 }, { "size", 12.8f }, { "gender", 1.0f }, { "age", 0.35f }, { "bright", 0.3f }, { "nasal", 0.25f }, { "vibDepth", 20 }, { "dblLevel", 0.3f }, { "presence", 4 } } },
        { "Classic Vocaloid", "Human", { { "style", 0 }, { "size", 13.4f }, { "gender", 0.9f }, { "breath", 0.05f }, { "jitter", 0.05f }, { "drift", 0.0f }, { "vibDepth", 18 }, { "porta", 60 }, { "bright", 0.25f }, { "presence", 4 } } },
        { "Robot", "Synthetic", { { "style", 13 }, { "size", 16.0f }, { "breath", 0.0f }, { "jitter", 0.0f }, { "bright", 0.4f }, { "nasal", 0.2f }, { "delayMix", 0.2f }, { "reverb", 2 } } },
        { "Rock Shout", "Human", { { "style", 6 }, { "size", 17.0f }, { "gender", -0.4f }, { "rasp", 0.3f }, { "tension", 0.4f }, { "drive", 0.35f }, { "comp", 0.7f }, { "presence", 5 } } },
        { "Metal Growl", "Human", { { "style", 7 }, { "size", 18.0f }, { "gender", -0.8f }, { "rasp", 0.8f }, { "tension", 0.6f }, { "drive", 0.6f }, { "comp", 0.8f }, { "vibDepth", 5 } } },
        { "Whisper ASMR", "Human", { { "style", 11 }, { "size", 15.0f }, { "gender", 0.3f }, { "breath", 0.9f }, { "tension", -0.8f }, { "consStrength", 0.9f }, { "reverb", 1 }, { "reverbMix", 0.1f }, { "comp", 0.6f } } },
        { "Jazz Lounge", "Human", { { "style", 8 }, { "size", 15.4f }, { "gender", 0.3f }, { "breath", 0.3f }, { "vibDepth", 30 }, { "fall", 0.6f }, { "reverb", 1 }, { "reverbMix", 0.2f } } },
        { "Folk Granny", "Human", { { "style", 9 }, { "size", 15.0f }, { "gender", 0.5f }, { "age", 0.9f }, { "nasal", 0.3f }, { "jitter", 0.6f }, { "vibDepth", 45 }, { "vibRate", 4.6f } } },
        { "Old Bluesman", "Human", { { "style", 3 }, { "size", 17.6f }, { "gender", -0.7f }, { "age", 0.85f }, { "rasp", 0.35f }, { "fall", 0.8f }, { "scoop", 0.8f }, { "drive", 0.15f } } },
        { "Choirboy", "Human", { { "style", 0 }, { "size", 16.0f }, { "age", 0.15f }, { "breath", 0.2f }, { "vibDepth", 8 }, { "reverb", 4 }, { "reverbMix", 0.45f } } },
        { "Gospel Power", "Human", { { "style", 10 }, { "size", 15.0f }, { "gender", 0.5f }, { "tension", 0.4f }, { "rasp", 0.1f }, { "h1", 1 }, { "h2", 5 }, { "h1Level", 0.5f }, { "h2Level", 0.45f }, { "reverb", 3 } } },
        { "Country Twang", "Human", { { "style", 12 }, { "size", 16.4f }, { "gender", -0.2f }, { "nasal", 0.45f }, { "scoop", 0.6f }, { "vibDepth", 25 } } },
        { "Kid Sister", "Human", { { "style", 5 }, { "size", 15.0f }, { "age", 0.1f }, { "gender", 0.5f }, { "bright", 0.2f }, { "vibDepth", 6 } } },
        { "Deep Bass", "Human", { { "style", 0 }, { "size", 19.5f }, { "gender", -1.0f }, { "ring", 0.5f }, { "vibDepth", 25 }, { "reverb", 3 } } },
        { "Alien", "Synthetic", { { "style", 0 }, { "size", 9.0f }, { "gender", 0.2f }, { "throat", 0.9f }, { "tongue", 0.8f }, { "nasal", 0.6f }, { "vibDepth", 90 }, { "vibRate", 8.5f }, { "delayMix", 0.3f } } },
        { "Chipmunk", "Synthetic", { { "style", 5 }, { "size", 9.5f }, { "gender", 1.0f }, { "bright", 0.4f }, { "vibDepth", 15 }, { "consLength", 0.6f } } },
        { "Harmony Trio", "Human", { { "style", 1 }, { "size", 15.2f }, { "gender", 0.4f }, { "h1", 1 }, { "h2", 2 }, { "h1Level", 0.55f }, { "h2Level", 0.5f }, { "width", 0.8f }, { "reverbMix", 0.3f } } },
        { "Double Tracked", "Human", { { "style", 1 }, { "size", 15.8f }, { "dblLevel", 0.9f }, { "dblDetune", 14 }, { "dblDelay", 25 }, { "width", 1.0f } } },
        { "Kitten", "Cat", { { "style", 14 }, { "cat", 1.0f }, { "size", 13.0f }, { "age", 0.1f }, { "gender", 0.6f }, { "vibDepth", 30 }, { "reverb", 1 } } },
        { "House Cat", "Cat", { { "style", 14 }, { "cat", 0.9f }, { "size", 16.0f }, { "vibDepth", 40 }, { "breath", 0.2f } } },
        { "Tomcat Crooner", "Cat", { { "style", 8 }, { "cat", 0.75f }, { "size", 19.0f }, { "gender", -0.6f }, { "rasp", 0.25f }, { "vibDepth", 50 }, { "fall", 0.7f } } },
        { "Cat Diva", "Cat", { { "style", 14 }, { "cat", 0.6f }, { "size", 14.6f }, { "gender", 0.7f }, { "vibDepth", 70 }, { "reverb", 3 }, { "reverbMix", 0.3f } } },
        { "Half Cat Half Human", "Cat", { { "style", 1 }, { "cat", 0.45f }, { "size", 15.5f }, { "gender", 0.3f }, { "vibDepth", 30 } } },
        { "Cat Choir", "Cat", { { "style", 14 }, { "cat", 0.85f }, { "h1", 1 }, { "h2", 6 }, { "h3", 10 }, { "h1Level", 0.6f }, { "h2Level", 0.5f }, { "h3Level", 0.4f }, { "width", 0.9f }, { "reverb", 4 } } },
        { "Big Lion", "Cat", { { "style", 6 }, { "cat", 0.5f }, { "size", 20.0f }, { "gender", -1.0f }, { "rasp", 0.6f }, { "drive", 0.3f } } },
        // ---- v2
        { "Bedroom Lo-Fi", "Human", { { "style", 2 }, { "size", 15.4f }, { "gender", 0.2f }, { "breath", 0.4f }, { "tension", -0.4f }, { "lowCut", 160 }, { "air", -3 },
                                      { "comp", 0.6f }, { "delayMix", 0.18f }, { "reverb", 1 }, { "reverbMix", 0.15f } } },
        { "Stadium Anthem", "Human", { { "style", 6 }, { "size", 16.6f }, { "gender", -0.2f }, { "tension", 0.3f }, { "h1", 9 }, { "h1Level", 0.4f }, { "dblLevel", 0.5f },
                                       { "reverb", 3 }, { "reverbMix", 0.35f }, { "reverbSize", 0.9f } } },
        { "K-Pop Bright", "Human", { { "style", 5 }, { "size", 14.0f }, { "gender", 0.7f }, { "bright", 0.4f }, { "air", 6 }, { "presence", 5 }, { "dblLevel", 0.4f },
                                     { "comp", 0.7f }, { "delayMix", 0.12f } } },
        { "Lullaby Mum", "Human", { { "style", 2 }, { "size", 14.8f }, { "gender", 0.6f }, { "breath", 0.35f }, { "tension", -0.5f }, { "vibDepth", 14 }, { "porta", 140 },
                                    { "reverb", 1 }, { "reverbMix", 0.25f } } },
        { "Telephone", "Synthetic", { { "style", 0 }, { "size", 16.0f }, { "lowCut", 380 }, { "presence", 9 }, { "air", -6 }, { "drive", 0.3f }, { "comp", 0.8f },
                                      { "reverb", 0 } } },
        { "Megaphone", "Synthetic", { { "style", 6 }, { "size", 16.5f }, { "lowCut", 400 }, { "presence", 9 }, { "drive", 0.8f }, { "comp", 0.9f }, { "reverb", 1 },
                                      { "reverbMix", 0.1f } } },
        { "Ghost", "Synthetic", { { "style", 11 }, { "size", 15.5f }, { "breath", 0.8f }, { "tension", -0.7f }, { "vibDepth", 60 }, { "vibRate", 3.5f },
                                  { "delayMix", 0.35f }, { "delayFb", 0.6f }, { "reverb", 4 }, { "reverbMix", 0.55f } } },
        { "Giant", "Synthetic", { { "style", 0 }, { "size", 20.0f }, { "gender", -1.0f }, { "throat", 0.6f }, { "ring", 0.6f }, { "porta", 200 }, { "reverb", 4 },
                                  { "reverbMix", 0.4f } } },
        { "Fairy", "Synthetic", { { "style", 5 }, { "size", 9.0f }, { "gender", 1.0f }, { "bright", 0.5f }, { "breath", 0.3f }, { "vibDepth", 35 }, { "vibRate", 7.5f },
                                  { "h1", 9 }, { "h1Level", 0.35f }, { "delayMix", 0.25f }, { "reverb", 3 }, { "reverbMix", 0.35f } } },
        { "Elf Choir", "Synthetic", { { "style", 0 }, { "size", 11.0f }, { "gender", 0.8f }, { "h1", 1 }, { "h2", 5 }, { "h3", 9 }, { "h1Level", 0.55f }, { "h2Level", 0.5f },
                                      { "h3Level", 0.35f }, { "width", 1.0f }, { "reverb", 4 }, { "reverbMix", 0.45f } } },
        { "Duet Partner", "Human", { { "style", 1 }, { "size", 15.6f }, { "gender", 0.1f }, { "h1", 2 }, { "h1Level", 0.7f }, { "h1Pan", 0.6f }, { "width", 0.8f },
                                     { "reverbMix", 0.25f } } },
        { "Barbershop Lead", "Human", { { "style", 8 }, { "size", 17.0f }, { "gender", -0.4f }, { "h1", 1 }, { "h2", 4 }, { "h3", 10 }, { "h1Level", 0.6f },
                                        { "h2Level", 0.55f }, { "h3Level", 0.5f }, { "vibDepth", 30 } } },
        { "Breathy Pop Girl", "Human", { { "style", 1 }, { "size", 14.2f }, { "gender", 0.8f }, { "breath", 0.5f }, { "air", 7 }, { "comp", 0.65f }, { "dblLevel", 0.25f } } },
        { "Space Cat", "Cat", { { "style", 14 }, { "cat", 0.8f }, { "size", 14.0f }, { "vibDepth", 60 }, { "vibRate", 7.0f }, { "delayMix", 0.35f }, { "delayFb", 0.55f },
                                { "reverb", 4 }, { "reverbMix", 0.5f } } },
        { "Sleepy Cat", "Cat", { { "style", 11 }, { "cat", 0.9f }, { "size", 15.0f }, { "breath", 0.6f }, { "tension", -0.6f }, { "porta", 220 }, { "vibDepth", 20 } } },
        { "Angry Alley Cat", "Cat", { { "style", 7 }, { "cat", 0.85f }, { "size", 16.0f }, { "rasp", 0.7f }, { "tension", 0.7f }, { "drive", 0.4f } } },
        { "Kitten Choir", "Cat", { { "style", 14 }, { "cat", 1.0f }, { "size", 12.5f }, { "age", 0.05f }, { "h1", 1 }, { "h2", 5 }, { "h1Level", 0.6f }, { "h2Level", 0.5f },
                                   { "width", 0.9f }, { "reverb", 3 }, { "reverbMix", 0.3f } } },
        { "Cat Rocker", "Cat", { { "style", 6 }, { "cat", 0.6f }, { "size", 17.0f }, { "rasp", 0.35f }, { "drive", 0.35f }, { "dblLevel", 0.5f }, { "comp", 0.7f } } },
    };
    return list;
}

const std::vector<TalkCatProcessor::DemoSong>& TalkCatProcessor::demoSongs()
{
    // notes: "pitch:beats" tokens (r = rest), then "|" and the lyric text
    static const std::vector<DemoSong> list = {
        { "Twinkle Twinkle (English)", 0, 1, 96.0,
          "60:1 60:1 67:1 67:1 69:1 69:1 67:2 65:1 65:1 64:1 64:1 62:1 62:1 60:2 67:1 67:1 65:1 65:1 64:1 64:1 62:2 "
          "67:1 67:1 65:1 65:1 64:1 64:1 62:2 60:1 60:1 67:1 67:1 69:1 69:1 67:2 65:1 65:1 64:1 64:1 62:1 62:1 60:2"
          "|twinkle twinkle little star how I wonder what you are up above the world so high like a diamond in the sky "
          "twinkle twinkle little star how I wonder what you are" },
        { "Hello Darling (English ballad)", 0, 2, 78.0,
          "r:1 64:0.5 67:0.5 69:1.5 67:0.5 64:1 62:1 64:2 r:1 64:0.5 67:0.5 69:1 72:1 71:1 69:0.5 67:0.5 69:3 r:1 "
          "72:1 71:0.5 69:0.5 67:1 64:1 67:1.5 69:0.5 67:2 r:1 64:0.5 62:0.5 60:1 62:1 64:1 62:1 60:4"
          "|hello my darling I missed you tonight the stars are all shining the moon is so bright "
          "come home to me baby and sing me your song I waited for you all night long" },
        { "Macicka (Slovak)", 1, 9, 100.0,
          "67:1 67:1 69:1 67:1 65:1 64:1 62:2 64:1 65:1 67:1 69:1 67:4 "
          "72:1 71:1 69:1 67:1 65:1 67:1 69:2 67:1 65:1 64:1 62:1 60:4"
          "|mal" "\xc3\xa1" " ma" "\xc4\x8d" "i" "\xc4\x8d" "ka spieva piese" "\xc5\x88" " do noci spieva veselo a do r" "\xc3\xa1" "na spieva s nami" },
        { "Hana (Japanese)", 2, 5, 92.0,
          "69:1 69:1 71:2 69:1 71:1 72:2 71:1 69:1 71:0.5 69:0.5 65:2 r:1 "
          "64:1 60:1 64:1 65:1 64:1 64:0.5 60:0.5 59:2 r:1 69:1 71:1 72:1 71:1 69:1 71:1 69:1 65:3"
          "|sakura no hana ga saite iru haru no kaze ni yurete iru kirei na sora no shita de" },
        { "Cat Opera (Cat)", 3, 14, 104.0,
          "67:1 72:1 71:0.5 69:0.5 67:2 64:1 65:1 67:2 69:0.5 71:0.5 72:1 74:1 72:2 67:1 69:1 65:1 64:1 60:4"
          "|meow meow mrrp mrrp meow purr meow meow nya nya meow yowl meow chirp meow trill meow" },
        { "Scale Exercise (do re mi)", 0, 0, 100.0,
          "60:1 62:1 64:1 65:1 67:1 69:1 71:1 72:2 71:1 69:1 67:1 65:1 64:1 62:1 60:2"
          "|do re mi fa so la ti do do ti la so fa mi re do" },
        { "Ode to Joy (English)", 0, 1, 104.0,
          "64:1 64:1 65:1 67:1 67:1 65:1 64:1 62:1 60:1 60:1 62:1 64:1 64:1.5 62:0.5 62:2 "
          "64:1 64:1 65:1 67:1 67:1 65:1 64:1 62:1 60:1 60:1 62:1 64:1 62:1.5 60:0.5 60:2"
          "|sing a song of joy to-ge-ther all the world is shi-ning bright "
          "hand in hand we sing for-e-ver from the mor-ning to the night" },
        { "Are You Sleeping (round)", 0, 9, 108.0,
          "60:1 62:1 64:1 60:1 60:1 62:1 64:1 60:1 64:1 65:1 67:2 64:1 65:1 67:2 "
          "67:0.5 69:0.5 67:0.5 65:0.5 64:1 60:1 67:0.5 69:0.5 67:0.5 65:0.5 64:1 60:1 60:1 55:1 60:2 60:1 55:1 60:2"
          "|are you slee-ping are you slee-ping bro-ther john bro-ther john "
          "mor-ning bells are rin-ging mor-ning bells are rin-ging ding dang dong ding dang dong" },
        { "Slnko svieti (Slovak)", 1, 9, 96.0,
          "60:1 64:1 67:1 67:1 69:1 67:1 64:2 65:1 64:1 62:1 64:1 60:4 "
          "67:1 67:1 69:1 72:1 71:1 69:1 67:2 65:1 64:1 62:1 62:1 60:4"
          "|slnko svieti na l\xc3\xbake ma\xc4\x8dka spieva pesni\xc4\x8dku "
          "vt\xc3\xa1\xc4\x8dik let\xc3\xad do neba spievame si spolu" },
        { "Hoshi no Uta (Japanese)", 2, 2, 88.0,
          "60:1 62:1 64:1 65:1 64:1 62:1 60:2 64:1 65:1 67:1 69:1 67:1 65:1 64:2 "
          "60:1 r:1 60:1 r:1 60:1 r:1 60:1 r:1 60:0.5 60:0.5 62:0.5 62:0.5 64:0.5 64:0.5 65:0.5 65:0.5 64:1 62:1 60:2"
          "|ho-shi ga hi-ka-ru yo-ru no so-ra ni min-na de u-ta-ou "
          "ra ra ra ra ki-ra ki-ra ki-ra ki-ra hi-ka-re" },
        { "Meow Blues (Cat)", 3, 8, 84.0,
          "60:1 63:1 65:1 66:0.5 67:1.5 r:1 67:1 70:1 67:1 65:1 63:2 r:1 "
          "65:1 65:1 68:1 65:1 63:1 60:1 r:1 63:1 60:1 58:1 60:3"
          "|meow meow mrrp purr meow nya meow mrrp meow yowl purr purr meow meow mrrp purr nya meow" },
        { "Rainy Window (English)", 0, 2, 72.0,
          "r:1 67:0.5 69:0.5 71:1 69:1 67:1 64:1 67:2 r:1 67:0.5 69:0.5 71:1 72:1 74:1 71:1 69:3 r:1 "
          "72:1 71:1 69:1 67:1 69:1.5 71:0.5 67:2 r:1 64:1 67:1 69:1 67:1 64:1 62:1 60:4"
          "|rain is fal-ling on my win-dow drops are dan-cing in the light "
          "I will wait here for the morn-ing hum-ming soft-ly through the night" },
    };
    return list;
}

//==============================================================================
TalkCatProcessor::TalkCatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    {
        cat::LinkClient::Caps caps;
        caps.key = true;
        auto& link = presetManager.enableLink (apvts, caps);
        link.getKey = [this]
        {
            const int root = juce::roundToInt (apvts.getRawParameterValue ("key")->load());
            const int sc = juce::roundToInt (apvts.getRawParameterValue ("scale")->load());
            const auto masks = talkcat::scaleMasks();
            return std::pair<int, juce::uint16> { root, cat::keys::rotate (masks[(size_t) juce::jlimit (0, (int) masks.size() - 1, sc)], root) };
        };
        link.applyKey = [this] (int root, juce::uint16 absMask)
        {
            const int sc = cat::keys::nearestScale (cat::keys::rotate (absMask, -root), talkcat::scaleMasks());
            if (auto* k = apvts.getParameter ("key")) k->setValueNotifyingHost (k->convertTo0to1 ((float) root));
            if (auto* s = apvts.getParameter ("scale"); s != nullptr && sc >= 0) s->setValueNotifyingHost (s->convertTo0to1 ((float) sc));
        };
    }
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            refs[r->getParameterID()] = apvts.getRawParameterValue (r->getParameterID());

    liveSyllables = textToSyllables (liveLyricsText, Language::English);
    previewQueue.reserve (64);
    previewAudio.reserve (64);
    liveSyllablesAudio.reserve (2048);
    events.reserve (256);
    liveHeld.reserve (128);
    loadDemoSong (0);
    undoStack.clear();
    startTimerHz (15);
}

TalkCatProcessor::~TalkCatProcessor() { stopTimer(); }

float TalkCatProcessor::p (const char* id) const
{
    auto it = refs.find (id);
    return it != refs.end() ? it->second->load() : 0.0f;
}

Language TalkCatProcessor::language() const { return (Language) juce::roundToInt (p ("language")); }

bool TalkCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void TalkCatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    hostSync.prepare (sampleRate);
    work.setSize (2, juce::jmax (512, samplesPerBlock), false, false, true);
    expectedBeat = -1.0e9;
    playingNote = -1;
    wasPlaying = false;
    liveHeld.clear();
    if (active == nullptr && ! hasIncoming) recompile();
}

Settings TalkCatProcessor::readSettings() const
{
    Settings s;
    s.size = p ("size"); s.gender = p ("gender"); s.age = p ("age"); s.cat = p ("cat");
    s.breath = p ("breath"); s.tension = p ("tension"); s.rasp = p ("rasp"); s.jitter = p ("jitter"); s.nasal = p ("nasal");
    s.ring = p ("ring"); s.jaw = p ("jaw"); s.lips = p ("lips"); s.tongue = p ("tongue"); s.throat = p ("throat"); s.bright = p ("bright");
    s.vibDepth = p ("vibDepth"); s.vibRate = p ("vibRate"); s.vibDelay = p ("vibDelay");
    s.portaMs = p ("porta"); s.overshoot = p ("overshoot"); s.scoop = p ("scoop"); s.fall = p ("fall"); s.drift = p ("drift"); s.prep = p ("prep");
    s.consLength = p ("consLength"); s.consStrength = p ("consStrength");
    s.attack = juce::roundToInt (p ("attack")); s.releaseMs = p ("release"); s.style = juce::roundToInt (p ("style")); s.autoBreath = p ("autoBreath");
    s.key = juce::roundToInt (p ("key")); s.scale = juce::roundToInt (p ("scale"));
    for (int h = 0; h < 3; ++h)
    {
        const juce::String pre = "h" + juce::String (h + 1);
        s.harmony[(size_t) h] = juce::roundToInt (p (pre.toRawUTF8()));
        s.harmonyLevel[(size_t) h] = p ((pre + "Level").toRawUTF8());
        s.harmonyPan[(size_t) h] = p ((pre + "Pan").toRawUTF8());
    }
    s.doubleLevel = p ("dblLevel"); s.doubleDetune = p ("dblDetune"); s.doubleDelayMs = p ("dblDelay"); s.width = p ("width");
    s.lowCut = p ("lowCut"); s.presence = p ("presence"); s.air = p ("air"); s.comp = p ("comp"); s.drive = p ("drive");
    s.delayMix = p ("delayMix"); s.delayBeats = (float) delayBeats (juce::roundToInt (p ("delayTime"))); s.delayFeedback = p ("delayFb");
    s.reverb = juce::roundToInt (p ("reverb")); s.reverbMix = p ("reverbMix"); s.reverbSize = p ("reverbSize");
    s.volume = juce::Decibels::decibelsToGain (p ("volume"));
    s.bendRange = p ("bendRange");
    s.bpm = lastBpm;
    return s;
}

//==============================================================================
void TalkCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    mergeComputerKeyboard (midi, numSamples);      // standalone app: notes from the computer keyboard
    buffer.clear();

    hostSync.beginBlock (presetManager.linkClient->playHead (getPlayHead(), numSamples, getSampleRate()), numSamples);   // Cat Link: shared tempo in the apps
    midiLearn.processMidi (midi);
    const auto t = hostSync.get();
    lastBpm = t.bpm;
    uiBpm = t.bpm;
    engine.setSettings (readSettings());

    // compiled score from the message thread
    if (swapLock.tryEnter())
    {
        if (hasIncoming) { std::swap (active, incoming); hasIncoming = false; }
        swapLock.exit();
    }
    // live lyrics
    if (liveLock.tryEnter())
    {
        if (liveChanged)
        {
            liveSyllablesAudio.clear();
            for (const auto& s : liveSyllables) if (liveSyllablesAudio.size() < 2000) liveSyllablesAudio.push_back (s);
            liveChanged = false;
            if (livePos >= (int) liveSyllablesAudio.size()) livePos = -1;
        }
        liveLock.exit();
    }
    if (liveRestart.exchange (false)) { livePos = -1; uiLiveSyllable = -1; }
    if (t.hostPlaying && t.jumped) { livePos = -1; uiLiveSyllable = -1; }

    // preview requests
    if (previewLock.tryEnter())
    {
        previewAudio.clear();
        for (auto& r : previewQueue) previewAudio.push_back (r);
        previewQueue.clear();
        previewLock.exit();
    }
    for (auto& r : previewAudio)
    {
        if (r.stop) { engine.noteRelease (1, false); previewActive = false; continue; }
        NoteEvent ev;
        ev.pitch = (float) r.pitch;
        ev.velocity = 0.8f;
        ev.syl = r.syl;
        ev.legato = previewActive;
        engine.noteStart (1, ev);
        previewActive = true;
        previewSamplesLeft = (int) (0.55 * getSampleRate());
    }
    previewAudio.clear();
    if (previewActive)
    {
        previewSamplesLeft -= numSamples;
        if (previewSamplesLeft <= 0) { engine.noteRelease (1, false); previewActive = false; }
    }

    events.clear();
    const int mode = juce::roundToInt (p ("mode"));
    if (mode != 1) playScore (numSamples, t);
    else if (playingNote >= 0) { engine.noteRelease (0, false); playingNote = -1; }

    if (work.getNumSamples() < numSamples) work.setSize (2, numSamples, false, false, true);
    work.clear();
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);

    // merge score events and live MIDI by sample position
    auto midiIt = midi.begin();
    size_t ei = 0;
    int pos = 0;
    while (true)
    {
        int nextMidi = midiIt != midi.end() ? juce::jlimit (0, numSamples, (*midiIt).samplePosition) : numSamples + 1;
        int nextEv = ei < events.size() ? juce::jlimit (0, numSamples, events[ei].sample) : numSamples + 1;
        const int next = juce::jmin (nextMidi, nextEv);
        if (next > numSamples) break;
        if (next > pos) { engine.render (l + pos, r + pos, next - pos); pos = next; }
        if (nextEv <= nextMidi)
        {
            const auto& e = events[ei++];
            if (active != nullptr && e.note >= 0 && e.note < (int) active->notes.size())
            {
                const auto& cn = active->notes[(size_t) e.note];
                const double bps = juce::jmax (0.5, t.bpm / 60.0);
                if (e.type == 0)
                {
                    const auto& s = engine.getSettings();
                    NoteEvent ev;
                    ev.pitch = cn.pitch;
                    ev.velocity = cn.velocity;
                    ev.syl = cn.syl;
                    ev.legato = cn.legatoFromPrev && playingNote == e.note - 1;
                    ev.skipOnset = false;
                    float pre = onsetSeconds (cn.syl, s.consLength * cn.consScale);
                    if (ev.legato && e.note > 0) pre += codaSeconds (active->notes[(size_t) e.note - 1].syl, s.consLength * active->notes[(size_t) e.note - 1].consScale);
                    ev.pitchDelay = pre;
                    ev.lengthSec = (float) ((cn.end - cn.start) / bps);
                    ev.portaMs = cn.portaMs; ev.vibDepth = cn.vibDepth; ev.vibRate = cn.vibRate; ev.vibDelay = cn.vibDelay;
                    ev.consScale = cn.consScale; ev.consStrength = cn.consStrength;
                    engine.noteStart (0, ev);
                    playingNote = e.note;
                    uiActiveNote = cn.sourceId;
                }
                else if (e.type == 1)
                {
                    if (playingNote == e.note) { engine.noteRelease (0, cn.phraseEnd); playingNote = -1; uiActiveNote = -1; }
                }
                else if (e.type == 3)
                {
                    // playback started inside this note: straight to the vowel
                    NoteEvent ev;
                    ev.pitch = cn.pitch; ev.velocity = cn.velocity; ev.syl = cn.syl; ev.skipOnset = true;
                    ev.lengthSec = (float) ((cn.end - cn.start) / bps);
                    engine.noteStart (0, ev);
                    playingNote = e.note;
                    uiActiveNote = cn.sourceId;
                }
                else if (e.type == 2)
                    engine.breathIn (0, (float) juce::jlimit (0.15, 0.45, 0.4));
            }
        }
        else
        {
            const auto m = (*midiIt).getMessage();
            ++midiIt;
            if (mode != 0) handleLiveMidi (m, pos);
            else if (m.isNoteOn()) noteMonitor.noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity());
            else if (m.isNoteOff()) noteMonitor.noteOff (m.getNoteNumber());
        }
    }
    if (pos < numSamples) engine.render (l + pos, r + pos, numSamples - pos);
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

void TalkCatProcessor::playScore (int numSamples, const cat::TransportState& t)
{
    if (active == nullptr || ! t.hostPlaying)
    {
        if (wasPlaying && playingNote >= 0) engine.noteRelease (0, true);
        if (wasPlaying) engine.allOff (0);
        playingNote = -1;
        wasPlaying = false;
        uiScoreBeat = -1.0;
        uiActiveNote = -1;
        expectedBeat = -1.0e9;
        return;
    }
    const auto& sc = *active;
    const double bps = juce::jmax (0.5, t.bpm / 60.0);
    const double perSample = t.ppqPerSample > 0.0 ? t.ppqPerSample : bps / getSampleRate();
    const double b0 = t.ppqStart, b1 = b0 + perSample * numSamples;
    const double len = juce::jmax (1.0, sc.lengthBeats);
    auto toLocal = [&] (double b)
    {
        double l = b - sc.startBeat;
        if (sc.loop && l >= 0.0) l = std::fmod (l, len);
        return l;
    };
    const auto& s = engine.getSettings();

    // expression lanes at the block start
    const double lb0 = toLocal (b0);
    Expr ex;
    ex.pitch = sc.laneValue (LanePitch, lb0);
    ex.dynamics = sc.laneValue (LaneDynamics, lb0);
    ex.breath = sc.laneValue (LaneBreath, lb0);
    ex.bright = sc.laneValue (LaneBright, lb0);
    ex.gender = sc.laneValue (LaneGender, lb0);
    ex.tension = sc.laneValue (LaneTension, lb0);
    ex.growl = sc.laneValue (LaneGrowl, lb0);
    ex.vibrato = sc.laneValue (LaneVibrato, lb0);
    ex.cat = sc.laneValue (LaneCat, lb0);
    ex.mouth = sc.laneValue (LaneMouth, lb0);
    engine.setExpression (0, ex);
    uiScoreBeat = lb0;

    const bool jumped = ! wasPlaying || t.jumped || std::abs (b0 - expectedBeat) > 0.02;
    wasPlaying = true;
    expectedBeat = b1;

    if (jumped)
    {
        if (playingNote >= 0) { engine.noteRelease (0, false); playingNote = -1; }
        // inside a note? start it right away (straight to the vowel)
        for (int i = 0; i < (int) sc.notes.size(); ++i)
        {
            const auto& n = sc.notes[(size_t) i];
            const double pre = onsetSeconds (n.syl, s.consLength * n.consScale) * bps;
            if (lb0 >= n.start + 0.01 && lb0 < n.end - 0.05) { events.push_back ({ 0, 3, i }); break; }
            if (lb0 >= n.start - pre && lb0 < n.start + 0.01) { events.push_back ({ 0, 0, i }); break; }
        }
    }

    // events inside [b0, b1) - the window can wrap at the loop end
    auto scan = [&] (double w0, double w1, int sampleOffset)
    {
        for (int i = 0; i < (int) sc.notes.size(); ++i)
        {
            const auto& n = sc.notes[(size_t) i];
            const float cs = s.consLength * n.consScale;
            double pre = onsetSeconds (n.syl, cs);
            if (n.legatoFromPrev && i > 0) pre += codaSeconds (sc.notes[(size_t) i - 1].syl, s.consLength * sc.notes[(size_t) i - 1].consScale);
            double startEvt = n.start - pre * bps;
            if (n.legatoFromPrev && i > 0) startEvt = juce::jmax (startEvt, sc.notes[(size_t) i - 1].start + 0.02);
            const double gapBefore = i == 0 ? 1.0e9 : (n.start - sc.notes[(size_t) i - 1].end) / bps;
            auto check = [&] (double e, int type)
            {
                for (double shift : { 0.0, len, -len })
                {
                    if (shift != 0.0 && ! sc.loop) continue;
                    const double x = e + shift;
                    if (x >= w0 && x < w1)
                    {
                        events.push_back ({ sampleOffset + juce::jlimit (0, numSamples - 1, (int) ((x - w0) / perSample)), type, i });
                        return;
                    }
                }
            };
            check (startEvt, 0);
            if (n.phraseEnd)
                check (juce::jmax (n.start + 0.02, n.end - codaSeconds (n.syl, cs) * bps), 1);
            if (! n.legatoFromPrev && gapBefore > 0.7 && s.autoBreath > 0.01f)
                check (startEvt - 0.4 * bps, 2);
        }
    };
    const double l0 = lb0, l1 = l0 + (b1 - b0);
    if (sc.loop && l0 >= 0.0 && l1 > len)
    {
        const int split = juce::jlimit (0, numSamples, (int) ((len - l0) / perSample));
        scan (l0, len, 0);
        scan (0.0, l1 - len, split);
    }
    else
        scan (l0, l1, 0);

    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b)
    {
        if (a.sample != b.sample) return a.sample < b.sample;
        return a.type == 1 && b.type != 1;      // releases before starts at the same sample
    });
}

void TalkCatProcessor::handleLiveMidi (const juce::MidiMessage& m, int)
{
    if (m.isNoteOn())
    {
        noteMonitor.noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity());
        const bool legato = ! liveHeld.empty();
        liveHeld.push_back ({ m.getNoteNumber(), m.getFloatVelocity() });
        // next live syllable
        if (! liveSyllablesAudio.empty())
        {
            livePos = (livePos + 1) % (int) liveSyllablesAudio.size();
            liveSyl = liveSyllablesAudio[(size_t) livePos];
            uiLiveSyllable = livePos;
        }
        else
        {
            liveSyl = {};
            liveSyl.nucleus.x = 0.5f; liveSyl.nucleus.y = 0.9f;
        }
        NoteEvent ev;
        ev.pitch = (float) m.getNoteNumber();
        ev.velocity = 0.3f + 0.7f * m.getFloatVelocity();
        ev.syl = liveSyl;
        ev.legato = legato;
        engine.noteStart (1, ev);

        if (recordArmed.load() && uiTransportRunning.load())
        {
            int s1, n1, s2, n2;
            recFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0) { recBuffer[(size_t) s1] = { uiScoreBeat.load(), m.getNoteNumber(), m.getFloatVelocity(), true }; recFifo.finishedWrite (1); }
        }
    }
    else if (m.isNoteOff())
    {
        noteMonitor.noteOff (m.getNoteNumber());
        const bool wasTop = ! liveHeld.empty() && liveHeld.back().note == m.getNoteNumber();
        liveHeld.erase (std::remove_if (liveHeld.begin(), liveHeld.end(), [&] (const Held& h) { return h.note == m.getNoteNumber(); }), liveHeld.end());
        if (wasTop)
        {
            if (! liveHeld.empty())
            {
                // back to the still-held note, same vowel (trills)
                NoteEvent ev;
                ev.pitch = (float) liveHeld.back().note;
                ev.velocity = 0.3f + 0.7f * liveHeld.back().vel;
                ev.syl.hold = true;
                ev.legato = true;
                engine.noteStart (1, ev);
            }
            else
                engine.noteRelease (1, true);
        }
        if (recordArmed.load() && uiTransportRunning.load())
        {
            int s1, n1, s2, n2;
            recFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0) { recBuffer[(size_t) s1] = { uiScoreBeat.load(), m.getNoteNumber(), 0.0f, false }; recFifo.finishedWrite (1); }
        }
    }
    else if (m.isPitchWheel())
        engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController() && m.getControllerNumber() == 1)
        engine.modWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isChannelPressure())
        engine.pressure ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        liveHeld.clear();
        engine.allOff (1);
        noteMonitor.clear();
    }
}

//==============================================================================
Score TalkCatProcessor::getScore() const
{
    const juce::ScopedLock sl (scoreLock);
    return score;
}

void TalkCatProcessor::setScore (const Score& s, bool undoable)
{
    {
        const juce::ScopedLock sl (scoreLock);
        if (undoable)
        {
            undoStack.push_back (score);
            if (undoStack.size() > 80) undoStack.erase (undoStack.begin());
            redoStack.clear();
        }
        score = s;
    }
    recompile();
    ++scoreVersion;
}

bool TalkCatProcessor::undo()
{
    {
        const juce::ScopedLock sl (scoreLock);
        if (undoStack.empty()) return false;
        redoStack.push_back (score);
        score = undoStack.back();
        undoStack.pop_back();
    }
    recompile();
    ++scoreVersion;
    return true;
}

bool TalkCatProcessor::redo()
{
    {
        const juce::ScopedLock sl (scoreLock);
        if (redoStack.empty()) return false;
        undoStack.push_back (score);
        score = redoStack.back();
        redoStack.pop_back();
    }
    recompile();
    ++scoreVersion;
    return true;
}

void TalkCatProcessor::recompile()
{
    auto compiled = compileScore (getScore(), language());
    const juce::SpinLock::ScopedLockType sl (swapLock);
    incoming = std::move (compiled);
    hasIncoming = true;
}

void TalkCatProcessor::timerCallback()
{
    const int lang = juce::roundToInt (p ("language"));
    if (lang != lastLanguage)
    {
        lastLanguage = lang;
        recompilePending = true;
        setLiveLyrics (liveLyricsText);
    }
    if (recompilePending.exchange (false))
        recompile();

    // recorded live notes -> score
    int s1, n1, s2, n2;
    recFifo.prepareToRead (recFifo.getNumReady(), s1, n1, s2, n2);
    auto handle = [&] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto& e = recBuffer[(size_t) (start + i)];
            if (e.on) recOpen[e.pitch] = { e.beat, e.vel };
            else if (auto it = recOpen.find (e.pitch); it != recOpen.end())
            {
                auto sc = getScore();
                ScoreNote n;
                const double q = 0.25;
                n.start = std::round (it->second.first / q) * q;
                n.length = juce::jmax (q, std::round ((e.beat - it->second.first) / q) * q);
                n.pitch = e.pitch;
                n.velocity = it->second.second;
                const int idx = uiLiveSyllable.load();
                const juce::SpinLock::ScopedLockType sl (liveLock);
                n.lyric = idx >= 0 && idx < (int) liveSyllables.size() ? juce::String::fromUTF8 (liveSyllables[(size_t) idx].text) : juce::String ("la");
                sc.addNote (n);
                sc.makeMonophonic();
                setScore (sc);
                recOpen.erase (it);
            }
        }
    };
    handle (s1, n1);
    handle (s2, n2);
    recFifo.finishedRead (n1 + n2);
}

//==============================================================================
void TalkCatProcessor::setLiveLyrics (const juce::String& text)
{
    liveLyricsText = text;
    auto parsed = textToSyllables (text, language());
    if (parsed.size() > 2000) parsed.resize (2000);
    const juce::SpinLock::ScopedLockType sl (liveLock);
    liveSyllables = std::move (parsed);
    liveChanged = true;
}

void TalkCatProcessor::preview (int pitch, const juce::String& lyric, int lang)
{
    ScoreNote n;
    n.lyric = lyric;
    n.language = lang;
    auto syls = noteSyllables (n, language());
    if (lyric.startsWith ("[")) { n.phonemes = lyric; syls = noteSyllables (n, language()); }
    PreviewReq r { pitch, syls.empty() ? Syllable() : syls[0], false };
    const juce::SpinLock::ScopedLockType sl (previewLock);
    if (previewQueue.size() < 32) previewQueue.push_back (r);
}

void TalkCatProcessor::previewStop()
{
    const juce::SpinLock::ScopedLockType sl (previewLock);
    if (previewQueue.size() < 32) previewQueue.push_back ({ 60, {}, true });
}

juce::MidiFile TalkCatProcessor::exportMidi() const
{
    juce::MidiFile f;
    f.setTicksPerQuarterNote (960);
    auto seq = getScore().toMidi();
    juce::MidiMessageSequence meta;
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / juce::jmax (30.0, uiBpm.load())));
    tempo.setTimeStamp (0);
    meta.addEvent (tempo);
    auto name = juce::MidiMessage::textMetaEvent (3, "Talk-Cat Vocal");
    name.setTimeStamp (0);
    meta.addEvent (name);
    meta.addSequence (seq, 0.0);
    f.addTrack (meta);
    return f;
}

//==============================================================================
void TalkCatProcessor::applyVoicePreset (int index)
{
    const auto& list = voicePresets();
    if (index < 0 || index >= (int) list.size()) return;
    for (const auto& id : voiceParamIds())
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->getDefaultValue());
    for (const auto& [id, v] : list[(size_t) index].values)
        cat::SurpriseControls::setParam (*this, id, v);
    currentVoice = index;
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
}

void TalkCatProcessor::loadDemoSong (int index)
{
    const auto& list = demoSongs();
    if (index < 0 || index >= (int) list.size()) return;
    const auto& d = list[(size_t) index];
    const juce::String all (juce::CharPointer_UTF8 (d.notes));
    const auto melody = all.upToFirstOccurrenceOf ("|", false, false);
    const auto text = all.fromFirstOccurrenceOf ("|", false, false);
    Score s;
    double t = 0.0;
    for (const auto& tok : juce::StringArray::fromTokens (melody, " ", ""))
    {
        if (tok.isEmpty()) continue;
        const auto pitch = tok.upToFirstOccurrenceOf (":", false, false);
        const double beats = tok.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
        if (pitch != "r")
        {
            ScoreNote n;
            n.start = t;
            n.length = beats * 0.97;
            n.pitch = pitch.getIntValue();
            s.addNote (n);
        }
        t += beats;
    }
    s.lengthBeats = juce::jmax (16.0, std::ceil (t / 4.0) * 4.0);
    s.fillLyrics (text, 0, (Language) d.language);
    if (auto* prm = apvts.getParameter ("language")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) d.language));
    if (auto* prm = apvts.getParameter ("style")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) d.style));
    setScore (s);
}

void TalkCatProcessor::surprise()
{
    getStateInformation (surpriseUndo);
    juce::Random r;
    applyVoicePreset (r.nextInt ((int) voicePresets().size()));
    auto set = [this] (const char* id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    set ("size", juce::jlimit (8.0f, 20.0f, p ("size") + (r.nextFloat() - 0.5f) * 3.0f));
    set ("gender", juce::jlimit (-1.0f, 1.0f, p ("gender") + (r.nextFloat() - 0.5f) * 0.6f));
    set ("breath", r.nextFloat() * 0.5f);
    set ("vibDepth", 5.0f + r.nextFloat() * 70.0f);
    set ("vibRate", 4.5f + r.nextFloat() * 2.0f);
    set ("porta", 30.0f + r.nextFloat() * 180.0f);
    set ("overshoot", r.nextFloat() * 0.8f);
    set ("scoop", r.nextFloat() * 0.8f);
    set ("nasal", r.nextFloat() * 0.4f);
    set ("jaw", (r.nextFloat() - 0.5f) * 0.8f);
    set ("reverbMix", 0.1f + r.nextFloat() * 0.3f);
    currentVoice = -1;
}

int TalkCatProcessor::getNumPrograms() { return (int) voicePresets().size(); }
const juce::String TalkCatProcessor::getProgramName (int i)
{
    return i >= 0 && i < (int) voicePresets().size() ? juce::String (voicePresets()[(size_t) i].name) : juce::String();
}

//==============================================================================
void TalkCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree root ("TALKCAT");
    root.appendChild (state, nullptr);
    root.appendChild (getScore().toValueTree(), nullptr);
    root.setProperty ("liveLyrics", liveLyricsText, nullptr);
    root.setProperty ("voice", currentVoice.load(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destData);
}

void TalkCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("TALKCAT")) return;
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    setScore (Score::fromValueTree (root.getChildWithName ("SCORE")), false);
    setLiveLyrics (root.getProperty ("liveLyrics", "la la la").toString());
    currentVoice = (int) root.getProperty ("voice", 0);
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
    recompile();
}

juce::AudioProcessorEditor* TalkCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<TalkCatEditor> (*this), "Talk-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TalkCatProcessor();
}
