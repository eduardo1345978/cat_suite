#include "GlideData.h"
#include <cmath>
#include <cstdio>

namespace glidecat
{
using V = std::vector<std::pair<juce::String, float>>;

namespace
{
    // Layer parameters (prefix l1_ / l2_): id, name, min, max, default L1, default L2, skew
    struct LayerParam { const char* id; const char* name; float lo, hi, d1, d2, skew; };
    const std::vector<LayerParam>& layerParams()
    {
        static const std::vector<LayerParam> p = {
            { "saw", "Saw", 0, 1, 0.8f, 0.7f, 1 },          { "pulse", "Pulse", 0, 1, 0.3f, 0.2f, 1 },
            { "pw", "Pulse Width", 0.05f, 0.95f, 0.42f, 0.3f, 1 },
            { "pwmRate", "PWM Rate", 0.05f, 10, 0.5f, 0.7f, 0.4f }, { "pwm", "PWM", 0, 1, 0.25f, 0.3f, 1 },
            { "sine", "Sine", 0, 1, 0.1f, 0.0f, 1 },        { "noise", "Noise", 0, 1, 0.0f, 0.0f, 1 },
            { "detune", "Detune", 0, 50, 10, 8, 1 },        { "fine", "Fine", -50, 50, 0, 6, 1 },
            { "hpf", "HP Cutoff", 20, 2000, 70, 100, 0.3f }, { "hpRes", "HP Resonance", 0.5f, 4, 0.7f, 0.7f, 0.6f },
            { "lpf", "LP Cutoff", 40, 18000, 450, 700, 0.25f }, { "lpRes", "LP Resonance", 0.5f, 8, 1.6f, 1.3f, 0.5f },
            { "key", "Key Tracking", 0, 1, 0.45f, 0.45f, 1 }, { "egDepth", "Filter EG Depth", -4, 6, 2.6f, 2.0f, 1 },
            { "egIL", "EG Initial Level", -1, 1, -0.4f, -0.3f, 1 }, { "egAL", "EG Attack Level", -1, 1, 0.8f, 0.7f, 1 },
            { "egAtk", "EG Attack", 0.001f, 8, 1.2f, 1.6f, 0.35f }, { "egDec", "EG Decay", 0.01f, 12, 2.6f, 3.0f, 0.35f },
            { "egRel", "EG Release", 0.01f, 12, 1.4f, 1.6f, 0.35f },
            { "attack", "Attack", 0.001f, 8, 0.6f, 0.9f, 0.35f }, { "decay", "Decay", 0.01f, 12, 2.0f, 2.4f, 0.35f },
            { "sustain", "Sustain", 0, 1, 0.9f, 0.9f, 1 },  { "release", "Release", 0.01f, 12, 1.8f, 2.0f, 0.35f },
            { "brill", "Brilliance", -1, 1, 0.1f, 0.0f, 1 }, { "level", "Level", 0, 1, 0.8f, 0.6f, 1 },
        };
        return p;
    }

    juce::String lid (int layer, const char* id) { return "l" + juce::String (layer) + "_" + id; }

    const char* layerUnit (const juce::String& id)
    {
        if (id == "hpf" || id == "lpf" || id == "pwmRate") return "Hz";
        if (id == "detune" || id == "fine") return "ct";
        if (id.startsWith ("eg") && id != "egDepth" && id != "egIL" && id != "egAL") return "s";
        if (id == "attack" || id == "decay" || id == "release") return "s";
        if (id == "egDepth") return "oct";
        if (id == "saw" || id == "pulse" || id == "pw" || id == "pwm" || id == "sine" || id == "noise" || id == "key"
            || id == "sustain" || id == "level") return "%";
        return "";
    }

