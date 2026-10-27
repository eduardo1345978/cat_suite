// Cat Box 2: the Cat Suite groovebox (8 tracks, 5 sound engines, steps, note FX, mixer) - on the
// PC as an app, on the Raspberry Pi inside the box as the whole instrument.
//
//   Cat-Box                     window with the screen + a clickable front panel
//   Cat-Box --kiosk             only the 800 x 480 screen, full screen (the Pi in the box)
//   Cat-Box --data <folder>     where presets and the last state live
//   Cat-Box --selftest <out.wav>  plays everything offline, checks it, writes <out>.txt (exit code 0 = ok)

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Controller.h"
#include "View.h"
#include "BoxLink.h"
#include "LinkPanel.h"
#include "Samples.h"
#include <cmath>
#include <cstdio>

namespace catbox
{
//==============================================================================
/** Offline check of the whole box: controls, every engine and sound, the sequencer, Rec, note FX,
    mixer, scenes, MIDI, the screen, the link and memory. Exit code 0 = all fine. */
class SelfTest
{
public:
    int run (const juce::File& wavOut)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("catbox-selftest-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
        {
            auto boxOwner = std::make_unique<Controller> (dir);   // on the heap: a Controller is big (Windows has a 1 MB stack)
            Controller& box = *boxOwner;
            auto& audio = box.getAudio();
            audio.prepare (sr, block);
            auto press = [&box] (int b) { box.buttonDown (b); box.buttonUp (b); };
            auto shiftPress = [&box, &press] (int b) { box.buttonDown (BtnShift); press (b); box.buttonUp (BtnShift); };
            auto soloOnly = [&box] (int t) { for (int j = 0; j < numTracks; ++j) { box.setSolo (j, j == t); box.setMute (j, false); } };
            auto noSolo = [&box] { for (int j = 0; j < numTracks; ++j) box.setSolo (j, false); };
            const auto& p = box.getProject();

            // ---- the box's hardware on MIDI channel 16
            box.setMode (ModeProject);
            const float tempo0 = p.m.tempo;
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (controllerChannel, firstKnobCC, 5));
            check (std::abs (p.m.tempo - (tempo0 + 5.0f)) < 0.01f, "encoder 1 (CC 20) turns the tempo on the PROJECT page", juce::String (p.m.tempo));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (controllerChannel, firstKnobCC, 123));
            check (std::abs (p.m.tempo - tempo0) < 0.01f, "and back (two's complement)", {});
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (controllerChannel, firstButtonCC + BtnSeq, 127));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (controllerChannel, firstButtonCC + BtnSeq, 0));
            check (box.getMode() == ModeSeq, "the Seq button (CC 111) opens SEQ", modeName (box.getMode()));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (controllerChannel, firstButtonCC + BtnTrack1 + 2, 127));
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (controllerChannel, firstButtonCC + BtnTrack1 + 2, 0));
            check (box.getTrack() == 2, "Track 3 button picks track 3", {});
            box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (controllerChannel, sliderCC, 10));
            check (p.m.djFilter < 0.2f, "the slider moves the DJ filter", juce::String (p.m.djFilter));
            box.setSlider (0.5f);

            // ---- modes: Sound, Sound again = browser, pages with Prev / Next
            box.selectTrack (3);
            press (BtnSound);
            check (box.getMode() == ModeSound, "Sound shows the sound", {});
            const int pages = box.numPagesNow();
            press (BtnNext);
            check (box.pageIndex() == 1 % pages, "Next = the next knob page", box.pageName());
            press (BtnPrev);
            check (box.pageIndex() == 0, "Prev = back", {});
            for (int k = 0; k < numKnobs; ++k) check (box.knob (k).used && box.knob (k).name.isNotEmpty(), "Analog page 1: knob " + juce::String (k + 1) + " = " + box.knob (k).name, box.knob (k).value);
            box.buttonDown (BtnShift);
            check (box.knob (0).used && box.knob (0).name != "Osc 1", "Shift: 12 more knobs", box.knob (0).name);
            box.buttonUp (BtnShift);
            press (BtnSound);
            check (box.getMode() == ModeBrowse, "Sound again = the browser", {});

            // ---- the browser: every engine, every sound plays clean
            int sounds = 0, quiet = 0;
            juce::StringArray quietNames;
            soloOnly (3);
            for (int e = 0; e < NumEngineTypes; ++e)
            {
                const auto& d = engineDef (e);
                for (int i = 0; i < (int) d.presets.size(); ++i)
                {
                    box.loadSound (3, e, i);
                    renderSeconds (audio, 0.02, nullptr);
                    if (d.drums) { for (int pad : { 0, 1, 3 }) { box.padDown (pad, 110); box.padUp (pad); } }
                    else { box.keyDown (60, 100); box.keyDown (64, 90); }
                    const auto st = renderSeconds (audio, 0.9, nullptr);
                    box.keyUp (60); box.keyUp (64);
                    renderSeconds (audio, 0.15, nullptr);
                    ++sounds;
                    if (! (st.finite && st.peak <= 1.0f)) check (false, d.name + " / " + d.presets[(size_t) i].name + " stays clean", "peak " + juce::String (st.peak));
                    if (st.rms < 0.004f) { ++quiet; quietNames.add (d.presets[(size_t) i].name); }
                }
            }
            check (sounds > 150, juce::String (sounds) + " factory sounds", {});
            check (quiet <= 3, "every sound is heard (slow pads may start quiet)", juce::String (quiet) + " quiet: " + quietNames.joinIntoString (", "));

            // browse with knob 3: the sound changes and is heard right away (a preview while stopped)
            box.setMode (ModeBrowse);
            box.turnKnob (0, (float) EngineAcid - box.knob (0).norm * (float) (NumEngineTypes - 1));
            check (box.getProject().tracks[3].engine == EngineAcid, "knob 1 in the browser picks the engine", box.knob (0).value);
            const auto name0 = box.presetName (3);
            box.turnKnob (2, 2.0f);
            check (box.presetName (3) != name0, "knob 3 picks the next sound", name0 + " > " + box.presetName (3));
            const auto pre = renderSeconds (audio, 0.5, nullptr);
            check (pre.rms > 0.005f, "a new sound is heard at once (preview)", "rms " + juce::String (pre.rms, 4));
            press (BtnRec);   // in the browser: save as a User sound
            check (box.browseCategories().contains ("User"), "Rec in the browser saves a User sound", box.flashText());
            press (BtnSound);

            // ---- SEQ: tap a step, hold a step + key, hold + knob; drums
            box.selectTrack (6);   // the pluck, empty pattern
            box.setMode (ModeSeq);
            if (audio.isPlaying()) press (BtnPlay);
            box.keyDown (62, 100); box.keyUp (62);   // stopped: a key goes into step 1 (step input)
            check (p.tracks[6].pat.steps[0].has (62) && box.selectedStep() == 0, "SEQ, stopped: a played key fills step 1", {});
            box.keyDown (64, 100); box.keyDown (67, 100); box.keyUp (64); box.keyUp (67);
            check (p.tracks[6].pat.steps[1].has (64) && p.tracks[6].pat.steps[1].has (67) && box.selectedStep() == 1, "the next keys (together) fill step 2 as a chord", {});
            box.turnKnob (1, -8.0f);
            check (p.tracks[6].pat.steps[1].vel < 100, "the knobs change the picked step (no holding)", juce::String ((int) p.tracks[6].pat.steps[1].vel));
            box.padDown (1, 100); box.padUp (1);
            check (! p.tracks[6].pat.steps[1].on(), "a tap on the picked step turns it off", {});
            box.keyDown (62, 100); box.keyUp (62);
            box.padDown (2, 100); box.padUp (2);
            check (p.tracks[6].pat.steps[2].notes[0] == 62, "a tap on pad 3 puts the last note on step 3", juce::String (p.tracks[6].pat.steps[2].notes[0]));
            box.padDown (5, 100);
            box.keyDown (67, 90); box.keyUp (67);
            box.keyDown (71, 90); box.keyUp (71);
            box.turnKnob (1, -10.0f);
            box.turnKnob (3, 1.0f);
            box.buttonDown (BtnShift); box.turnKnob (0, 1.0f); box.turnKnob (0, -1.0f); box.buttonUp (BtnShift);
            box.padUp (5);
            const auto& st5 = p.tracks[6].pat.steps[5];
            check (st5.has (67) && st5.has (71) && st5.vel < 90 && st5.slide, "hold a step: keys = notes (a chord), knobs = velocity, slide, Shift + knob 1 = semitones", juce::String (st5.count()));
            box.padDown (2, 100); box.padUp (2);
            check (! p.tracks[6].pat.steps[2].on(), "a tap on a set step clears it", {});
            box.turnKnob (0, 16.0f);
            check (p.tracks[6].pat.length == 32, "nothing held: knob 1 = the pattern length", juce::String (p.tracks[6].pat.length));
            press (BtnNext);
            check (box.seqBar() == 1, "Next = bar 2", {});
            box.padDown (0, 100); box.padUp (0);
            check (p.tracks[6].pat.steps[16].on(), "pads are the steps of bar 2", {});
            press (BtnPrev);
            box.buttonDown (BtnShift); press (BtnPrev); box.buttonUp (BtnShift);   // copy bar 1
            press (BtnNext);
            box.buttonDown (BtnShift); press (BtnNext); box.buttonUp (BtnShift);   // paste into bar 2
            check (p.tracks[6].pat.steps[21].has (67) && p.tracks[6].pat.steps[16].has (62), "Shift + Prev / Next: copy bar 1, paste into bar 2", {});
            press (BtnPrev);
            box.selectTrack (5);   // 909 drums, empty
            box.buttonDown (BtnShift); box.padDown (3, 100); box.padUp (3); box.buttonUp (BtnShift);
            check (box.seqLane() == 3, "Shift + pad picks the drum (closed hat)", juce::String (box.seqLane()));
            for (int s : { 0, 2, 4, 6, 8, 10, 12, 14 }) { box.padDown (s, 100); box.padUp (s); }
            int hats = 0;
            for (auto v : p.tracks[5].pat.drums[3]) hats += v > 0 ? 1 : 0;
            check (hats == 8, "eight hat steps", juce::String (hats));
            box.padDown (14, 100); box.turnKnob (2, 2.0f); box.padUp (14);
            check (p.tracks[5].pat.rolls[3][14] == 3 && p.tracks[5].pat.drums[3][14] > 0, "hold a drum step + knob 3 = a roll (3 hits)", juce::String ((int) p.tracks[5].pat.rolls[3][14]));
            press (BtnPlay);
            soloOnly (5);
            const auto drums = renderSeconds (audio, 2.0, nullptr);
            check (drums.rms > 0.01f && drums.finite && drums.peak <= 1.0f, "the drum steps play", "rms " + juce::String (drums.rms, 4));
            soloOnly (6);
            const auto pluck = renderSeconds (audio, 4.5, nullptr);
            check (pluck.rms > 0.003f, "the melodic steps play", "rms " + juce::String (pluck.rms, 4));
            noSolo();

            // ---- every track of the default song, then all together
            for (int t = 0; t < 2; ++t)
            {
                soloOnly (t);
                const auto st = renderSeconds (audio, 2.0, nullptr);
                check (st.rms > 0.01f && st.finite && st.peak <= 1.0f, "track " + juce::String (t + 1) + " (" + engineDef (p.tracks[(size_t) t].engine).name + ") plays its pattern",
                       "rms " + juce::String (st.rms, 4));
            }
            noSolo();
            juce::AudioBuffer<float> song (2, (int) (sr * 12.0));
            for (int t : { 2, 4 }) { box.selectTrack (t); box.keyDown (60, 90); box.keyDown (63, 80); box.keyDown (67, 80); }
            const auto all = renderSeconds (audio, 12.0, &song);
            for (int n : { 60, 63, 67 }) box.keyUp (n);
            check (all.rms > 0.03f && all.finite && all.peak <= 1.0f, "everything together", "rms " + juce::String (all.rms, 4) + " peak " + juce::String (all.peak, 3));
            writeWav (wavOut, song);

            // ---- Rec: a drum and a legato line written while it plays
            box.selectTrack (5);
            box.setMode (ModeSound);
            press (BtnRec);
            check (audio.isRecording(), "Rec records", {});
            renderSeconds (audio, 0.05, nullptr);
            box.padDown (1, 115); box.padUp (1);
            renderSeconds (audio, 0.1, nullptr);
            box.pump();
            int snares = 0;
            for (auto v : box.getProject().tracks[5].pat.drums[1]) snares += v > 0 ? 1 : 0;
            check (snares == 1, "Rec writes a played drum into the steps", juce::String (snares));
            box.selectTrack (7);
            clearAnd (box, 7);
            box.keyDown (48, 100);
            renderSeconds (audio, 0.25, nullptr);
            box.keyDown (55, 127);
            renderSeconds (audio, 0.1, nullptr);
            box.keyUp (48); box.keyUp (55);
            renderSeconds (audio, 0.05, nullptr);
            box.pump();
            bool slid = false, accent = false;
            int notes = 0;
            for (const auto& s : box.getProject().tracks[7].pat.steps) { notes += s.count(); slid = slid || s.slide; accent = accent || (s.has (55) && s.accent); }
            check (notes == 2 && slid && accent, "Rec writes notes (legato = slide, hard = accent)", juce::String (notes));
            {
                box.selectTrack (1);   // the acid line: turn the cutoff while it records
                box.setMode (ModeSound);
                const float cut0 = box.getProject().tracks[1].params[1];
                renderSeconds (audio, 0.2, nullptr);
                box.turnKnob (1, 30.0f);
                int locked = 0;
                for (int i = 0; i < maxSteps; ++i) locked += box.getProject().tracks[1].pat.hasLocks (i) ? 1 : 0;
                check (locked == 1 && box.getProject().tracks[1].params[1] == cut0, "Rec + a knob: the move is recorded into the step (the sound itself stays)", box.flashText());
                const auto st = renderSeconds (audio, 2.0, nullptr);
                check (st.finite && st.peak <= 1.0f, "the recorded knob move plays", {});
            }
            press (BtnRec);

            // ---- note FX on a track: arp, chords, orbit (keys held)
            box.selectTrack (3);
            box.loadSound (3, EngineAnalog, 0);
            soloOnly (3);
            press (BtnPlay);   // stop: the note FX run by themselves too
            box.setMode (ModeFx);
            for (int fx = FxArp; fx < NumNoteFx; ++fx)
            {
                while (box.getProject().tracks[3].fxType != fx) press (BtnNext);
                box.keyDown (60, 100); box.keyDown (63, 100); box.keyDown (67, 100);
                const auto st = renderSeconds (audio, 2.0, nullptr);
                for (int n : { 60, 63, 67 }) box.keyUp (n);
                renderSeconds (audio, 0.6, nullptr);
                check (st.rms > 0.004f && st.finite, "note FX " + noteFxNames()[fx] + " plays", "rms " + juce::String (st.rms, 4));
                for (int k = 0; k < numKnobs; ++k) if (! box.knob (k).used && fx != FxOrbit) {}
            }
            press (BtnNext);
            check (box.getProject().tracks[3].fxType == FxOff, "and Off again", {});
            noSolo();

            // ---- MIX: mute and solo on the pads
            box.setMode (ModeMix);
            box.padDown (0, 100); box.padUp (0);
            check (box.getProject().tracks[0].mute, "MIX: pad 1 mutes track 1", {});
            box.padDown (0, 100); box.padUp (0);
            box.padDown (9, 100); box.padUp (9);
            check (box.getProject().tracks[1].solo, "MIX: pad 10 solos track 2", {});
            box.padDown (9, 100); box.padUp (9);
            box.turnKnob (0, -100.0f);
            check (box.getProject().tracks[0].volume < 0.01f, "MIX: knob 1 = track 1 volume", {});
            box.resetKnob (0);
            check (std::abs (box.getProject().tracks[0].volume - 0.75f) < 0.01f, "double-click = back to the start", juce::String (box.getProject().tracks[0].volume));

            // ---- every master and send knob up, then the DJ filter both ways: clean
            press (BtnPlay);
            box.setMode (ModeMix);
            for (int pg = 0; pg < 4; ++pg)
            {
                for (int k = 8; k < 12; ++k) box.turnKnob (k, 100.0f);
                box.buttonDown (BtnShift);
                for (int k = 0; k < 12; ++k) box.turnKnob (k, 100.0f);
                box.buttonUp (BtnShift);
                for (int k = 0; k < 8; ++k) box.turnKnob (k, 100.0f);
                press (BtnNext);
            }
            const auto loud = renderSeconds (audio, 3.0, nullptr);
            check (loud.rms > 0.02f && loud.finite && loud.peak <= 1.0f, "every mix and send knob up stays clean", "rms " + juce::String (loud.rms, 4) + " peak " + juce::String (loud.peak, 3));
            for (float f : { 0.0f, 0.2f, 0.8f, 1.0f })
            {
                box.setSlider (f);
                const auto st = renderSeconds (audio, 0.6, nullptr);
                check (st.finite && st.peak <= 1.0f, "DJ filter at " + juce::String (f, 1), "rms " + juce::String (st.rms, 4));
            }
            box.setSlider (0.5f);

            // ---- scenes: save, change, recall at the next bar
            press (BtnScene);
            box.buttonDown (BtnRec); box.padDown (2, 100); box.padUp (2); box.buttonUp (BtnRec);
            check (box.sceneExists (2), "Scene + Rec + pad saves a scene", {});
            const auto kick0 = box.getProject().tracks[0].pat.drums[0];
            box.clearPattern (0);
            box.padDown (2, 100); box.padUp (2);
            check (box.getProject().tracks[0].pat.drums[0] == kick0, "the scene comes back", {});
            check (audio.sceneWaiting.load(), "while playing it waits for the next bar", {});
            renderSeconds (audio, 2.4, nullptr);
            check (! audio.sceneWaiting.load(), "and then it plays", {});
            press (BtnScene);

            // ---- Chaos (hold) and Shift + Chaos (back)
            box.selectTrack (1);
            box.setMode (ModeSound);
            const auto before = box.getProject().tracks[1].params;
            box.buttonDown (BtnChaos);
            box.buttonUp (BtnChaos);
            box.pump();
            check (box.getProject().tracks[1].params == before, "a quick tap on Chaos does nothing", {});
            box.chaos();
            check (box.getProject().tracks[1].params != before, "Chaos: new sound settings", box.presetName (1));
            const auto ch = renderSeconds (audio, 1.0, nullptr);
            check (ch.finite && ch.peak <= 1.0f, "the chaos sound is clean", {});
            shiftPress (BtnChaos);
            check (box.getProject().tracks[1].params == before, "Shift + Chaos takes it back", {});
            {
                const auto drums0 = box.getProject().tracks[0].pat.drums;
                box.clearPattern (0);
                box.selectTrack (0);
                box.setMode (ModeSeq);
                box.padDown (5, 100); box.padUp (5);
                shiftPress (BtnChaos);
                shiftPress (BtnChaos);
                check (box.getProject().tracks[0].pat.drums == drums0, "Undo (Shift + Chaos) twice: the step, then the cleared pattern come back", box.flashText());
            }

            // ---- the sampler: starter samples, recording one from the input, playing it, chops
            {
                check (SampleBank::get().count() >= 8 && SampleBank::get().name (4) == "Break 120", "the starter samples are there",
                       juce::String (SampleBank::get().count()) + ": " + SampleBank::get().name (0));
                box.selectTrack (7);
                box.loadSound (7, EngineSampler, 0);
                box.setMode (ModeSound);
                soloOnly (7);
                if (audio.isPlaying()) press (BtnPlay);
                press (BtnRec);
                check (box.isSampling(), "Rec on a sampler track records a sample", box.flashText());
                std::vector<float> in (480);
                for (int b = 0; b < 100; ++b)   // one second of a 220 Hz tone with silence before it
                {
                    for (int i = 0; i < 480; ++i) in[(size_t) i] = b < 20 ? 0.0f : 0.5f * std::sin (6.2831853f * 220.0f * (float) (b * 480 + i) / 48000.0f);
                    audio.feedInput (in.data(), in.data(), 480);
                }
                press (BtnRec);
                const int slot = juce::roundToInt (box.getProject().tracks[7].params[(size_t) engineDef (EngineSampler).index ("sample")]);
                const auto* smp = SampleBank::get().at (slot);
                check (! box.isSampling() && smp != nullptr && smp->name.startsWith ("Rec") && smp->length() < 48000 * 0.95,
                       "Rec again stops: the new sample is on the track (silence cut off)", smp != nullptr ? smp->name + " " + juce::String (smp->length()) : juce::String ("none"));
                box.keyDown (67, 100);
                const auto st = renderSeconds (audio, 0.5, nullptr);
                box.keyUp (67);
                renderSeconds (audio, 0.4, nullptr);
                check (st.rms > 0.02f && st.finite && st.peak <= 1.0f, "the recorded sample plays on the keys", "rms " + juce::String (st.rms, 4));
                const auto& sd = engineDef (EngineSampler);
                int chop = 0;
                for (size_t i = 0; i < sd.presets.size(); ++i) if (sd.presets[i].name == "Break Chop") chop = (int) i;
                box.loadSound (7, EngineSampler, chop);
                for (int pad = 0; pad < 16; pad += 5) { box.padDown (pad, 100); renderSeconds (audio, 0.05, nullptr); box.padUp (pad); }
                const auto ch = renderSeconds (audio, 0.3, nullptr);
                check (ch.finite && ch.peak <= 1.0f, "Break Chop: slices on the pads", "rms " + juce::String (ch.rms, 4));
                noSolo();
            }

            // ---- MIDI learn: a keyboard's knob, button and pad
            {
                box.setMode (ModeProject);
                box.startLearn();
                check (box.isLearning() && box.learnPrompt() == "Knob 1", "MIDI learn asks for knob 1 first", box.learnPrompt());
                for (int v : { 60, 62, 64, 66 }) box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (1, 70, v));
                wait (450);
                check (box.learnStep() == 1 && box.learnLast().contains ("CC 70"), "a knob that was moved is learned", box.learnLast());
                for (int i = 1; i < numKnobs; ++i) box.learnSkip();
                check (box.learnPrompt() == "Play button", "then the buttons", box.learnPrompt());
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (1, 80, 127));
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (1, 80, 0));
                box.learnDone();
                check (! box.isLearning() && box.numLearned() == 2, "Done: two controls learned", juce::String (box.numLearned()));
                const float t0 = box.getProject().m.tempo;
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (1, 70, 100));
                check (box.getProject().m.tempo > t0 + 20.0f, "the learned knob turns the tempo", juce::String (box.getProject().m.tempo));
                const bool playing0 = audio.isPlaying();
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (1, 80, 127));
                box.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (1, 80, 0));
                renderSeconds (audio, 0.02, nullptr);
                check (audio.isPlaying() != playing0, "the learned button is Play", {});
                box.forgetMidiMap();
                check (box.numLearned() == 0, "and it can be forgotten", {});
                box.setMaster ([t0] (Master& m) { m.tempo = t0; });
            }

            // ---- the keyboard's keys play the track; pads like a piano
            check (box.padNote (3, 8) == 48 && box.padNote (3, 15) == 60 && box.padNote (3, 0) == 49 && box.padNote (3, 2) < 0,
                   "pads: bottom row C D E F G A B C, top row the black keys", juce::String (box.padNote (3, 8)));
            check (box.padNote (0, 0) == 36 && box.padNote (0, 1) == 37, "drum pads: 1 kick, 2 snare ...", {});

            // ---- customization: every insert effect (knobs all up, all down), macros, MY knobs, modulation, sends
            {
                if (! audio.isPlaying()) press (BtnPlay);
                box.setSlider (0.5f);
                box.selectTrack (2);
                box.setMode (ModeSound);
                for (int i = 0; i < 30 && box.pageName() != "FX"; ++i) press (BtnNext);
                check (box.pageName() == "FX" && box.soundExtraPage() == Controller::ExtraFx, "SOUND has an FX page", box.pageName());
                int typesOk = 0;
                juce::String bad;
                for (int type = 1; type < NumInsertTypes; ++type)
                {
                    box.turnKnob (1, 1.0f);
                    if (box.getProject().tracks[2].mods.inserts[0].type != type) { bad << "type " << type << " not set; "; continue; }
                    bool ok = true;
                    for (float dir : { 100.0f, -100.0f, 0.0f })
                    {
                        if (dir != 0.0f) for (int k = 2; k < 10; ++k) box.turnKnob (k, dir);
                        else for (int k = 2; k < 10; ++k) box.resetKnob (k);
                        const auto st = renderSeconds (audio, 0.5, nullptr);
                        if (! st.finite || st.peak > 1.0f) { ok = false; bad << insertDef (type).name << " " << dir << " peak " << st.peak << "; "; }
                    }
                    typesOk += ok ? 1 : 0;
                }
                check (typesOk == NumInsertTypes - 1, juce::String (NumInsertTypes - 1) + " insert effects, every knob up and down: clean", bad);
                // four in a chain
                box.setMode (ModeMenu);
                box.padDown (15, 100); box.padUp (15);   // More
                check (box.menuTitle().startsWith ("MORE"), "MENU > More", box.menuTitle());
                box.padDown (5, 100); box.padUp (5);     // FX chain
                box.padDown (2, 100); box.padUp (2);     // Dub
                const auto& ins = box.getProject().tracks[2].mods.inserts;
                check (ins[0].type == InsFilter && ins[1].type == InsDelay && ins[2].type == InsReverb, "FX chain: Dub = filter > delay > reverb", {});
                const auto chainSt = renderSeconds (audio, 1.0, nullptr);
                check (chainSt.finite && chainSt.rms > 0.001f, "the chain plays", "rms " + juce::String (chainSt.rms, 4));
                // a macro: More > Assign > macro 1, then turn a knob of the sound
                box.setMode (ModeMenu);
                box.padDown (15, 100); box.padUp (15);
                box.padDown (2, 100); box.padUp (2);
                box.padDown (0, 100); box.padUp (0);
                check (box.assigning() == 0 && box.getMode() == ModeSound, "Assign to macro 1: back to the knobs", {});
                for (int i = 0; i < 30 && box.soundExtraPage() != -1; ++i) press (BtnNext);
                box.turnKnob (0, 3.0f);
                const auto& md = box.getProject().tracks[2].mods;
                check (md.slots[0].source == SrcMacro1 && md.slots[0].dest >= 0 && box.assigning() < 0, "a turned knob is linked to macro 1", box.modSlotText (2, 0));
                // MY knobs: More > Add to MY knobs, then turn a knob
                box.setMode (ModeMenu);
                box.padDown (15, 100); box.padUp (15);
                box.padDown (0, 100); box.padUp (0);
                box.turnKnob (1, 1.0f);
                check (md.my[0] >= 0, "Add to MY knobs: the turned knob is on MY page", box.destName (2, md.my[0]));
                // random modulation and turning the macro: clean
                box.setMode (ModeMenu);
                box.padDown (15, 100); box.padUp (15);
                box.padDown (4, 100); box.padUp (4);
                check (md.anyActive(), "Random modulation puts LFOs on the sound", box.modSlotText (2, 1));
                for (int i = 0; i < 30 && box.pageName() != "MACRO"; ++i) press (BtnNext);
                box.turnKnob (0, 100.0f);
                const auto modSt = renderSeconds (audio, 1.5, nullptr);
                check (modSt.finite && modSt.peak <= 1.0f, "modulation + macro 1 all the way: clean", "rms " + juce::String (modSt.rms, 4));
                // the MOD page: a slot by knobs
                for (int i = 0; i < 30 && box.pageName() != "MOD"; ++i) press (BtnNext);
                box.turnKnob (0, 5.0f);      // slot 6
                box.turnKnob (1, 2.0f);      // source: LFO 2
                box.turnKnob (2, 3.0f);      // destination: the 3rd
                box.turnKnob (3, 20.0f);     // amount
                const auto& s6 = md.slots[(size_t) box.modSlotNow()];
                juce::String all;
                for (int i = 0; i < 8; ++i) all << box.modSlotText (2, i) << " | ";
                all << box.pageName() << " k1=" << box.knob (1).name << " " << box.knob (1).value << " k2=" << box.knob (2).name << " " << box.knob (2).value;
                check (box.modSlotNow() == 5 && s6.source == SrcLfo2 && s6.dest >= 0 && s6.amount > 0.1f, "MOD page: slot, source, destination, amount", all);
                // every reverb type, delay type and an FX send, with the sends up
                for (int t = 0; t < numTracks; ++t) { box.setMaster ([] (Master&) {}); }
                bool sendsOk = true;
                juce::String sendInfo;
                for (int k = 0; k < 7; ++k)
                {
                    box.setMaster ([k] (Master& m) { m.revType = k; m.delayType = k; m.revDuck = 0.5f; m.delayDuck = 0.5f; m.delayToRev = 0.5f; m.delayFb = 0.9f;
                                                     m.fxSend.type = 1 + (k * 3) % (NumInsertTypes - 1); insertDef (m.fxSend.type).defaults (m.fxSend.p.data()); });
                    for (int t = 0; t < numTracks; ++t)
                        box.editPattern (t, [] (Pattern&) {});
                    const auto st = renderSeconds (audio, 1.0, nullptr);
                    if (! st.finite || st.peak > 1.0f || st.rms < 0.001f) { sendsOk = false; sendInfo << revTypeNames()[k] << "/" << delayTypeNames()[k] << " rms " << st.rms << "; "; }
                }
                check (sendsOk, "7 reverb types, 7 delay types and FX sends: clean", sendInfo);
                box.setMaster ([] (Master& m) { m.revType = 3; m.delayType = 1; m.revDuck = 0.0f; m.delayDuck = 0.0f; m.delayToRev = 0.2f; m.delayFb = 0.45f; });
                renderSeconds (audio, 4.0, nullptr);
                const auto after = renderSeconds (audio, 2.0, nullptr);
                check (after.rms > 0.05f, "the shimmer reverb with every send up doesn't run away", "rms " + juce::String (after.rms, 4));
                // the look
                auto lk = box.getLook();
                lk.theme = 1; lk.accent = 4; lk.knobStyle = 1; lk.trackColours = true;
                box.setLook (lk);
                check (box.getDataDirectory().getChildFile ("look.json").existsAsFile(), "the look is remembered", {});
                box.setMode (ModeSound);
            }

            // ---- the libraries (when they are there: CATBOX_LIBRARY or CatBox/library)
            if (SampleBank::get().libraryFolders().size() > 0)
            {
                auto& bank = SampleBank::get();
                const int folders = bank.libraryFolders().size();
                check (folders > 100, "the libraries are indexed", juce::String (folders) + " folders, " + juce::String (bank.count()) + " samples");
                auto presetOf = [] (int engine, const char* name) { const auto& d = engineDef (engine); for (size_t i = 0; i < d.presets.size(); ++i) if (d.presets[i].name == name) return (int) i; return 0; };
                if (! audio.isPlaying()) press (BtnPlay);
                // every library sound: its samples are found, read, and it plays clean
                juce::String missing, levels;
                int sounds = 0, silent = 0;
                for (int engine : { EngineKit, EngineSampler, EngineGranular })
                {
                    const auto& d = engineDef (engine);
                    for (size_t i = 0; i < d.presets.size(); ++i)
                    {
                        if (d.presets[i].samples.empty()) continue;
                        for (const auto& [id, nm] : d.presets[i].samples) if (bank.find (nm) < 0) missing << d.presets[i].name << ": " << nm << "; ";
                        const int t = engine == EngineKit ? 0 : 7;
                        box.loadSound (t, engine, (int) i);
                        ++sounds;
                        if (engine != EngineKit)
                        {
                            audio.post ({ Event::NoteOn, (int8_t) t, 60, 0.9f });
                            const auto st = renderSeconds (audio, 0.5, nullptr);
                            audio.post ({ Event::NoteOff, (int8_t) t, 60, 0.0f });
                            renderSeconds (audio, 0.1, nullptr);
                            if (! st.finite || st.peak > 1.0f) missing << d.presets[i].name << " not clean; ";
                            if (st.rms < 0.01f) ++silent;
                        }
                    }
                }
                check (missing.isEmpty() && silent < 3, juce::String (sounds) + " library sounds find their samples and play clean", missing.substring (0, 600) + juce::String (silent) + " quiet");
                box.loadSound (0, EngineKit, presetOf (EngineKit, "TR-808"));
                const auto kitSt = renderSeconds (audio, 1.0, nullptr);
                check (kitSt.finite && kitSt.rms > 0.002f && box.laneName (0, 0).startsWith ("BD"), "Kit: the TR-808 plays the beat", box.laneName (0, 0) + " rms " + juce::String (kitSt.rms, 4));
                box.loadSound (7, EngineSampler, presetOf (EngineSampler, "Violins"));
                const auto& d = engineDef (EngineSampler);
                check ((int) box.getProject().tracks[7].params[(size_t) d.index ("root")] == SampleBank::noteFromName (bank.name ((int) box.getProject().tracks[7].params[(size_t) d.index ("sample")])),
                       "Sampler: Violins play from the sample's own note", bank.name ((int) box.getProject().tracks[7].params[(size_t) d.index ("sample")]));
                {
                    const int vs = (int) box.getProject().tracks[7].params[(size_t) d.index ("sample")];
                    const auto* zm = bank.zonesFor (vs);
                    check (zm != nullptr && zm->keys[48].root != zm->keys[84].root && zm->keys[60].layers >= 1,
                           "Violins are a multi-sample: other notes, other recordings", zm != nullptr ? juce::String (zm->keys[48].root) + " / " + juce::String (zm->keys[84].root) : juce::String ("no zones"));
                    // play low, middle and high: every one sounds
                    bool allPlay = true;
                    for (int note : { 43, 60, 79, 91 })
                    {
                        audio.post ({ Event::NoteOn, 7, (int16_t) note, 0.8f });
                        const auto st = renderSeconds (audio, 0.6, nullptr);
                        audio.post ({ Event::NoteOff, 7, (int16_t) note, 0.0f });
                        renderSeconds (audio, 0.3, nullptr);
                        allPlay = allPlay && st.finite && st.rms > 0.003f;
                    }
                    check (allPlay, "a multi-sample plays from the bottom to the top", {});
                }
                // the browser lists the library folders and picks a sample from them
                box.selectTrack (7);
                box.setMode (ModeSound);
                press (BtnSound);
                const auto cats = box.browseCategories();
                int libCat = -1;
                for (int i = 0; i < cats.size(); ++i) if (cats[i].endsWith ("/Harp")) libCat = i;
                check (libCat >= 0, "the browser has the library folders (Sampler)", juce::String (cats.size()) + " categories");
                if (libCat >= 0)
                {
                    box.turnKnob (1, (float) (libCat - box.browseCategory()));
                    box.turnKnob (2, 5.0f);
                    check (box.presetName (7) == box.browsePresets()[box.browsePresetIndex()], "turning picks a sample of the folder", box.presetName (7));
                }
                box.setMode (ModeSound);
                box.loadSound (0, EngineDrums, presetOf (EngineDrums, "808 Classic"));
                box.loadSound (7, EngineSampler, presetOf (EngineSampler, "Break Chop"));
            }

            // ---- the MENU (Project twice): drum pattern, clear (press twice), save / load, help
            {
                box.selectTrack (5);
                box.setMode (ModeProject);
                press (BtnProject);
                check (box.getMode() == ModeMenu && box.menuTiles()[6].label == "Drum pattern", "Project twice = the MENU", box.menuTitle());
                box.padDown (6, 100); box.padUp (6);
                check (box.menuTitle().startsWith ("DRUM PATTERN"), "pad 7: drum patterns", box.menuTitle());
                box.padDown (0, 100); box.padUp (0);
                check (box.getProject().tracks[5].pat.drums[0][4] > 0 && box.getProject().tracks[5].pat.drums[2][4] > 0, "pad 1: a house beat on the drum track", {});
                box.padDown (15, 100); box.padUp (15);   // Back
                check (box.menuTitle() == "MENU", "pad 16: back to the menu", box.menuTitle());
                box.padDown (3, 100); box.padUp (3);
                check (! box.getProject().tracks[5].pat.empty() && box.menuConfirming() == 3, "Clear track: the first press only asks", {});
                box.padDown (3, 100); box.padUp (3);
                check (box.getProject().tracks[5].pat.empty(), "the second press clears", {});
                shiftPress (BtnChaos);   // undo the clear
                shiftPress (BtnChaos);   // undo the drum pattern
                check (box.getProject().tracks[5].pat.drums[0][4] == 0 || box.getProject().tracks[5].pat.drums[2][4] == 0, "Undo takes the drum pattern away again", {});
                box.setMode (ModeMenu);
                box.padDown (0, 100); box.padUp (0);   // Save project
                box.padDown (2, 100); box.padUp (2);   // into place 3
                check (box.projectExists (2), "Save project into place 3", {});
                box.clearPattern (5);
                box.padDown (1, 100); box.padUp (1);   // Load project
                box.padDown (2, 100); box.padUp (2);
                check (! box.getProject().tracks[5].pat.empty(), "Load project 3 brings it back", {});
                box.setMode (ModeMenu);
                box.padDown (11, 100); box.padUp (11);
                check (box.isHelpShown() && box.helpLines().size() > 2, "Help shows how the page works", box.helpLines()[0]);
                box.toggleHelp();
                box.selectTrack (6);
                box.setMode (ModeMenu);
                box.padDown (7, 100); box.padUp (7);
                box.padDown (3, 100); box.padUp (3);
                int bassNotes = 0;
                for (int i = 0; i < 16; ++i) bassNotes += box.getProject().tracks[6].pat.steps[(size_t) i].on() ? 1 : 0;
                check (bassNotes > 6, "Bass line: an acid line on a melody track", juce::String (bassNotes));
                shiftPress (BtnChaos);
                box.setMode (ModeSound);
            }

            // ---- factory reset: PROJECT knob 12 all the way right (undoable)
            {
                box.setMode (ModeProject);
                const auto before = box.getProject().tracks[5].pat.drums;
                box.turnKnob (11, 10.0f);
                check (box.knob (11).name == "Factory reset" && box.knob (11).value.contains ("keep turning"), "PROJECT knob 12: Factory reset (turn all the way)", box.knob (11).value);
                check (box.getProject().tracks[5].pat.drums == before, "half way it does nothing", {});
                box.turnKnob (11, 20.0f);
                check (box.getProject().tracks[5].pat.empty() && ! box.sceneExists (2) && box.getTrack() == 0, "all the way: factory reset", box.flashText());
                box.buttonDown (BtnShift); box.buttonDown (BtnChaos); box.buttonUp (BtnChaos); box.buttonUp (BtnShift);
                check (box.getProject().tracks[5].pat.drums == before && box.sceneExists (2), "Undo brings the project and the scenes back", box.flashText());
            }

            box.saveState();

            // ---- the screen in every mode, and the PC panel
            {
                View screen (box, true);
                screen.setSize (View::screenW, View::screenH);
                View pc (box, false);
                pc.setSize (View::panelW, View::panelH);
                int shot = 0;
                for (int m : { ModeSound, ModeBrowse, ModeSeq, ModeFx, ModeMix, ModeProject })
                {
                    box.selectTrack (m == ModeSeq ? 0 : 1);
                    box.setMode (m);
                    const auto img = screen.snapshotScreen();
                    check (img.isValid() && img.getWidth() == View::screenW, juce::String ("the screen draws ") + modeName (m), {});
                    juce::PNGImageFormat png;
                    const auto pngFile = wavOut.getSiblingFile (wavOut.getFileNameWithoutExtension() + "-screen" + juce::String (++shot) + ".png");
                    pngFile.deleteFile();
                    if (auto os = pngFile.createOutputStream())
                        png.writeImageToStream (img, *os);
                }
                {
                    box.selectTrack (7);   // the sampler (Break Chop): its waveform and slices
                    box.setMode (ModeSound);
                    const auto img = screen.snapshotScreen();
                    const auto pngFile = wavOut.getSiblingFile (wavOut.getFileNameWithoutExtension() + "-screen7.png");
                    pngFile.deleteFile();
                    juce::PNGImageFormat png;
                    if (auto os = pngFile.createOutputStream()) png.writeImageToStream (img, *os);
                    check (img.isValid(), "the screen draws the sampler", {});
                }
                const juce::StringArray extraPages { "FX", "MACRO", "MOD", "MY" };
                for (int e = 0; e < 6; ++e)   // 10-13: the FX, MACRO, MOD, MY pages (light theme, rings); 14: RETURNS; 15: LOOK
                {
                    box.selectTrack (2);
                    if (e < 4)
                    {
                        box.setMode (ModeSound);
                        for (int i = 0; i < 30 && box.pageName() != extraPages[e]; ++i) { box.buttonDown (BtnNext); box.buttonUp (BtnNext); }
                    }
                    else if (e == 4) { box.setMode (ModeMix); for (int i = 0; i < 4 && box.pageIndex() != 3; ++i) { box.buttonDown (BtnNext); box.buttonUp (BtnNext); } }
                    else { box.setMode (ModeProject); for (int i = 0; i < 2 && box.pageIndex() != 1; ++i) { box.buttonDown (BtnNext); box.buttonUp (BtnNext); } }
                    const auto img = screen.snapshotScreen();
                    check (img.isValid(), "the screen draws " + (e < 4 ? extraPages[e] : e == 4 ? juce::String ("RETURNS") : juce::String ("LOOK")), {});
                    const auto pngFile = wavOut.getSiblingFile (wavOut.getFileNameWithoutExtension() + "-screen" + juce::String (10 + e) + ".png");
                    pngFile.deleteFile();
                    juce::PNGImageFormat png;
                    if (auto os = pngFile.createOutputStream()) png.writeImageToStream (img, *os);
                }
                box.setLook (Look());
                for (int extra : { 8, 9 })   // the MENU, and the help on the SEQ page
                {
                    box.selectTrack (0);
                    box.setMode (extra == 8 ? ModeMenu : ModeSeq);
                    if (extra == 9) box.toggleHelp();
                    const auto img = screen.snapshotScreen();
                    if (extra == 9) box.toggleHelp();
                    check (img.isValid(), extra == 8 ? "the screen draws the MENU" : "the screen draws the help", {});
                    const auto pngFile = wavOut.getSiblingFile (wavOut.getFileNameWithoutExtension() + "-screen" + juce::String (extra) + ".png");
                    pngFile.deleteFile();
                    juce::PNGImageFormat png;
                    if (auto os = pngFile.createOutputStream()) png.writeImageToStream (img, *os);
                }
                box.setMode (ModeMenu);
                const auto panel = pc.createComponentSnapshot (pc.getLocalBounds());
                check (panel.isValid(), "the PC panel draws", {});
                juce::PNGImageFormat png;
                const auto panelFile = wavOut.getSiblingFile (wavOut.getFileNameWithoutExtension() + "-panel.png");
                panelFile.deleteFile();
                if (auto os = panelFile.createOutputStream())
                    png.writeImageToStream (panel, *os);
            }
        }

        // ---- the link: a box and a PC app talk over the network (here: on this computer)
        {
            const auto dirA = dir.getSiblingFile (dir.getFileName() + "-box"), dirB = dir.getSiblingFile (dir.getFileName() + "-pc");
            {
                auto boxAOwner = std::make_unique<Controller> (dirA), pcOwner = std::make_unique<Controller> (dirB);
                Controller& boxA = *boxAOwner;
                Controller& pc = *pcOwner;
                boxA.getAudio().prepare (sr, block);
                pc.getAudio().prepare (sr, block);
                const int port = 50700 + juce::Random::getSystemRandom().nextInt (200);
                LinkServer server (boxA, port, false, "127.0.0.1");
                check (server.isListening(), "the box listens for a PC", "port " + juce::String (port));
                LinkClient client (pc, false);
                check (client.connect ("127.0.0.1", port) && pc.isRemote(), "the PC app connects to the box", {});
                wait (600);
                boxA.setMode (ModeProject);
                wait (300);
                const float t0 = boxA.getProject().m.tempo;
                pc.turnKnob (0, 3.0f);
                wait (500);
                check (std::abs (boxA.getProject().m.tempo - (t0 + 3.0f)) < 0.01f, "a knob on the PC turns the box", juce::String (boxA.getProject().m.tempo));
                check (std::abs (pc.getProject().m.tempo - (t0 + 3.0f)) < 0.01f, "the PC shows what the box has", juce::String (pc.getProject().m.tempo));
                pc.buttonDown (BtnTrack1 + 4); pc.buttonUp (BtnTrack1 + 4);
                wait (400);
                check (boxA.getTrack() == 4 && pc.getTrack() == 4, "a button on the PC presses it on the box", juce::String (boxA.getTrack()));
                pc.buttonDown (BtnPlay); pc.buttonUp (BtnPlay);
                for (int i = 0; i < 10; ++i) { renderSeconds (boxA.getAudio(), 0.05, nullptr); wait (50); }
                check (pc.getAudio().isPlaying(), "the PC shows that the box plays", {});
                const auto silent = renderSeconds (pc.getAudio(), 0.3, nullptr);
                check (silent.rms == 0.0f, "linked, the PC itself stays silent", {});
                client.disconnect();
                check (! pc.isRemote(), "after Disconnect the PC plays by itself again", {});
            }
            dirA.deleteRecursively();
            dirB.deleteRecursively();
        }

        // ---- after a restart everything is there
        {
            auto againOwner = std::make_unique<Controller> (dir);
            Controller& again = *againOwner;
            int snares = 0;
            for (auto v : again.getProject().tracks[5].pat.drums[1]) snares += v > 0 ? 1 : 0;
            check (snares == 1, "after a restart: the recorded drum", juce::String (snares));
            check (again.getProject().tracks[2].mods.inserts[1].type == InsDelay && again.getProject().tracks[2].mods.anyActive()
                   && again.getProject().m.revType == 3, "after a restart: the FX chain, the modulation, the sends", {});
            check (again.getProject().tracks[6].pat.length == 32 && again.getProject().tracks[6].pat.steps[5].has (71), "after a restart: the steps", {});
            check (again.sceneExists (2), "after a restart: the scene", {});
            check (again.getProject().tracks[3].engine == EngineAnalog, "after a restart: the sounds", {});
        }
        dir.deleteRecursively();

        const auto report = wavOut.withFileExtension ("txt");
        report.replaceWithText (log.joinIntoString ("\n") + "\n");
        std::printf ("%s\n", log.joinIntoString ("\n").toRawUTF8());
        std::printf ("%d checks, %d failed\n", log.size(), failures);
        return failures == 0 ? 0 : 1;
    }

