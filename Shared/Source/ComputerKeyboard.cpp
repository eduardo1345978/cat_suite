#include "ComputerKeyboard.h"
#include "CatLookAndFeel.h"

namespace cat
{
namespace
{
    // one octave and a bit, laid out like a piano on the letter rows
    const char* const keyChars = "awsedftgyzhujkolp";
}

ComputerKeyboardBar::ComputerKeyboardBar (juce::MidiKeyboardState& s) : state (s)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Play with your computer keyboard: A S D F G H J K L = white keys, W E T Y U O P = black keys. "
                "Left / Right arrow = octave, Down / Up arrow = velocity. Click here if the keys stop playing.");
    startTimerHz (20);
}

ComputerKeyboardBar::~ComputerKeyboardBar()
{
    releaseAll();
}

int ComputerKeyboardBar::semitoneFor (juce::juce_wchar c) noexcept
{
    switch (juce::CharacterFunctions::toLowerCase (c))
    {
        case 'a': return 0;  case 'w': return 1;  case 's': return 2;  case 'e': return 3;  case 'd': return 4;
        case 'f': return 5;  case 't': return 6;  case 'g': return 7;  case 'y': case 'z': return 8;
        case 'h': return 9;  case 'u': return 10; case 'j': return 11; case 'k': return 12; case 'o': return 13;
        case 'l': return 14; case 'p': return 15;
        default:  return -1;
    }
}

bool ComputerKeyboardBar::typingIntoText()
{
    auto* f = juce::Component::getCurrentlyFocusedComponent();
    return dynamic_cast<juce::TextEditor*> (f) != nullptr;
}

bool ComputerKeyboardBar::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (typingIntoText()) return false;
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;

    const int code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        releaseAll();
        octave = juce::jlimit (1, 9, octave + (code == juce::KeyPress::rightKey ? 1 : -1));
        repaint();
        return true;
    }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        velocity = juce::jlimit (10, 127, velocity + (code == juce::KeyPress::upKey ? 15 : -15));
        repaint();
        return true;
    }

    const int semi = semitoneFor (key.getTextCharacter() != 0 ? key.getTextCharacter() : (juce::juce_wchar) code);
    if (semi < 0) return false;
    for (const auto& h : held)
        if (h.keyCode == code) return true;          // key repeat
    const int note = juce::jlimit (0, 127, 12 * octave + semi);
    state.noteOn (1, note, (float) velocity / 127.0f);
    held.push_back ({ code, note });
    repaint();
    return true;
}

bool ComputerKeyboardBar::keyStateChanged (bool, juce::Component*)
{
    bool changed = false;
    for (size_t i = 0; i < held.size();)
    {
        if (! juce::KeyPress::isKeyCurrentlyDown (held[i].keyCode))
        {
            state.noteOff (1, held[i].note, 0.0f);
            held.erase (held.begin() + (long) i);
            changed = true;
            continue;
        }
        ++i;
    }
    if (changed) repaint();
    return false;            // let others see key releases too
}

void ComputerKeyboardBar::timerCallback()
{
    // safety net: a key released while the window lost focus
    if (! held.empty()) keyStateChanged (false, this);
}

void ComputerKeyboardBar::releaseAll()
{
    for (const auto& h : held) state.noteOff (1, h.note, 0.0f);
    held.clear();
}

void ComputerKeyboardBar::mouseDown (const juce::MouseEvent&)
{
    if (auto* p = getParentComponent()) p->grabKeyboardFocus();
}

void ComputerKeyboardBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colours::panelLight);
    g.fillRect (b);
    g.setColour (Colours::outline);
    g.drawHorizontalLine (0, b.getX(), b.getRight());

    auto r = b.reduced (10.0f, 3.0f);
    g.setFont (catFont (11.0f, true));
    g.setColour (Colours::accent);
    g.drawText ("COMPUTER KEYBOARD", r.removeFromLeft (128.0f), juce::Justification::centredLeft);

    // little key caps; the pressed ones light up
    const juce::String keys (keyChars);
    for (int i = 0; i < keys.length(); ++i)
    {
        const auto c = keys[i];
        if (c == 'z') continue;              // same note as Y
        const int semi = semitoneFor (c);
        const bool black = semi % 12 == 1 || semi % 12 == 3 || semi % 12 == 6 || semi % 12 == 8 || semi % 12 == 10;
        bool down = false;
        for (const auto& h : held) down = down || h.note == 12 * octave + semi;
        auto cap = r.removeFromLeft (17.0f).reduced (1.5f, 1.0f);
        g.setColour (down ? Colours::accent : (black ? Colours::background : Colours::raised));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (down ? Colours::background : (black ? Colours::textDim : Colours::text));
        g.setFont (catFont (10.0f, true));
        g.drawText (juce::String::charToString (c).toUpperCase() + (c == 'y' ? "/Z" : ""), cap.expanded (c == 'y' ? 6.0f : 0.0f, 0.0f),
                    juce::Justification::centred);
        if (c == 'y') r.removeFromLeft (6.0f);
    }

    g.setFont (catFont (11.0f));
    g.setColour (Colours::textDim);
    r.removeFromLeft (14.0f);
    const juce::String info = "A = C" + juce::String (octave - 2) + "  (Left / Right)      Velocity  " + juce::String (velocity)
                            + "  (Down / Up)";
    g.drawText (info, r, juce::Justification::centredLeft);
}

} // namespace cat
