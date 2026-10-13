#include "CatWidgets.h"

namespace cat
{
juce::String cubaseNoteName (int midiNote)
{
    return juce::MidiMessage::getMidiNoteName (midiNote, true, true, 3);
}

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& paramID, const juce::String& captionText, MidiLearnManager* learn, juce::Colour colour)
{
    slider.setMidiLearn (learn, paramID);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);

    caption.setText (captionText, juce::dontSendNotification);
    caption.setFont (catFont (11.5f, true));
    caption.setColour (juce::Label::textColourId, Colours::textDim);
    caption.setJustificationType (juce::Justification::centred);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
BarSlider::BarSlider (APVTS& state, const juce::String& paramID, const juce::String& caption, MidiLearnManager* learn, int captionWidth)
    : captionText (caption), captionW (captionWidth)
{
    slider.setMidiLearn (learn, paramID);
    slider.setColour (juce::Slider::trackColourId, Colours::blue);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);
}

void BarSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panelLight.darker (0.2f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::textDim);
    g.setFont (catFont (12.5f));
    g.drawText (captionText, r.withWidth ((float) captionW).withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

void BarSlider::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedLeft (captionW).reduced (2));
}

//==============================================================================
ParamCombo::ParamCombo (APVTS& state, const juce::String& paramID, const juce::String& caption, MidiLearnManager* learn, int captionWidth)
    : captionText (caption), captionW (captionWidth)
{
    combo.setMidiLearn (learn, paramID);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
        combo.addItemList (choice->choices, 1);
    else if (auto* p = state.getParameter (paramID))
        for (int i = 0; i < p->getNumSteps(); ++i)
            combo.addItem (p->getText ((float) i / (float) juce::jmax (1, p->getNumSteps() - 1), 32), i + 1);

    addAndMakeVisible (combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, combo);
}

void ParamCombo::paint (juce::Graphics& g)
{
    if (captionW > 0)
    {
        g.setColour (Colours::textDim);
        g.setFont (catFont (12.5f));
        g.drawText (captionText, getLocalBounds().withWidth (captionW), juce::Justification::centredLeft);
    }
}

void ParamCombo::resized()
{
    combo.setBounds (getLocalBounds().withTrimmedLeft (captionW));
}

//==============================================================================
ParamToggle::ParamToggle (APVTS& state, const juce::String& paramID, const juce::String& buttonText, MidiLearnManager* learnManager)
{
    setButtonText (buttonText);
    setClickingTogglesState (true);
    setMidiLearn (learnManager, paramID);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, *this);
}

//==============================================================================
Stepper::Stepper (juce::String caption, int minValue, int maxValue, std::function<int()> getter,
                  std::function<void (int)> setter, std::function<juce::String (int)> formatter)
    : captionText (std::move (caption)), minV (minValue), maxV (maxValue),
      get (std::move (getter)), set (std::move (setter)), format (std::move (formatter))
{
    startTimerHz (15);
}

std::unique_ptr<Stepper> Stepper::forParameter (APVTS& state, const juce::String& paramID, const juce::String& caption,
                                                std::function<juce::String (int)> formatter)
{
    auto* param = state.getParameter (paramID);
    jassert (param != nullptr);
    const auto range = param->getNormalisableRange();

    auto stepper = std::make_unique<Stepper> (caption, (int) range.start, (int) range.end, nullptr, nullptr, std::move (formatter));
    auto* raw = stepper.get();

    stepper->paramAttachment = std::make_unique<juce::ParameterAttachment> (*param, [raw] (float v)
    {
        raw->paramValue = v;
        raw->repaint();
    });
    stepper->paramAttachment->sendInitialUpdate();
    stepper->get = [raw] { return juce::roundToInt (raw->paramValue); };
    stepper->set = [raw] (int v) { raw->paramAttachment->setValueAsCompleteGesture ((float) v); };
    return stepper;
}

