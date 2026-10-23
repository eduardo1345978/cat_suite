#include "Engines.h"
#include "BoxModel.h"

namespace catbox
{
namespace engine
{
namespace
{
    int presetIndex (const std::vector<glidecat::Preset>& list, const char* name)
    {
        for (size_t i = 0; i < list.size(); ++i)
            if (juce::String (list[i].name) == name) return (int) i;
        return 0;
    }

    /** The glide presets that may be used (the Autopilot ones play by themselves: left out). */
    const std::vector<int>& motionIndices()
    {
        static const std::vector<int> list = []
        {
            std::vector<int> v;
            const auto& all = glidecat::glidePresets();
            for (size_t i = 0; i < all.size(); ++i)
                if (juce::String (all[i].category) != "Autopilot") v.push_back ((int) i);
            return v;
        }();
        return list;
    }

    void resetIds (EngineState& e, const juce::StringArray& ids)
    {
        const auto& ps = glidecat::params();
        for (const auto& id : ids)
            if (const int i = glidecat::paramIndex (id); i >= 0) e.v[(size_t) i] = ps[(size_t) i].def;
    }
}

void initGlide (EngineState& e)
{
    const auto& ps = glidecat::params();
    for (size_t i = 0; i < ps.size() && i < e.v.size(); ++i) e.v[i] = ps[i].def;
    e.sound = 0;    // the first factory sound is the plain default sound
    e.motion = 0;
    e.edited = false;
}

void loadGlideSound (EngineState& e, int preset)
{
    const auto& list = glidecat::soundPresets();
    preset = juce::jlimit (0, (int) list.size() - 1, preset);
    resetIds (e, glidecat::soundParamIds());
    for (const auto& [id, v] : list[(size_t) preset].values) set (e, id, v);
    e.sound = preset;
    e.edited = false;
}

void loadGlideMotion (EngineState& e, int preset)
{
    const auto& idx = motionIndices();
    preset = juce::jlimit (0, (int) idx.size() - 1, preset);
    resetIds (e, glidecat::glideParamIds());
    for (const auto& [id, v] : glidecat::glidePresets()[(size_t) idx[(size_t) preset]].values) set (e, id, v);
    e.motion = preset;
}

EngineState glideDefaults (int instrument)
{
    EngineState e;
    initGlide (e);
    const auto& motions = motionIndices();
    auto motion = [&motions] (const char* name)
    {
        const int all = presetIndex (glidecat::glidePresets(), name);
        for (size_t i = 0; i < motions.size(); ++i) if (motions[i] == all) return (int) i;
        return 0;
    };
    if (instrument == Bass)
    {
        loadGlideSound (e, presetIndex (glidecat::soundPresets(), "Glide Bass"));
        loadGlideMotion (e, motion ("Legato Only"));   // like the acid: only notes that slide glide
        set (e, "glideMs", 90.0f);
    }
    else
    {
        loadGlideSound (e, presetIndex (glidecat::soundPresets(), "Glossy Keys"));
        loadGlideMotion (e, motion ("Quick Slide"));
    }
    e.edited = false;
    return e;
}

juce::StringArray glideSoundNames()
{
    juce::StringArray n;
    for (const auto& p : glidecat::soundPresets()) n.add (p.name);
    return n;
}

juce::StringArray glideMotionNames()
{
    juce::StringArray n;
    for (int i : motionIndices()) n.add (glidecat::glidePresets()[(size_t) i].name);
    return n;
}

float get (const EngineState& e, const juce::String& id)
{
    const int i = glidecat::paramIndex (id);
    return i >= 0 ? e.v[(size_t) i] : 0.0f;
}

void set (EngineState& e, const juce::String& id, float value)
{
    const int i = glidecat::paramIndex (id);
    if (i < 0) return;
    const auto& p = glidecat::params()[(size_t) i];
    value = juce::jlimit (p.lo, p.hi, value);
    e.v[(size_t) i] = p.type == glidecat::ParamInfo::Float ? value : (float) juce::roundToInt (value);
}

void toSettings (const EngineState& e, glidecat::Settings& s, glidecat::FxSettings& f)
{
    glidecat::readSettings ([&e] (const char* id) { return get (e, id); }, s, f);
    s.pilot = glidecat::Pilot::Off;   // nothing plays by itself
}

juce::var toVar (const EngineState& e)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("sound", e.sound);
    o->setProperty ("motion", e.motion);
    o->setProperty ("edited", e.edited);
    auto* vals = new juce::DynamicObject();
    const auto& ps = glidecat::params();
    for (size_t i = 0; i < ps.size(); ++i)
        if (e.v[i] != ps[i].def) vals->setProperty (ps[i].id, e.v[i]);   // only what differs from the defaults
    o->setProperty ("values", juce::var (vals));
    return juce::var (o);
}

void fromVar (const juce::var& v, EngineState& e)
{
    if (! v.isObject()) return;
    initGlide (e);
    e.sound = juce::jlimit (0, (int) glidecat::soundPresets().size() - 1, (int) v.getProperty ("sound", 0));
    e.motion = juce::jlimit (0, (int) motionIndices().size() - 1, (int) v.getProperty ("motion", 0));
    e.edited = (bool) v.getProperty ("edited", false);
    if (auto* vals = v["values"].getDynamicObject())
        for (const auto& kv : vals->getProperties())
            set (e, kv.name.toString(), (float) kv.value);
}

juce::String shortName (const glidecat::ParamInfo& p)
{
    if (p.name.startsWith ("Layer II ")) return p.name.substring (9);
    if (p.name.startsWith ("Layer I ")) return p.name.substring (8);
    return p.name;
}

const std::vector<Page>& glidePages()
{
    static const std::vector<Page> pages = []
    {
        std::vector<Page> v;
        auto page = [&v] (const char* name, std::initializer_list<const char*> ids)
        {
            Page p;
            p.name = name;
            int i = 0;
            for (auto* id : ids) if (i < 24) p.ids[(size_t) i++] = id;
            v.push_back (p);
        };
        auto layer = [] (int l)
        {
            const juce::String n = l == 1 ? "I" : "II";
            auto id = [l] (const char* k) { return juce::String ("l") + juce::String (l) + "_" + k; };
            Page osc, filt, amp;
            osc.name = "Osc " + n;
            filt.name = "Filter " + n;
            amp.name = "Amp " + n;
            const char* o[] = { "on", "feet", "unison", "saw", "pulse", "pw", "pwmRate", "pwm", "sine", "noise", "detune", "fine" };
            const char* f[] = { "hpf", "hpRes", "lpf", "lpRes", "key", "egDepth", "egIL", "egAL", "egAtk", "egDec", "egRel" };
            const char* a[] = { "attack", "decay", "sustain", "release", "level", "brill" };
            for (int i = 0; i < 12; ++i) osc.ids[(size_t) i] = id (o[i]);
            for (int i = 0; i < 11; ++i) filt.ids[(size_t) i] = id (f[i]);
            for (int i = 0; i < 6; ++i) amp.ids[(size_t) i] = id (a[i]);
            return std::array<Page, 3> { osc, filt, amp };
        };

        page ("Sound", { "@sound", "@preset", "@motion", "voices", "volume", "panSpread", "ring", "swell", "brighten", "drift", "vibDepth", "roomMix",
                         "@init" });
        for (int l = 1; l <= 2; ++l)
            for (auto& p : layer (l)) v.push_back (p);
        page ("Glide", { "glideMode", "glideMs", "rateMs", "curve", "glideSpread", "leading", "newVoices", "merge", "staggerMs", "staggerOrd", "path", "pathDepth",
                         "startFrom", "startDepth", "releaseBend", "lineMode", "lineMemory", "bendRange" });
        page ("Expression", { "swell", "swellTime", "brighten", "drift", "wander", "vibDepth", "vibRate", "vibDelay", "ring", "panSpread" });
        page ("Modulators", { "m1Type", "m1Mode", "m1Rate", "m2Type", "m2Mode", "m2Rate", "m3Type", "m3Mode", "m3Rate", "m4Type", "m4Mode", "m4Rate" });
        page ("Matrix", { "mx1Src", "mx1Dst", "mx1Amt", "mx2Src", "mx2Dst", "mx2Amt", "mx3Src", "mx3Dst", "mx3Amt", "mx4Src", "mx4Dst", "mx4Amt",
                          "mx5Src", "mx5Dst", "mx5Amt", "mx6Src", "mx6Dst", "mx6Amt", "mx7Src", "mx7Dst", "mx7Amt", "mx8Src", "mx8Dst", "mx8Amt" });
        page ("FX", { "ensMode", "ensMix", "ensRate", "ensDepth", "prismMix", "prismTime", "prismFb", "prismI1", "prismI2", "prismI3", "roomType", "roomMix",
                      "roomSize", "drive", "driveTone", "echoMix", "echoTime", "echoFb", "echoTone", "tapeWow", "tapeFlutter", "tapeAge" });
        return v;
    }();
    return pages;
}
} // namespace engine
} // namespace catbox