    std::vector<ParamInfo> buildParams()
    {
        std::vector<ParamInfo> out;
        auto fl = [&] (const juce::String& id, const juce::String& name, float lo, float hi, float def, float skew = 1.0f, const char* unit = "")
        {
            ParamInfo p; p.id = id; p.name = name; p.unit = unit; p.type = ParamInfo::Float; p.lo = lo; p.hi = hi; p.def = def; p.skew = skew;
            out.push_back (p);
        };
        auto pct = [&] (const juce::String& id, const juce::String& name, float def) { fl (id, name, 0.0f, 1.0f, def, 1.0f, "%"); };
        auto ch = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
        {
            ParamInfo p; p.id = id; p.name = name; p.type = ParamInfo::Choice; p.lo = 0.0f; p.hi = (float) (items.size() - 1); p.def = (float) def; p.choices = items;
            out.push_back (p);
        };
        auto in = [&] (const juce::String& id, const juce::String& name, int lo, int hi, int def)
        {
            ParamInfo p; p.id = id; p.name = name; p.type = ParamInfo::Int; p.lo = (float) lo; p.hi = (float) hi; p.def = (float) def;
            out.push_back (p);
        };
        auto bo = [&] (const juce::String& id, const juce::String& name, bool def)
        {
            ParamInfo p; p.id = id; p.name = name; p.type = ParamInfo::Bool; p.lo = 0.0f; p.hi = 1.0f; p.def = def ? 1.0f : 0.0f;
            out.push_back (p);
        };

        for (int l = 1; l <= 2; ++l)
        {
            const juce::String pre = l == 1 ? "Layer I " : "Layer II ";
            bo (lid (l, "on"), pre + "On", true);
            ch (lid (l, "feet"), pre + "Feet", feetNames(), 1);
            in (lid (l, "unison"), pre + "Unison", 1, 3, l == 1 ? 3 : 2);
            for (const auto& p : layerParams())
                fl (lid (l, p.id), pre + p.name, p.lo, p.hi, l == 1 ? p.d1 : p.d2, p.skew, layerUnit (p.id));
        }
        pct ("ring", "Ring Mod", 0.0f);
        in ("voices", "Voices", 1, maxVoices, 8);
        fl ("volume", "Volume", -36.0f, 6.0f, 0.0f, 1.0f, "dB");
        pct ("panSpread", "Pan Spread", 0.4f);
        in ("bendRange", "Bend Range", 1, 48, 48);

        // Glide
        ch ("glideMode", "Glide Mode", { "Time", "Rate" }, 0);
        fl ("glideMs", "Glide Time", 5.0f, 5000.0f, 600.0f, 0.35f, "ms");
        fl ("rateMs", "Glide Rate", 5.0f, 500.0f, 60.0f, 0.5f, "ms/st");
        ch ("curve", "Glide Curve", curveNames(), 2);
        pct ("glideSpread", "Glide Spread", 0.2f);
        ch ("leading", "Voice Leading", { "Smart", "Classic" }, 0);
        ch ("newVoices", "New Voices", { "Branch", "Fresh" }, 0);
        bo ("merge", "Merging Tails", true);
        fl ("staggerMs", "Stagger", 0.0f, 500.0f, 0.0f, 0.5f, "ms");
        ch ("staggerOrd", "Stagger Order", staggerNames(), 0);
        ch ("path", "Glide Path", pathNames(), 0);
        fl ("pathDepth", "Path Depth", 0.0f, 12.0f, 3.0f, 1.0f, "st");
        ch ("startFrom", "Start Direction", startNames(), 0);
        fl ("startDepth", "Start Depth", 0.0f, 24.0f, 0.0f, 1.0f, "st");
        fl ("releaseBend", "Release Bend", -24.0f, 24.0f, 0.0f, 1.0f, "st");
        ch ("lineMode", "Lines", { "Released Lines", "Legato Only" }, 0);
        fl ("lineMemory", "Line Memory", 0.0f, 20.0f, 3.0f, 0.5f, "s");

        // Autopilot
        ch ("pilot", "Autopilot", pilotNames(), 0);
        fl ("pilotRate", "Pilot Rate", 0.05f, 12.0f, 1.0f, 0.4f, "Hz");
        ch ("pilotSync", "Pilot Sync", syncNames(), 0);
        pct ("pilotRandom", "Pilot Timing Random", 0.0f);
        pct ("pilotDensity", "Pilot Density", 0.7f);
        pct ("pilotSpread", "Pilot Spread", 0.2f);
        pct ("pilotSmooth", "Pilot Smoothness", 0.5f);
        fl ("pilotRange", "Pilot Range", 1.0f, 48.0f, 12.0f, 1.0f, "st");
        ch ("pilotScale", "Pilot Scale", scaleNames(), 1);
        ch ("pilotRelease", "Pilot Release", { "Return", "Hold" }, 0);
        static const int steps[] = { 0, 7, 12, 7, 0, -5, 3, 5 };
        for (int s = 1; s <= 8; ++s)
            in ("step" + juce::String (s), "Sequence Step " + juce::String (s), -24, 24, steps[s - 1]);

        // Expression
        pct ("swell", "Swell", 0.3f);
        fl ("swellTime", "Swell Time", 0.05f, 10.0f, 2.0f, 0.4f, "s");
        pct ("brighten", "Brighten", 0.3f);
        pct ("drift", "Drift", 0.2f);
        pct ("wander", "Timbre Wander", 0.2f);
        fl ("vibDepth", "Vibrato Depth", 0.0f, 1.0f, 0.15f, 1.0f, "st");
        fl ("vibRate", "Vibrato Rate", 0.5f, 10.0f, 5.2f, 1.0f, "Hz");
        fl ("vibDelay", "Vibrato Delay", 0.0f, 5.0f, 0.8f, 0.5f, "s");

        // Modulators + matrix
        for (int m = 1; m <= 4; ++m)
        {
            ch ("m" + juce::String (m) + "Type", "M" + juce::String (m) + " Type", modTypeNames(), m == 4 ? 6 : 0);
            ch ("m" + juce::String (m) + "Mode", "M" + juce::String (m) + " Mode", modModeNames(), 0);
            fl ("m" + juce::String (m) + "Rate", "M" + juce::String (m) + " Rate", 0.02f, 20.0f, (float) m * 0.4f, 0.35f, "Hz");
        }
        for (int s = 1; s <= 8; ++s)
        {
            ch ("mx" + juce::String (s) + "Src", "Mod " + juce::String (s) + " Source", modSourceNames(), 0);
            ch ("mx" + juce::String (s) + "Dst", "Mod " + juce::String (s) + " Target", modDestNames(), 0);
            fl ("mx" + juce::String (s) + "Amt", "Mod " + juce::String (s) + " Amount", -1.0f, 1.0f, 0.0f);
        }

        // FX
        ch ("ensMode", "Ensemble", ensembleModeNames(), 1);
        pct ("ensMix", "Ensemble Mix", 0.3f);
        fl ("ensRate", "Ensemble Rate", 0.1f, 3.0f, 0.6f, 0.6f, "Hz");
        pct ("ensDepth", "Ensemble Depth", 0.5f);
        pct ("prismMix", "Prism Mix", 0.0f);
        fl ("prismTime", "Prism Time", 20.0f, 2000.0f, 380.0f, 0.5f, "ms");
        fl ("prismFb", "Prism Feedback", 0.0f, 0.9f, 0.35f, 1.0f, "%");
        ch ("prismI1", "Prism Interval 1", intervalNames(), 8);
        ch ("prismI2", "Prism Interval 2", intervalNames(), 9);
        ch ("prismI3", "Prism Interval 3", intervalNames(), 1);
        ch ("roomType", "Room", roomTypeNames(), 1);
        pct ("roomMix", "Room Mix", 0.25f);
        pct ("roomSize", "Room Size", 0.6f);
        pct ("drive", "Drive", 0.0f);
        pct ("driveTone", "Drive Tone", 0.55f);
        pct ("echoMix", "Echo Mix", 0.0f);
        ch ("echoTime", "Echo Time", echoTimeNames(), 2);
        fl ("echoFb", "Echo Feedback", 0.0f, 0.92f, 0.4f, 1.0f, "%");
        pct ("echoTone", "Echo Tone", 0.5f);
        pct ("tapeWow", "Tape Wow", 0.0f);
        pct ("tapeFlutter", "Tape Flutter", 0.0f);
        pct ("tapeAge", "Tape Age", 0.0f);
        return out;
    }

    V L (int layer, std::initializer_list<std::pair<const char*, float>> kv)
    {
        V v;
        for (const auto& [k, x] : kv) v.push_back ({ lid (layer, k), x });
        return v;
    }
    V G (std::initializer_list<std::pair<const char*, float>> kv)
    {
        V v;
        for (const auto& [k, x] : kv) v.push_back ({ k, x });
        return v;
    }
    V join (std::initializer_list<V> parts)
    {
        V v;
        for (const auto& p : parts) v.insert (v.end(), p.begin(), p.end());
        return v;
    }
}

//==============================================================================
juce::String ParamInfo::format (float v) const
{
    switch (type)
    {
        case Choice: return choices[juce::jlimit (0, choices.size() - 1, juce::roundToInt (v))];
        case Int:    return juce::String (juce::roundToInt (v));
        case Bool:   return v > 0.5f ? "On" : "Off";
        case Float:  break;
    }
    const juce::String& u = unit;
    if (u == "%")   return juce::String (juce::roundToInt (v * 100.0f)) + "%";
    if (u == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : (v < 10.0f ? juce::String (v, 2) : juce::String (juce::roundToInt (v))) + " Hz";
    if (u == "ms" || u == "ms/st") return juce::String (juce::roundToInt (v)) + " " + u;
    if (u == "s")   return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
    if (u == "dB")  return juce::String (v, 1) + " dB";
    if (u == "st")  return juce::String (v, 1) + " st";
    if (u == "ct")  return juce::String (v, 1) + " ct";
    if (u == "oct") return juce::String (v, 2) + " oct";
    return juce::String (v, 2);
}

float ParamInfo::toNormalised (float v) const
{
    if (hi <= lo) return 0.0f;
    const float n = juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
    return type == Float && skew != 1.0f ? std::pow (n, skew) : n;
}

float ParamInfo::fromNormalised (float n) const
{
    n = juce::jlimit (0.0f, 1.0f, n);
    if (type == Float && skew != 1.0f && n > 0.0f) n = std::pow (n, 1.0f / skew);
    const float v = lo + (hi - lo) * n;
    return type == Float ? v : (float) juce::roundToInt (v);
}

const std::vector<ParamInfo>& params()
{
    static const std::vector<ParamInfo> list = buildParams();
    return list;
}

int paramIndex (const juce::String& id)
{
    const auto& list = params();
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].id == id) return (int) i;
    return -1;
}

juce::StringArray glideParamIds()
{
    juce::StringArray ids { "glideMode", "glideMs", "rateMs", "curve", "glideSpread", "leading", "newVoices", "merge", "staggerMs",
                            "staggerOrd", "path", "pathDepth", "startFrom", "startDepth", "releaseBend", "lineMode", "lineMemory",
                            "pilot", "pilotRate", "pilotSync", "pilotRandom", "pilotDensity", "pilotSpread", "pilotSmooth",
                            "pilotRange", "pilotScale", "pilotRelease" };
    for (int s = 1; s <= 8; ++s) ids.add ("step" + juce::String (s));
    return ids;
}

