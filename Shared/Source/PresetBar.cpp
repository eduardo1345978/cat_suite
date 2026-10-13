#include "PresetBar.h"

namespace cat
{
PresetManager::PresetManager (juce::AudioProcessor& p, const juce::String& pluginName, Factory f, juce::StringArray extraUserExtensions)
    : link (pluginName), processor (p), plugin (pluginName), factory (std::move (f)), extensions (std::move (extraUserExtensions))
{
    extensions.addIfNotAlreadyThere ("catpreset");
    loadFavourites();
    link.onChain = [this] (const juce::String& chain) { if (followChains) applyChain (chain, false); };
}

bool PresetManager::applyChain (const juce::String& chainName, bool broadcast)
{
    lastChain = chainName;
    if (broadcast)
        link.publishChain (chainName);
    const auto part = chainPartFor (chainName, plugin);
    const int index = factoryNames().indexOf (part);
    if (part.isEmpty() || index < 0)
        return false;
    applyFactory (index);
    return true;
}

juce::File PresetManager::folder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("CatSuite").getChildFile (plugin).getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::Array<juce::File> PresetManager::userPresets() const
{
    juce::Array<juce::File> files;
    for (const auto& ext : extensions)
        files.addArray (folder().findChildFiles (juce::File::findFiles, false, "*." + ext));
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
    });
    return files;
}

void PresetManager::applyFactory (int index)
{
    loadedUser = juce::File();
    if (factory.apply) factory.apply (index);
    // Let the host (Cubase's preset field, a test host...) know the program changed
    processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
}

bool PresetManager::saveUser (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    juce::MemoryBlock data;
    processor.getStateInformation (data);
    auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), (int) data.getSize());
    if (xml == nullptr) return false;
    const auto file = folder().getChildFile (clean + ".catpreset");
    if (! xml->writeTo (file)) return false;
    loadedUser = file;
    factoryAtUserLoad = factory.current ? factory.current() : -1;
    return true;
}

bool PresetManager::loadUser (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary (*xml, data);
    processor.setStateInformation (data.getData(), (int) data.getSize());
    loadedUser = file;
    factoryAtUserLoad = factory.current ? factory.current() : -1;
    return true;
}

bool PresetManager::deleteUser (const juce::File& file)
{
    favourites.removeString (userKey (file));
    saveFavourites();
    if (file == loadedUser) loadedUser = juce::File();
    return file.moveToTrash();
}

juce::File PresetManager::currentUserFile() const
{
    // A user preset stays "current" until a factory preset is picked (here or by the host)
    const int f = factory.current ? factory.current() : -1;
    return loadedUser.existsAsFile() && f == factoryAtUserLoad ? loadedUser : juce::File();
}

juce::String PresetManager::currentName() const
{
    if (const auto u = currentUserFile(); u != juce::File())
        return u.getFileNameWithoutExtension();
    const int f = factory.current ? factory.current() : -1;
    const auto names = factoryNames();
    return juce::isPositiveAndBelow (f, names.size()) ? names[f] : juce::String();
}

juce::String PresetManager::currentKey() const
{
    if (const auto u = currentUserFile(); u != juce::File())
        return userKey (u);
    const auto n = currentName();
    return n.isEmpty() ? juce::String() : factoryKey (n);
}

void PresetManager::toggleFavourite (const juce::String& key)
{
    if (key.isEmpty()) return;
    if (favourites.contains (key)) favourites.removeString (key);
    else favourites.add (key);
    saveFavourites();
}

std::vector<PresetManager::Item> PresetManager::items (bool favOnly) const
{
    std::vector<Item> list;
    const auto names = factoryNames();
    for (int i = 0; i < names.size(); ++i)
        if (! favOnly || isFavourite (factoryKey (names[i])))
            list.push_back ({ names[i], factoryKey (names[i]), i, {} });
    for (const auto& f : userPresets())
        if (! favOnly || isFavourite (userKey (f)))
            list.push_back ({ f.getFileNameWithoutExtension(), userKey (f), -1, f });
    return list;
}

void PresetManager::step (int delta, bool favOnly)
{
    const auto list = items (favOnly);
    if (list.empty()) return;
    const auto key = currentKey();
    int index = -1;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].key == key) index = i;
    index = index < 0 ? (delta > 0 ? 0 : (int) list.size() - 1)
                      : (index + delta + (int) list.size()) % (int) list.size();
    const auto& item = list[(size_t) index];
    if (item.factoryIndex >= 0) applyFactory (item.factoryIndex);
    else loadUser (item.file);
}

void PresetManager::loadFavourites()
{
    favourites.clear();
    const auto f = folder().getChildFile ("Favourites.txt");
    if (f.existsAsFile())
        favourites.addLines (f.loadFileAsString());
    favourites.removeEmptyStrings();
}

void PresetManager::saveFavourites() const
{
    folder().getChildFile ("Favourites.txt").replaceWithText (favourites.joinIntoString ("\n"));
}

//==============================================================================
PresetBar::PresetBar (PresetManager& m, const juce::String& c) : manager (m), caption (c)
{
    star.setTooltip ("Add / remove this preset from your favourites");
    star.onClick = [this] { manager.toggleFavourite (manager.currentKey()); timerCallback(); };
    prev.onClick = [this] { manager.step (-1, manager.favouritesOnly); timerCallback(); };
    next.onClick = [this] { manager.step (1, manager.favouritesOnly); timerCallback(); };
    name.setTooltip ("Presets: factory, yours and favourites");
    name.onClick = [this] { showMenu(); };
    save.setTooltip ("Save everything as your own preset");
    save.onClick = [this] { askSaveName(); };
    for (auto* b : { &star, &prev, &next, &name, &save })
        addAndMakeVisible (b);
    startTimerHz (8);
    timerCallback();
}

