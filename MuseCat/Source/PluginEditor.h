#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"
#include "ScaledEditor.h"

/** The song's map: sections, chords, a lane of notes per part, and the playhead. */
class SongMap : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setSong (std::shared_ptr<const cat::muse::Song>);
    void setBeat (double beat);                  // < 0 = not playing
    void paint (juce::Graphics&) override;
    void resized() override { rebuild(); }
    static juce::Colour partColour (int part);
    static constexpr float labelWidth = 86.0f;
    /** Click a section: the arranger. Drag a part's lane: that part as MIDI. */
    std::function<void (int section, juce::Point<int> screenPos)> onSectionClick;
    std::function<void (int part)> onDragPart;
    int sectionAt (juce::Point<int>) const;
    int partAt (juce::Point<int>) const;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void rebuild();
    std::shared_ptr<const cat::muse::Song> song;
    juce::Image image;
    double beat = -1.0;
    int dragPart = -1;
    bool dragged = false;
};

/** Write your own words and chords: one box of chords per progression (verse, chorus, pre-chorus, bridge) and the words,
    one line per sung line. Apply re-fits the melody, bass and counter-melody. */
class WordsPanel : public juce::Component
{
public:
    explicit WordsPanel (MuseCatProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void load();                                  // fill the boxes from the song playing now
    bool apply();                                 // false: a chord couldn't be read (shown)
    std::function<void()> onApplied;

    std::array<juce::TextEditor, 4> chordBoxes;
    juce::TextEditor wordsBox;
    juce::TextButton applyButton { "Apply" }, resetButton { "Muse-Cat's own" }, closeButton { "Close" };
    juce::String message;
    bool messageIsError = false;

private:
    MuseCatProcessor& processor;
    std::array<juce::String, 4> shownChords;      // what the boxes showed when filled (unchanged = not yours)
    juce::String shownWords;
    std::array<juce::Rectangle<int>, 4> chordLabels;
    juce::Rectangle<int> wordsLabel, helpArea, exampleArea;
};

/** Muse-Cat's window. */
class MuseCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit MuseCatEditor (MuseCatProcessor&);
    ~MuseCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void exportSong (int what);
    void refreshSong();
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void arrangeSection (int section, int menuResult);
    void dragPartOut (int part);
    void showWords (bool show);

    MuseCatProcessor& processor;
    cat::PresetBar presetBar { processor.presetManager, "Song" };
    juce::TextButton playButton { "Play" }, newSeedButton { "New number" }, songButton { "Song" }, endlessButton { "Endless" },
                     exportButton { "Export..." };
    cat::DragButton dragButton { "Drag MIDI" };
    juce::TextButton wordsButton { "Words & chords" };
    WordsPanel wordsPanel { processor };
    cat::ParamCombo genreBox { processor.apvts, "genre", "Genre", nullptr, 52 }, moodBox { processor.apvts, "mood", "Mood", nullptr, 46 };
    juce::TextEditor seedBox;
    juce::OwnedArray<juce::TextButton> renewButtons;
    juce::OwnedArray<cat::ParamCombo> soundBoxes;
    juce::OwnedArray<cat::Knob> volumeKnobs;
    cat::Knob masterKnob { processor.apvts, "master", "Volume" };
    cat::ParamToggle soundToggle { processor.apvts, "sound", "Own sound" }, midiToggle { processor.apvts, "midiOut", "MIDI out" };
    SongMap map;

    std::shared_ptr<const cat::muse::Song> shownSong;
    int shownLine = -2;
    bool shownPlaying = false;
    juce::Rectangle<int> logoArea, headerArea, renewLabelArea, lyricsArea, partsArea, midiInfoArea;
    std::array<juce::Rectangle<int>, cat::muse::NumParts> partLabelAreas;
    std::array<float, cat::muse::NumParts> shownMeters {};
    juce::String status;
    std::vector<cat::muse::Section> undoForm;
    juce::uint32 undoKey = 0;
    int labelDragPart = -1;
    bool labelDragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuseCatEditor)
};