juce::StringArray soundParamIds()
{
    juce::StringArray ids;
    for (int l = 1; l <= 2; ++l)
    {
        ids.add (lid (l, "on")); ids.add (lid (l, "feet")); ids.add (lid (l, "unison"));
        for (const auto& p : layerParams()) ids.add (lid (l, p.id));
    }
    for (auto* id : { "ring", "panSpread", "swell", "swellTime", "brighten", "drift", "wander", "vibDepth", "vibRate", "vibDelay",
                      "ensMode", "ensMix", "ensRate", "ensDepth", "prismMix", "prismTime", "prismFb", "prismI1", "prismI2", "prismI3",
                      "roomType", "roomMix", "roomSize", "drive", "driveTone", "echoMix", "echoTime", "echoFb", "echoTone",
                      "tapeWow", "tapeFlutter", "tapeAge" })
        ids.add (id);
    for (int m = 1; m <= 4; ++m)
        for (auto* k : { "Type", "Mode", "Rate" }) ids.add ("m" + juce::String (m) + k);
    for (int s = 1; s <= 8; ++s)
        for (auto* k : { "Src", "Dst", "Amt" }) ids.add ("mx" + juce::String (s) + k);
    return ids;
}

//==============================================================================
void readSettings (const std::function<float (const char*)>& p, Settings& s, FxSettings& f)
{
    for (int l = 0; l < 2; ++l)
    {
        auto& ls = s.layers[(size_t) l];
        char buf[32];
        auto g = [&] (const char* id) { std::snprintf (buf, sizeof (buf), "l%d_%s", l + 1, id); return p (buf); };
        ls.on = g ("on") > 0.5f;
        ls.feet = juce::roundToInt (g ("feet"));
        ls.unison = juce::roundToInt (g ("unison"));
        ls.saw = g ("saw"); ls.pulse = g ("pulse"); ls.pw = g ("pw"); ls.pwmRate = g ("pwmRate"); ls.pwm = g ("pwm");
        ls.sine = g ("sine"); ls.noise = g ("noise"); ls.detune = g ("detune"); ls.fine = g ("fine");
        ls.hpf = g ("hpf"); ls.hpRes = g ("hpRes"); ls.lpf = g ("lpf"); ls.lpRes = g ("lpRes"); ls.key = g ("key");
        ls.egDepth = g ("egDepth"); ls.egIL = g ("egIL"); ls.egAL = g ("egAL"); ls.egAtk = g ("egAtk"); ls.egDec = g ("egDec"); ls.egRel = g ("egRel");
        ls.attack = g ("attack"); ls.decay = g ("decay"); ls.sustain = g ("sustain"); ls.release = g ("release");
        ls.brill = g ("brill"); ls.level = g ("level");
    }
    s.ring = p ("ring");
    s.voices = juce::roundToInt (p ("voices"));
    s.volume = juce::Decibels::decibelsToGain (p ("volume"), -36.0f);
    s.panSpread = p ("panSpread");
    s.bendRange = p ("bendRange");

    s.rateMode = juce::roundToInt (p ("glideMode")) == 1;
    s.glideMs = p ("glideMs"); s.rateMs = p ("rateMs");
    s.curve = (Curve) juce::roundToInt (p ("curve"));
    s.glideSpread = p ("glideSpread");
    s.smartLeading = juce::roundToInt (p ("leading")) == 0;
    s.branch = juce::roundToInt (p ("newVoices")) == 0;
    s.merge = p ("merge") > 0.5f;
    s.staggerMs = p ("staggerMs");
    s.staggerOrder = (Stagger) juce::roundToInt (p ("staggerOrd"));
    s.path = (Path) juce::roundToInt (p ("path"));
    s.pathDepth = p ("pathDepth");
    s.startFrom = (StartFrom) juce::roundToInt (p ("startFrom"));
    s.startDepth = p ("startDepth");
    s.releaseBend = p ("releaseBend");
    s.releasedLines = juce::roundToInt (p ("lineMode")) == 0;
    s.lineMemory = p ("lineMemory");

    s.pilot = (Pilot) juce::roundToInt (p ("pilot"));
    s.pilotRateHz = p ("pilotRate");
    s.pilotSync = juce::roundToInt (p ("pilotSync"));
    s.pilotRandom = p ("pilotRandom"); s.pilotDensity = p ("pilotDensity"); s.pilotSpread = p ("pilotSpread");
    s.pilotSmooth = p ("pilotSmooth"); s.pilotRange = p ("pilotRange");
    s.pilotScale = juce::roundToInt (p ("pilotScale"));
    s.pilotReturn = juce::roundToInt (p ("pilotRelease")) == 0;
    static const char* stepIds[] = { "step1", "step2", "step3", "step4", "step5", "step6", "step7", "step8" };
    for (int k = 0; k < 8; ++k) s.pilotSteps[(size_t) k] = juce::roundToInt (p (stepIds[k]));

    s.swell = p ("swell"); s.swellTime = p ("swellTime"); s.brighten = p ("brighten"); s.drift = p ("drift"); s.wander = p ("wander");
    s.vibDepth = p ("vibDepth"); s.vibRate = p ("vibRate"); s.vibDelay = p ("vibDelay");

    static const char* mt[] = { "m1Type", "m2Type", "m3Type", "m4Type" };
    static const char* mm[] = { "m1Mode", "m2Mode", "m3Mode", "m4Mode" };
    static const char* mr[] = { "m1Rate", "m2Rate", "m3Rate", "m4Rate" };
    for (int m = 0; m < 4; ++m)
    {
        s.modType[(size_t) m] = juce::roundToInt (p (mt[m]));
        s.modMode[(size_t) m] = juce::roundToInt (p (mm[m]));
        s.modRate[(size_t) m] = p (mr[m]);
    }
    static const char* ms[] = { "mx1Src", "mx2Src", "mx3Src", "mx4Src", "mx5Src", "mx6Src", "mx7Src", "mx8Src" };
    static const char* md[] = { "mx1Dst", "mx2Dst", "mx3Dst", "mx4Dst", "mx5Dst", "mx6Dst", "mx7Dst", "mx8Dst" };
    static const char* ma[] = { "mx1Amt", "mx2Amt", "mx3Amt", "mx4Amt", "mx5Amt", "mx6Amt", "mx7Amt", "mx8Amt" };
    for (int k = 0; k < 8; ++k)
        s.matrix[(size_t) k] = { juce::roundToInt (p (ms[k])), juce::roundToInt (p (md[k])), p (ma[k]) };

    f.ensembleMode = juce::roundToInt (p ("ensMode"));
    f.ensembleMix = p ("ensMix"); f.ensembleRate = p ("ensRate"); f.ensembleDepth = p ("ensDepth");
    f.prismMix = p ("prismMix"); f.prismTimeMs = p ("prismTime"); f.prismFeedback = p ("prismFb");
    f.prismI1 = juce::roundToInt (p ("prismI1")); f.prismI2 = juce::roundToInt (p ("prismI2")); f.prismI3 = juce::roundToInt (p ("prismI3"));
    f.roomType = juce::roundToInt (p ("roomType"));
    f.roomMix = p ("roomMix"); f.roomSize = p ("roomSize");
    f.drive = p ("drive"); f.driveTone = p ("driveTone");
    f.echoMix = p ("echoMix"); f.echoFeedback = p ("echoFb"); f.echoTone = p ("echoTone");
    f.echoBeats = echoTimeBeats (juce::roundToInt (p ("echoTime")));
    f.tapeWow = p ("tapeWow"); f.tapeFlutter = p ("tapeFlutter"); f.tapeAge = p ("tapeAge");
}