private:
    struct Stats { float rms = 0.0f, peak = 0.0f; bool finite = true; };

    static void clearAnd (Controller& box, int t) { box.clearPattern (t); }

    static void wait (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    Stats renderSeconds (Audio& audio, double seconds, juce::AudioBuffer<float>* keep)
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
    AppContent (View* v, LinkPanel* p) : view (v), panel (p)
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
    View& getView() { return *view; }
    LinkPanel* getPanel() { return panel.get(); }

private:
    std::unique_ptr<View> view;
    std::unique_ptr<LinkPanel> panel;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (Controller& box, juce::AudioDeviceManager& dm, bool kiosk, LinkClient* link, std::function<juce::String()> info)
        : DocumentWindow ("Cat Box", juce::Colour (0xff0b0d12), kiosk ? 0 : DocumentWindow::allButtons),
          deviceManager (dm)
    {
        auto* view = new View (box, kiosk);
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
                setBounds (d->logicalBounds.toNearestInt());
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
    const juce::String getApplicationVersion() override { return "2.0.0"; }
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

        box = std::make_unique<Controller> (dataDir);
        startAudio (dataDir);
        box->enableMicrophone = [this]
        {
            // the input is switched on only when a sample is recorded (no microphone open all the time)
            if (auto* dev = deviceManager.getCurrentAudioDevice(); dev != nullptr && dev->getActiveInputChannels().countNumberOfSetBits() > 0) return true;
            auto setup = deviceManager.getAudioDeviceSetup();
            if (auto* type = deviceManager.getCurrentDeviceTypeObject())
            {
                const auto ins = type->getDeviceNames (true);
                if (ins.isEmpty()) return false;
                if (setup.inputDeviceName.isEmpty()) setup.inputDeviceName = ins[juce::jmax (0, type->getDefaultDeviceIndex (true))];
            }
            setup.useDefaultInputChannels = false;
            setup.inputChannels.clear();
            setup.inputChannels.setRange (0, 2, true);
            const auto err = deviceManager.setAudioDeviceSetup (setup, true);
            auto* dev = deviceManager.getCurrentAudioDevice();
            return err.isEmpty() && dev != nullptr && dev->getActiveInputChannels().countNumberOfSetBits() > 0;
        };
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
    std::unique_ptr<Controller> box;
    std::unique_ptr<LinkServer> server;   // the box
    std::unique_ptr<LinkClient> link;     // the PC app
    std::unique_ptr<MainWindow> window;
    juce::StringArray seenMidi;
};
} // namespace catbox

START_JUCE_APPLICATION (catbox::CatBoxApplication)
