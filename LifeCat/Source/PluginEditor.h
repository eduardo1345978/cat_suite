#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "CatVisualizer.h"
#include "Surprise.h"
#include "PresetBar.h"
#include "LifeArt.h"

enum class GardenTool { Inspect = 0, Food, Add, Remove, Wall, Warm, Cold, Current, Paint, Erase, PlaceLab };
constexpr int numGardenTools = 10;        // PlaceLab is reached from the Gene Lab

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
    lifecat::Role addRole = lifecat::Role::Grazer;
    lifecat::Terrain paintType = lifecat::Terrain::Water;
    float brush = 0.06f;
    // gene lab
    lifecat::Role labRole = lifecat::Role::Grazer;
    lifecat::Genes labGenes;
    // camera
    float zoom = 1.0f, camX = 0.5f, camY = 0.5f;
    bool follow = false;
    // feedback
    juce::String message;
    double messageTime = 0.0;

    void say (const juce::String& text);
    const lifecat::Creature* selectedCreature() const noexcept;
};

//==============================================================================
/** The living world: draws everything and turns mouse gestures into gardener commands.
    Mouse wheel zooms, middle-drag or Alt-drag pans, double-click with the middle button resets. */
class WorldView : public juce::Component, public juce::SettableTooltipClient
{
public:
    WorldView (LifeCatProcessor&, LifeViewState&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    /** Called after each new snapshot: effects, camera follow. */
    void tick();
    void resetCamera();
    std::function<void()> onSelectionChanged;

    static uint32_t creatureAt (const lifecat::World&, juce::Point<float> worldPos, float radius);
    juce::Point<float> toWorld (juce::Point<float> screen) const;
    juce::Point<float> toScreen (float x, float y) const;

private:
    juce::Rectangle<float> area() const;
    float unitPixels() const;                      // pixels per world unit (smaller side)
    float creatureRadius (const lifecat::Creature&) const;
    void clampCamera();
    void zoomAt (juce::Point<float> screen, float factor);
    void drawZones (juce::Graphics&, double time) const;
    void drawWalls (juce::Graphics&) const;
    void drawEffects (juce::Graphics&, double time);
    void drawGesture (juce::Graphics&) const;
    void drawHud (juce::Graphics&, juce::Rectangle<float>, double time) const;
    juce::Rectangle<float> zoomButton (int index) const;   // 0 -, 1 +, 2 fit

    LifeCatProcessor& processor;
    LifeViewState& view;

    juce::Image terrainImage;
    uint32_t terrainHash = 0;
    int terrainTheme = -1;

    struct Fx { float x, y; uint8_t kind; lifecat::Role role; double t0; };
    std::vector<Fx> fx;
    int lastEventPos = 0;
    double lastWorldTime = -1.0;

    bool dragging = false, hovering = false, panning = false;
    juce::Point<float> downW, curW, hoverW, panStart;
    float panCamX = 0.5f, panCamY = 0.5f;
    double lastDrop = 0.0;
};

//==============================================================================
/** Selected creature as a card over the world: genes, Listen / Favourite / Clone / Pair / Follow / Lab / Remove. */
class InspectorCard : public juce::Component
{
public:
    InspectorCard (LifeCatProcessor&, LifeViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onToLab;

private:
    LifeCatProcessor& processor;
    LifeViewState& view;
    juce::TextButton listen { "Listen" }, favourite { "Favourite" }, cloneButton { "Clone" }, pair { "Pair..." },
                     follow { "Follow" }, toLab { "To Lab" }, remove { "Remove" }, close { "x" };
    lifecat::Creature shown;
    bool found = false;
};

//==============================================================================
/** The gardener's tools, plus the choices for the current tool (which animal, which terrain). */
class ToolPanel : public juce::Component, private juce::Timer
{
public:
    ToolPanel (LifeViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void updateNames();
    std::function<void (GardenTool)> onTool;

private:
    void timerCallback() override;
    juce::Rectangle<float> choiceBox (int index) const;
    juce::Rectangle<float> brushBar() const;
    int numChoices() const;

    LifeViewState& view;
    juce::OwnedArray<juce::TextButton> tools;
    juce::Rectangle<int> contextArea;
};

//==============================================================================
/** One row per kind of creature: sing, volume, octave, sound, speed, how many. */
class SpeciesPanel : public juce::Component
{
public:
    SpeciesPanel (LifeCatProcessor&, LifeViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        std::unique_ptr<cat::ParamToggle> sing;
        std::unique_ptr<cat::BarSlider> volume, speed;
        std::unique_ptr<cat::ParamCombo> sound;
        std::unique_ptr<cat::Stepper> octave, max;
    };
    LifeCatProcessor& processor;
    LifeViewState& view;
    std::array<Row, lifecat::numRoles> rows;
};

//==============================================================================
/** Edit all genes with bars, pick a kind, then listen, apply to the selected creature or drop it in. */
class GeneLab : public juce::Component
{
public:
    GeneLab (LifeCatProcessor&, LifeViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void loadFrom (const lifecat::Creature&);
    std::function<void()> onPlace;

private:
    juce::Rectangle<float> barArea (int gene) const;
    void setFromMouse (juce::Point<float>);

    LifeCatProcessor& processor;
    LifeViewState& view;
    juce::ComboBox kind;
    juce::TextButton fromSelected { "From Selected" }, randomise { "Random" }, mutate { "Mutate" }, listen { "Listen" },
                     apply { "Apply to Selected" }, place { "Place in World" };
    juce::Rectangle<int> barsArea;
    int dragGene = -1;
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
    void setBigView (bool);

    LifeCatProcessor& processor;
    cat::CatLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    LifeViewState view;

    // header
    cat::PresetBar presetBar;
    cat::ParamCombo theme;
    cat::ParamToggle running;
    juce::TextButton newWorldButton { "New World" }, bigButton { "Big View" };
    cat::SurpriseControls surpriseControls;
    cat::DragButton dragButton { "Drag MIDI" };

    ToolPanel toolPanel;
    WorldView worldView;
    InspectorCard inspector;
    PopulationGraph graph;

    // tabs
    juce::OwnedArray<juce::TextButton> tabButtons;
    int currentTab = 0;
    static constexpr int numTabs = 7;
    std::array<std::vector<juce::Component*>, numTabs> tabContent;

    // SPECIES / LAB
    SpeciesPanel speciesPanel;
    GeneLab geneLab;
    // WORLD
    cat::BarSlider gravity, wind, windDir, ambience, dayNightSound;
    cat::ParamCombo dayLength, seasonLength, season, weather;
    juce::TextButton clearWalls { "Clear walls & zones" }, clearTerrain { "Clear painted terrain" };
    juce::Label skyLabel;
    // MUSIC
    cat::ParamCombo pitchMode, key, scale, grid;
    std::unique_ptr<cat::Stepper> lowNote, highNote, maxNotes;
    cat::ParamToggle quantize, followGroove, playStopped;
    cat::BarSlider swing, density, noteLength;
    // LIFE
    cat::BarSlider lifeSpeed, foodGrowth, mutation, hunger, breeding, lifespan;
    std::unique_ptr<cat::Stepper> maxPop;
    cat::ParamToggle immigration;
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