//==============================================================================
const std::vector<Preset>& soundPresets()
{
    static const std::vector<Preset> list = {
        // ---------------- Brass
        { "Soft Brass Swell", "Brass", {} },
        { "Bright Brass", "Brass", join ({ L (1, { { "lpf", 900 }, { "egDepth", 3.2f }, { "attack", 0.12f }, { "egAtk", 0.25f } }),
                                          L (2, { { "lpf", 1200 }, { "attack", 0.15f }, { "egAtk", 0.3f } }), G ({ { "swell", 0.15f } }) }) },
        { "Brass Section", "Brass", join ({ L (1, { { "unison", 3 }, { "detune", 16 }, { "attack", 0.25f } }), L (2, { { "feet", 0 }, { "level", 0.45f } }),
                                           G ({ { "ensMix", 0.45f }, { "roomType", 1 }, { "roomMix", 0.3f } }) }) },
        { "Muted Horn", "Brass", join ({ L (1, { { "lpf", 300 }, { "egDepth", 1.5f }, { "pulse", 0.0f } }), L (2, { { "on", 0 } }),
                                        G ({ { "vibDepth", 0.08f }, { "roomType", 0 } }) }) },
        { "Synth Brass Stab", "Brass", join ({ L (1, { { "attack", 0.005f }, { "decay", 0.5f }, { "sustain", 0.5f }, { "release", 0.4f }, { "egAtk", 0.01f }, { "egDec", 0.4f }, { "egDepth", 4.0f } }),
                                              L (2, { { "attack", 0.005f }, { "decay", 0.6f }, { "sustain", 0.4f }, { "release", 0.4f }, { "egAtk", 0.01f }, { "egDec", 0.5f } }),
                                              G ({ { "swell", 0.0f }, { "vibDepth", 0.0f } }) }) },
        // ---------------- Strings
        { "Morphing Strings", "Strings", join ({ L (1, { { "pulse", 0.0f }, { "lpf", 1400 }, { "egDepth", 1.0f }, { "attack", 1.2f }, { "release", 2.5f } }),
                                                L (2, { { "feet", 2 }, { "level", 0.35f }, { "lpf", 2000 } }),
                                                G ({ { "ensMode", 2 }, { "ensMix", 0.6f }, { "roomType", 1 }, { "roomMix", 0.35f }, { "vibDepth", 0.12f } }) }) },
        { "Neon Strings", "Strings", join ({ L (1, { { "lpf", 2500 }, { "lpRes", 2.5f }, { "attack", 0.4f }, { "egDepth", 1.5f } }), L (2, { { "feet", 2 }, { "detune", 20 } }),
                                            G ({ { "ensMode", 2 }, { "ensMix", 0.5f }, { "prismMix", 0.2f }, { "roomType", 2 }, { "roomMix", 0.3f } }) }) },
        { "Chamber Strings", "Strings", join ({ L (1, { { "pulse", 0.1f }, { "lpf", 1100 }, { "attack", 0.5f }, { "unison", 2 }, { "detune", 6 } }), L (2, { { "on", 0 } }),
                                               G ({ { "ensMode", 2 }, { "ensMix", 0.35f }, { "roomType", 0 }, { "roomMix", 0.3f }, { "vibDepth", 0.18f }, { "vibDelay", 0.4f } }) }) },
        { "String Machine", "Strings", join ({ L (1, { { "saw", 0.3f }, { "pulse", 0.8f }, { "pwm", 0.6f }, { "lpf", 1800 }, { "egDepth", 0.5f } }), L (2, { { "feet", 2 }, { "pulse", 0.7f }, { "saw", 0.2f } }),
                                              G ({ { "ensMode", 2 }, { "ensMix", 0.8f }, { "ensDepth", 0.8f } }) }) },
        { "Slow Cellos", "Strings", join ({ L (1, { { "feet", 0 }, { "lpf", 600 }, { "attack", 1.6f }, { "release", 3.0f } }), L (2, { { "feet", 1 }, { "level", 0.4f } }),
                                           G ({ { "vibDepth", 0.2f }, { "vibRate", 4.6f }, { "roomType", 1 }, { "roomMix", 0.35f } }) }) },
        { "Pizzicato Glide", "Strings", join ({ L (1, { { "attack", 0.003f }, { "decay", 0.35f }, { "sustain", 0.0f }, { "release", 0.3f }, { "egAtk", 0.002f }, { "egDec", 0.25f }, { "egDepth", 3.5f } }),
                                               L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "roomType", 0 }, { "roomMix", 0.3f } }) }) },
        // ---------------- Pads
        { "Dusty Pad", "Pads", join ({ L (1, { { "lpf", 700 }, { "noise", 0.08f }, { "attack", 1.8f }, { "release", 3.5f } }), L (2, { { "feet", 2 }, { "lpf", 1200 }, { "level", 0.3f } }),
                                      G ({ { "drift", 0.5f }, { "wander", 0.5f }, { "ensMix", 0.5f }, { "roomType", 1 }, { "roomMix", 0.4f } }) }) },
        { "Glass Pad", "Pads", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.5f }, { "pw", 0.15f }, { "sine", 0.6f }, { "lpf", 3000 }, { "attack", 1.0f } }),
                                      L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.6f }, { "level", 0.3f } }),
                                      G ({ { "ring", 0.15f }, { "roomType", 2 }, { "roomMix", 0.45f }, { "prismMix", 0.15f } }) }) },
        { "Warm Analog Pad", "Pads", join ({ L (1, { { "lpf", 900 }, { "attack", 1.4f }, { "release", 3.0f }, { "detune", 14 } }), L (2, { { "feet", 0 }, { "level", 0.5f } }),
                                            G ({ { "ensMix", 0.5f }, { "roomType", 1 }, { "roomMix", 0.35f } }) }) },
        { "Choir Pad", "Pads", join ({ L (1, { { "saw", 0.4f }, { "pulse", 0.6f }, { "pw", 0.25f }, { "lpf", 1300 }, { "lpRes", 3.0f }, { "attack", 1.2f } }),
                                      L (2, { { "feet", 2 }, { "sine", 0.4f }, { "saw", 0.2f }, { "level", 0.4f } }),
                                      G ({ { "vibDepth", 0.2f }, { "vibRate", 5.6f }, { "ensMode", 2 }, { "ensMix", 0.6f }, { "roomType", 3 }, { "roomMix", 0.45f } }) }) },
        { "Breathing Pad", "Pads", join ({ L (1, { { "lpf", 600 }, { "attack", 2.5f }, { "release", 4.0f } }), L (2, { { "noise", 0.15f }, { "lpf", 2000 }, { "level", 0.3f } }),
                                          G ({ { "mx1Src", 1 }, { "mx1Dst", 4 }, { "mx1Amt", 0.25f }, { "m1Rate", 0.15f }, { "roomType", 1 }, { "roomMix", 0.4f } }) }) },
        { "Shimmer Pad", "Pads", join ({ L (1, { { "lpf", 1500 }, { "attack", 1.5f }, { "release", 4.0f } }), L (2, { { "feet", 2 }, { "sine", 0.5f }, { "level", 0.4f } }),
                                        G ({ { "roomType", 4 }, { "roomMix", 0.55f }, { "roomSize", 0.8f }, { "ensMix", 0.4f } }) }) },
        { "Dark Glass", "Pads", join ({ L (1, { { "lpf", 380 }, { "lpRes", 3.5f }, { "egDepth", 1.2f }, { "attack", 1.0f } }), L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.5f }, { "level", 0.25f } }),
                                       G ({ { "ring", 0.2f }, { "prismMix", 0.25f }, { "prismI1", 2 }, { "prismI2", 1 }, { "roomType", 3 }, { "roomMix", 0.4f } }) }) },
        // ---------------- Keys
        { "Velvet Keys", "Keys", join ({ L (1, { { "saw", 0.2f }, { "pulse", 0.4f }, { "sine", 0.6f }, { "attack", 0.004f }, { "decay", 2.5f }, { "sustain", 0.3f }, { "release", 1.0f },
                                                  { "egAtk", 0.003f }, { "egDec", 1.2f }, { "egDepth", 2.5f }, { "lpf", 800 } }),
                                        L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "ensMode", 1 }, { "ensMix", 0.35f }, { "roomType", 0 }, { "roomMix", 0.25f } }) }) },
        { "Electric Glide", "Keys", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 0.9f }, { "attack", 0.003f }, { "decay", 3.0f }, { "sustain", 0.2f }, { "release", 0.8f } }),
                                           L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.4f }, { "attack", 0.002f }, { "decay", 0.4f }, { "sustain", 0.0f }, { "level", 0.3f } }),
                                           G ({ { "ring", 0.25f }, { "ensMix", 0.4f }, { "swell", 0.0f } }) }) },
        { "Toy Piano", "Keys", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.6f }, { "pw", 0.12f }, { "attack", 0.002f }, { "decay", 0.8f }, { "sustain", 0.0f }, { "release", 0.5f }, { "lpf", 3000 } }),
                                      L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.5f }, { "attack", 0.002f }, { "decay", 0.3f }, { "sustain", 0.0f } }), G ({ { "swell", 0.0f }, { "vibDepth", 0.0f } }) }) },
        { "Organ Lines", "Keys", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 0.9f }, { "attack", 0.01f }, { "sustain", 1.0f }, { "release", 0.15f }, { "lpf", 6000 } }),
                                        L (2, { { "feet", 2 }, { "saw", 0.0f }, { "sine", 0.6f }, { "attack", 0.01f }, { "sustain", 1.0f }, { "release", 0.15f } }),
                                        G ({ { "swell", 0.0f }, { "vibDepth", 0.05f }, { "vibDelay", 0.0f }, { "vibRate", 6.5f }, { "ensMode", 1 }, { "ensMix", 0.5f } }) }) },
        { "Clav Glide", "Keys", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.9f }, { "pw", 0.2f }, { "attack", 0.002f }, { "decay", 0.6f }, { "sustain", 0.2f }, { "release", 0.2f },
                                                 { "hpf", 300 }, { "lpf", 1500 }, { "lpRes", 3.0f }, { "egAtk", 0.002f }, { "egDec", 0.3f } }),
                                       L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "roomMix", 0.1f } }) }) },
        // ---------------- Leads
        { "Saw Lead", "Leads", join ({ L (1, { { "lpf", 2500 }, { "attack", 0.01f }, { "egDepth", 1.5f }, { "unison", 2 } }), L (2, { { "on", 0 } }),
                                      G ({ { "vibDepth", 0.25f }, { "vibDelay", 0.4f }, { "prismMix", 0.15f }, { "roomType", 2 }, { "roomMix", 0.2f } }) }) },
        { "Pulse Lead", "Leads", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.9f }, { "pwm", 0.5f }, { "lpf", 2000 }, { "attack", 0.01f } }), L (2, { { "on", 0 } }),
                                        G ({ { "vibDepth", 0.2f }, { "roomMix", 0.2f } }) }) },
        { "Whistle Lead", "Leads", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 0.9f }, { "noise", 0.05f }, { "attack", 0.05f } }), L (2, { { "on", 0 } }),
                                          G ({ { "vibDepth", 0.3f }, { "vibDelay", 0.2f }, { "roomType", 1 }, { "roomMix", 0.3f } }) }) },
        { "Screamer", "Leads", join ({ L (1, { { "lpf", 1200 }, { "lpRes", 6.5f }, { "egDepth", 3.0f }, { "attack", 0.01f } }), L (2, { { "feet", 2 }, { "pulse", 0.6f } }),
                                      G ({ { "ring", 0.3f }, { "prismMix", 0.2f }, { "roomType", 2 }, { "roomMix", 0.2f } }) }) },
        { "Soft Sine Lead", "Leads", join ({ L (1, { { "saw", 0.1f }, { "pulse", 0.0f }, { "sine", 0.8f }, { "lpf", 1500 }, { "attack", 0.08f } }), L (2, { { "on", 0 } }),
                                            G ({ { "vibDepth", 0.18f }, { "ensMix", 0.3f }, { "roomType", 1 }, { "roomMix", 0.3f } }) }) },
        // ---------------- Basses
        { "Glide Bass", "Basses", join ({ L (1, { { "feet", 0 }, { "lpf", 400 }, { "lpRes", 2.0f }, { "attack", 0.005f }, { "egAtk", 0.005f }, { "egDec", 0.5f }, { "egDepth", 2.5f }, { "unison", 1 } }),
                                         L (2, { { "feet", 0 }, { "saw", 0.0f }, { "sine", 0.8f }, { "level", 0.5f } }), G ({ { "swell", 0.0f }, { "vibDepth", 0.0f }, { "ensMix", 0.0f }, { "roomMix", 0.05f } }) }) },
        { "Sub Lines", "Basses", join ({ L (1, { { "feet", 0 }, { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 1.0f }, { "attack", 0.01f } }), L (2, { { "on", 0 } }),
                                        G ({ { "swell", 0.0f }, { "vibDepth", 0.0f }, { "ensMix", 0.0f }, { "roomMix", 0.0f } }) }) },
        { "Acid Glide", "Basses", join ({ L (1, { { "feet", 0 }, { "saw", 0.9f }, { "pulse", 0.0f }, { "unison", 1 }, { "lpf", 300 }, { "lpRes", 7.0f }, { "egDepth", 4.0f },
                                                   { "egAtk", 0.003f }, { "egDec", 0.3f }, { "attack", 0.003f } }),
                                         L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "vibDepth", 0.0f }, { "ensMix", 0.0f }, { "roomMix", 0.05f } }) }) },
        { "Round Bass", "Basses", join ({ L (1, { { "feet", 0 }, { "saw", 0.4f }, { "pulse", 0.0f }, { "sine", 0.7f }, { "lpf", 500 }, { "unison", 1 } }), L (2, { { "on", 0 } }),
                                         G ({ { "swell", 0.0f }, { "vibDepth", 0.0f }, { "ensMix", 0.0f } }) }) },
        { "Pulse Bass", "Basses", join ({ L (1, { { "feet", 0 }, { "saw", 0.0f }, { "pulse", 0.9f }, { "pw", 0.3f }, { "lpf", 700 }, { "attack", 0.004f }, { "decay", 0.7f }, { "sustain", 0.6f } }),
                                         L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "vibDepth", 0.0f }, { "ensMix", 0.1f } }) }) },
        // ---------------- Plucks
        { "Glide Pluck", "Plucks", join ({ L (1, { { "attack", 0.002f }, { "decay", 0.6f }, { "sustain", 0.0f }, { "release", 0.6f }, { "egAtk", 0.002f }, { "egDec", 0.4f }, { "egDepth", 4.0f } }),
                                          L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "prismMix", 0.25f }, { "roomType", 2 }, { "roomMix", 0.3f } }) }) },
        { "Harp Lines", "Plucks", join ({ L (1, { { "saw", 0.3f }, { "pulse", 0.0f }, { "sine", 0.7f }, { "attack", 0.002f }, { "decay", 1.8f }, { "sustain", 0.0f }, { "release", 1.5f }, { "lpf", 2500 } }),
                                         L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "roomType", 1 }, { "roomMix", 0.4f } }) }) },
        { "Kalimba Glide", "Plucks", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 1.0f }, { "attack", 0.001f }, { "decay", 0.9f }, { "sustain", 0.0f }, { "release", 0.8f } }),
                                            L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.4f }, { "attack", 0.001f }, { "decay", 0.15f }, { "sustain", 0.0f } }),
                                            G ({ { "ring", 0.35f }, { "swell", 0.0f }, { "roomType", 0 }, { "roomMix", 0.3f } }) }) },
        { "Mallet Glide", "Plucks", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.4f }, { "pw", 0.5f }, { "sine", 0.6f }, { "attack", 0.001f }, { "decay", 1.2f }, { "sustain", 0.0f }, { "lpf", 1800 } }),
                                           L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "roomType", 1 }, { "roomMix", 0.35f } }) }) },
        // ---------------- Effects
        { "Ring Bells", "FX", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 1.0f }, { "attack", 0.002f }, { "decay", 3.0f }, { "sustain", 0.0f }, { "release", 3.0f } }),
                                     L (2, { { "feet", 2 }, { "fine", 37 }, { "saw", 0.0f }, { "sine", 1.0f }, { "attack", 0.002f }, { "decay", 2.0f }, { "sustain", 0.0f } }),
                                     G ({ { "ring", 0.9f }, { "swell", 0.0f }, { "roomType", 3 }, { "roomMix", 0.4f } }) }) },
        { "Prism Rain", "FX", join ({ L (1, { { "attack", 0.003f }, { "decay", 0.5f }, { "sustain", 0.1f }, { "egDepth", 3.0f }, { "egAtk", 0.003f }, { "egDec", 0.4f } }), L (2, { { "on", 0 } }),
                                     G ({ { "prismMix", 0.7f }, { "prismFb", 0.6f }, { "prismTime", 280 }, { "roomType", 4 }, { "roomMix", 0.4f } }) }) },
        { "Noise Wind", "FX", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "noise", 1.0f }, { "lpf", 800 }, { "lpRes", 6.0f }, { "attack", 1.5f } }), L (2, { { "on", 0 } }),
                                     G ({ { "mx1Src", 1 }, { "mx1Dst", 2 }, { "mx1Amt", 0.4f }, { "m1Rate", 0.1f }, { "roomType", 1 }, { "roomMix", 0.5f } }) }) },
        { "Space Sirens", "FX", join ({ L (1, { { "saw", 0.0f }, { "sine", 0.8f }, { "pulse", 0.3f } }), L (2, { { "feet", 2 }, { "sine", 0.5f } }),
                                       G ({ { "mx1Src", 1 }, { "mx1Dst", 1 }, { "mx1Amt", 0.15f }, { "m1Rate", 0.3f }, { "prismMix", 0.3f }, { "roomType", 4 }, { "roomMix", 0.5f } }) }) },
        { "Cathedral Drone", "FX", join ({ L (1, { { "feet", 0 }, { "lpf", 500 }, { "attack", 3.0f }, { "release", 6.0f } }), L (2, { { "feet", 2 }, { "lpf", 900 }, { "attack", 4.0f }, { "level", 0.5f } }),
                                          G ({ { "drift", 0.4f }, { "roomType", 3 }, { "roomMix", 0.6f }, { "roomSize", 0.9f } }) }) },
        // ---------------- Lo-Fi (tape)
        { "Cassette Strings", "Lo-Fi", join ({ L (1, { { "pulse", 0.1f }, { "lpf", 1300 }, { "attack", 0.6f }, { "release", 2.0f } }), L (2, { { "feet", 2 }, { "level", 0.3f } }),
                                                G ({ { "ensMode", 2 }, { "ensMix", 0.5f }, { "tapeWow", 0.45f }, { "tapeFlutter", 0.3f }, { "tapeAge", 0.5f }, { "roomType", 1 }, { "roomMix", 0.3f } }) }) },
        { "Warped Keys", "Lo-Fi", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.3f }, { "sine", 0.8f }, { "attack", 0.004f }, { "decay", 2.0f }, { "sustain", 0.25f }, { "release", 0.9f }, { "lpf", 900 } }),
                                           L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "tapeWow", 0.7f }, { "tapeFlutter", 0.4f }, { "tapeAge", 0.6f }, { "roomType", 0 }, { "roomMix", 0.25f } }) }) },
        { "VHS Pad", "Lo-Fi", join ({ L (1, { { "lpf", 800 }, { "attack", 1.5f }, { "release", 3.0f }, { "noise", 0.04f } }), L (2, { { "feet", 2 }, { "sine", 0.5f }, { "level", 0.3f } }),
                                       G ({ { "drift", 0.4f }, { "tapeWow", 0.55f }, { "tapeAge", 0.8f }, { "ensMix", 0.4f }, { "roomType", 1 }, { "roomMix", 0.35f } }) }) },
        { "Dusty Music Box", "Lo-Fi", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 1.0f }, { "attack", 0.001f }, { "decay", 1.4f }, { "sustain", 0.0f }, { "release", 1.0f } }),
                                               L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.35f }, { "attack", 0.001f }, { "decay", 0.3f }, { "sustain", 0.0f } }),
                                               G ({ { "ring", 0.2f }, { "swell", 0.0f }, { "tapeWow", 0.35f }, { "tapeFlutter", 0.6f }, { "tapeAge", 0.7f }, { "roomType", 0 }, { "roomMix", 0.3f } }) }) },
        // ---------------- Driven
        { "Tube Lead", "Driven", join ({ L (1, { { "lpf", 1800 }, { "attack", 0.01f }, { "egDepth", 1.5f }, { "unison", 2 } }), L (2, { { "on", 0 } }),
                                         G ({ { "drive", 0.55f }, { "driveTone", 0.6f }, { "vibDepth", 0.22f }, { "echoMix", 0.25f }, { "echoTime", 2 }, { "roomType", 2 }, { "roomMix", 0.2f } }) }) },
        { "Fuzz Brass", "Driven", join ({ L (1, { { "lpf", 1000 }, { "egDepth", 3.0f }, { "attack", 0.08f } }), L (2, { { "feet", 0 }, { "level", 0.4f } }),
                                          G ({ { "drive", 0.8f }, { "driveTone", 0.45f }, { "ensMix", 0.3f }, { "roomType", 1 }, { "roomMix", 0.25f } }) }) },
        { "Growl Bass", "Driven", join ({ L (1, { { "feet", 0 }, { "lpf", 500 }, { "lpRes", 3.0f }, { "egDepth", 3.0f }, { "egAtk", 0.004f }, { "egDec", 0.35f }, { "attack", 0.004f }, { "unison", 1 } }),
                                          L (2, { { "feet", 0 }, { "saw", 0.0f }, { "sine", 0.8f }, { "level", 0.4f } }),
                                          G ({ { "drive", 0.7f }, { "driveTone", 0.35f }, { "swell", 0.0f }, { "vibDepth", 0.0f }, { "ensMix", 0.0f }, { "roomMix", 0.05f } }) }) },
        { "Overdriven Organ", "Driven", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 0.9f }, { "attack", 0.01f }, { "sustain", 1.0f }, { "release", 0.15f }, { "lpf", 6000 } }),
                                                L (2, { { "feet", 2 }, { "saw", 0.0f }, { "sine", 0.6f }, { "attack", 0.01f }, { "sustain", 1.0f }, { "release", 0.15f } }),
                                                G ({ { "drive", 0.6f }, { "driveTone", 0.5f }, { "swell", 0.0f }, { "ensMode", 1 }, { "ensMix", 0.6f }, { "roomMix", 0.2f } }) }) },
        // ---------------- Echoes
        { "Echo Pluck", "Echoes", join ({ L (1, { { "attack", 0.002f }, { "decay", 0.45f }, { "sustain", 0.0f }, { "release", 0.4f }, { "egAtk", 0.002f }, { "egDec", 0.3f }, { "egDepth", 3.5f } }),
                                          L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "echoMix", 0.45f }, { "echoTime", 2 }, { "echoFb", 0.55f }, { "roomType", 1 }, { "roomMix", 0.25f } }) }) },
        { "Dub Chords", "Echoes", join ({ L (1, { { "saw", 0.4f }, { "pulse", 0.5f }, { "attack", 0.003f }, { "decay", 0.25f }, { "sustain", 0.0f }, { "release", 0.2f }, { "hpf", 250 }, { "lpf", 1600 } }),
                                          L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "echoMix", 0.55f }, { "echoTime", 3 }, { "echoFb", 0.7f }, { "echoTone", 0.25f }, { "roomType", 2 }, { "roomMix", 0.3f } }) }) },
        { "Dotted Lead", "Echoes", join ({ L (1, { { "lpf", 2200 }, { "attack", 0.01f }, { "unison", 2 } }), L (2, { { "on", 0 } }),
                                           G ({ { "vibDepth", 0.2f }, { "echoMix", 0.4f }, { "echoTime", 2 }, { "echoFb", 0.5f }, { "roomType", 1 }, { "roomMix", 0.25f } }) }) },
        { "Infinite Bells", "Echoes", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 1.0f }, { "attack", 0.002f }, { "decay", 2.5f }, { "sustain", 0.0f }, { "release", 2.0f } }),
                                              L (2, { { "feet", 2 }, { "fine", 25 }, { "saw", 0.0f }, { "sine", 1.0f }, { "attack", 0.002f }, { "decay", 1.5f }, { "sustain", 0.0f } }),
                                              G ({ { "ring", 0.6f }, { "swell", 0.0f }, { "echoMix", 0.5f }, { "echoTime", 4 }, { "echoFb", 0.8f }, { "roomType", 4 }, { "roomMix", 0.45f } }) }) },
        // ---------------- More pads and leads
        { "Aurora", "Pads", join ({ L (1, { { "lpf", 1200 }, { "attack", 2.0f }, { "release", 5.0f } }), L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.5f }, { "level", 0.3f } }),
                                     G ({ { "prismMix", 0.3f }, { "echoMix", 0.25f }, { "echoTime", 5 }, { "echoFb", 0.6f }, { "roomType", 4 }, { "roomMix", 0.55f }, { "roomSize", 0.9f } }) }) },
        { "Velvet Night", "Pads", join ({ L (1, { { "lpf", 650 }, { "attack", 1.6f }, { "release", 3.5f }, { "detune", 18 } }), L (2, { { "feet", 0 }, { "level", 0.45f } }),
                                           G ({ { "drive", 0.2f }, { "tapeWow", 0.2f }, { "ensMix", 0.6f }, { "roomType", 1 }, { "roomMix", 0.45f } }) }) },
        { "Silk Flute", "Leads", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 0.9f }, { "noise", 0.12f }, { "lpf", 2500 }, { "attack", 0.08f } }), L (2, { { "on", 0 } }),
                                         G ({ { "vibDepth", 0.25f }, { "vibDelay", 0.3f }, { "echoMix", 0.2f }, { "echoTime", 2 }, { "roomType", 1 }, { "roomMix", 0.35f } }) }) },
        { "Chiptune Glide", "Leads", join ({ L (1, { { "saw", 0.0f }, { "pulse", 1.0f }, { "pw", 0.25f }, { "pwm", 0.0f }, { "lpf", 12000 }, { "attack", 0.002f }, { "unison", 1 } }),
                                             L (2, { { "on", 0 } }), G ({ { "swell", 0.0f }, { "vibDepth", 0.3f }, { "vibDelay", 0.2f }, { "ensMix", 0.0f }, { "echoMix", 0.3f }, { "echoTime", 1 }, { "roomMix", 0.1f } }) }) },
        // ---------------- Aero (glossy, bright, watery: Frutiger Aero)
        { "Aqua Glass Pad", "Aero", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.3f }, { "pw", 0.2f }, { "sine", 0.8f }, { "lpf", 4000 }, { "attack", 0.9f }, { "release", 3.0f } }),
                                             L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.7f }, { "level", 0.3f }, { "attack", 1.2f } }),
                                             G ({ { "ring", 0.1f }, { "ensMode", 2 }, { "ensMix", 0.45f }, { "prismMix", 0.2f }, { "prismI1", 8 }, { "roomType", 4 }, { "roomMix", 0.5f }, { "roomSize", 0.8f } }) }) },
        { "Bubble Bells", "Aero", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 1.0f }, { "attack", 0.001f }, { "decay", 1.1f }, { "sustain", 0.0f }, { "release", 1.2f } }),
                                           L (2, { { "feet", 3 }, { "fine", 12 }, { "saw", 0.0f }, { "sine", 0.6f }, { "attack", 0.001f }, { "decay", 0.25f }, { "sustain", 0.0f } }),
                                           G ({ { "ring", 0.3f }, { "swell", 0.0f }, { "startFrom", 1 }, { "startDepth", 2.0f }, { "echoMix", 0.3f }, { "echoTime", 3 }, { "echoFb", 0.45f }, { "echoTone", 0.7f },
                                                { "roomType", 4 }, { "roomMix", 0.45f } }) }) },
        { "Sky Chimes", "Aero", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 1.0f }, { "attack", 0.002f }, { "decay", 2.8f }, { "sustain", 0.0f }, { "release", 2.5f } }),
                                         L (2, { { "feet", 3 }, { "fine", 31 }, { "saw", 0.0f }, { "sine", 0.8f }, { "attack", 0.002f }, { "decay", 1.6f }, { "sustain", 0.0f }, { "level", 0.5f } }),
                                         G ({ { "ring", 0.45f }, { "swell", 0.0f }, { "prismMix", 0.35f }, { "prismFb", 0.5f }, { "roomType", 4 }, { "roomMix", 0.55f }, { "roomSize", 0.85f } }) }) },
        { "Vista Sunrise", "Aero", join ({ L (1, { { "saw", 0.5f }, { "pulse", 0.2f }, { "lpf", 1800 }, { "attack", 1.8f }, { "release", 4.0f }, { "detune", 12 } }),
                                            L (2, { { "feet", 2 }, { "saw", 0.0f }, { "sine", 0.6f }, { "level", 0.35f } }),
                                            G ({ { "swell", 0.6f }, { "swellTime", 4.0f }, { "brighten", 0.6f }, { "ensMode", 2 }, { "ensMix", 0.55f }, { "roomType", 4 }, { "roomMix", 0.5f }, { "roomSize", 0.85f } }) }) },
        { "Crystal Marimba", "Aero", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.25f }, { "pw", 0.5f }, { "sine", 0.9f }, { "attack", 0.001f }, { "decay", 0.7f }, { "sustain", 0.0f }, { "release", 0.6f }, { "lpf", 3500 } }),
                                              L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.4f }, { "attack", 0.001f }, { "decay", 0.12f }, { "sustain", 0.0f } }),
                                              G ({ { "swell", 0.0f }, { "vibDepth", 0.0f }, { "roomType", 2 }, { "roomMix", 0.35f }, { "echoMix", 0.15f }, { "echoTime", 2 } }) }) },
        { "Dolphin Lead", "Aero", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 0.9f }, { "noise", 0.03f }, { "lpf", 5000 }, { "attack", 0.03f }, { "unison", 1 } }),
                                           L (2, { { "feet", 2 }, { "saw", 0.0f }, { "sine", 0.3f }, { "level", 0.25f } }),
                                           G ({ { "glideMs", 260 }, { "startFrom", 0 }, { "startDepth", 5.0f }, { "releaseBend", 7.0f }, { "vibDepth", 0.3f }, { "vibDelay", 0.15f },
                                                { "echoMix", 0.3f }, { "echoTime", 2 }, { "echoFb", 0.4f }, { "roomType", 4 }, { "roomMix", 0.4f } }) }) },
        { "Ocean Air", "Aero", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.0f }, { "sine", 0.6f }, { "noise", 0.35f }, { "hpf", 180 }, { "lpf", 1400 }, { "lpRes", 2.0f }, { "attack", 2.5f }, { "release", 3.0f } }),
                                        L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.5f }, { "level", 0.3f }, { "attack", 3.0f } }),
                                        G ({ { "mx1Src", 1 }, { "mx1Dst", 2 }, { "mx1Amt", 0.3f }, { "m1Rate", 0.08f }, { "drift", 0.3f }, { "roomType", 4 }, { "roomMix", 0.6f }, { "roomSize", 0.9f } }) }) },
        { "Glossy Keys", "Aero", join ({ L (1, { { "saw", 0.0f }, { "pulse", 0.2f }, { "sine", 0.9f }, { "attack", 0.003f }, { "decay", 2.2f }, { "sustain", 0.25f }, { "release", 1.2f }, { "lpf", 2600 },
                                                  { "egAtk", 0.003f }, { "egDec", 0.8f }, { "egDepth", 1.5f } }),
                                          L (2, { { "feet", 3 }, { "saw", 0.0f }, { "sine", 0.45f }, { "attack", 0.002f }, { "decay", 0.5f }, { "sustain", 0.0f }, { "level", 0.35f } }),
                                          G ({ { "ring", 0.12f }, { "swell", 0.0f }, { "ensMode", 1 }, { "ensMix", 0.4f }, { "roomType", 2 }, { "roomMix", 0.35f } }) }) },
    };
    return list;
}

