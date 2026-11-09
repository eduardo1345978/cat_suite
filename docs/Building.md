# Building it yourself


Requirements: CMake ≥ 3.22, plus a C++20 compiler (Visual Studio 2022 or 2026 on Windows 11, Xcode 15 on macOS, or GCC/Clang on Linux). JUCE 8.0.15 is downloaded automatically as a zip archive, so git is not needed. Without internet access, point `CAT_JUCE_DIR` at a local JUCE copy.

```bat
:: Windows 11 (CMake picks the newest installed Visual Studio)
cmake -B build -A x64
cmake --build build --config Release

:: install: copy the three bundles to the system VST3 folder (admin prompt)
xcopy /E /I build\ChordCat\ChordCat_artefacts\Release\VST3\Chord-Cat.vst3 "C:\Program Files\Common Files\VST3\Chord-Cat.vst3"
xcopy /E /I build\PattCat\PattCat_artefacts\Release\VST3\Patt-Cat.vst3   "C:\Program Files\Common Files\VST3\Patt-Cat.vst3"
xcopy /E /I build\BendCat\BendCat_artefacts\Release\VST3\Bend-Cat.vst3   "C:\Program Files\Common Files\VST3\Bend-Cat.vst3"
xcopy /E /I build\DrumCat\DrumCat_artefacts\Release\VST3\Drum-Cat.vst3   "C:\Program Files\Common Files\VST3\Drum-Cat.vst3"
xcopy /E /I build\OrbitCat\OrbitCat_artefacts\Release\VST3\Orbit-Cat.vst3 "C:\Program Files\Common Files\VST3\Orbit-Cat.vst3"
xcopy /E /I build\SingCat\SingCat_artefacts\Release\VST3\Sing-Cat.vst3   "C:\Program Files\Common Files\VST3\Sing-Cat.vst3"
```

Or simply double-click **`build_and_install.bat`**: it builds, runs the tests and installs all seven into your **user** VST3 folder, `%LOCALAPPDATA%\Programs\Common\VST3`, which Cubase also scans. No administrator rights are needed. (`install_vst3.bat` does just the copy.)

Then rescan in Cubase via *Studio > VST Plug-in Manager*.

| CMake option | Default | Meaning |
|---|---|---|
| `CAT_CUBASE_INSTRUMENT_MODE` | `ON` | Instrument + MIDI out (needed for Cubase); `OFF` = pure MIDI effect |
| `CAT_PLUGIN_FORMATS` | `VST3;Standalone` | Formats to build (add `AU` on macOS) |
| `CAT_COPY_AFTER_BUILD` | `OFF` | Copy into the system plug-in folder after each build (needs write access) |
| `CAT_JUCE_DIR` | *(empty)* | Use a local JUCE checkout instead of downloading |
| `CAT_BUILD_TESTS` | `OFF` | Build the headless VST3 host test (below) |

Each plug-in also configures on its own, e.g. `cmake -S ChordCat -B build-chordcat`.

### Tests
`-DCAT_BUILD_TESTS=ON` builds `CatSuiteHostTest`. This headless host loads the seven **built `.vst3` binaries**, chains them at 120 BPM and checks:

- one-finger triggering and voice leading
- pass-through of out-of-range notes and CCs
- that the pattern lands on the beat to the sample
- MPE channel allocation, and initial bend sent before each note-on
- the MPE Configuration Message
- note-on/off balance across the chain
- state save/restore
- Sing-Cat: audible, in tune (pitch measured from the audio), vowels move the formants, silent after release, MPE bend, all presets finite and unclipped, 64 singers in real time
- Echo-Cat: loads as an audio effect and a click comes back on time (1/8 dotted at 120 BPM = 375 ms)

`-DCAT_BUILD_STRESS=ON` also builds `EchoCheck` (echo timing synced and free, decay, ping-pong sides, freeze, ducking, every preset finite and bounded, state) and `ComposeCheck` (incl. your own chords and words).

```
cmake -B build -DCAT_BUILD_TESTS=ON && cmake --build build --config Release
build/Tests/CatSuiteHostTest_artefacts/Release/CatSuiteHostTest
```

---

## Project layout

```
CMakeLists.txt            suite root (JUCE fetch, three plug-ins, optional tests)
cmake/                    JuceSetup.cmake, CatPlugin.cmake (shared juce_add_plugin settings)
Shared/Source/            look & feel, scalable editor window, widgets with MIDI Learn, MIDI scheduler, host sync, MIDI export
ChordCat/Source/          ChordFormulas, Scales, ChordEngine, ProgressionGenerator, PatternPlayer,
                          FactoryPresets, PluginProcessor, PluginEditor
PattCat/Source/           PattCatEngine, EuclideanGenerator, PatternGridUI, PluginProcessor, PluginEditor
BendCat/Source/           BendCatEngine, MPEAllocator, ScalaParser, EnvelopeEditorUI, BendVisualizer,
                          PluginProcessor, PluginEditor
DrumCat/Source/           DrumCatEngine (style models + generator), PluginProcessor, PluginEditor
OrbitCat/Source/          OrbitEngine (orbital model + presets), PluginProcessor, PluginEditor
SingCat/Source/           VoiceModel (glottis + vocal tract + nose), TalkEngine (singers, casts, styles, stage, room),
                          PluginProcessor (parameters, 54 presets), PluginEditor (vowel chart, stage with faces)
BassCat/Source/           BassEngine (style models, chord analysis), PluginProcessor, PluginEditor (degree grid)
TalkCat/Source/           Phonetics (EN / SK / JA / Cat -> syllables), Vocal (tract), VoiceEngine (singer, pitch model, harmonies, FX),
                          Score (notes, lyrics, curves), PluginProcessor (score player, live), PluginEditor (piano roll, lanes, tabs)
GlideCat/Source/          GlideSynth (voice leading, glides, autopilot, 2-layer voices, modulation), GlideFX (Ensemble, Prism, Rooms),
                          PluginProcessor (parameters, 42 sound + 32 glide presets), PluginEditor (Glide View + tabs)
Tests/                    headless VST3 host test
docs/reference/           UI reference screenshots
```

