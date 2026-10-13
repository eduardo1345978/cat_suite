#include "ScaledEditor.h"

namespace cat
{
namespace
{
    juce::PropertiesFile::Options settingsOptions (const juce::String& pluginName)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = pluginName;
        o.folderName = "CatSuite";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
}

ScaledEditor::ScaledEditor (juce::AudioProcessor& p, std::unique_ptr<juce::Component> c, const juce::String& pluginName)
    : AudioProcessorEditor (p), content (std::move (c)), name (pluginName)
{
    setLookAndFeel (&lnf);

    // The editor sets its own design size in its constructor
    designWidth = juce::jmax (1, content->getWidth());
    designHeight = juce::jmax (1, content->getHeight());
    addAndMakeVisible (*content);

    // Standalone app: play the instrument from the computer keyboard
    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        if (auto* target = dynamic_cast<ComputerKeyboardTarget*> (&p))
        {
            keyboardBar = std::make_unique<ComputerKeyboardBar> (target->keyboardState);
            barHeight = ComputerKeyboardBar::height;
            addAndMakeVisible (*keyboardBar);
            addKeyListener (keyboardBar.get());
            setWantsKeyboardFocus (true);
        }
    const int fullHeight = designHeight + barHeight;

    // Read before setting limits: JUCE resizes (and calls resized()) while limits are applied
    const float initialScale = loadScale();

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (designWidth * minScale), juce::roundToInt (fullHeight * minScale),
                     juce::roundToInt (designWidth * maxScale), juce::roundToInt (fullHeight * maxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) designWidth / (double) fullHeight);

    currentScale = initialScale;
    setSize (juce::roundToInt (designWidth * initialScale), juce::roundToInt (fullHeight * initialScale));
}

ScaledEditor::~ScaledEditor()
{
    saveScale (currentScale);
    if (keyboardBar != nullptr) removeKeyListener (keyboardBar.get());
    keyboardBar.reset();
    content.reset();
    setLookAndFeel (nullptr);
}

void ScaledEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
}

void ScaledEditor::resized()
{
    currentScale = juce::jlimit (minScale, maxScale, (float) getWidth() / (float) designWidth);
    content->setBounds (0, 0, designWidth, designHeight);
    content->setTransform (juce::AffineTransform::scale (currentScale));
    if (keyboardBar != nullptr)
    {
        keyboardBar->setBounds (0, designHeight, designWidth, barHeight);
        keyboardBar->setTransform (juce::AffineTransform::scale (currentScale));
    }
}

void ScaledEditor::parentHierarchyChanged()
{
    // take the keys as soon as the app window opens
    if (keyboardBar != nullptr)
    {
        juce::Component::SafePointer<ScaledEditor> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus(); });
    }
}

float ScaledEditor::loadScale() const
{
    juce::PropertiesFile settings (settingsOptions (name));
    return juce::jlimit (minScale, maxScale, (float) settings.getDoubleValue ("uiScale", 1.0));
}

void ScaledEditor::saveScale (float scale) const
{
    juce::PropertiesFile settings (settingsOptions (name));
    settings.setValue ("uiScale", (double) scale);
    settings.saveIfNeeded();
}

} // namespace cat
