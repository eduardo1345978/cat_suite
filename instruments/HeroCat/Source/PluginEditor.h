#pragma once

#include "PluginProcessor.h"
#include "CatLookAndFeel.h"
#include "CatWidgets.h"
#include "PresetBar.h"

/**
    Hero-Cat's window looks like an old PC (Windows 98 era) running a game:
    a desktop with icons (they open the tabs), the game's box art, a big green CRAFT button,
    a red-framed window with tabs and a green LCD (oscilloscope, chord, the sound's name), keys at the bottom.
*/

/** The box art: a pixel sunset with a castle and the cat hero; sparks fly when you CRAFT. */
class BoxArt : public juce::Component
{
public:
    explicit BoxArt (HeroCatProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void tick();
    void forge();                  // the CRAFT animation: a flash and sparks from the sword

private:
    HeroCatProcessor& processor;
    struct Spark { float x, y, vx, vy, life; };
    std::vector<Spark> sparks, coins;
    int frame = 0, seenLog = 0;
    float scroll = 0.0f, jump = 0.0f, jumpV = 0.0f;
    float flash = 0.0f;
    juce::Random rng;
};

/** The big glossy green button. */
class CraftButton : public juce::Button
{
public:
    CraftButton() : juce::Button ("CRAFT") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

/** A desktop icon: a little pixel picture and its name; clicking it opens a tab. */
class DesktopIcon : public juce::Button
{
public:
    DesktopIcon (const juce::String& name, int kind) : juce::Button (name), kind (kind) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool selected = false;

private:
    int kind;
};

/** A Windows 98 style tab or button (grey, bevelled). */
class RetroButton : public juce::Button
{
public:
    explicit RetroButton (const juce::String& name, bool isTab = false) : juce::Button (name), tab (isTab) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool selected = false;

private:
    bool tab;
};

/** The green LCD: the oscilloscope, the chord, the sound's name, what the arp does. */
class LcdScreen : public juce::Component
{
public:
    explicit LcdScreen (HeroCatProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void tick();
    juce::String message;          // a short line shown for a while (e.g. "CRAFTED!")
    int messageFrames = 0;

private:
    HeroCatProcessor& processor;
    std::array<float, 400> wave {};
    float meter = 0.0f;
    int frame = 0;
};

/** The drawn arp pattern on a small LCD: 16 columns, drag up / down for loudness, double-click on / off. */
class PatternView : public juce::Component
{
public:
    explicit PatternView (HeroCatProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void setFrom (juce::Point<float>);
    HeroCatProcessor& processor;
};

class HeroCatEditor : public juce::Component, private juce::Timer
{
public:
    explicit HeroCatEditor (HeroCatProcessor&);
    ~HeroCatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int);
    void doCraft();

    HeroCatProcessor& processor;
    int tab = 0;
    cat::PresetBar presetBar { processor.presetManager, "" };
    BoxArt art { processor };
    CraftButton craftButton;
    RetroButton undo { "UNDO" }, dice { "DICE" }, clear { "CLEAR" }, quest { "QUEST" }, newSong { "NEW SONG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> questAttach;
    std::array<std::unique_ptr<DesktopIcon>, 4> icons;
    std::array<std::unique_ptr<RetroButton>, 4> tabs;
    LcdScreen lcd { processor };
    PatternView pattern { processor };
    std::array<std::vector<juce::Component*>, 4> pages;

    cat::ParamCombo srcA { processor.apvts, "srcA", "sound A", nullptr, 56 }, srcB { processor.apvts, "srcB", "sound B", nullptr, 56 },
                    octB { processor.apvts, "octB", "B oct", nullptr, 42 },
                    chordMode { processor.apvts, "chordMode", "chord", nullptr, 44 }, key { processor.apvts, "key", "key", nullptr, 30 },
                    scale { processor.apvts, "scale", "scale", nullptr, 38 },
                    arpMode { processor.apvts, "arpMode", "arp", nullptr, 44 }, rate { processor.apvts, "rate", "rate", nullptr, 34 },
                    octaves { processor.apvts, "octaves", "oct", nullptr, 30 },
                    texture { processor.apvts, "texture", "texture", nullptr, 56 }, lfoShape { processor.apvts, "lfoShape", "LFO shape", nullptr, 70 },
                    lfoTarget { processor.apvts, "lfoTarget", "LFO moves", nullptr, 70 };
    cat::LineSlider mix { processor.apvts, "mix", "A / B" }, detune { processor.apvts, "detune", "DETUNE" },
                    cutoff { processor.apvts, "cutoff", "CUTOFF" }, reso { processor.apvts, "reso", "RESO" },
                    envAmt { processor.apvts, "envAmt", "F.ENV" }, vibrato { processor.apvts, "vibrato", "VIBRATO" },
                    attack { processor.apvts, "attack", "ATTACK" }, decay { processor.apvts, "decay", "DECAY" },
                    sustain { processor.apvts, "sustain", "SUSTAIN" }, release { processor.apvts, "release", "RELEASE" },
                    glide { processor.apvts, "glide", "GLIDE" },
                    strum { processor.apvts, "strum", "STRUM" }, gate { processor.apvts, "gate", "GATE" },
                    swing { processor.apvts, "swing", "SWING" }, chaos { processor.apvts, "chaos", "CHAOS" },
                    texLevel { processor.apvts, "texLevel", "TEXTURE LEVEL" }, lfoRate { processor.apvts, "lfoRate", "LFO RATE" },
                    lfoDepth { processor.apvts, "lfoDepth", "LFO DEPTH" }, drive { processor.apvts, "drive", "DRIVE" },
                    crush { processor.apvts, "crush", "CRUSH" }, chorus { processor.apvts, "chorus", "CHORUS" },
                    delay { processor.apvts, "delay", "DELAY" }, reverb { processor.apvts, "reverb", "REVERB" },
                    width { processor.apvts, "width", "WIDTH" }, output { processor.apvts, "output", "OUTPUT" };
    cat::ParamToggle voiceLead { processor.apvts, "voiceLead", "VOICE LEAD" }, latch { processor.apvts, "latch", "LATCH" },
                     midiOut { processor.apvts, "midiOut", "MIDI OUT" };
    juce::MidiKeyboardComponent keys { processor.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeroCatEditor)
};
