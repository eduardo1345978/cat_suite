#include "CryptLook.h"
#include <map>

namespace tomb::look
{
using namespace juce;

namespace
{
const std::map<juce_wchar, std::array<const char*, 7>>& glyphs()
{
    static const std::map<juce_wchar, std::array<const char*, 7>> g {
        { 'A', { " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" } },
        { 'B', { "#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### " } },
        { 'C', { " ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### " } },
        { 'D', { "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### " } },
        { 'E', { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####" } },
        { 'F', { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    " } },
        { 'G', { " ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ####" } },
        { 'H', { "#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" } },
        { 'I', { " ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " } },
        { 'J', { "  ###", "   # ", "   # ", "   # ", "   # ", "#  # ", " ##  " } },
        { 'K', { "#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #" } },
        { 'L', { "#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####" } },
        { 'M', { "#   #", "## ##", "# # #", "# # #", "#   #", "#   #", "#   #" } },
        { 'N', { "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #" } },
        { 'O', { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " } },
        { 'P', { "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    " } },
        { 'Q', { " ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #" } },
        { 'R', { "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #" } },
        { 'S', { " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### " } },
        { 'T', { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  " } },
        { 'U', { "#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " } },
        { 'V', { "#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  " } },
        { 'W', { "#   #", "#   #", "#   #", "# # #", "# # #", "# # #", " # # " } },
        { 'X', { "#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #" } },
        { 'Y', { "#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  " } },
        { 'Z', { "#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####" } },
        { '0', { " ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### " } },
        { '1', { "  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " } },
        { '2', { " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####" } },
        { '3', { "#####", "   # ", "  #  ", "   # ", "    #", "#   #", " ### " } },
        { '4', { "   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # " } },
        { '5', { "#####", "#    ", "#### ", "    #", "    #", "#   #", " ### " } },
        { '6', { "  ## ", " #   ", "#    ", "#### ", "#   #", "#   #", " ### " } },
        { '7', { "#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   " } },
        { '8', { " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### " } },
        { '9', { " ### ", "#   #", "#   #", " ####", "    #", "   # ", " ##  " } },
        { '-', { "     ", "     ", "     ", " ### ", "     ", "     ", "     " } },
        { '+', { "     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     " } },
        { '.', { "     ", "     ", "     ", "     ", "     ", " ##  ", " ##  " } },
        { ',', { "     ", "     ", "     ", "     ", " ##  ", "  #  ", " #   " } },
        { ':', { "     ", " ##  ", " ##  ", "     ", " ##  ", " ##  ", "     " } },
        { '/', { "    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    " } },
        { '%', { "##  #", "##  #", "   # ", "  #  ", " #   ", "#  ##", "#  ##" } },
        { '!', { "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "     ", "  #  " } },
        { '?', { " ### ", "#   #", "    #", "   # ", "  #  ", "     ", "  #  " } },
        { '_', { "     ", "     ", "     ", "     ", "     ", "     ", "#####" } },
        { '\'', { "  #  ", "  #  ", " #   ", "     ", "     ", "     ", "     " } },
        { '(', { "   # ", "  #  ", " #   ", " #   ", " #   ", "  #  ", "   # " } },
        { ')', { " #   ", "  #  ", "   # ", "   # ", "   # ", "  #  ", " #   " } },
        { '&', { " ##  ", "#  # ", "#  # ", " ##  ", "# # #", "#  # ", " ## #" } },
        { '>', { " #   ", "  #  ", "   # ", "    #", "   # ", "  #  ", " #   " } },
        { '<', { "   # ", "  #  ", " #   ", "#    ", " #   ", "  #  ", "   # " } },
        { '=', { "     ", "     ", "#####", "     ", "#####", "     ", "     " } },
        { '#', { " # # ", " # # ", "#####", " # # ", "#####", " # # ", " # # " } },
    };
    return g;
}
}

float pixelTextWidth (const String& text, float px)
{
    return (float) text.length() * 6.0f * px - px;
}

float pixelText (Graphics& g, const String& text, float x, float y, float px, Colour c, bool shadow)
{
    const auto& gl = glyphs();
    const String up = text.toUpperCase();
    for (int pass = shadow ? 0 : 1; pass < 2; ++pass)
    {
        g.setColour (pass == 0 ? ink.withAlpha (0.9f) : c);
        const float off = pass == 0 ? px : 0.0f;
        float cx = x;
        for (auto ch : up)
        {
            if (auto it = gl.find (ch); it != gl.end())
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (it->second[(size_t) row][col] == '#')
                            g.fillRect (cx + (float) col * px + off, y + (float) row * px + off, px, px);
            cx += 6.0f * px;
        }
    }
    return pixelTextWidth (up, px);
}

void pixelTextCentred (Graphics& g, const String& text, Rectangle<float> box, float px, Colour c, bool shadow)
{
    const float w = pixelTextWidth (text, px);
    pixelText (g, text, std::round (box.getCentreX() - w * 0.5f), std::round (box.getCentreY() - 3.5f * px), px, c, shadow);
}

void wall (Graphics& g, Rectangle<float> r, float light, float px)
{
    g.setColour (mortar);
    g.fillRect (r);
    const float bw = px * 12.0f, bh = px * 6.0f;
    Random rnd (77);
    int row = 0;
    for (float y = r.getY(); y < r.getBottom(); y += bh, ++row)
        for (float x = r.getX() - (row % 2 == 0 ? 0.0f : bw * 0.5f); x < r.getRight(); x += bw)
        {
            const float shade = 0.75f + 0.5f * rnd.nextFloat();
            // light from the torches: the middle of the wall is darker
            const float dx = std::abs ((x + bw * 0.5f - r.getCentreX()) / (r.getWidth() * 0.5f));
            const float lit = juce::jlimit (0.0f, 1.0f, 0.35f + 0.65f * dx * light);
            const auto c = stone.interpolatedWith (stoneLight, shade * 0.5f).interpolatedWith (Colour (0xff3a2418), lit * 0.35f);
            g.setColour (c);
            g.fillRect (x + px, y + px, bw - px, bh - px);
            g.setColour (c.brighter (0.15f));
            g.fillRect (x + px, y + px, bw - px, px);
            if (rnd.nextFloat() < 0.12f)       // a crack or moss
            {
                g.setColour (rnd.nextBool() ? Colour (0xff1e3a24) : mortar);
                g.fillRect (x + px * (2.0f + (float) rnd.nextInt (8)), y + px * 3.0f, px * 2.0f, px);
            }
        }
}

void torch (Graphics& g, Point<float> base, float t, float px)
{
    // the holder
    g.setColour (Colour (0xff3b2a1e));
    g.fillRect (base.x - px, base.y, px * 2.0f, px * 7.0f);
    g.setColour (Colour (0xff5a4030));
    g.fillRect (base.x - px * 2.0f, base.y, px * 4.0f, px * 2.0f);
    // the glow
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (ember.withAlpha (0.05f + 0.01f * std::sin (t * 11.0f)));
        const float r = px * (5.0f + 4.0f * (float) i);
        g.fillEllipse (base.x - r, base.y - px * 4.0f - r, r * 2.0f, r * 2.0f);
    }
    // the flame: pixel columns that move
    const int heights[] = { 3, 5, 7, 5, 3 };
    for (int c = 0; c < 5; ++c)
    {
        const int h = heights[c] + (int) std::round (1.5f * std::sin (t * (9.0f + (float) c * 2.3f) + (float) c));
        for (int k = 0; k < h; ++k)
        {
            const float f = (float) k / (float) juce::jmax (1, h);
            g.setColour (f < 0.35f ? flame : f < 0.7f ? ember : blood);
            g.fillRect (base.x + (float) (c - 2) * px, base.y - px * (float) (k + 1), px, px);
        }
    }
}

void skull (Graphics& g, Point<float> c, float px, float jaw, Colour eyes)
{
    static const char* top[] = { "  #######  ", " ######### ", "###########", "###########", "##   #   ##", "##   #   ##", "###########", " #### #### ", "  #######  " };
    const float x0 = c.x - 5.5f * px, y0 = c.y - 6.0f * px;
    for (int r = 0; r < 9; ++r)
        for (int col = 0; col < 11; ++col)
        {
            const char ch = top[r][col];
            if (ch == '#') { g.setColour (r < 2 ? bone.brighter (0.2f) : bone); g.fillRect (x0 + (float) col * px, y0 + (float) r * px, px, px); }
            else if (r == 4 || r == 5)
                if ((col >= 2 && col <= 4) || (col >= 6 && col <= 8)) { g.setColour (eyes); g.fillRect (x0 + (float) col * px, y0 + (float) r * px, px, px); }
        }
    // the jaw, opening
    const float jy = y0 + 9.0f * px + std::round (jaw * 3.0f) * px;
    static const char* bottom[] = { " # # # # # ", "  #######  " };
    for (int r = 0; r < 2; ++r)
        for (int col = 0; col < 11; ++col)
            if (bottom[r][col] == '#') { g.setColour (boneDim.brighter (0.4f)); g.fillRect (x0 + (float) col * px, jy + (float) r * px, px, px); }
}

void panel (Graphics& g, Rectangle<float> r, float px)
{
    g.setColour (ink.withAlpha (0.85f));
    g.fillRect (r);
    g.setColour (Colour (0xff3a3448));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), px);
    g.fillRect (r.getX(), r.getY(), px, r.getHeight());
    g.setColour (Colour (0xff0a0910));
    g.fillRect (r.getX(), r.getBottom() - px, r.getWidth(), px);
    g.fillRect (r.getRight() - px, r.getY(), px, r.getHeight());
}

//==============================================================================
PixelButton::PixelButton (const String& text, Colour a) : Button (text), accent (a) {}

void PixelButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    const float px = 2.0f;
    g.setColour (ink);
    g.fillRect (r);
    auto face = r.reduced (px).translated (0.0f, down ? px : 0.0f);
    face.removeFromBottom (down ? 0.0f : px);
    g.setColour (on ? accent.withMultipliedSaturation (0.8f).withMultipliedBrightness (0.45f) : (over ? Colour (0xff2c2838) : Colour (0xff201d2a)));
    g.fillRect (face);
    g.setColour (on ? accent : Colour (0xff3a3448));
    g.fillRect (face.getX(), face.getY(), face.getWidth(), px);
    if (! down)
    {
        g.setColour (Colour (0xff0d0c12));
        g.fillRect (r.getX() + px, r.getBottom() - px * 2.0f, r.getWidth() - px * 2.0f, px);
    }
    pixelTextCentred (g, getButtonText(), face, textPx, on ? bone.brighter (0.3f) : (isEnabled() ? (over ? accent.brighter (0.3f) : accent) : boneDim.withAlpha (0.5f)));
}

//==============================================================================
PixelSlider::PixelSlider (AudioProcessorValueTreeState& state, const String& id, const String& cap, Colour c)
    : param (state.getParameter (id)), caption (cap), colour (c)
{
    addAndMakeVisible (slider);
    slider.setSliderSnapsToMousePosition (false);
    slider.setMouseDragSensitivity (180);
    slider.setVelocityBasedMode (false);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
    if (param != nullptr)
    {
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        param->addListener (this);
        slider.setTooltip (param->getName (64));
    }
}

PixelSlider::~PixelSlider()
{
    cancelPendingUpdate();
    if (param != nullptr) param->removeListener (this);
}

void PixelSlider::resized() { slider.setBounds (getLocalBounds()); }

void PixelSlider::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float v = param != nullptr ? param->getValue() : 0.0f;
    pixelText (g, caption, b.getX(), b.getY(), 2.0f, boneDim);
    const String value = param != nullptr ? param->getCurrentValueAsText() : String();
    pixelText (g, value, b.getRight() - pixelTextWidth (value.toUpperCase(), 2.0f), b.getY(), 2.0f, colour.brighter (0.2f));
    // the blocks
    constexpr int blocks = 20;
    const float top = b.getY() + 20.0f, h = 12.0f;
    const float bw = b.getWidth() / (float) blocks;
    const int lit = (int) std::round (v * (float) blocks);
    for (int i = 0; i < blocks; ++i)
    {
        const Rectangle<float> blk (b.getX() + (float) i * bw, top, bw - 2.0f, h);
        g.setColour (ink);
        g.fillRect (blk);
        if (i < lit)
        {
            const float f = (float) i / (float) blocks;
            g.setColour (colour.withMultipliedBrightness (0.55f + 0.45f * f));
            g.fillRect (blk.reduced (1.0f));
            g.setColour (colour.brighter (0.4f).withAlpha (0.8f));
            g.fillRect (blk.reduced (1.0f).withHeight (2.0f));
        }
        else
        {
            g.setColour (Colour (0xff1c1a24));
            g.fillRect (blk.reduced (1.0f));
        }
    }
}

//==============================================================================
Laf::Laf()
{
    setColour (ComboBox::backgroundColourId, ink);
    setColour (ComboBox::textColourId, bone);
    setColour (ComboBox::outlineColourId, Colour (0xff3a3448));
    setColour (ComboBox::arrowColourId, ember);
    setColour (PopupMenu::backgroundColourId, Colour (0xff100e16));
    setColour (PopupMenu::textColourId, bone);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3a2418));
    setColour (PopupMenu::highlightedTextColourId, flame);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff100e16));
    setColour (TooltipWindow::textColourId, bone);
    setColour (TooltipWindow::outlineColourId, Colour (0xff3a3448));
}

void Laf::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const Rectangle<float> r (0.0f, 0.0f, (float) w, (float) h);
    panel (g, r, 2.0f);
    // a pixel arrow
    g.setColour (box.findColour (ComboBox::arrowColourId));
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f - 2.0f;
    for (int k = 0; k < 4; ++k) g.fillRect (ax + (float) k * 2.0f, ay + (float) k * 2.0f, (float) (14 - k * 4) - (float) k * 2.0f + 2.0f, 2.0f);
}

Font Laf::getComboBoxFont (ComboBox&) { return FontOptions (13.0f, Font::bold); }

void Laf::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 26, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void Laf::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (Colour (0xff3a3448));
    g.drawRect (0, 0, w, h, 2);
}
} // namespace tomb::look
