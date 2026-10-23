// Cat Box: the Cat Suite groovebox (drums, arp, bass) - on the PC as an app, on the
// Raspberry Pi inside the box as the whole instrument.
//
//   Cat-Box                     window with the screen + a clickable front panel
//   Cat-Box --kiosk             only the 800 x 480 screen, full screen (the Pi in the box)
//   Cat-Box --data <folder>     where presets and the last state live
//   Cat-Box --selftest <out.wav>  plays everything offline, checks it, writes <out>.txt (exit code 0 = ok)

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "BoxController.h"
#include "BoxView.h"
#include "BoxLink.h"
#include "LinkPanel.h"
#include <cmath>
#include <cstdio>

namespace catbox
{
//==============================================================================
/** Offline check of the whole box: the controller map, every instrument, Rec, presets, memory. */
class SelfTest
{
public:
    int run (const juce::File& wavOut)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("catbox-selftest-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
        {
            BoxController box (dir);
            auto& audio = box.getAudio();
            audio.prepare (sr, block);
            auto press = [&box] (int b) { box.buttonDown (b); box.buttonUp (b); };
            auto tap = [&box] (int pad) { box.padDown (pad, 100); box.padUp (pad); };
            auto shiftPress = [&box, &press] (int b) { box.buttonDown (BtnShift); press (b); box.buttonUp (BtnShift); };
            auto soloOnly = [&box] (int inst) { for (int j = 0; j < NumInstruments; ++j) box.setMuted (j, inst >= 0 && j != inst); };

            // the hardware MIDI map: knob 1 (tempo) five clicks right and back, then the Instr button
            const float tempo0 = box.getData().globals[GTempo];
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 20, 5));
            check (std::abs (box.getData().globals[GTempo] - (tempo0 + 5.0f)) < 0.01f, "knob CC 20 turns the tempo",
                   juce::String (box.getData().globals[GTempo]));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 20, 123));
            check (std::abs (box.getData().globals[GTempo] - tempo0) < 0.01f, "knob turns back (two's complement)", {});
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 106, 127));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (16, 106, 0));
            check (box.getPage() == Arp, "Instr button goes to the Arp page", juce::String (box.getPage()));
            press (BtnInstrument); press (BtnInstrument);
            check (box.getPage() == PageChords, "after Bass comes the Chords page", juce::String (box.getPage()));
            press (BtnInstrument);
            check (box.getPage() == PageFx, "the fifth page is FX", juce::String (box.getPage()));
            press (BtnInstrument);
            check (box.getPage() == Drums, "pages go round", {});
            box.buttonDown (BtnInstrument);
            box.turnKnob (4, 2.0f);
            check (box.getPage() == Bass, "Instr held + a knob flips through the pages", juce::String (box.getPage()));
            box.turnKnob (4, -3.0f);
            box.buttonUp (BtnInstrument);
            check (box.getPage() == PageFx && std::abs (box.getData().globals[GTempo] - tempo0) < 0.01f,
                   "turning back goes round, and letting go of Instr stays there", juce::String (box.getPage()));
            press (BtnInstrument);
            check (box.getPage() == Drums, "a plain Instr press still goes to the next page", {});

            // Steps is per page: Steps on the drums does not turn the Arp pads into steps
            press (BtnSteps);
            box.setPage (Arp);
            check (! box.isStepView(), "Steps on Drums leaves the Arp pads playing", {});

            // the arp sounds at once, even for a short tap while it runs
            box.setPage (Drums);
            press (BtnPlay);
            soloOnly (Arp);
            renderSeconds (audio, 0.31, nullptr);
            box.setPage (Arp);
            box.padDown (0, 100);
            const auto shortTap = renderSeconds (audio, 0.04, nullptr);
            box.padUp (0);
            renderSeconds (audio, 0.5, nullptr);
            check (shortTap.rms > 0.002f, "a short tap on an Arp pad sounds at once", "rms " + juce::String (shortTap.rms, 4));

            // Arp: Rec = Hold, then a chord on the pads (notes 36.. on channel 16)
            press (BtnRec);
            check (box.isLatched(), "Rec holds the arp", {});
            for (int pad : { 0, 2, 4 })
            {
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (16, 36 + pad, (juce::uint8) 100));
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOff (16, 36 + pad));
            }
            for (int snd : { arpSoundSupersaw, arpSoundAcid })
            {
                box.buttonDown (BtnShift);
                box.turnKnob (slot::ASound - numKnobs, (float) (snd - juce::roundToInt (box.getData().inst[Arp].values[slot::ASound])));
                box.buttonUp (BtnShift);
                const auto st = renderSeconds (audio, 2.0, nullptr);
                check (st.rms > 0.005f && st.finite && st.peak <= 1.0f, juce::String ("Arp sound ") + arpSoundNames()[snd] + " plays",
                       "rms " + juce::String (st.rms, 4));
            }
            box.buttonDown (BtnShift);
            box.turnKnob (slot::ASound - numKnobs, (float) (2 - juce::roundToInt (box.getData().inst[Arp].values[slot::ASound])));
            box.buttonUp (BtnShift);

            // Bass steps: tap an empty step, raise its note with knob 2, slide with knob 3
            box.setPage (Bass);
            press (BtnSteps);
            tap (1);
            const int n0 = box.getData().inst[Bass].bassSteps[1].note;
            check (n0 >= 0 && box.selectedBassStep() == 1, "tapping an empty bass step sets it", noteName (n0));
            box.turnKnob (1, 2.0f);
            const int n1 = box.getData().inst[Bass].bassSteps[1].note;
            check (n1 > n0, "knob 2 moves the step's note up the scale", noteName (n0) + " -> " + noteName (n1));
            box.turnKnob (2, 1.0f);
            check (box.getData().inst[Bass].bassSteps[1].slide, "knob 3 sets the slide", {});
            tap (1);
            check (box.getData().inst[Bass].bassSteps[1].note < 0, "tapping the picked step clears it", {});
            tap (1);
            press (BtnSteps);

            // Drums steps: the kick gets step 3, then an accent
            box.setPage (Drums);
            if (! box.isStepView()) press (BtnSteps);
            tap (2);
            check (box.getData().inst[Drums].drumSteps[0][2] == 100, "a drum step goes on", {});
            tap (2);
            check (box.getData().inst[Drums].drumSteps[0][2] == 127, "tapped again it is an accent", {});
            press (BtnSteps);

            // each instrument on its own must be heard
            for (int i = 0; i < NumInstruments; ++i)
            {
                soloOnly (i);
                const auto st = renderSeconds (audio, 4.0, nullptr);
                check (st.rms > 0.005f, instrumentName (i) + " plays", "rms " + juce::String (st.rms, 4));
                check (st.finite && st.peak <= 1.0f, instrumentName (i) + " stays clean", "peak " + juce::String (st.peak, 3));
            }
            soloOnly (-1);

            // the whole box, written to the wav
            juce::AudioBuffer<float> song (2, (int) (sr * 16.0));
            const auto all = renderSeconds (audio, 16.0, &song);
            check (all.rms > 0.02f && all.finite && all.peak <= 1.0f, "all three together",
                   "rms " + juce::String (all.rms, 4) + " peak " + juce::String (all.peak, 3));
            writeWav (wavOut, song);

            // FX page: every knob up, every performance pad held - all clean
            box.setPage (PageFx);
            for (int k = 1; k <= 10; ++k) box.turnKnob (k, 30.0f);
            const auto fxAll = renderSeconds (audio, 2.0, nullptr);
            check (fxAll.rms > 0.01f && fxAll.finite && fxAll.peak <= 1.0f, "every FX knob turned up stays clean",
                   "rms " + juce::String (fxAll.rms, 4) + " peak " + juce::String (fxAll.peak, 3));
            shiftPress (BtnSteps);   // Shift + Steps on the FX page = FX back to the start
            check (std::abs (box.getData().fx[slot::FDjFilter] - 0.5f) < 0.001f, "Shift + Steps resets the FX page", {});
            for (int p = 0; p < NumPerf; ++p)
            {
                box.padDown (p, 100);
                const auto st = renderSeconds (audio, 1.0, nullptr);
                box.padUp (p);
                renderSeconds (audio, 0.1, nullptr);
                const bool mayBeQuiet = p == PerfTapeStop || p == PerfKillDrums || p == PerfKillArp || p == PerfKillBass;
                check (st.finite && st.peak <= 1.0f && (mayBeQuiet || st.rms > 0.005f), juce::String ("pad ") + perfName (p),
                       "rms " + juce::String (st.rms, 4));
            }
            soloOnly (Drums);
            box.padDown (PerfKillDrums, 100);
            renderSeconds (audio, 0.2, nullptr);
            const auto killed = renderSeconds (audio, 1.0, nullptr);
            box.padUp (PerfKillDrums);
            check (killed.rms < 0.02f, "Kill drums silences the drums (the reverb tail may ring)", "rms " + juce::String (killed.rms, 4));
            soloOnly (-1);

            // the bass follows the arp: hold F (C minor) and the line moves up a fourth
            box.setPage (Bass);
            box.buttonDown (BtnShift);
            box.turnKnob (slot::BFollow - numKnobs, 1.0f);
            box.buttonUp (BtnShift);
            box.setPage (Arp);
            shiftPress (BtnSteps);   // release the held notes
            box.padDown (11, 100);   // the piano pads: bottom row C D E F -> F
            box.padUp (11);
            bool moved = false;
            for (int i = 0; i < 400 && ! moved; ++i)
            {
                renderSeconds (audio, 0.01, nullptr);
                moved = audio.bassNoteNow.load() == 41;   // C2 + 5
            }
            check (moved, "Follow arp moves the bass line to the arp's chord", {});

            // pads like a piano on Arp and Bass (bottom row white keys, top row black keys), drums as before
            check (box.arpPadNote (8) == 48 && box.arpPadNote (11) == 53 && box.arpPadNote (15) == 60, "Arp pads: the bottom row is C D E F G A B C", {});
            check (box.arpPadNote (0) == 49 && box.arpPadNote (2) < 0 && box.arpPadNote (6) < 0 && box.arpPadNote (3) == 54,
                   "Arp pads: the top row is the black keys (C# D# - F# ...)", {});
            check (box.bassPadNote (8) == 36, "Bass pads start on C", noteName (box.bassPadNote (8)));
            check (drumPadNote (0) == 36 && drumPadNote (1) == 38, "Drum pads stay drums", {});

            // Chaos: new sound settings on the page, the pattern stays; Shift + Chaos takes them back
            box.setPage (Bass);
            const auto bassBefore = box.getData().inst[Bass];
            press (BtnChaos);
            box.pump();
            check (box.getData().inst[Bass].values == bassBefore.values, "a quick tap on Chaos does nothing", {});
            box.buttonDown (BtnChaos);
            juce::Thread::sleep (300);
            const float half = box.chaosHoldProgress();
            check (half > 0.3f && half < 0.95f, "holding Chaos fills the bar on the screen", juce::String (half, 2));
            juce::Thread::sleep (350);
            box.pump();
            box.buttonUp (BtnChaos);
            check (box.getData().inst[Bass].values != bassBefore.values, "holding Chaos changes the bass sound", {});
            check (box.getData().inst[Bass].bassSteps == bassBefore.bassSteps, "Chaos keeps the bass line", {});
            check (box.getData().inst[Bass].values[slot::BVolume] == bassBefore.values[slot::BVolume]
                   && box.getData().globals[GTempo] == tempo0, "Chaos keeps volume and tempo", {});
            const auto chaosSound = renderSeconds (audio, 2.0, nullptr);
            check (chaosSound.finite && chaosSound.peak <= 1.0f, "the chaos sound stays clean", "peak " + juce::String (chaosSound.peak, 3));
            shiftPress (BtnChaos);
            check (box.getData().inst[Bass].values == bassBefore.values, "Shift + Chaos takes it back", {});
            box.turnKnob (slot::BCutoff, 20.0f);
            check (box.getData().inst[Bass].values[slot::BCutoff] != paramDesc (Bass, slot::BCutoff).def, "the cutoff knob moved", {});
            box.resetKnob (slot::BCutoff);
            check (box.getData().inst[Bass].values[slot::BCutoff] == paramDesc (Bass, slot::BCutoff).def, "double-click puts a knob back to its start", {});
            box.setPage (PageFx);
            for (int i = 0; i < 20; ++i) { box.chaos(); renderSeconds (audio, 0.15, nullptr); }
            const auto fxChaos = renderSeconds (audio, 1.0, nullptr);
            check (fxChaos.finite && fxChaos.peak <= 1.0f && fxChaos.rms > 0.005f, "20 x Chaos on the FX page stays clean",
                   "rms " + juce::String (fxChaos.rms, 4));
            box.setPage (Arp);

            // the Glide engine: Sound = Glide, the Sound button opens its knob pages
            {
                box.setPage (Arp);
                const int arpSound0 = juce::roundToInt (box.getData().inst[Arp].values[slot::ASound]);
                box.buttonDown (BtnShift);
                box.turnKnob (slot::ASound - numKnobs, 30.0f);
                box.buttonUp (BtnShift);
                check (box.isGlide (Arp), "the Arp's Sound can be the Glide engine", box.engineTitle());
                press (BtnSound);
                check (box.isSoundMode() && box.knobView (1).name == "Preset", "Sound: the knobs edit the engine", box.knobView (1).name);
                soloOnly (Arp);
                box.turnKnob (1, -500.0f);
                const int numSounds = engine::glideSoundNames().size();
                int broken = 0, quiet = 0;
                juce::String quietNames;
                for (int i = 0; i < numSounds; ++i)
                {
                    if (i > 0) box.turnKnob (1, 1.0f);
                    box.padDown (8, 100);
                    const auto r = renderSeconds (audio, 0.3, nullptr);
                    box.padUp (8);
                    renderSeconds (audio, 0.05, nullptr);
                    if (! r.finite || r.peak > 1.0f) ++broken;
                    if (r.rms < 0.001f) { ++quiet; quietNames << engine::glideSoundNames()[i] << " "; }
                }
                check (box.getData().inst[Arp].glide.sound == numSounds - 1, "the Preset knob steps through every Glide sound", juce::String (numSounds));
                check (broken == 0 && quiet <= 8, "every Glide sound plays and stays clean (slow pads may be quiet in 0.3 s)", juce::String (broken) + " broken, quiet: " + quietNames);
                press (BtnOctUp);
                check (box.enginePageName() == "Osc I" && box.knobView (3).name == "Saw", "Oct + goes to the next knob page", box.enginePageName() + " / " + box.knobView (3).name);
                const auto saw0 = box.knobView (3).value;
                box.turnKnob (3, 30.0f);
                check (box.knobView (3).value != saw0 && box.engineTitle().endsWith ("*"), "a knob changes the Glide sound (marked *)", saw0 + " > " + box.knobView (3).value);
                box.resetKnob (3);
                check (std::abs (engine::get (box.getData().inst[Arp].glide, "l1_saw") - 0.8f) < 0.001f, "double-click puts a Glide knob back", {});
                const int sound0 = box.getData().inst[Arp].glide.sound;
                box.savePreset (Arp, 13);
                press (BtnOctDown);
                box.turnKnob (1, -5.0f);
                check (box.getData().inst[Arp].glide.sound != sound0, "the Preset knob loads another Glide sound", {});
                check (box.loadPreset (Arp, 13) && box.getData().inst[Arp].glide.sound == sound0 && box.isGlide (Arp),
                       "a page preset keeps its Glide sound", juce::String (box.getData().inst[Arp].glide.sound));
                const auto glideSound = renderSeconds (audio, 0.5, nullptr);
                check (glideSound.finite, "the Glide engine runs clean after loading a preset", {});

                // Bass with the Glide engine (knob 1 in Sound mode = the engine)
                box.setPage (Bass);
                box.turnKnob (0, 30.0f);
                check (box.isGlide (Bass), "the Bass can play the Glide engine", box.engineTitle());
                soloOnly (Bass);
                const auto glideBass = renderSeconds (audio, 1.5, nullptr);
                check (glideBass.finite && glideBass.rms > 0.003f && glideBass.peak <= 1.0f, "the bass line plays on the Glide engine",
                       "rms " + juce::String (glideBass.rms, 4));
                box.turnKnob (0, -30.0f);
                check (! box.isGlide (Bass), "and back to the Acid", {});
                box.setPage (Arp);
                box.turnKnob (0, (float) (arpSound0 - arpSoundGlide));
                press (BtnSound);
                check (! box.isSoundMode() && juce::roundToInt (box.getData().inst[Arp].values[slot::ASound]) == arpSound0, "Sound off, the Arp sound is back", {});
                soloOnly (-1);
            }

            // the Chords page: one pad = one chord on the Arp's sound; Orchid: type on top, root below; edit; presets; a keyboard
            {
                box.setPage (PageChords);
                soloOnly (Arp);
                check (box.chordPadLabel (8).isNotEmpty() && box.chordPadLabel (0) != box.chordPadLabel (8), "the chord pads have chord names",
                       box.chordPadLabel (8) + " / " + box.chordPadLabel (0));
                box.padDown (8, 100);
                check (box.chordPadSounding (8) && box.chordNow().isNotEmpty(), "a chord pad plays a chord", box.chordNow());
                const auto chordSound = renderSeconds (audio, 0.6, nullptr);
                box.padUp (8);
                renderSeconds (audio, 0.1, nullptr);
                check (chordSound.finite && chordSound.rms > 0.002f && ! box.chordPadSounding (8), "the chord sounds on the Arp and stops", "rms " + juce::String (chordSound.rms, 4));
                box.turnKnob (slot::CStrum, 30.0f);
                box.padDown (11, 110);
                const auto strum = renderSeconds (audio, 0.4, nullptr);
                box.padUp (11);
                check (strum.finite && strum.rms > 0.002f, "a strummed chord plays", {});
                box.turnKnob (slot::CPlay, 1.0f);
                check (box.isOrchid() && box.chordPadLabel (1) == "Min", "Orchid: the top row picks the chord type", box.chordPadLabel (1));
                box.padDown (1, 100);
                box.padDown (12, 100);
                const auto orchid = box.chordNow();
                check (orchid.upToFirstOccurrenceOf (" ", false, false).endsWith ("m"), "Orchid: Min held + a root = a minor chord", orchid);
                box.padUp (12); box.padUp (1);
                box.turnKnob (slot::CPlay, -1.0f);
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100));
                const auto keyChord = box.chordNow();
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOff (1, 62));
                check (keyChord.startsWith ("D"), "a MIDI keyboard on the Chords page plays a chord on each key", keyChord);
                press (BtnSteps);
                box.padDown (0, 100); box.padUp (0);
                const auto before = box.chordPadLabel (0);
                box.turnKnob (2, 2.0f);
                check (box.selectedChord() == 0 && box.chordPadLabel (0) != before, "Steps: knob 3 changes the picked chord's type", before + " > " + box.chordPadLabel (0));
                box.savePreset (PageChords, 3);
                box.turnKnob (2, 3.0f);
                check (box.loadPreset (PageChords, 3) && box.getData().chords[0].type != 0, "a Chords preset keeps the chords", box.chordPadLabel (0));
                press (BtnSteps);
                soloOnly (-1);
                box.setPage (Arp);
            }

            // presets: save to pad 5, change the sound, load pad 5 back
            box.savePreset (Arp, 4);
            check (box.presetFile (Arp, 4).existsAsFile(), "preset file written", {});
            const float mode = box.getData().inst[Arp].values[slot::AMode];
            box.turnKnob (slot::AMode, 3.0f);
            check (box.getData().inst[Arp].values[slot::AMode] != mode, "knob changes the arp mode", {});
            check (box.loadPreset (Arp, 4) && box.getData().inst[Arp].values[slot::AMode] == mode, "preset loads back", {});
            check (! box.loadPreset (Arp, 9), "empty preset pad is refused", {});
            box.buttonDown (BtnShift); press (BtnRec);
            tap (11);
            box.buttonUp (BtnShift);
            check (box.presetExists (Arp, 11), "Shift + Rec + pad saves a preset", {});
            check (box.isLatched(), "Shift + Rec does not touch Hold", {});

            // scenes: Scene, Rec + pad saves everything; change things; recall waits for the bar
            press (BtnScene);
            press (BtnRec);
            tap (6);
            check (box.sceneExists (6), "Scene + Rec + pad saves a scene", {});
            const auto drums0 = box.getData().inst[Drums].drumSteps;
            box.setPage (Drums);
            shiftPress (BtnSteps);   // clear all drums
            check (box.getData().inst[Drums].drumSteps != drums0, "the drums were cleared", {});
            tap (6);
            check (box.getData().inst[Drums].drumSteps == drums0 && box.getData().loadedScene == 6, "the scene comes back", {});
            check (audio.sceneWaiting.load(), "while playing it waits for the next bar", {});
            renderSeconds (audio, 2.3, nullptr);
            check (! audio.sceneWaiting.load(), "and then it plays", {});
            press (BtnScene);

            // Rec on the drum page: a pad played while it runs is written into its steps
            box.setPage (Drums);
            press (BtnRec);
            check (audio.isRecording(), "Rec arms recording", {});
            box.padDown (2, 115); box.padUp (2);
            renderSeconds (audio, 0.2, nullptr);
            box.pump();
            int clapSteps = 0;
            for (auto v : box.getData().inst[Drums].drumSteps[2]) if (v > 0) ++clapSteps;
            check (clapSteps == 1, "Rec writes a played drum into the steps", juce::String (clapSteps));

            // Rec on the bass page: two pads held together = a slide
            box.setPage (Bass);
            shiftPress (BtnSteps);   // clear the line
            box.padDown (0, 100);
            renderSeconds (audio, 0.3, nullptr);
            box.padDown (4, 120);
            renderSeconds (audio, 0.1, nullptr);
            box.padUp (0); box.padUp (4);
            box.pump();
            bool slid = false, accented = false;
            for (const auto& st : box.getData().inst[Bass].bassSteps)
            {
                slid = slid || st.slide;
                accented = accented || (st.note == box.bassPadNote (4) && st.accent);
            }
            check (slid && accented, "Rec writes the bass line (legato = slide, hard = accent)", {});
            box.setPage (Drums);
            press (BtnRec);

            box.saveState();

            // the box screen and the PC panel draw without trouble (also right after a knob turn)
            {
                BoxView screen (box, true);
                screen.setSize (BoxView::screenW, BoxView::screenH);
                box.setPage (Bass);
                box.turnKnob (2, 2.0f);
                const auto shot = screen.createComponentSnapshot (screen.getLocalBounds());
                check (shot.isValid() && shot.getWidth() == BoxView::screenW, "the 5\" box screen draws (knob card shown)", {});
                box.buttonDown (BtnShift);
                const auto grid = screen.createComponentSnapshot (screen.getLocalBounds());
                box.buttonUp (BtnShift);
                check (grid.isValid(), "the box screen draws the preset grid", {});
                juce::PNGImageFormat png;
                if (auto os = wavOut.getSiblingFile (wavOut.getFileNameWithoutExtension() + "-screen.png").createOutputStream())
                    png.writeImageToStream (shot, *os);
            }
        }

        // the link: a box and a PC app talk over the network (here: on this computer)
        {
            const auto dirA = dir.getSiblingFile (dir.getFileName() + "-box"), dirB = dir.getSiblingFile (dir.getFileName() + "-pc");
            {
                BoxController boxA (dirA), pc (dirB);
                boxA.getAudio().prepare (sr, block);
                pc.getAudio().prepare (sr, block);
                const int port = 50700 + juce::Random::getSystemRandom().nextInt (200);
                LinkServer server (boxA, port, false, "127.0.0.1");
                check (server.isListening(), "the box listens for a PC", "port " + juce::String (port));
                LinkClient client (pc, false);
                const bool connected = client.connect ("127.0.0.1", port);
                check (connected && pc.isRemote(), "the PC app connects to the box", {});
                wait (600);

                const float t0 = boxA.getData().globals[GTempo];
                pc.turnKnob (0, 3.0f);
                wait (500);
                check (std::abs (boxA.getData().globals[GTempo] - (t0 + 3.0f)) < 0.01f, "a knob on the PC turns the box",
                       juce::String (boxA.getData().globals[GTempo]));
                check (std::abs (pc.getData().globals[GTempo] - (t0 + 3.0f)) < 0.01f, "the PC shows what the box has", {});

                pc.buttonDown (BtnInstrument); pc.buttonUp (BtnInstrument);
                wait (400);
                check (boxA.getPage() == Arp && pc.getPage() == Arp, "a button on the PC presses it on the box", {});

                boxA.savePreset (Arp, 2);
                wait (500);
                check (pc.presetName (Arp, 2) == "Arp 3", "the PC sees the box's presets", pc.presetName (Arp, 2));
                pc.renamePreset (Arp, 2, "Night Pluck");
                wait (500);
                check (boxA.presetName (Arp, 2) == "Night Pluck" && pc.presetName (Arp, 2) == "Night Pluck",
                       "a preset renamed on the PC is renamed on the box", boxA.presetName (Arp, 2));
                bool copied = false;
                client.requestPreset (Arp, 2, [&] (const juce::var& json) { copied = pc.storeLocalPresetJson (Arp, 2, json); });
                wait (500);
                check (copied && pc.localPresetName (Arp, 2) == "Night Pluck", "a preset copies from the box to the PC", {});
                {
                    LinkPanel panel (pc, client);
                    panel.setSize (LinkPanel::panelWidth, 900);
                    panel.showPage (Arp);
                    check (panel.rowText (2) == "Night Pluck", "the PC panel lists the box's preset", panel.rowText (2));
                    const auto img = panel.createComponentSnapshot (panel.getLocalBounds());
                    check (img.isValid(), "the PC panel draws", {});
                }

                pc.buttonDown (BtnPlay); pc.buttonUp (BtnPlay);
                for (int i = 0; i < 10; ++i) { renderSeconds (boxA.getAudio(), 0.05, nullptr); wait (50); }
                check (pc.getAudio().isPlaying(), "the PC shows that the box plays", {});
                const auto silent = renderSeconds (pc.getAudio(), 0.3, nullptr);
                check (silent.rms == 0.0f, "linked, the PC itself stays silent (the box plays)", {});

                client.disconnect();
                check (! pc.isRemote(), "after Disconnect the PC plays by itself again", {});
            }
            dirA.deleteRecursively();
            dirB.deleteRecursively();
        }
        {
            BoxController again (dir);
            int clapSteps = 0;
            for (auto v : again.getData().inst[Drums].drumSteps[2]) if (v > 0) ++clapSteps;
            int bassNotes = 0;
            for (const auto& st : again.getData().inst[Bass].bassSteps) if (st.note >= 0) ++bassNotes;
            check (clapSteps == 1, "after a restart: the recorded drum", juce::String (clapSteps));
            check (again.getData().loadedScene == 6, "after a restart: the scene number", juce::String (again.getData().loadedScene));
            check (again.getData().inst[Drums].drumSteps[0][2] == 127, "after a restart: the drum steps", {});
            check (bassNotes == 2, "after a restart: the recorded bass line", juce::String (bassNotes));
        }
        dir.deleteRecursively();

        const auto report = wavOut.withFileExtension ("txt");
        report.replaceWithText (log.joinIntoString ("\n") + "\n");
        std::printf ("%s\n", log.joinIntoString ("\n").toRawUTF8());
        return failures == 0 ? 0 : 1;
    }

