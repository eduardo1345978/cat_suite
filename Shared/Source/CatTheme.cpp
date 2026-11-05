#include "CatTheme.h"
#include "CatLookAndFeel.h"
#include "CatFonts.h"

namespace cat
{
namespace
{
    Theme& current()
    {
        static Theme t;
        return t;
    }

    Theme make (const char* name, juce::uint32 bg, juce::uint32 panel, juce::uint32 panelLight, juce::uint32 raised, juce::uint32 outline,
                juce::uint32 text, juce::uint32 textDim, juce::uint32 accent)
    {
        Theme t;
        t.name = name;
        t.background = juce::Colour (bg); t.panel = juce::Colour (panel); t.panelLight = juce::Colour (panelLight);
        t.raised = juce::Colour (raised); t.outline = juce::Colour (outline); t.text = juce::Colour (text);
        t.textDim = juce::Colour (textDim); t.accent = juce::Colour (accent);
        return t;
    }

    void font (Theme& t, const char* data, int size, float scale = 1.0f) { t.fontData = data; t.fontSize = size; t.fontScale = scale; }

    Theme themeFor (const juce::String& plugin)
    {
        using namespace CatFonts;
        const auto p = plugin.toLowerCase();
        Theme t;
        if (p.startsWith ("chord"))           // backlit synth: black panel, wood cheeks, orange glow
        {
            t = make ("Backlit Synth", 0xff141518, 0xff1c1d21, 0xff24252a, 0xff2e2f35, 0xff34353b, 0xffece6dc, 0xff9a948a, 0xffff8a2a);
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff1b1c1f); t.knobRing = juce::Colour (0xff2a2b30); t.knobPointer = t.accent; t.knobGlow = true;
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Wood; t.corner = 8.0f;
            font (t, MichromaRegular_ttf, MichromaRegular_ttfSize, 0.82f);
        }
        else if (p.startsWith ("patt"))       // Eurorack module: rails, jacks, yellow LEDs
        {
            t = make ("Eurorack", 0xff0c0c0e, 0xff26282e, 0xff2f3138, 0xff3a3c43, 0xff45474f, 0xffededf0, 0xffa7a9b0, 0xffffd23f);
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff1a1b1f); t.knobRing = juce::Colour (0xff0f0f11); t.knobPointer = t.accent;
            t.button = ButtonStyle::Hardware; t.backdrop = Backdrop::Rack; t.corner = 3.0f;
            font (t, BarlowCondensedBold_ttf, BarlowCondensedBold_ttfSize, 1.12f);
        }
        else if (p.startsWith ("bend"))       // liquid chrome
        {
            t = make ("Liquid Chrome", 0xff0f1014, 0xff181a20, 0xff20232a, 0xff2a2e37, 0xff3a3f4a, 0xffe9edf5, 0xff9aa3b4, 0xff9db4ff);
            t.knob = KnobStyle::Chrome; t.knobPointer = juce::Colour (0xff0b0c0f);
            t.button = ButtonStyle::Metal; t.backdrop = Backdrop::ChromeSheen; t.corner = 14.0f;
            font (t, SyneExtraBold_ttf, SyneExtraBold_ttfSize, 1.0f);
        }
        else if (p.startsWith ("drum"))       // grey MPC hardware: rubber pads, black knobs, red
        {
            t = make ("MPC Hardware", 0xffb8b9b4, 0xffc9cac5, 0xffd6d7d2, 0xffe4e5e0, 0xff9c9d98, 0xff1c1c1b, 0xff4b4c47, 0xffd8452c);
            t.light = true;
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff1c1c1b); t.knobRing = juce::Colour (0xff9c9d98); t.knobPointer = juce::Colour (0xfff2f2ee);
            t.button = ButtonStyle::Hardware; t.backdrop = Backdrop::Plastic; t.corner = 6.0f;
            font (t, ArchivoBlackRegular_ttf, ArchivoBlackRegular_ttfSize, 0.95f);
        }
        else if (p.startsWith ("orbit"))      // deep space: stars, gold
        {
            t = make ("Deep Space", 0xff070915, 0xff10142b, 0xff161b38, 0xff1f2548, 0xff2a3160, 0xffe8ecff, 0xff9aa6d8, 0xffffd27a);
            t.knob = KnobStyle::Glow; t.knobBody = juce::Colour (0xff15123a); t.knobGlow = true;
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Stars; t.corner = 12.0f;
            font (t, Exo2Bold_ttf, Exo2Bold_ttfSize, 1.0f);
        }
        else if (p.startsWith ("sing"))       // radio studio: on air red, navy
        {
            t = make ("On Air", 0xff0b0e1c, 0xff141830, 0xff1a1f3a, 0xff232947, 0xff2c3356, 0xffe9ecf7, 0xffa7aecb, 0xffff3b4e);
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff161a2e); t.knobRing = juce::Colour (0xff3a2034); t.knobPointer = juce::Colour (0xffff6676); t.knobGlow = true;
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Studio; t.corner = 10.0f;
            font (t, BebasNeueRegular_ttf, BebasNeueRegular_ttfSize, 1.25f);
        }
        else if (p.startsWith ("life"))       // bioluminescence
        {
            t = make ("Bioluminescence", 0xff03161a, 0xff082226, 0xff0c2b30, 0xff12363c, 0xff1a4a50, 0xffd8fff4, 0xff8fd9c6, 0xff5cffd6);
            t.knob = KnobStyle::Glow; t.knobBody = juce::Colour (0xff062226); t.knobGlow = true;
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Biolum; t.corner = 14.0f;
            font (t, SoraBold_ttf, SoraBold_ttfSize, 0.95f);
        }
        else if (p.startsWith ("bass"))       // silver acid box
        {
            t = make ("Silver Acid", 0xffcdcfd3, 0xffdcdee1, 0xffe6e7ea, 0xfff1f1f2, 0xffa2a5aa, 0xff1a1a1c, 0xff46474c, 0xffc1272d);
            t.light = true;
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff141416); t.knobRing = juce::Colour (0xffb2b4b8); t.knobPointer = juce::Colour (0xfff1f1f2);
            t.button = ButtonStyle::Hardware; t.backdrop = Backdrop::Silver; t.corner = 4.0f;
            font (t, RobotoCondensedBold_ttf, RobotoCondensedBold_ttfSize, 1.05f);
        }
        else if (p.startsWith ("glide"))      // nixie tubes and an amber scope
        {
            t = make ("Nixie & Scope", 0xff1d1a16, 0xff26221d, 0xff2e2923, 0xff3a332c, 0xff4a3a2a, 0xffede3d3, 0xffa89a86, 0xffff9c3a);
            t.knob = KnobStyle::Cream; t.knobPointer = juce::Colour (0xff2a1a08);
            t.button = ButtonStyle::Hardware; t.backdrop = Backdrop::Amber; t.corner = 6.0f;
            font (t, OswaldSemiBold_ttf, OswaldSemiBold_ttfSize, 1.05f);
        }
        else if (p.startsWith ("talk"))       // green terminal
        {
            t = make ("Green Terminal", 0xff061108, 0xff0b1a0d, 0xff0f2312, 0xff163018, 0xff1f4a24, 0xff7dff8a, 0xff4fc25c, 0xff7dff8a);
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff10190f); t.knobRing = juce::Colour (0xff1f4a24); t.knobPointer = t.accent; t.knobGlow = true;
            t.button = ButtonStyle::Terminal; t.backdrop = Backdrop::Terminal; t.corner = 2.0f;
            font (t, VT323Regular_ttf, VT323Regular_ttfSize, 1.3f);
            t.uiFont = true;
        }
        else if (p.startsWith ("muse"))       // mission control
        {
            t = make ("Mission Control", 0xff17191d, 0xff1e2126, 0xff252930, 0xff2e3238, 0xff3a3f46, 0xffdde3ea, 0xff8a93a0, 0xff4fd1ff);
            t.knob = KnobStyle::Gauge; t.knobBody = juce::Colour (0xff0a0f14); t.knobPointer = juce::Colour (0xffffb347);
            t.button = ButtonStyle::Hardware; t.backdrop = Backdrop::Console; t.corner = 8.0f;
            font (t, OrbitronExtraBold_ttf, OrbitronExtraBold_ttfSize, 0.85f);
        }
        else if (p.startsWith ("echo"))       // analog tape studio: cream metal, wood
        {
            t = make ("Analog Studio", 0xffd8d2c4, 0xffe4dfd3, 0xffece8de, 0xfff4f1ea, 0xffa79e8c, 0xff26221d, 0xff5a5348, 0xffc9561a);
            t.light = true;
            t.knob = KnobStyle::Cream; t.knobPointer = juce::Colour (0xff1a1714);
            t.button = ButtonStyle::Hardware; t.backdrop = Backdrop::Cream; t.corner = 6.0f;
            font (t, PlayfairDisplayBoldItalic_ttf, PlayfairDisplayBoldItalic_ttfSize, 1.0f);
        }
        else if (p.startsWith ("micro"))      // observatory: navy and brass
        {
            t = make ("Observatory", 0xff0b1530, 0xff111e3e, 0xff172649, 0xff1e2f57, 0xff2b3d6a, 0xffede3cc, 0xffb5a98e, 0xffd8b169);
            t.knob = KnobStyle::Brass; t.knobPointer = juce::Colour (0xff1a1206);
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Observatory; t.corner = 10.0f;
            font (t, CormorantSCBold_ttf, CormorantSCBold_ttfSize, 1.12f);
        }
        else if (p.startsWith ("phase"))      // black lab instrument, violet
        {
            t = make ("Phase Lab", 0xff111216, 0xff18191e, 0xff1e2026, 0xff272830, 0xff2e2f37, 0xffe4e2ec, 0xff9894a8, 0xffb88cff);
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff16171c); t.knobRing = juce::Colour (0xff2a2b32); t.knobPointer = t.accent; t.knobGlow = true;
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Lab; t.corner = 6.0f;
            font (t, RajdhaniBold_ttf, RajdhaniBold_ttfSize, 1.12f);
        }
        else if (p.startsWith ("glitch"))     // a broken screen: black, acid green, magenta errors
        {
            t = make ("Broken Screen", 0xff050607, 0xff0b0d10, 0xff12151a, 0xff1a1e24, 0xff2a313a, 0xffd9ffe9, 0xff6f9a84, 0xff3dff9a);
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff0d1013); t.knobRing = juce::Colour (0xff1f2a26); t.knobPointer = t.accent; t.knobGlow = true;
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Glitch; t.corner = 2.0f;
            font (t, VT323Regular_ttf, VT323Regular_ttfSize, 1.3f);
        }
        else if (p.startsWith ("hum"))        // vocal booth: acoustic foam, a warm tally light
        {
            t = make ("Vocal Booth", 0xff17141a, 0xff1f1b22, 0xff27222b, 0xff302a35, 0xff3b3440, 0xfff3e9e2, 0xffa89aa3, 0xffff6f4f);
            t.knob = KnobStyle::Hardware; t.knobBody = juce::Colour (0xff221e26); t.knobRing = juce::Colour (0xff3b3440); t.knobPointer = t.accent; t.knobGlow = true;
            t.button = ButtonStyle::Glow; t.backdrop = Backdrop::Foam; t.corner = 12.0f;
            font (t, SyneExtraBold_ttf, SyneExtraBold_ttfSize, 1.0f);
        }
        return t;
    }

    juce::Typeface::Ptr typeface()
    {
        static juce::Typeface::Ptr tf;
        static const char* loaded = nullptr;
        const auto& t = current();
        if (t.fontData != nullptr && loaded != t.fontData)
        {
            tf = juce::Typeface::createSystemTypefaceFor (t.fontData, (size_t) t.fontSize);
            loaded = t.fontData;
        }
        return t.fontData != nullptr ? tf : nullptr;
    }

    void woodCheek (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::ColourGradient wood (juce::Colour (0xff3e2414), r.getX(), 0.0f, juce::Colour (0xff3a2112), r.getRight(), 0.0f, false);
        wood.addColour (0.5, juce::Colour (0xff6e4428));
        g.setGradientFill (wood);
        g.fillRect (r);
        g.setColour (juce::Colour (0x22000000));
        for (float y = r.getY() + 7.0f; y < r.getBottom(); y += 13.0f)
            g.drawLine (r.getX() + 2.0f, y, r.getRight() - 2.0f, y + 5.0f, 1.0f);
    }
}

