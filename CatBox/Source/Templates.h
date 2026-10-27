#pragma once

#include "Project.h"

/** Ready-made drum patterns and bass lines (the MENU): a start you then change. */
namespace catbox
{
juce::StringArray drumTemplateNames();
void applyDrumTemplate (Pattern&, int index);
juce::StringArray bassTemplateNames();
/** A line in the key and scale (root octave 2). */
void applyBassTemplate (Pattern&, int index, int key, int scale);
} // namespace catbox