private:
    struct Stats { float rms = 0.0f, peak = 0.0f; bool finite = true; };

    /** Lets the message loop run (network messages, timers). */
    static void wait (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    Stats renderSeconds (BoxAudio& audio, double seconds, juce::AudioBuffer<float>* keep)
    {
        Stats st;
        const int total = (int) (sr * seconds);
        std::vector<float> l ((size_t) block), r ((size_t) block);
        double sum = 0.0;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            std::fill (l.begin(), l.end(), 0.0f);
            std::fill (r.begin(), r.end(), 0.0f);
            audio.render (l.data(), r.data(), n);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) st.finite = false;
                st.peak = juce::jmax (st.peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]));
                sum += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
            }
            if (keep != nullptr && pos + n <= keep->getNumSamples())
            {
                keep->copyFrom (0, pos, l.data(), n);
                keep->copyFrom (1, pos, r.data(), n);
            }
        }
        st.rms = (float) std::sqrt (sum / juce::jmax (1, 2 * total));
        return st;
    }

    void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
    {
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        if (out == nullptr) { check (false, "write " + f.getFullPathName(), {}); return; }
        auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
        if (w == nullptr) { check (false, "wav writer", {}); return; }
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    void check (bool ok, const juce::String& what, const juce::String& detail)
    {
        log.add ((ok ? "ok   " : "FAIL ") + what + (detail.isNotEmpty() ? "  (" + detail + ")" : juce::String()));
        if (! ok) ++failures;
    }

    const double sr = 48000.0;
    const int block = 256;
    juce::StringArray log;
    int failures = 0;
};

