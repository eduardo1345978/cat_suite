#include "CatLink.h"

namespace cat
{
CatLink::CatLink (juce::String pluginName) : plugin (std::move (pluginName)), instanceId (juce::Uuid().toString())
{
    // Start from what is already there: old chain messages are not replayed, the groove is kept
    if (auto xml = juce::XmlDocument::parse (linkFile()))
    {
        if (auto* c = xml->getChildByName ("CHAIN"))
            lastChainStamp = c->getStringAttribute ("stamp").getLargeIntValue();
        if (auto* g = xml->getChildByName ("GROOVE"))
        {
            groove.swing = (float) g->getDoubleAttribute ("swing");
            groove.human = (float) g->getDoubleAttribute ("human");
            groove.grid = g->getIntAttribute ("grid", 1);
            groove.stamp = lastGrooveStamp = g->getStringAttribute ("stamp").getLargeIntValue();
            groove.valid = true;
        }
    }
    startTimer (250);
}

CatLink::~CatLink()
{
    stopTimer();
}

juce::File CatLink::linkFile()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CatSuiteLink.xml");
}

void CatLink::write (const std::function<void (juce::XmlElement&)>& modify)
{
    juce::InterProcessLock lock ("CatSuiteLink");
    const juce::InterProcessLock::ScopedLockType scoped (lock);
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (linkFile()));
    if (xml == nullptr || ! xml->hasTagName ("CATLINK"))
        xml = std::make_unique<juce::XmlElement> ("CATLINK");
    modify (*xml);
    xml->writeTo (linkFile());
}

void CatLink::publishGroove (float swing, float human, int grid)
{
    const auto stamp = juce::Time::currentTimeMillis();
    groove = { swing, human, grid, stamp, true };
    lastGrooveStamp = stamp;
    write ([&] (juce::XmlElement& x)
    {
        auto* g = x.getChildByName ("GROOVE");
        if (g == nullptr) g = x.createNewChildElement ("GROOVE");
        g->setAttribute ("swing", swing);
        g->setAttribute ("human", human);
        g->setAttribute ("grid", grid);
        g->setAttribute ("stamp", juce::String (stamp));
        g->setAttribute ("source", instanceId);
    });
}

void CatLink::publishChain (const juce::String& chainName)
{
    const auto stamp = juce::Time::currentTimeMillis();
    lastChainStamp = stamp;
    write ([&] (juce::XmlElement& x)
    {
        auto* c = x.getChildByName ("CHAIN");
        if (c == nullptr) c = x.createNewChildElement ("CHAIN");
        c->setAttribute ("name", chainName);
        c->setAttribute ("stamp", juce::String (stamp));
        c->setAttribute ("source", instanceId);
    });
}

void CatLink::timerCallback()
{
    if (onTick) onTick();

    const auto f = linkFile();
    if (! f.existsAsFile())
        return;

    std::unique_ptr<juce::XmlElement> xml;
    {
        juce::InterProcessLock lock ("CatSuiteLink");
        const juce::InterProcessLock::ScopedLockType scoped (lock);
        xml = juce::XmlDocument::parse (f);
    }
    if (xml == nullptr) return;

    if (auto* g = xml->getChildByName ("GROOVE"))
    {
        const auto stamp = g->getStringAttribute ("stamp").getLargeIntValue();
        if (stamp > lastGrooveStamp)
        {
            lastGrooveStamp = stamp;
            groove = { (float) g->getDoubleAttribute ("swing"), (float) g->getDoubleAttribute ("human"),
                       g->getIntAttribute ("grid", 1), stamp, true };
            if (g->getStringAttribute ("source") != instanceId && onGroove)
                onGroove (groove);
        }
    }
    if (auto* c = xml->getChildByName ("CHAIN"))
    {
        const auto stamp = c->getStringAttribute ("stamp").getLargeIntValue();
        if (stamp > lastChainStamp)
        {
            lastChainStamp = stamp;
            if (c->getStringAttribute ("source") != instanceId && onChain)
                onChain (c->getStringAttribute ("name"));
        }
    }
}

