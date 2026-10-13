#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CatLookAndFeel.h"
#include "MidiLearn.h"
#include "MidiScheduler.h"
#include <climits>

namespace cat
{
/** Adds a right-click "MIDI Learn / Clear" menu to any JUCE control. */
template <typename Base>
class Learnable : public Base
{
public:
    using Base::Base;

    void setMidiLearn (MidiLearnManager* manager, const juce::String& id)
    {
        learn = manager;
        paramId = id;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (learn != nullptr && e.mods.isPopupMenu())
        {
            swallowing = true;
            showLearnMenu();
            return;
        }
        swallowing = false;
        Base::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override { if (! swallowing) Base::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! swallowing) Base::mouseUp (e);
        swallowing = false;
    }

private:
    void showLearnMenu()
    {
        const int cc = learn->getCCForParam (paramId);
        juce::PopupMenu menu;
        menu.addSectionHeader (cc >= 0 ? "MIDI: CC " + juce::String (cc) : juce::String ("MIDI: not mapped"));
        menu.addItem (1, learn->isLearning (paramId) ? "Cancel MIDI Learn" : "MIDI Learn (move a controller)");
        menu.addItem (2, "Clear MIDI mapping", cc >= 0);

        juce::Component::SafePointer<juce::Component> safe (this);
        auto* mgr = learn;
        const auto id = paramId;
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, mgr, id] (int result)
        {
            if (safe == nullptr) return;
            if (result == 1) { if (mgr->isLearning (id)) mgr->cancelLearning(); else mgr->startLearning (id); }
            if (result == 2) mgr->clearMapping (id);
        });
    }

    MidiLearnManager* learn = nullptr;
    juce::String paramId;
    bool swallowing = false;
};

using APVTS = juce::AudioProcessorValueTreeState;

/** Rotary knob + caption + value read-out bound to a parameter. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramID, const juce::String& caption, MidiLearnManager* learn = nullptr,
          juce::Colour colour = Colours::accent);
    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    Learnable<juce::Slider> slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label caption;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

/** Horizontal bar slider with an inline caption, Bendimist style ("Depth  [ 100% ]"). */
class BarSlider : public juce::Component
{
public:
    BarSlider (APVTS& state, const juce::String& paramID, const juce::String& caption, MidiLearnManager* learn = nullptr,
               int captionWidth = 52);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    Learnable<juce::Slider> slider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::String captionText;
    int captionW;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

/** Choice parameter shown as a combo box with optional caption on the left. */
class ParamCombo : public juce::Component
{
public:
    ParamCombo (APVTS& state, const juce::String& paramID, const juce::String& caption = {}, MidiLearnManager* learn = nullptr,
                int captionWidth = 0);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::ComboBox& getCombo() noexcept { return combo; }

private:
    Learnable<juce::ComboBox> combo;
    juce::String captionText;
    int captionW;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

/** Toggle button bound to a bool parameter. */
class ParamToggle : public Learnable<juce::TextButton>
{
public:
    ParamToggle (APVTS& state, const juce::String& paramID, const juce::String& buttonText, MidiLearnManager* learnManager = nullptr);

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** "Caption  < value >" stepper, as used in Chordimist's top bar. */
class Stepper : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Stepper (juce::String caption, int minValue, int maxValue,
             std::function<int()> getter, std::function<void (int)> setter,
             std::function<juce::String (int)> formatter = {});

    /** Convenience: binds to an int / choice parameter. */
    static std::unique_ptr<Stepper> forParameter (APVTS& state, const juce::String& paramID, const juce::String& caption,
                                                  std::function<juce::String (int)> formatter = {});

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setCaptionWidth (int w) { captionW = w; repaint(); }

private:
    void timerCallback() override;
    void step (int delta);

    juce::String captionText;
    int minV, maxV, captionW = -1, lastShown = INT_MIN;
    std::function<int()> get;
    std::function<void (int)> set;
    std::function<juce::String (int)> format;
    std::unique_ptr<juce::ParameterAttachment> paramAttachment;
    float paramValue = 0.0f;
};

/** Read-only keyboard that lights the keys the plug-in is currently sending (Cubase octave names, C3 = 60). */
class PianoDisplay : public juce::Component, private juce::Timer
{
public:
    PianoDisplay (const NoteMonitor& monitor, int lowestNote = 36, int highestNote = 96);
    void paint (juce::Graphics&) override;
    void setColourForChannel (std::function<juce::Colour (int channel)> fn) { colourFor = std::move (fn); }
    void setRange (int low, int high) { lowNote = low; highNote = high; repaint(); }

private:
    void timerCallback() override;
    static bool isBlack (int note) noexcept;

    const NoteMonitor& monitor;
    int lowNote, highNote;
    std::array<uint8_t, 128> shown {};
    std::function<juce::Colour (int)> colourFor;
};

/** Button that starts an external drag (MIDI file -> Cubase) when dragged. */
class DragButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    std::function<void()> onDragOut;

    void mouseDown (const juce::MouseEvent& e) override { dragging = false; juce::TextButton::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging && e.getDistanceFromDragStart() > 6 && onDragOut)
        {
            dragging = true;
            onDragOut();
        }
    }

private:
    bool dragging = false;
};

/** Cubase note name: C3 == MIDI 60. */
juce::String cubaseNoteName (int midiNote);

} // namespace cat