//==============================================================================
/** The PC window: the box (screen + front panel) and, on the right, the link and preset library. */
class AppContent : public juce::Component
{
public:
    AppContent (BoxView* v, LinkPanel* p) : view (v), panel (p)
    {
        addAndMakeVisible (view.get());
        if (panel != nullptr) addAndMakeVisible (panel.get());
    }
    void resized() override
    {
        auto r = getLocalBounds();
        if (panel != nullptr) panel->setBounds (r.removeFromRight (LinkPanel::panelWidth));
        view->setBounds (r);
    }
    BoxView& getView() { return *view; }
    LinkPanel* getPanel() { return panel.get(); }

private:
    std::unique_ptr<BoxView> view;
    std::unique_ptr<LinkPanel> panel;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (BoxController& box, juce::AudioDeviceManager& dm, bool kiosk, LinkClient* link, std::function<juce::String()> info)
        : DocumentWindow ("Cat Box", juce::Colour (0xff0b0d12), kiosk ? 0 : DocumentWindow::allButtons),
          deviceManager (dm)
    {
        auto* view = new BoxView (box, kiosk);
        view->onSettings = [this] { showSettings(); };
        view->onToggleFullScreen = [this] { setFullScreen (! isFullScreen()); };
        view->linkInfo = std::move (info);
        auto* content = new AppContent (view, link != nullptr ? new LinkPanel (box, *link) : nullptr);
        setContentOwned (content, false);

        const auto lb = view->logicalBounds();
        if (kiosk)
        {
            setTitleBarHeight (0);
            setResizable (false, false);
            setFullScreen (true);
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                setBounds (d->totalArea);
        }
        else
        {
            const int extra = link != nullptr ? LinkPanel::panelWidth : 0;
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setResizeLimits (400 + extra, 465, 2200, 1900);
            centreWithSize (juce::roundToInt (lb.getWidth() * 0.82) + extra, juce::roundToInt (lb.getHeight() * 0.82));
            // opens where it was closed (size and place)
            windowFile = box.getDataDirectory().getChildFile ("window.txt");
            if (windowFile.existsAsFile()) restoreWindowStateFromString (windowFile.loadFileAsString());
        }
        setVisible (true);
        view->grabKeyboardFocus();
    }

