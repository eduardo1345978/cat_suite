# Using it in your DAW

The Cats are **VST3** plug-ins, so they work in any DAW that loads VST3: **Cubase, FL Studio, Ableton Live, Reaper, Bitwig Studio, Studio One, Ardour** and others (Windows, Mac and Linux). Logic Pro and GarageBand only load Audio Units, so they can't use them.

## Effects and instruments: just load them
- **Effects** (Echo-Cat, Phase-Cat, Molten, Swarm, Vortex): put them in an insert / FX slot of an audio track.
- **Instruments** (Sing-Cat, Talk-Cat, Glide-Cat, Glitch-Cat, Negative, Meow-Cat ...): load them on an instrument track and play.
- Everything that goes "in time" follows the DAW's tempo and Play; every control can be automated.

## MIDI Cats: route their MIDI out
Chord, Patt, Bend, Drum, Orbit, Bass, Micro, Hum, Life and Muse-Cat **make MIDI** for another instrument. They load as an **instrument with a MIDI output**, and the DAW sends that MIDI to your synth:

| DAW | How |
|---|---|
| **Cubase** | Two instrument tracks: the Cat, and your synth with *MIDI Input = "Chord-Cat - MIDI Out"* and Monitor on (details below). |
| **Reaper** | Put the Cat and then your synth in the **same track's FX chain**: the MIDI flows from one to the next. |
| **FL Studio** | In the Cat's wrapper settings set a **MIDI output port**, and give your synth the same **MIDI input port**. |
| **Bitwig, Studio One** | Send the Cat's note / MIDI output to the synth's track (see your DAW's manual for routing plug-in MIDI). |
| **Any DAW** | Don't want to route? Use the Cat's own sound where it has one (Micro-Cat, Hum-Cat, Muse-Cat), drag **Drag MIDI** straight onto a track, or let the Cats follow each other with **[Cat Link](Cat-Link.md)**. |

Some DAWs (Ableton Live, for example) can't route a plug-in's MIDI output to another track; there, the last row is the way.

---

## Cubase in detail

Cubase's **MIDI Insert slots only host Steinberg's own MIDI plug-ins**; no third-party VST3 can be loaded there. Third-party MIDI generators such as Scaler, Chordimist and Cthulhu are loaded as *instruments*, and their **MIDI Out** is routed to other tracks. That is why the default build (`CAT_CUBASE_INSTRUMENT_MODE=ON`) registers each plug-in as a VST3 instrument with MIDI in/out and a silent stereo output.

### One plug-in
1. *Project > Add Track > Instrument* and choose **Chord-Cat** (it appears under *Instrument / Generator*).
2. Add a second Instrument track with your synth (piano, Pigments, etc.).
3. On the synth track, set **MIDI Input = "Chord-Cat - MIDI Out"** and enable **Monitor**.
4. Record-enable the Chord-Cat track and play C3 to B4 (Cubase octave names, C3 = MIDI 60).

### The full chain (Chord-Cat → Patt-Cat → Bend-Cat → synth)
| Track | Instrument | MIDI Input | Monitor |
|---|---|---|---|
| 1 | Chord-Cat | your keyboard | on |
| 2 | Patt-Cat | Chord-Cat - MIDI Out | on |
| 3 | Bend-Cat | Patt-Cat - MIDI Out | on |
| 4 | your synth (set to MPE, bend range = Bend-Cat's **Bend Range**) | Bend-Cat - MIDI Out | on |

**Sing-Cat is a normal instrument:** load it on an instrument track and play it, or feed it from another Cat (e.g. Chord-Cat's MIDI Out) to make a choir sing your progression. For MPE controllers, switch **MPE** on and set **Bend** to 48.

Drum-Cat and Orbit-Cat work the same way: load them as instruments and route their MIDI Out to a drum instrument (Groove Agent SE uses the General MIDI map) or a synth. Drum-Cat follows the transport; with the transport stopped, holding any key on its track plays the pattern. Orbit-Cat needs held notes (or Latch), e.g. from your keyboard or Chord-Cat's MIDI Out.

To print the result, record on a MIDI track whose input is the last plug-in's MIDI Out. Alternatively, drag **Drag MIDI** from any plug-in straight onto a track.

### Cubase integration notes
- **Quick Controls / remote devices:** all parameters are automatable, so you can assign them to Quick Controls directly. The build keeps JUCE's `IMidiMapping` implementation (`JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=1`), so CCs reach the plug-in and MIDI Learn works with hardware controllers.
- **Chord Track:** the VST3 SDK gives plug-ins no access to Cubase's Chord Track. Two workarounds:
  - *Import:* turn on **Chord Track Sync** (GENERATE tab) and route the chord track's MIDI to Chord-Cat (Chord Track > *Chords to MIDI*, or a MIDI track that plays the chords). Each new chord is recognised, written to the next pad and re-voiced. Alternatively, use **Import MIDI / Chord Track…** on an exported MIDI file.
  - *Export:* **Drag MIDI** writes each chord name as a marker. Drop the file on a track, then use *Project > Chord Track > Create Chord Symbols*.
- **Expression Maps:** key-switch notes outside the trigger range, plus CCs, program changes and aftertouch, pass through unchanged.
- **MPE / Note Expression:** Bend-Cat sends a standard MPE lower zone: the MPE Configuration Message (RPN 6) and pitch-bend sensitivity (RPN 0) on channel 1, then one member channel per voice from channel 2 up (15 voices by default). The set-up is repeated before the first note after a pause and whenever the transport starts. Use it with an instrument **in MPE mode**. Its pitch-bend range must equal Bend-Cat's **Bend Range**: many MPE instruments default to 48, so either set Bend Range to 48 or set the instrument to 2. Turn MPE off for ordinary (non-MPE) instruments; Bend-Cat then sends a single channel-wide bend.

## Pure MIDI-effect build (other DAWs)
For hosts that support VST3 MIDI effects natively (e.g. Bitwig, Reaper), build with `-DCAT_CUBASE_INSTRUMENT_MODE=OFF`. This sets `isMidiEffect() == true` and removes the audio bus.