//==============================================================================
const std::vector<ChainPreset>& chainPresets()
{
    static const std::vector<ChainPreset> chains = {
        { "Neo Soul Keys",    { { "Chord-Cat", "Neo Soul Glide" }, { "Patt-Cat", "Neo-Soul Keys" }, { "Bend-Cat", "Gentle Scoop" },
                                { "Drum-Cat", "Velvet Pocket" }, { "Bass-Cat", "Neo Soul Walk" }, { "Glide-Cat", "Velvet Keys" } } },
        { "Lo-Fi Study",      { { "Chord-Cat", "Study Session" }, { "Patt-Cat", "LoFi Swing" }, { "Bend-Cat", "Tape Wow" },
                                { "Drum-Cat", "Study Beats" }, { "Bass-Cat", "Lo-Fi Thump" }, { "Glide-Cat", "Dusty Pad" } } },
        { "Trap Night",       { { "Chord-Cat", "Dark Trap" }, { "Patt-Cat", "Trap Pluck" }, { "Bend-Cat", "808 Drop" },
                                { "Drum-Cat", "808 Rolls" }, { "Bass-Cat", "808 Glide" }, { "Glide-Cat", "Dark Glass" } } },
        { "Gospel Sunday",    { { "Chord-Cat", "Gospel Turnaround" }, { "Patt-Cat", "Gospel Shout" }, { "Bend-Cat", "Singer Vibrato" },
                                { "Drum-Cat", "Funky Drummer" }, { "Bass-Cat", "Gospel Walk" }, { "Sing-Cat", "Gospel Mass" }, { "Glide-Cat", "Soft Brass Swell" } } },
        { "Jazz Club",        { { "Chord-Cat", "Jazz 2-5-1" }, { "Patt-Cat", "Jazz Comp" }, { "Bend-Cat", "Analog Choir" },
                                { "Drum-Cat", "Late Night Soul" }, { "Bass-Cat", "Jazz Walking" } } },
        { "Dilla Dust",       { { "Chord-Cat", "Dilla Donuts" }, { "Patt-Cat", "Chillhop Keys" }, { "Bend-Cat", "Tape Wow" },
                                { "Drum-Cat", "Donut Shop" }, { "Bass-Cat", "Dilla Bounce" } } },
        { "Synthwave Drive",  { { "Chord-Cat", "Neon Drive" }, { "Patt-Cat", "Synthwave Octaves" }, { "Bend-Cat", "Portamento" },
                                { "Drum-Cat", "Motor City" }, { "Bass-Cat", "Synthwave Eighths" }, { "Orbit-Cat", "Steady Arp" }, { "Glide-Cat", "Neon Strings" } } },
        { "Cinematic",        { { "Chord-Cat", "Epic Trailer" }, { "Patt-Cat", "Cinematic Ostinato" }, { "Bend-Cat", "Vibrato Swell" },
                                { "Bass-Cat", "Cinematic Drone" }, { "Sing-Cat", "Requiem" }, { "Orbit-Cat", "Glacial" }, { "Glide-Cat", "Morphing Strings" } } },
        { "Pop Anthem",       { { "Chord-Cat", "Four Chord Anthem" }, { "Patt-Cat", "Pop Piano 8ths" }, { "Bend-Cat", "Guitar Bend" },
                                { "Drum-Cat", "Head Nod" }, { "Bass-Cat", "Pop Root Eighths" } } },
        { "Detroit Basement", { { "Chord-Cat", "Quiet Storm" }, { "Patt-Cat", "Deep Chord Echo" }, { "Bend-Cat", "Seasick" },
                                { "Drum-Cat", "Basement Jam" }, { "Bass-Cat", "Detroit Groove" } } },
    };
    return chains;
}

juce::String chainPartFor (const juce::String& chain, const juce::String& plugin)
{
    for (const auto& c : chainPresets())
        if (chain == c.name)
            for (const auto& [p, preset] : c.parts)
                if (plugin == p)
                    return preset;
    return {};
}

} // namespace cat