    ~MainWindow() override
    {
        if (windowFile != juce::File() && ! isFullScreen()) windowFile.replaceWithText (getWindowStateAsString());
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    void showSettings()
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 2, true, false, true, false);
        sel->setSize (520, 420);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.dialogTitle = "Cat Box: sound card and MIDI";
        o.dialogBackgroundColour = juce::Colour (0xff1b1e26);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

private:
    juce::AudioDeviceManager& deviceManager;
    juce::File windowFile;
};

//==============================================================================
class CatBoxApplication : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "Cat-Box"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        const auto args = juce::StringArray::fromTokens (commandLine, true);
        if (const int at = args.indexOf ("--selftest"); at >= 0)
        {
            const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 1].unquoted().isNotEmpty()
                                                                                      ? args[at + 1].unquoted() : "catbox-selftest.wav");
            setApplicationReturnValue (SelfTest().run (out));
            quit();
            return;
        }

        // --kiosk: the box (full screen, answers PCs on the network); --box: the same in a window (to try it on a PC)
        const bool kiosk = args.contains ("--kiosk");
        const bool isBox = kiosk || args.contains ("--box");
        juce::File dataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                 .getChildFile ("CatSuite").getChildFile ("CatBox");
        if (const int at = args.indexOf ("--data"); at >= 0 && args[at + 1].isNotEmpty())
            dataDir = juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 1].unquoted());
        dataDir.createDirectory();

        box = std::make_unique<BoxController> (dataDir);
        startAudio (dataDir);
        std::function<juce::String()> info;
        if (isBox)
        {
            server = std::make_unique<LinkServer> (*box);
            info = [this] { const auto a = LinkServer::ownAddress();
                            return server != nullptr && server->isListening() ? "PC link: " + (a.isNotEmpty() ? a : juce::String ("no network"))
                                                                              : juce::String ("PC link: port busy"); };
        }
        else link = std::make_unique<LinkClient> (*box);
        window = std::make_unique<MainWindow> (*box, deviceManager, kiosk, link.get(), info);
        startTimer (2000);   // a controller plugged in later (the Pico) is switched on by itself
    }

    void shutdown() override
    {
        stopTimer();
        window = nullptr;
        link = nullptr;
        server = nullptr;
        if (box != nullptr)
        {
            if (auto xml = deviceManager.createStateXml())
                xml->writeTo (box->getDataDirectory().getChildFile ("audio-settings.xml"));
            deviceManager.removeAudioCallback (&box->getAudio());
            deviceManager.removeMidiInputDeviceCallback ({}, box.get());
            deviceManager.closeAudioDevice();
            box->saveState();
            box = nullptr;
        }
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    void startAudio (const juce::File& dataDir)
    {
        const auto settingsFile = dataDir.getChildFile ("audio-settings.xml");
        std::unique_ptr<juce::XmlElement> saved;
        if (settingsFile.existsAsFile()) saved = juce::XmlDocument::parse (settingsFile);
        deviceManager.initialise (0, 2, saved.get(), true);

        if (saved == nullptr)
        {
            // first start: prefer a HiFiBerry card and a short buffer (the Pi in the box)
            auto setup = deviceManager.getAudioDeviceSetup();
            if (auto* type = deviceManager.getCurrentDeviceTypeObject())
                for (const auto& name : type->getDeviceNames (false))
                    if (name.containsIgnoreCase ("hifiberry")) setup.outputDeviceName = name;
           #if JUCE_LINUX
            setup.bufferSize = 256;
           #endif
            deviceManager.setAudioDeviceSetup (setup, true);
        }

        deviceManager.addAudioCallback (&box->getAudio());
        enableMidiInputs();
        deviceManager.addMidiInputDeviceCallback ({}, box.get());
    }

    void enableMidiInputs()
    {
        // only devices seen for the first time: one the user switched off in the settings stays off
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (! seenMidi.contains (d.identifier))
            {
                seenMidi.add (d.identifier);
                deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
            }
    }

    void timerCallback() override { enableMidiInputs(); }

    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<BoxController> box;
    std::unique_ptr<LinkServer> server;   // the box
    std::unique_ptr<LinkClient> link;     // the PC app
    std::unique_ptr<MainWindow> window;
    juce::StringArray seenMidi;
};
} // namespace catbox

START_JUCE_APPLICATION (catbox::CatBoxApplication)