const std::vector<Preset>& glidePresets()
{
    // curve: 0 Lin 1 Exp 2 S 3 Elastic 4 Stairs | path: 0 Direct 1 ArcUp 2 ArcDown 3 Zigzag 4 Swirl
    // startFrom: 0 Below 1 Above 2 Random 3 Centre 4 Outside 5 Alt | staggerOrd: 0 LowHigh 1 HighLow 2 OutsideIn 3 Random 4 Shortest
    // pilot: 0 Off 1 Octave 2 ScaleWalk 3 Arp 4 Bounce 5 Weave 6 Breathe 7 Wander 8 Mirror 9 Sequence 10 RandomLoop
    static const std::vector<Preset> list = {
        { "Smooth Morph", "Glide", {} },
        { "Quick Slide", "Glide", G ({ { "glideMs", 120 }, { "curve", 1 } }) },
        { "Slow Motion", "Glide", G ({ { "glideMs", 1800 }, { "curve", 2 }, { "glideSpread", 0.3f } }) },
        { "Elastic Snap", "Glide", G ({ { "glideMs", 380 }, { "curve", 3 } }) },
        { "Semitone Stairs", "Glide", G ({ { "glideMode", 1 }, { "rateMs", 90 }, { "curve", 4 } }) },
        { "Rate Glide", "Glide", G ({ { "glideMode", 1 }, { "rateMs", 45 }, { "curve", 0 } }) },
        { "Staggered Rise", "Glide", G ({ { "staggerMs", 90 }, { "staggerOrd", 0 }, { "glideMs", 450 } }) },
        { "Waterfall", "Glide", G ({ { "staggerMs", 130 }, { "staggerOrd", 1 }, { "glideMs", 600 } }) },
        { "Closing In", "Glide", G ({ { "staggerMs", 110 }, { "staggerOrd", 2 }, { "glideMs", 700 } }) },
        { "Arc Up", "Glide", G ({ { "path", 1 }, { "pathDepth", 4 }, { "glideMs", 900 } }) },
        { "Arc Down", "Glide", G ({ { "path", 2 }, { "pathDepth", 4 }, { "glideMs", 900 } }) },
        { "Zigzag Lines", "Glide", G ({ { "path", 3 }, { "pathDepth", 2 }, { "glideMs", 1000 } }) },
        { "Swirl", "Glide", G ({ { "path", 4 }, { "pathDepth", 3 }, { "glideMs", 1200 } }) },
        { "Rise From Below", "Glide", G ({ { "newVoices", 1 }, { "startFrom", 0 }, { "startDepth", 7 } }) },
        { "Fall From Above", "Glide", G ({ { "newVoices", 1 }, { "startFrom", 1 }, { "startDepth", 12 } }) },
        { "Bloom From Centre", "Glide", G ({ { "newVoices", 1 }, { "startFrom", 3 }, { "startDepth", 12 }, { "glideMs", 900 } }) },
        { "Open Outward", "Glide", G ({ { "newVoices", 1 }, { "startFrom", 4 }, { "startDepth", 5 }, { "glideMs", 500 } }) },
        { "Classic Leading", "Glide", G ({ { "leading", 1 }, { "newVoices", 1 } }) },
        { "Legato Only", "Glide", G ({ { "lineMode", 1 }, { "glideMs", 250 } }) },
        { "Long Memory", "Glide", G ({ { "lineMemory", 12 }, { "glideMs", 800 } }) },
        { "Release Fall", "Glide", G ({ { "releaseBend", -12 } }) },
        { "Release Rise", "Glide", G ({ { "releaseBend", 7 } }) },
        { "No Merging", "Glide", G ({ { "merge", 0 }, { "newVoices", 1 } }) },
        { "Octave Leaper", "Autopilot", G ({ { "pilot", 1 }, { "pilotRate", 0.8f }, { "pilotDensity", 0.5f }, { "pilotRange", 12 }, { "pilotSmooth", 0.4f } }) },
        { "Scale Walker", "Autopilot", G ({ { "pilot", 2 }, { "pilotSync", 4 }, { "pilotDensity", 0.6f }, { "pilotScale", 1 }, { "pilotSmooth", 0.3f } }) },
        { "Glide Arpeggio", "Autopilot", G ({ { "pilot", 3 }, { "pilotSync", 4 }, { "pilotDensity", 1.0f }, { "pilotSmooth", 0.25f } }) },
        { "Bouncing Voices", "Autopilot", G ({ { "pilot", 4 }, { "pilotRate", 2.0f }, { "pilotRange", 7 }, { "pilotSpread", 0.6f } }) },
        { "Weaving Lines", "Autopilot", G ({ { "pilot", 5 }, { "pilotRate", 0.6f }, { "pilotSmooth", 0.8f }, { "pilotDensity", 1.0f } }) },
        { "Breathing Chord", "Autopilot", G ({ { "pilot", 6 }, { "pilotRate", 0.25f }, { "pilotSmooth", 0.9f }, { "pilotDensity", 1.0f } }) },
        { "Free Wander", "Autopilot", G ({ { "pilot", 7 }, { "pilotRate", 0.4f }, { "pilotRange", 7 }, { "pilotScale", 0 }, { "pilotRandom", 0.6f } }) },
        { "Mirror World", "Autopilot", G ({ { "pilot", 8 }, { "pilotRate", 0.5f }, { "pilotSmooth", 0.6f }, { "pilotDensity", 1.0f } }) },
        { "Step Sequence", "Autopilot", G ({ { "pilot", 9 }, { "pilotSync", 5 }, { "pilotSmooth", 0.15f }, { "pilotDensity", 1.0f } }) },
        { "Evolving Loop", "Autopilot", G ({ { "pilot", 10 }, { "pilotSync", 4 }, { "pilotRange", 12 }, { "pilotScale", 7 }, { "pilotSmooth", 0.3f } }) },
        { "Rubber Band", "Glide", G ({ { "glideMs", 520 }, { "curve", 3 }, { "path", 1 }, { "pathDepth", 2 } }) },
        { "Lazy River", "Glide", G ({ { "glideMs", 2600 }, { "curve", 2 }, { "glideSpread", 0.5f }, { "path", 4 }, { "pathDepth", 1.5f } }) },
        { "Sharp Turns", "Glide", G ({ { "glideMs", 60 }, { "curve", 1 }, { "staggerMs", 25 }, { "staggerOrd", 3 } }) },
        { "Dive Bomb", "Glide", G ({ { "newVoices", 1 }, { "startFrom", 1 }, { "startDepth", 24 }, { "glideMs", 400 }, { "curve", 1 } }) },
        { "Tidal", "Glide", G ({ { "path", 3 }, { "pathDepth", 5 }, { "glideMs", 1600 }, { "releaseBend", -5 } }) },
        { "Fifths Walker", "Autopilot", G ({ { "pilot", 2 }, { "pilotSync", 3 }, { "pilotScale", 8 }, { "pilotDensity", 0.7f }, { "pilotSmooth", 0.4f } }) },
        { "Octave Rain", "Autopilot", G ({ { "pilot", 3 }, { "pilotSync", 5 }, { "pilotScale", 9 }, { "pilotRange", 24 }, { "pilotSmooth", 0.2f } }) },
        { "Sleepy Drift", "Autopilot", G ({ { "pilot", 7 }, { "pilotRate", 0.15f }, { "pilotRange", 5 }, { "pilotScale", 6 }, { "pilotSmooth", 0.95f } }) },
    };
    return list;
}


} // namespace glidecat
