#include "Surprise.h"

namespace cat
{
SurpriseControls::SurpriseControls (juce::AudioProcessor& p, juce::MemoryBlock& undoSnapshot, std::function<void()> fn)
    : processor (p), snapshot (undoSnapshot), randomise (std::move (fn))
{
    surpriseButton.setTooltip ("Surprise: randomise this plug-in within musical ranges (the undo arrow brings back what you had)");
    surpriseButton.setColour (juce::TextButton::buttonColourId, Colours::accent.withAlpha (0.16f));
    surpriseButton.onClick = [this]
    {
        snapshot.reset();
        processor.getStateInformation (snapshot);
        if (randomise) randomise();
        timerCallback();
    };

    undoButton.setTooltip ("Undo the last Surprise");
    undoButton.onClick = [this]
    {
        if (snapshot.getSize() == 0) return;
        processor.setStateInformation (snapshot.getData(), (int) snapshot.getSize());
        snapshot.reset();
        timerCallback();
    };

    addAndMakeVisible (surpriseButton);
    addAndMakeVisible (undoButton);
    startTimerHz (4);
    timerCallback();
}

void SurpriseControls::resized()
{
    auto r = getLocalBounds();
    undoButton.setBounds (r.removeFromRight (juce::jmin (r.getHeight(), 30)));
    r.removeFromRight (4);
    surpriseButton.setBounds (r);
}

void SurpriseControls::timerCallback()
{
    undoButton.setEnabled (snapshot.getSize() > 0);
}

void SurpriseControls::setParam (juce::AudioProcessor& processor, const juce::String& paramID, float value)
{
    for (auto* prm : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (ranged->getParameterID() == paramID)
            {
                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
                ranged->endChangeGesture();
                return;
            }
    jassertfalse; // unknown parameter
}

void SurpriseControls::UndoIcon::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panelLight.brighter (down ? 0.06f : (over && isEnabled() ? 0.1f : 0.0f)));
    g.fillRoundedRectangle (r, 4.0f);

    // Undo arrow: points left, then hooks round and down
    const auto b = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()) * 0.56f, juce::jmin (r.getWidth(), r.getHeight()) * 0.5f);
    const float x0 = b.getX(), y0 = b.getY(), w = b.getWidth(), h = b.getHeight();
    juce::Path stem;
    stem.startNewSubPath (x0 + w * 0.2f, y0 + h * 0.3f);
    stem.lineTo (x0 + w * 0.55f, y0 + h * 0.3f);
    stem.cubicTo (x0 + w * 1.05f, y0 + h * 0.3f, x0 + w * 1.05f, y0 + h * 0.95f, x0 + w * 0.5f, y0 + h * 0.95f);
    juce::Path head;
    head.addTriangle (x0, y0 + h * 0.3f, x0 + w * 0.32f, y0, x0 + w * 0.32f, y0 + h * 0.6f);

    const auto colour = (isEnabled() ? Colours::text : Colours::textDim).withAlpha (isEnabled() ? 0.9f : 0.35f);
    g.setColour (colour);
    g.strokePath (stem, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillPath (head);
}

} // namespace cat