void Stepper::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panelLight);
    g.fillRoundedRectangle (r, 5.0f);

    const int value = get ? get() : 0;
    lastShown = value;

    g.setFont (catFont (13.0f));
    auto area = r.reduced (6.0f, 0.0f);
    if (captionText.isNotEmpty())
    {
        const float cw = captionW >= 0 ? (float) captionW : area.getWidth() * 0.42f;
        g.setColour (Colours::textDim);
        g.drawText (captionText, area.removeFromLeft (cw), juce::Justification::centredLeft);
    }

    auto left = area.removeFromLeft (14.0f);
    auto right = area.removeFromRight (14.0f);
    CatLookAndFeel::drawTriangle (g, left, false, value > minV ? Colours::text : Colours::outline);
    CatLookAndFeel::drawTriangle (g, right, true, value < maxV ? Colours::text : Colours::outline);

    g.setColour (Colours::text);
    g.drawFittedText (format ? format (value) : juce::String (value), area.toNearestInt(), juce::Justification::centred, 1);
}

void Stepper::mouseDown (const juce::MouseEvent& e)
{
    step (e.x > getWidth() / 2 ? 1 : -1);
}

void Stepper::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) > 0.0f)
        step (w.deltaY > 0 ? 1 : -1);
}

void Stepper::step (int delta)
{
    if (! get || ! set)
        return;
    const int v = juce::jlimit (minV, maxV, get() + delta);
    set (v);
    repaint();
}

void Stepper::timerCallback()
{
    if (get && get() != lastShown)
        repaint();
}

//==============================================================================
PianoDisplay::PianoDisplay (const NoteMonitor& m, int low, int high) : monitor (m), lowNote (low), highNote (high)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

bool PianoDisplay::isBlack (int note) noexcept
{
    const int pc = note % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

void PianoDisplay::timerCallback()
{
    bool changed = false;
    for (int n = 0; n < 128; ++n)
    {
        const auto v = monitor.velocity[(size_t) n].load (std::memory_order_relaxed);
        if (v != shown[(size_t) n]) { shown[(size_t) n] = v; changed = true; }
    }
    if (changed)
        repaint();
}

void PianoDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::background);
    g.fillRoundedRectangle (r, 4.0f);

    int numWhite = 0;
    for (int n = lowNote; n <= highNote; ++n)
        if (! isBlack (n)) ++numWhite;
    if (numWhite == 0) return;

    const float keyW = r.getWidth() / (float) numWhite;
    const float blackH = r.getHeight() * 0.6f;

    auto colourOf = [this] (int note)
    {
        const int ch = monitor.channel[(size_t) note].load (std::memory_order_relaxed);
        return colourFor ? colourFor (ch) : Colours::slate;
    };

    // White keys
    int wi = 0;
    for (int n = lowNote; n <= highNote; ++n)
    {
        if (isBlack (n)) continue;
        auto key = juce::Rectangle<float> (r.getX() + wi * keyW, r.getY(), keyW, r.getHeight()).reduced (0.75f, 0.0f);
        const bool on = shown[(size_t) n] > 0;
        g.setColour (on ? colourOf (n) : juce::Colour (0xff1b1e25));
        g.fillRoundedRectangle (key, 2.0f);

        if (n % 12 == 0 || on)
        {
            g.setColour (on ? Colours::text : Colours::textDim);
            g.setFont (catFont (juce::jmin (11.0f, keyW * 0.8f)));
            const auto label = on ? juce::MidiMessage::getMidiNoteName (n, true, n % 12 == 0, 3) : cubaseNoteName (n);
            g.drawText (label, key.removeFromBottom (16.0f), juce::Justification::centred);
        }
        ++wi;
    }

    // Black keys
    wi = 0;
    for (int n = lowNote; n <= highNote; ++n)
    {
        if (! isBlack (n)) { ++wi; continue; }
        auto key = juce::Rectangle<float> (r.getX() + wi * keyW - keyW * 0.32f, r.getY(), keyW * 0.64f, blackH);
        const bool on = shown[(size_t) n] > 0;
        g.setColour (on ? colourOf (n).brighter (0.2f) : juce::Colour (0xff0e1014));
        g.fillRoundedRectangle (key, 2.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (key, 2.0f, 1.0f);
    }
}

} // namespace cat