PresetBar::~PresetBar()
{
    if (saveWindow != nullptr) saveWindow->exitModalState (0);
}

void PresetBar::paint (juce::Graphics& g)
{
    if (caption.isNotEmpty())
    {
        g.setColour (Colours::textDim);
        g.setFont (catFont (12.0f));
        g.drawText (caption, getLocalBounds().removeFromLeft (52).reduced (4, 0), juce::Justification::centredLeft);
    }
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty()) r.removeFromLeft (52);
    star.setBounds (r.removeFromLeft (r.getHeight()).reduced (1));
    save.setBounds (r.removeFromRight (46).reduced (1));
    const bool arrows = r.getWidth() >= 150;     // narrow bars drop the arrows (the menu still has everything)
    prev.setVisible (arrows);
    next.setVisible (arrows);
    if (arrows)
    {
        next.setBounds (r.removeFromRight (24).reduced (1));
        prev.setBounds (r.removeFromLeft (24).reduced (1));
    }
    name.setBounds (r.reduced (1));
}

void PresetBar::timerCallback()
{
    auto n = manager.currentName();
    if (n.isEmpty()) n = "-";
    if (manager.favouritesOnly) n = juce::String::fromUTF8 ("\xe2\x98\x85 ") + n;
    if (n != shownName)
    {
        shownName = n;
        name.setButtonText (n);
    }
    const bool fav = manager.isFavourite (manager.currentKey());
    const auto starText = juce::String::fromUTF8 (fav ? "\xe2\x98\x85" : "\xe2\x98\x86");
    if (star.getButtonText() != starText)
        star.setButtonText (starText);
    star.setColour (juce::TextButton::textColourOffId, fav ? Colours::accent : Colours::textDim);
}

void PresetBar::showMenu()
{
    juce::PopupMenu menu, factoryMenu, userMenu, favMenu;
    const auto names = manager.factoryNames();
    const auto key = manager.currentKey();
    int id = 1;
    std::vector<std::function<void()>> actions;
    auto add = [&] (juce::PopupMenu& m, const juce::String& text, bool ticked, std::function<void()> fn)
    {
        actions.push_back (std::move (fn));
        m.addItem (id++, text, true, ticked);
    };

    for (const auto& item : manager.items (true))
    {
        const auto it = item;
        add (favMenu, it.name, it.key == key, [this, it] { if (it.factoryIndex >= 0) manager.applyFactory (it.factoryIndex); else manager.loadUser (it.file); });
    }
    for (int i = 0; i < names.size(); ++i)
    {
        const auto fav = manager.isFavourite (PresetManager::factoryKey (names[i]));
        add (factoryMenu, juce::String (i + 1) + ". " + names[i] + (fav ? juce::String::fromUTF8 ("  \xe2\x98\x85") : juce::String()),
             PresetManager::factoryKey (names[i]) == key, [this, i] { manager.applyFactory (i); });
    }
    const auto users = manager.userPresets();
    for (const auto& f : users)
    {
        const auto fav = manager.isFavourite (PresetManager::userKey (f));
        add (userMenu, f.getFileNameWithoutExtension() + (fav ? juce::String::fromUTF8 ("  \xe2\x98\x85") : juce::String()),
             PresetManager::userKey (f) == key, [this, f] { manager.loadUser (f); });
    }

    menu.addSectionHeader ("Presets");
    menu.addSubMenu (juce::String::fromUTF8 ("\xe2\x98\x85 Favourites"), favMenu, favMenu.getNumItems() > 0);
    menu.addSubMenu ("Factory", factoryMenu, factoryMenu.getNumItems() > 0);
    menu.addSubMenu ("My Presets", userMenu, userMenu.getNumItems() > 0);
    juce::PopupMenu chainMenu;
    for (const auto& c : chainPresets())
    {
        const juce::String chainName (c.name);
        const auto part = chainPartFor (chainName, manager.pluginName());
        add (chainMenu, chainName + (part.isNotEmpty() ? "   (" + part + ")" : juce::String ("   (other Cats only)")),
             chainName == manager.lastChain, [this, chainName] { manager.applyChain (chainName, true); });
    }
    chainMenu.addSeparator();
    add (chainMenu, "Follow chains picked in other Cats", manager.followChains, [this] { manager.followChains = ! manager.followChains; });
    menu.addSubMenu ("Chains (all Cats at once)", chainMenu);
    menu.addSeparator();
    add (menu, "Save as...", false, [this] { askSaveName(); });
    if (const auto u = manager.currentUserFile(); u != juce::File())
        add (menu, "Delete \"" + u.getFileNameWithoutExtension() + "\"", false, [this, u] { manager.deleteUser (u); });
    add (menu, "Arrows step through favourites only", manager.favouritesOnly, [this] { manager.favouritesOnly = ! manager.favouritesOnly; });
    add (menu, "Open presets folder", false, [this] { manager.folder().startAsProcess(); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name),
                        [this, actions] (int result)
                        {
                            if (result > 0 && result <= (int) actions.size())
                                actions[(size_t) result - 1]();
                            timerCallback();
                        });
}

void PresetBar::askSaveName()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon, this);
    auto current = manager.currentName();
    saveWindow->addTextEditor ("name", current.isEmpty() ? juce::String ("My Preset") : current + " (mine)");
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
    {
        if (safe == nullptr || safe->saveWindow == nullptr) return;
        const auto text = safe->saveWindow->getTextEditorContents ("name");
        safe->saveWindow.reset();
        if (result == 1)
            safe->manager.saveUser (text);
        safe->timerCallback();
    }), false);
}

} // namespace cat
