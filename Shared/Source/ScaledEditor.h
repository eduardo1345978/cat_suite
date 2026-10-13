#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CatLookAndFeel.h"

namespace cat
{
/**
    Plug-in window that scales its whole UI as one piece.

    The real editor is laid out once at a fixed design size and drawn through an
    AffineTransform, so every line, curve and letter is re-rendered at the new size
    (vector graphics, no bitmap stretching) and stays sharp at any scale. Drag the
    corner to resize; the aspect ratio is kept. The chosen scale is remembered per
    plug-in (and combines with Windows display scaling, which JUCE applies on top).
*/
class ScaledEditor : public juce::AudioProcessorEditor
{
public:
    ScaledEditor (juce::AudioProcessor& processor, std::unique_ptr<juce::Component> content, const juce::String& pluginName);
    ~ScaledEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr float minScale = 0.5f, maxScale = 2.5f;

private:
    float loadScale() const;
    void saveScale (float scale) const;

    CatLookAndFeel lnf;
    std::unique_ptr<juce::Component> content;
    juce::String name;
    int designWidth, designHeight;
    float currentScale = 1.0f;
};

} // namespace cat
