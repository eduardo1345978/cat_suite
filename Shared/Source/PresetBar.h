#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CatLookAndFeel.h"
#include "CatLink.h"

namespace cat
{
/**
    Factory + user presets and favourites, shared by every Cat.

    User presets are the plug-in's complete state (getStateInformation as XML) saved to
    <user app data>/CatSuite/<Plug-in>/Presets/<name>.catpreset. Favourites are kept in
    Favourites.txt next to them ("F:<factory name>" / "U:<file name>").
    Owned by the processor so it survives closing the editor.
*/
class PresetManager
{
public:
    struct Factory
    {
        std::function<juce::StringArray()> names;
        std::function<void (int)> apply;
        std::function<int()> current;     // -1 = none
    };

    PresetManager (juce::AudioProcessor& processor, const juce::String& pluginName, Factory factory,
                   juce::StringArray extraUserExtensions = {});

    juce::File folder() const;
    juce::Array<juce::File> userPresets() const;
    juce::StringArray factoryNames() const { return factory.names ? factory.names() : juce::StringArray(); }

    void applyFactory (int index);
    bool saveUser (const juce::String& name);
    bool loadUser (const juce::File& file);
    bool deleteUser (const juce::File& file);

    /** Name of what is loaded now (user preset, else factory, else empty). */
    juce::String currentName() const;
    juce::String currentKey() const;       // favourites key of the current preset
    juce::File currentUserFile() const;

    bool isFavourite (const juce::String& key) const { return favourites.contains (key); }
    void toggleFavourite (const juce::String& key);
    static juce::String factoryKey (const juce::String& name) { return "F:" + name; }
    static juce::String userKey (const juce::File& f) { return "U:" + f.getFileNameWithoutExtension(); }

    /** Every preset in browsing order (favourites-only optional): factory first, then user. */
    struct Item { juce::String name, key; int factoryIndex; juce::File file; };
    std::vector<Item> items (bool favouritesOnly) const;
    void step (int delta, bool favouritesOnly);

    bool favouritesOnly = false;

    /** Chain presets: load this Cat's part of a chain; broadcast = tell the other Cats too. */
    bool applyChain (const juce::String& chainName, bool broadcast);
    juce::String lastChain;
    bool followChains = true;           // accept chains picked in other Cats

    CatLink link;                       // shared with the other Cats in the host (chains, groove)
    const juce::String& pluginName() const { return plugin; }

private:
    void loadFavourites();
    void saveFavourites() const;

    juce::AudioProcessor& processor;
    juce::String plugin;
    Factory factory;
    juce::StringArray extensions;
    juce::StringArray favourites;
    juce::File loadedUser;
    int factoryAtUserLoad = -2;
};

/**
    "Follow Drum": when the bool parameter "followGroove" is on, every groove Drum-Cat publishes
    (and the latest one at the moment it is switched on) is handed to apply() on the message thread.
*/
inline void followDrumGroove (PresetManager& manager, juce::AudioProcessorValueTreeState& state,
                              std::function<void (const CatLink::Groove&)> apply)
{
    auto* follow = state.getRawParameterValue ("followGroove");
    auto wasOn = std::make_shared<bool> (false);
    auto& link = manager.link;
    link.onGroove = [follow, apply] (const CatLink::Groove& g) { if (follow->load() > 0.5f) apply (g); };
    link.onTick = [follow, apply, wasOn, &link]
    {
        const bool on = follow->load() > 0.5f;
        if (on && ! *wasOn && link.latestGroove().valid)
            apply (link.latestGroove());
        *wasOn = on;
    };
}

/** Compact preset bar: [star] [<] [ name v ] [>] [Save]. */
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PresetManager& manager, const juce::String& caption = "Preset");
    ~PresetBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showMenu();
    void askSaveName();

    PresetManager& manager;
    juce::String caption;
    juce::TextButton star { juce::String::fromUTF8 ("\xe2\x98\x86") }, prev { "<" }, next { ">" }, name, save { "Save" };
    juce::String shownName;
    std::unique_ptr<juce::AlertWindow> saveWindow;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace cat
