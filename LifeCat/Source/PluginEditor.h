#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"

enum class GardenTool { Inspect = 0, Food, Grazer, Hunter, Drifter, Remove, Wall, Warm, Cold, Current, Erase };
constexpr int numGardenTools = 11;

/** What the screen currently shows (a copy of the world from the audio thread + UI state). */
struct LifeViewState
{
    lifecat::World world;
    bool valid = false, singing = false, running = true;
    double ppq = 0.0;
    int theme = 0;
    uint32_t selected = 0;
    bool pairing = false;
    GardenTool tool = GardenTool::Inspect;
    juce::String message;          // short feedback ("Paired!")
    double messageTime = 0.0;

    static juce::String roleName (int theme, lifecat::Role, bool plural = false);
    static juce::Colour roleColour (int theme, lifecat::Role);
    static juce::Colour creatureColour (int theme, const lifecat::Creature&);
    void say (const juce::String& text);
};

//==============================================================================
/** The living world: draws the four themes and turns mouse gestures into gardener commands. */
class WorldView : public juce::Component, public juce::SettableTooltipClient
{
public:
    WorldView (LifeCatProcessor&, LifeViewState&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Called after each new snapshot: picks up births / deaths for the little effects. */
    void tick();
    std::function<void()> onSelectionChanged;

    static uint32_t creatureAt (const lifecat::World&, juce::Point<float> worldPos, float radius);

private:
    juce::Rectangle<float> area() const;
    juce::Point<float> toWorld (juce::Point<float> screen) const;
    juce::Point<float> toScreen (float x, float y) const;
    float creatureRadius (const lifecat::Creature&) const;

    void drawBackground (juce::Graphics&, juce::Rectangle<float>, double time) const;
    void drawZones (juce::Graphics&, double time) const;
    void drawWalls (juce::Graphics&) const;
    void drawPlants (juce::Graphics&, double time) const;
    void drawCreature (juce::Graphics&, const lifecat::Creature&, double time) const;
    void drawEffects (juce::Graphics&, double time);
    void drawGesture (juce::Graphics&) const;
    void drawHud (juce::Graphics&, juce::Rectangle<float>, double time) const;

    LifeCatProcessor& processor;
    LifeViewState& view;

    struct Fx { float x, y; uint8_t kind; lifecat::Role role; double t0; };
    std::vector<Fx> fx;
    int lastEventPos = 0;
    double lastWorldTime = -1.0;

    bool dragging = false, hovering = false;
    juce::Point<float> downW, curW, hoverW;
    double lastDrop = 0.0;
};

//==============================================================================
/** Selected creature: its genes (body + sound) and Listen / Favourite / Clone / Pair / Remove. */
class Inspector : public juce::Component
{
public:
    Inspector (LifeCatProcessor&, LifeViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    LifeCatProcessor& processor;
    LifeViewState& view;
    juce::TextButton listen { "Listen" }, favourite { "Favourite" }, cloneButton { "Clone" }, pair { "Pair..." }, remove { "Remove" };
    lifecat::Creature shown;
    bool found = false;
};

//==============================================================================
/** Population over the last 8 minutes + counters. */
class PopulationGraph : public juce::Component
{
public:
    explicit PopulationGraph (LifeViewState& v) : view (v) {}
    void paint (juce::Graphics&) override;

private:
    LifeViewState& view;
};

//==============================================================================
class LifeCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit LifeCatEditor (LifeCatProcessor&);
    ~LifeCatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setTool (GardenTool);
    void showTab (int);
    void newWorld();
    void updateToolNames();

    LifeCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    LifeViewState view;

    // header
    cat::PresetBar presetBar;
    cat::ParamCombo theme;
    cat::ParamToggle running;
    juce::TextButton newWorldButton { "New World" };
    cat::SurpriseControls surpriseControls;
    cat::DragButton dragButton { "Drag MIDI" };

    // gardener tools
    juce::OwnedArray<juce::TextButton> toolButtons;
    juce::Label toolsLabel;

    WorldView worldView;
    Inspector inspector;
    PopulationGraph graph;

    // tabs
    juce::OwnedArray<juce::TextButton> tabButtons;
    int currentTab = 0;
    std::array<std::vector<juce::Component*>, 4> tabContent;

    // MUSIC
    cat::ParamCombo pitchMode, key, scale, grid;
    std::unique_ptr<cat::Stepper> lowNote, highNote, maxNotes;
    cat::ParamToggle quantize, followGroove, playStopped;
    cat::BarSlider swing, density, noteLength;
    // LIFE
    cat::BarSlider lifeSpeed, foodGrowth, mutation, hunger, breeding, lifespan;
    std::unique_ptr<cat::Stepper> maxPop;
    cat::ParamToggle immigration;
    juce::TextButton clearWalls { "Clear walls & zones" };
    // SOUND
    cat::ParamToggle ownSound;
    cat::BarSlider brightness, attack, release, reverbMix, reverbSize, delayMix, delayFb, width, volume;
    cat::ParamCombo delayTime;
    // MIDI
    cat::ParamToggle midiOut;
    cat::ParamCombo midiChannels;
    juce::Label midiInfo;

    cat::PianoDisplay keyboard;
    cat::CatVisualizer catView;
    cat::BootScreen bootScreen;

    int lastTheme = -1, tickCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LifeCatEditor)
};