## How it works

**Shared engine pieces**
- `HostSync` reads the playhead once per block and gives ppq for any sample. When the transport is stopped, a free-running clock starts on the first key, so rhythms and LFOs still work in the Standalone app.
- `MidiScheduler` is a time-ordered queue in absolute sample time. Strums, gates and ratchets can therefore end beyond the current block without adding latency. It keeps note-on/off counts per channel, so stacked or re-struck notes never hang.
- Each `processBlock` renders time-based events (rhythm hits, bend points) up to the next incoming MIDI event, then applies that event. This keeps everything sample-accurate.

**Chord-Cat**
- **Voicing:** formula + tensions → voicing model → every inversion × octave shift → voice-leading cost (Σ|Δ| against the previous chord) blended with the inversion you chose, according to Voice Leading % → per-voice custom offsets.
- **Wide Neo-Soul** voices Cmaj9 as C2-G3-E4-B4-D5.
- **Generator:** a Markov chain over 13 states (7 diatonic degrees plus bII, bIII, bVI, bVII, V/V, V/vi). It is learned at start-up from small Neo-Soul / R&B / Lo-Fi / Jazz / Pop progression corpora, weighted by mood, with extensions set by complexity.
- **Trigger range:** default C3–B4. Pads repeat across the range; white-key mode is available.
- **Several trigger keys:** the most recent key owns the chord. Keys pressed within 30 ms of each other count as one press (the last key wins). Releasing a key never re-triggers the chord of another key that is still held.

**Patt-Cat**
- Layers A–D each have their own length, division, swing and channel. Bar-locked layers restart on bar lines; **Poly** layers loop freely, e.g. 12 against 16 steps.
- **Input modes:** *Hold Chord* plays the held chord. *Advance Chords* remembers each new chord, and step *n* plays chord *n*.
- **Chord changes:** note-offs and note-ons that arrive on the same sample count as one chord change, so the pattern does not stop and restart. A note-on more than 50 ms after the previous one starts a new chord, even if keys of the old chord are still down, so legato playing does not merge two chords. If a chord arrives up to 80 ms after a hit has started, that hit switches to the new chord, and long notes follow the chord change. Shared notes keep ringing in both cases. The chord's velocity is that of its loudest key.

**Bend-Cat**
- **Bend Range** (1–48 st) is only the MIDI pitch-bend scaling and must match the instrument. **Amount** (1–24 st) sets how far the envelope and LFO bend at 100 % depth. The two are independent, so changing the range never changes how far the notes move.
- Human Detune and Drift default to 0, so notes stay exactly in tune unless you dial them in.
- Bend is computed per voice at the chosen control rate (32–256 samples) and sent only when the 14-bit value changes. A voice's initial bend is sent one sample before its note-on.
- 14-bit value = `8192 + semitones / range × 8192`.

**Drum-Cat**
- Each style is a hand-built model: kick probabilities around the backbeat, hat behaviour (8ths, 16ths, ride, trap rolls, sparse + shaker), ghost-note and rim habits, and its feel (snare lateness, kick / hat looseness, swung 16ths). A one-bar core is varied bar by bar (A B A B') with an optional fill at the end.
- Playback reads the pattern ahead of the playhead, so Human and the style's feel can play hits slightly early as well as late without adding latency. Chance, velocity randomness and open hats are deterministic per loop: scrubbing back plays the same pass again.

**Orbit-Cat**
- Period = 2 beats x (radius / 8 AU)^1.5 / speed / sqrt(0.25 + 1.5 x gravity). Eccentric orbits follow Kepler's equation, so with 2 or more spokes they pulse unevenly. Mutual pull is mass- and distance-weighted and damped per note; drag shrinks the orbits while the keys are held (an accelerando), and turbulence adds smooth random wobble.
- Quantize delays each trigger to the next grid line of the host's grid.

**Sing-Cat**
- Source: a Rosenberg-style glottal pulse (its derivative, so lip radiation is included). Tension sets the open phase, plus aspiration noise pulsed by the airflow, jitter, shimmer, and period-doubling rasp.
- Tract: a Kelly-Lochbaum waveguide with one section per sample step, so a 17 cm tract has ~24 sections at 48 kHz (twice that in High quality). A nasal tube is coupled at the velum by a 3-way junction. The output is the volume velocity at the lips and nostrils, which keeps vowels at similar loudness.
- Vowels: 8 anchor vowels (i e ae a aw o u uh) have area functions fitted offline to adult formants (e.g. /i/ 270/2190 Hz, /a/ 730/1130 Hz, /u/ 305/890 Hz). Any chart point blends the anchors in log-area. Shorter tracts (women ~14-15 cm, children 11.5, cats 5.5-9) raise every formant naturally.
- Articulation: Hum, N and Meow onsets close the lips or tongue and open the velum. A meow is m - i - a - ow with a rising then falling pitch. Purring closes the mouth, opens the nose and pulses ~25 times a second.
- Each singer gets seeded traits (tract size, tuning, vibrato rate/depth, drift, onset timing), so **Dice** gives a new choir. Tutti divides the held notes from low to high voices and puts each note in the singer's own octave. Stage depth lowers level and treble and raises the room send.
