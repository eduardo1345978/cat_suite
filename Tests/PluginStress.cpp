// Stress test for one Cat plug-in, compiled straight from its shared code (no VST3 wrapper),
// meant to run under AddressSanitizer + bounds-checked containers:
//   - audio thread: processBlock with a running transport and random notes
//   - message thread: the real editor on a (virtual) desktop, random clicks, drags,
//     button presses, combo / slider changes, presets, state save + load
// Any out-of-bounds write or use-after-free stops the run with a report.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <atomic>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
namespace juce::detail { bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages); }

// Pumps the message loop (timers, async updates, repaints) for a while.
static void pump (int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
        if (! juce::detail::dispatchNextMessageOnSystemQueue (true))
            juce::Thread::sleep (1);
}

using namespace juce;

struct FakePlayHead : AudioPlayHead
{
    std::atomic<double> ppq { 0.0 };
    std::atomic<bool> playing { true };
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i;
        i.setBpm (120.0);
        i.setTimeSignature (TimeSignature { 4, 4 });
        i.setIsPlaying (playing.load());
        i.setPpqPosition (ppq.load());
        i.setPpqPositionOfLastBarStart (std::floor (ppq.load() / 4.0) * 4.0);
        i.setTimeInSamples ((int64) (ppq.load() * 24000.0));
        return i;
    }
};

static void collect (Component& c, Array<Component*>& out)
{
    for (auto* ch : c.getChildren())
    {
        if (ch->isVisible()) out.add (ch);
        collect (*ch, out);
    }
}

