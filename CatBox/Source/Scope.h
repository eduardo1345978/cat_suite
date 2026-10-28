#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

/**
    Pictures for the oscilloscope: a shape becomes a sound. Every picture is a table of points the
    beam follows once per period of the note (left = x, right = y, z for the 3D ones). The built-in
    pictures are made once at start; the text and the drawing of each track are rebuilt when they
    change (the message thread writes the free half of a double buffer, then flips it).
*/
namespace catbox
{
constexpr int scopePoints = 2048;
struct ScopeXYZ { float x = 0.0f, y = 0.0f, z = 0.0f; };

juce::StringArray scopePictureNames();   // ... "Text", "Drawing"
int scopePictureText();                  // the index of "Text"
int scopePictureDrawing();               // the index of "Drawing"
bool scopePicture3D (int picture);       // a 3D wireframe (the turn knob shows it best)

/** Makes the built-in pictures (call once, not on the audio thread). */
void scopeInit();
/** The table the beam follows: scopePoints points, about -1..1. Safe on the audio thread. */
const ScopeXYZ* scopePicture (int picture, int track);

/** A drawing: x, y pairs in -1..1 (y up); a pair with x >= penUp starts a new stroke. */
constexpr float scopePenUp = 9.0f;
void setScopeText (int track, const juce::String&);
void setScopeDrawing (int track, const std::vector<float>&);
juce::String scopeDrawingToString (const std::vector<float>&);   // how a project keeps it
std::vector<float> scopeDrawingFromString (const juce::String&);

/** The strokes of a picture for drawing it small (jumps between strokes left out). */
std::vector<std::vector<juce::Point<float>>> scopeOutline (int picture, int track);
} // namespace catbox