const Theme& theme() { return current(); }

void applyTheme (const juce::String& pluginName)
{
    auto& t = current();
    t = themeFor (pluginName);
    Colours::background = t.background;
    Colours::panel = t.panel;
    Colours::panelLight = t.panelLight;
    Colours::raised = t.raised;
    Colours::outline = t.outline;
    Colours::text = t.text;
    Colours::textDim = t.textDim;
    Colours::accent = t.accent;
    if (t.light)
    {
        // darker, stronger helper colours on a light panel
        Colours::blue = juce::Colour (0xff3f5a9e); Colours::purple = juce::Colour (0xff6a4f9e); Colours::red = juce::Colour (0xffa3332c);
        Colours::teal = juce::Colour (0xff2f7a80); Colours::green = juce::Colour (0xff3d7f52); Colours::slate = juce::Colour (0xff4f5a75);
    }
}

juce::Font lcdFont (float height)
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (CatFonts::VT323Regular_ttf, (size_t) CatFonts::VT323Regular_ttfSize);
    return tf != nullptr ? juce::Font (juce::FontOptions (tf).withHeight (height)) : juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

juce::Font displayFont (float height)
{
    if (auto tf = typeface())
        return juce::Font (juce::FontOptions (tf).withHeight (height * current().fontScale));
    return juce::Font (juce::FontOptions (height).withStyle ("Bold"));
}

