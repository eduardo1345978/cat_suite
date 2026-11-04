#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cat
{
/**
    Every Cat has its own world (the "Every Cat" designs, Nov 2026): its colours, its knobs, its buttons,
    a backdrop (wood cheeks, rack rails, stars, scan lines ...) and its own logo font.

    Each plug-in is its own binary, so the theme is simply set once per plug-in (by its PresetManager,
    from the plug-in's name) before any window is made. The apps (Cat Hub, Ocean, Box) keep the default.
*/
enum class KnobStyle { Arc, Hardware, Chrome, Cream, Brass, Glow, Gauge };
enum class ButtonStyle { Flat, Hardware, Glow, Metal, Terminal };
enum class Backdrop { Plain, Wood, Rack, ChromeSheen, Plastic, Stars, Studio, Biolum, Silver, Amber, Terminal, Console, Cream, Observatory, Lab };

struct Theme
{
    juce::String name = "Graphite";
    juce::Colour background { 0xff0f1013 }, panel { 0xff15161a }, panelLight { 0xff1b1c21 }, raised { 0xff24262c },
                 outline { 0xff2a2c33 }, text { 0xffe4e6ea }, textDim { 0xff7a7e88 }, accent { 0xff8fa3f0 };
    bool light = false;                    // a light panel (dark text)
    KnobStyle knob = KnobStyle::Arc;
    juce::Colour knobBody { 0xff1c1d21 }, knobRing { 0xff2a2c33 }, knobPointer { 0xffe4e6ea };
    bool knobGlow = false;                 // the pointer / arc glows
    ButtonStyle button = ButtonStyle::Flat;
    Backdrop backdrop = Backdrop::Plain;
    float corner = 4.0f;
    const char* fontData = nullptr;        // the logo / display font (embedded), null = the system font
    int fontSize = 0;
    float fontScale = 1.0f;                // how much bigger the display font must be set to look the same size
    bool uiFont = false;                   // use the display font for all text too (Talk-Cat's terminal)
};

/** The theme of this plug-in (default: Graphite). */
const Theme& theme();
/** Picks the theme for a plug-in by its name ("Echo-Cat" ...) and sets the shared colours. */
void applyTheme (const juce::String& pluginName);
/** The theme's display font (logo, big headings). */
juce::Font displayFont (float height);
/** The pixel LCD font (VT323), for displays that look like hardware screens. */
juce::Font lcdFont (float height);
/** The window's background: colour, texture and the theme's decorations. */
void paintBackdrop (juce::Graphics&, juce::Rectangle<int> bounds);
} // namespace cat
