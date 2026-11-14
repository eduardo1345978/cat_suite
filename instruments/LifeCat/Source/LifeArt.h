#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LifeWorld.h"

/**
    How Life-Cat's world looks: four themes (Garden, Ocean, Space, Cells), ten kinds of
    creature drawn per theme, growing from baby to adult, with their genes visible (colour,
    size, spots / stripes, glow, sleepy eyes at night), plus terrain, plants, remains,
    day / night, seasons and weather.
*/
namespace lifeart
{
using lifecat::Role;
using lifecat::Terrain;

juce::String roleName (int theme, Role, bool plural = false);
juce::Colour roleColour (int theme, Role);
juce::Colour creatureColour (int theme, const lifecat::Creature&);
juce::String terrainName (int theme, Terrain);
juce::Colour terrainColour (int theme, Terrain);
juce::String weatherName (lifecat::Weather);
juce::String seasonName (float season);
juce::String clockText (float dayPhase);       // "14:30"

/** Shared drawing context for one frame. */
struct Scene
{
    int theme = 0;
    double time = 0.0;
    float light = 1.0f;          // world light (0 night .. 1 day)
    float season = 1.0f;
    float pixelsPerUnit = 600.0f;
};

/** 0.45 for a newborn .. 1 for an adult (after ~20 s). */
float maturity (const lifecat::Creature&) noexcept;

void drawBackground (juce::Graphics&, juce::Rectangle<float> area, int theme, double time);
/** The painted terrain as a transparent image (world square -> size x size pixels). */
juce::Image renderTerrain (const lifecat::World&, int theme, int size);
void drawPlant (juce::Graphics&, juce::Point<float> pos, float size, float growth, int index, const Scene&);
void drawCorpse (juce::Graphics&, juce::Point<float> pos, float r, const lifecat::Corpse&, const Scene&);
void drawNest (juce::Graphics&, juce::Point<float> pos, float r, const lifecat::Nest&, const Scene&);
/** Fruit trees (an apple tree, a coral tree, a crystal spire, a cell colony). */
void drawTree (juce::Graphics&, juce::Point<float> pos, float r, const lifecat::Tree&, int index, const Scene&);
void drawFruit (juce::Graphics&, juce::Point<float> pos, float s, int index, const Scene&);
/** Mushrooms (anemones, crystals, spore puffs): they glow a little at night. */
void drawMushroom (juce::Graphics&, juce::Point<float> pos, float s, int index, const Scene&);
juce::String eventName (int theme, lifecat::WorldEvent);
void drawCreature (juce::Graphics&, const lifecat::Creature&, juce::Point<float> pos, float r, const Scene&);
/** Light that shines through the night: glowing creatures, singing rings. Drawn after the sky. */
void drawGlow (juce::Graphics&, const lifecat::Creature&, juce::Point<float> pos, float r, const Scene&);
/** Night, dusk, winter frost over the whole world. */
void drawSky (juce::Graphics&, juce::Rectangle<float> area, const lifecat::World&, const Scene&);
/** Rain, snow, fog, storm flashes. */
void drawWeather (juce::Graphics&, juce::Rectangle<float> area, const lifecat::World&, const Scene&, float windX);
/** A small icon of a creature (for buttons and lists). */
void drawIcon (juce::Graphics&, juce::Rectangle<float> box, int theme, Role);

} // namespace lifeart