void paintBackdrop (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto& t = current();
    const auto b = bounds.toFloat();
    auto vertical = [&] (juce::Colour top, juce::Colour bottom)
    {
        g.setGradientFill (juce::ColourGradient (top, 0.0f, b.getY(), bottom, 0.0f, b.getBottom(), false));
        g.fillRect (b);
    };
    auto radial = [&] (juce::Colour centre, juce::Colour edge, float cx, float cy)
    {
        g.setGradientFill (juce::ColourGradient (centre, b.getX() + b.getWidth() * cx, b.getY() + b.getHeight() * cy,
                                                 edge, b.getX(), b.getY(), true));
        g.fillRect (b);
    };
    auto stars = [&] (int count, float alpha, juce::Colour c)
    {
        juce::Random rng (1234);
        for (int i = 0; i < count; ++i)
        {
            const float x = b.getX() + rng.nextFloat() * b.getWidth(), y = b.getY() + rng.nextFloat() * b.getHeight();
            const float s = 0.6f + rng.nextFloat() * 1.6f;
            g.setColour (c.withAlpha (alpha * (0.3f + 0.7f * rng.nextFloat())));
            g.fillEllipse (x, y, s, s);
        }
    };

    switch (t.backdrop)
    {
        case Backdrop::Plain:
            g.fillAll (t.background);
            break;
        case Backdrop::Wood:
            vertical (t.background.brighter (0.12f), t.background);
            woodCheek (g, b.withWidth (8.0f));
            woodCheek (g, b.withLeft (b.getRight() - 8.0f));
            break;
        case Backdrop::Rack:
        {
            g.fillAll (t.background);
            for (auto rail : { b.withHeight (9.0f), b.withTop (b.getBottom() - 9.0f) })
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9bcc2), 0.0f, rail.getY(), juce::Colour (0xff7e8188), 0.0f, rail.getBottom(), false));
                g.fillRect (rail);
                g.setColour (juce::Colour (0xff2a2b2f));
                for (float x = b.getX() + 30.0f; x < b.getRight(); x += 120.0f)
                    g.fillEllipse (x, rail.getCentreY() - 2.5f, 9.0f, 5.0f);
            }
            break;
        }
        case Backdrop::ChromeSheen:
            radial (juce::Colour (0xff2b2f3a), juce::Colour (0xff08090b), 0.7f, 0.25f);
            break;
        case Backdrop::Plastic:
            vertical (juce::Colour (0xffcacbc6), juce::Colour (0xffadaea9));
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawHorizontalLine (bounds.getY() + 1, b.getX(), b.getRight());
            break;
        case Backdrop::Stars:
            radial (juce::Colour (0xff141b3a), juce::Colour (0xff030409), 0.4f, 0.45f);
            stars (160, 0.8f, juce::Colours::white);
            break;
        case Backdrop::Studio:
            radial (juce::Colour (0xff1d2440), juce::Colour (0xff080a14), 0.3f, 0.15f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0x26ff3b4e), b.getRight() - 80.0f, b.getY() + 10.0f,
                                                     juce::Colour (0x00ff3b4e), b.getRight() - 380.0f, b.getY() + 220.0f, true));
            g.fillRect (b);
            break;
        case Backdrop::Biolum:
            radial (juce::Colour (0xff062a2e), juce::Colour (0xff010a0c), 0.3f, 0.5f);
            stars (70, 0.5f, juce::Colour (0xff5cffd6));
            break;
        case Backdrop::Silver:
            vertical (juce::Colour (0xffe7e8ea), juce::Colour (0xffc4c6ca));   // (Bass-Cat draws its own stripe)
            break;
        case Backdrop::Amber:
            vertical (juce::Colour (0xff2a2622), juce::Colour (0xff17140f));
            break;
        case Backdrop::Terminal:
            radial (juce::Colour (0xff0e2a12), juce::Colour (0xff030a04), 0.5f, 0.45f);
            g.setColour (juce::Colour (0x18000000));
            for (int y = bounds.getY(); y < bounds.getBottom(); y += 3)
                g.fillRect (bounds.getX(), y, bounds.getWidth(), 1);
            break;
        case Backdrop::Console:
            vertical (juce::Colour (0xff23262c), juce::Colour (0xff141619));
            g.setColour (juce::Colour (0x0a4fd1ff));
            for (float x = b.getX(); x < b.getRight(); x += 40.0f) g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            for (float y = b.getY(); y < b.getBottom(); y += 40.0f) g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
            break;
        case Backdrop::Cream:
            vertical (juce::Colour (0xffe4dfd3), juce::Colour (0xffcfc8b8));
            g.setColour (juce::Colour (0x0d000000));
            for (float x = b.getX(); x < b.getRight(); x += 3.0f) g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            woodCheek (g, b.withWidth (8.0f));
            woodCheek (g, b.withLeft (b.getRight() - 8.0f));
            break;
        case Backdrop::Observatory:
            radial (juce::Colour (0xff16264a), juce::Colour (0xff060b1a), 0.35f, 0.45f);
            stars (60, 0.45f, juce::Colour (0xffede3cc));
            g.setColour (juce::Colour (0x1ad8b169));
            g.drawEllipse (b.withSizeKeepingCentre (b.getHeight() * 1.3f, b.getHeight() * 1.3f).translated (-b.getWidth() * 0.18f, 0.0f), 1.5f);
            break;
        case Backdrop::Lab:
            vertical (juce::Colour (0xff1e1f24), juce::Colour (0xff0e0f12));
            g.setColour (juce::Colour (0x0cb88cff));
            for (float x = b.getX(); x < b.getRight(); x += 24.0f) g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            break;
        case Backdrop::Foam:
        {
            // acoustic foam: a checkerboard of pyramid tiles, lit from the top
            g.fillAll (juce::Colour (0xff141116));
            const float tile = 46.0f;
            int row = 0;
            for (float y = b.getY(); y < b.getBottom(); y += tile, ++row)
            {
                int col = 0;
                for (float x = b.getX(); x < b.getRight(); x += tile, ++col)
                {
                    const juce::Point<float> c (x + tile * 0.5f, y + tile * 0.5f);
                    const juce::Point<float> tl (x, y), tr (x + tile, y), br (x + tile, y + tile), bl (x, y + tile);
                    const bool turn = ((row + col) & 1) != 0;
                    auto face = [&] (juce::Point<float> a, juce::Point<float> e, float shade)
                    {
                        juce::Path tri; tri.addTriangle (a, e, c);
                        g.setColour (juce::Colour (0xff221d25).withMultipliedBrightness (shade));
                        g.fillPath (tri);
                    };
                    face (tl, tr, turn ? 1.35f : 1.15f);
                    face (tr, br, turn ? 0.75f : 0.95f);
                    face (br, bl, turn ? 0.55f : 0.65f);
                    face (bl, tl, turn ? 1.05f : 1.25f);
                }
            }
            g.setGradientFill (juce::ColourGradient (juce::Colour (0x22ff6f4f), b.getCentreX(), b.getY(), juce::Colour (0x00ff6f4f), b.getCentreX(), b.getY() + b.getHeight() * 0.7f, true));
            g.fillRect (b);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0x00000000), 0.0f, b.getY() + b.getHeight() * 0.4f, juce::Colour (0x88000000), 0.0f, b.getBottom(), false));
            g.fillRect (b);
            break;
        }
        case Backdrop::Glitch:
        {
            g.fillAll (juce::Colour (0xff050607));
            g.setColour (juce::Colour (0x0d3dff9a));
            for (float y = b.getY() + 4.0f; y < b.getBottom(); y += 8.0f)
                for (float x = b.getX() + 4.0f; x < b.getRight(); x += 8.0f)
                    g.fillRect (x, y, 1.0f, 1.0f);
            juce::Random rng (77);
            for (int i = 0; i < 40; ++i)          // dead pixels and stray blocks
            {
                const juce::Colour c = rng.nextInt (3) == 0 ? juce::Colour (0x40ff2bd6) : rng.nextInt (2) == 0 ? juce::Colour (0x303dff9a) : juce::Colour (0x2833d6ff);
                g.setColour (c);
                const float w = (float) (2 + rng.nextInt (rng.nextInt (6) == 0 ? 60 : 8)), h = (float) (2 + rng.nextInt (4));
                g.fillRect (b.getX() + rng.nextFloat() * b.getWidth(), b.getY() + rng.nextFloat() * b.getHeight(), w, h);
            }
            g.setColour (juce::Colour (0x30000000));
            for (int y = bounds.getY(); y < bounds.getBottom(); y += 3) g.fillRect (bounds.getX(), y, bounds.getWidth(), 1);
            break;
        }
    }
}
} // namespace cat
