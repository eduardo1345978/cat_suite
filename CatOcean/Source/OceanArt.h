#pragma once

#include "OceanSim.h"
#include <vector>

/**
    Cat Ocean: draws the ocean as pixel art on a 480 x 270 canvas (the stream shows every pixel
    as a 4 x 4 block: 1920 x 1080). No fonts or images: everything, the text too, is drawn pixel by pixel,
    so it can run on the stream thread.
*/
namespace ocean
{
using Colour = juce::uint32;   // 0xAARRGGBB

class Canvas
{
public:
    Canvas() : pixels ((size_t) (W * H), 0xff000000) {}
    Colour* data() noexcept { return pixels.data(); }
    const Colour* data() const noexcept { return pixels.data(); }

    void set (int x, int y, Colour c) noexcept { if ((unsigned) x < (unsigned) W && (unsigned) y < (unsigned) H) pixels[(size_t) (y * W + x)] = c | 0xff000000u; }
    Colour get (int x, int y) const noexcept { return (unsigned) x < (unsigned) W && (unsigned) y < (unsigned) H ? pixels[(size_t) (y * W + x)] : 0xff000000u; }
    void blend (int x, int y, Colour c, float alpha) noexcept;
    void add (int x, int y, Colour c, float amount) noexcept;   // light
    void fillRect (int x, int y, int w, int h, Colour c, float alpha = 1.0f) noexcept;

    /** 3 x 5 pixel font (upper case). scale 1 or 2. Returns the width drawn. */
    int text (const juce::String&, int x, int y, Colour c, int scale = 1, Colour outline = 0xff10121c, bool withOutline = true) noexcept;
    static int textWidth (const juce::String&, int scale = 1) noexcept;

private:
    std::vector<Colour> pixels;
};

/** What the screen shows about the music. */
struct MusicInfo
{
    juce::String style, key, scale, section;
    int bpm = 0, piece = 0;
    float targetBpm = 0;
    juce::String targetKey;
    juce::String place;
};

class Painter
{
public:
    Painter();
    /** Draws one frame (dt = time since the last frame, for bubbles and sway). */
    void paint (Canvas&, const Sim&, const MusicInfo&, float dt);

    /** One fish on its own (for the tests / previews). */
    static void drawFish (Canvas&, const Fish&, float time);

private:
    struct Bubble { float x, y, r, speed, wobble; };
    struct Weed { float x; int height; Colour colour; float phase; bool glow = false; };
    struct Rock { int x, w, h; Colour colour; };

    void background (Canvas&, const Sim&, float t);
    void decor (Canvas&, const Sim&, float t);
    void boat (Canvas&, const Sim&, float t);
    void floor (Canvas&, const Sim&, float t);
    void bubbles (Canvas&, const Sim&, float dt);
    void weather (Canvas&, const Sim&, float t);
    void overlay (Canvas&, const Sim&, const MusicInfo&, float t);

    std::vector<Bubble> bubbleList;
    std::array<std::vector<Weed>, NumPlaces> weedSets;
    std::vector<Rock> rocks;
    std::vector<int> sandNoise;
    juce::Random rng { 1234 };
    float clock = 0, tipTimer = 0;
    int tip = 0, boardIndex = 0;
    float boardTimer = 0;
};
} // namespace ocean
