#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

/**
    The lava on Molten's screen: a small particle simulation (smoothed-particle style: pressure keeps the volume,
    a soft pull holds the blob together, gravity makes it sag), drawn as a metaball surface with a lit, glossy top,
    a cooling crust with glowing cracks that drifts over it, and a soft glow round it.

    HEAT makes it runnier (it sags and drips more), hotter in colour (dark red crust -> yellow-white) and its crust
    breaks up; DRIP lets drops fall off the bottom (one for every falling echo the sound makes); the sound's level
    makes it breathe and bubble; WOBBLE sways it.
*/
class LavaSim
{
public:
    struct Input
    {
        float heat = 0.5f, drip = 0.3f, wobble = 0.3f;
        float level = 0.0f;        // the sound coming in (0..1)
        int dripCount = 0;         // the engine's falling-echo counter
        float dt = 1.0f / 30.0f;
    };

    LavaSim();
    void setSize (int width, int height);
    void step (const Input&);
    void poke (juce::Point<float> fieldPos, float strength);   // a click: push the lava away from there
    void render();

    const juce::Image& image() const noexcept { return lava; }
    const juce::Image& glow() const noexcept { return glowImage; }
    juce::Point<float> centre() const noexcept { return { cx, cy }; }
    int width() const noexcept { return W; }
    int height() const noexcept { return H; }
    int numFree() const noexcept;

private:
    struct P { float x, y, vx, vy, t; bool free; float qx, qy; };   // qx/qy: where it sits in the blob's rest shape
    void reset();
    void dropFrom (int count);
    float noise (float x, float y) const noexcept;
    struct Cell { float d1, d2, id; };
    Cell cells (float x, float y) const noexcept;

    int W = 300, H = 210;
    float cx = 150.0f, cy = 90.0f, restR = 64.0f;
    std::vector<P> ps;
    std::vector<float> field, temp;
    std::vector<int> cellStart, cellNext;
    std::vector<float> noiseTable;
    juce::Image lava, glowImage;
    juce::Random rng { 0x1a7a };
    float comX = 150.0f, comY = 90.0f;
    float time = 0.0f, heat = 0.5f, lastLevel = 0.0f, flowX = 0.0f, flowY = 0.0f;
    int lastDripCount = -1;
};