static bool unsafeButton (const String& t)
{
    for (auto* bad : { "Save", "Load", "Export", "Import", "...", "Drag", "Folder", "Open", "Rename", "Delete" })
        if (t.containsIgnoreCase (bad)) return true;
    return false;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double seconds = argc > 1 ? String (argv[1]).getDoubleValue() : 60.0;
    const int seed = argc > 2 ? String (argv[2]).getIntValue() : 1;
    Random rng (seed);

    std::unique_ptr<AudioProcessor> p (createPluginFilter());
    const double sr = 48000.0;
    const int bs = 256;
    p->setPlayConfigDetails (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels(), sr, bs);
    p->prepareToPlay (sr, bs);
    FakePlayHead ph;
    p->setPlayHead (&ph);
    std::cout << "plug-in: " << p->getName() << ", presets " << p->getNumPrograms() << ", " << seconds << " s, seed " << seed << std::endl;

    std::atomic<bool> stop { false };
    CriticalSection stateLock;  // hosts never call setState during processBlock... Cubase can, but keep it as hosts do: callbackLock
    std::thread audio ([&]
    {
        Random r (seed * 7 + 1);
        AudioBuffer<float> buf (jmax (2, p->getTotalNumOutputChannels()), bs);
        MidiBuffer midi;
        int held = -1;
        while (! stop)
        {
            buf.clear();
            midi.clear();
            if (r.nextInt (40) == 0)
            {
                if (held >= 0) midi.addEvent (MidiMessage::noteOff (1, held), r.nextInt (bs));
                held = 36 + r.nextInt (60);
                midi.addEvent (MidiMessage::noteOn (1, held, (uint8) (1 + r.nextInt (127))), r.nextInt (bs));
            }
            if (r.nextInt (2000) == 0) ph.playing = ! ph.playing.load();
            {
                const ScopedLock sl (p->getCallbackLock());
                p->processBlock (buf, midi);
            }
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                for (int i = 0; i < bs; ++i)
                    if (! std::isfinite (buf.getSample (ch, i))) { std::cout << "!! non-finite audio" << std::endl; std::abort(); }
            ph.ppq = ph.ppq.load() + (ph.playing ? bs / (sr * 0.5) : 0.0);
            std::this_thread::sleep_for (std::chrono::microseconds (r.nextInt (2) == 0 ? 0 : 500));
        }
    });

    std::unique_ptr<AudioProcessorEditor> ed (p->createEditorIfNeeded());
    std::unique_ptr<Component> window;
    if (ed != nullptr)
    {
        ed->setTopLeftPosition (0, 0);
        ed->addToDesktop (0);
        ed->setVisible (true);
    }

    const auto start = Time::getMillisecondCounterHiRes();
    int actions = 0;
    while (Time::getMillisecondCounterHiRes() - start < seconds * 1000.0)
    {
        pump (5 + rng.nextInt (20));
        if (ed == nullptr) continue;
        if (rng.nextInt (3) == 0)
        {
            Image img (Image::ARGB, jmax (1, ed->getWidth()), jmax (1, ed->getHeight()), true);
            Graphics g (img);
            ed->paintEntireComponent (g, true);
        }

        // close any popup menus that our clicks opened
        if (rng.nextInt (3) == 0) PopupMenu::dismissAllActiveMenus();

        Array<Component*> comps;
        collect (*ed, comps);
        if (comps.isEmpty()) continue;
        auto* c = comps[rng.nextInt (comps.size())];
        ++actions;

        const int what = rng.nextInt (100);
        if (what < 30)
        {
            if (auto* b = dynamic_cast<Button*> (c))
            {
                if (! unsafeButton (b->getButtonText()) && ! unsafeButton (b->getName()))
                    b->triggerClick();
                continue;
            }
        }
        if (what < 45)
        {
            if (auto* cb = dynamic_cast<ComboBox*> (c))
            {
                if (cb->getNumItems() > 0) cb->setSelectedItemIndex (rng.nextInt (cb->getNumItems()), sendNotificationSync);
                continue;
            }
            if (auto* s = dynamic_cast<Slider*> (c))
            {
                s->setValue (s->getMinimum() + rng.nextDouble() * (s->getMaximum() - s->getMinimum()), sendNotificationSync);
                continue;
            }
        }
        if (what < 80)
        {
            // mouse gestures sent straight to the component (no desktop window needed)
            {
                const auto r = c->getLocalBounds();
                if (r.isEmpty()) continue;
                auto pt = [&] { return Point<float> ((float) rng.nextInt (jmax (1, r.getWidth())), (float) rng.nextInt (jmax (1, r.getHeight()))); };
                auto src = Desktop::getInstance().getMainMouseSource();
                const auto mods = rng.nextInt (6) == 0 ? ModifierKeys (ModifierKeys::rightButtonModifier) : ModifierKeys (ModifierKeys::leftButtonModifier);
                const auto now = Time::getCurrentTime();
                const auto down = pt();
                auto ev = [&] (Point<float> pos, ModifierKeys m, bool dragged)
                {
                    return MouseEvent (src, pos, m, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, c, now, down, now, 1, dragged);
                };
                Component::SafePointer<Component> safe (c);
                c->mouseEnter (ev (down, ModifierKeys(), false));
                if (safe) c->mouseMove (ev (down, ModifierKeys(), false));
                if (safe) c->mouseDown (ev (down, mods, false));
                const int steps = rng.nextInt (6);
                for (int k = 0; k < steps && safe; ++k) c->mouseDrag (ev (pt(), mods, true));
                if (safe) c->mouseUp (ev (pt(), ModifierKeys(), steps > 0));
                if (safe && rng.nextInt (8) == 0) c->mouseDoubleClick (ev (down, mods, false));
                if (safe && rng.nextInt (4) == 0) c->mouseWheelMove (ev (pt(), ModifierKeys(), false), MouseWheelDetails { 0.0f, rng.nextFloat() - 0.5f, false, false, false });
                if (safe) c->mouseExit (ev (down, ModifierKeys(), false));
            }
            continue;
        }
        if (what < 88)
        {
            auto& params = p->getParameters();
            if (! params.isEmpty())
                params[rng.nextInt (params.size())]->setValueNotifyingHost (rng.nextFloat());
            continue;
        }
        if (what < 94)
        {
            if (p->getNumPrograms() > 1) p->setCurrentProgram (rng.nextInt (p->getNumPrograms()));
            continue;
        }
        {
            MemoryBlock mb;
            p->getStateInformation (mb);
            p->setStateInformation (mb.getData(), (int) mb.getSize());
        }
    }

    // close and reopen the editor a few times (Cubase does this a lot)
    for (int k = 0; k < 5 && ed != nullptr; ++k)
    {
        p->editorBeingDeleted (ed.get());
        ed.reset();
        pump (50);
        ed.reset (p->createEditorIfNeeded());
        ed->addToDesktop (0);
        ed->setVisible (true);
        pump (100);
    }

    stop = true;
    audio.join();

    // The Cubase crash: LINK panel open -> close the editor -> remove the plug-in.
    if (ed != nullptr)
    {
        Array<Component*> comps;
        collect (*ed, comps);
        for (auto* c : comps)
            if (auto* b = dynamic_cast<Button*> (c))
                if (b->getName() == "Link") { b->triggerClick(); std::cout << "opened the LINK panel" << std::endl; break; }
        pump (300);
        ed.reset();
        pump (600);
    }
    p->releaseResources();
    p.reset();
    pump (600);   // the panel's timer would now run on a deleted plug-in
    std::cout << "STRESS OK after " << actions << " UI actions" << std::endl;
    return 0;
}
