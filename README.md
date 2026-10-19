# Cat Suite — Chord-Cat · Patt-Cat · Bend-Cat · Drum-Cat · Orbit-Cat · Bass-Cat · Sing-Cat · Glide-Cat · Talk-Cat · Life-Cat · Muse-Cat

## Download (Windows)

1. Open **[Releases > latest](../../releases/tag/latest)** and download **Cat-Suite-Windows.zip**.
2. Unzip it, close Cubase and double-click **Install-Cat-Suite.bat**.
3. In Cubase: **Studio > VST Plug-in Manager > Rescan**.

**Only the instruments that make sound, as apps** (Sing-Cat, Talk-Cat, Glide-Cat, Life-Cat): download **[Get-Cat-Sound-Apps.bat](../../releases/download/latest/Get-Cat-Sound-Apps.bat)** and double-click it (if Windows warns, click *More info > Run anyway*). It downloads just those four, installs them as standalone apps with Start menu and Desktop shortcuts, and also as VST3 plug-ins. In the apps you can play with a MIDI keyboard or your **computer keyboard**: `A S D F G H J K L` = white keys, `W E T Y U O P` = black keys (Z works as Y on Slovak / German keyboards), Left / Right arrow = octave, Down / Up arrow = velocity.

**Cat Hub** (comes with Get-Cat-Sound-Apps.bat): one window where **all the Cats jam together** without Cubase. Press **Play**: the Hub walks through Chord-Cat's pads, Bass-Cat plays a bass line to the chords, Drum-Cat the drums and Life-Cat's creatures sing along, all at one tempo.
- **Rack**: one row per part (up to 8). Each part takes notes from the chords, the bass line, the drums, Life-Cat's creatures or a lead line (the chords' top note), plays them with a built-in sound (pads, e-piano, pluck, strings, 4 basses, 4 drum kits) or a **sound Cat** (Glide-, Sing-, Talk-Cat) with all of its presets, and can run them through a **MIDI-effect Cat** first (Patt-Cat patterns, Bend-Cat bends, Orbit-Cat arpeggios).
- **Advanced mode**: filter, drive, chorus, echo, crush, room and pan on every part, "+ Add a part". **Simple mode** shows just sound, preset, volume and what is playing.
- **Song bar**: 10 styles (Lo-fi Study, Trap Night, House Party, Neo Soul, Ambient Garden, Afro Sun, Rock Anthem, Jazz Cafe, Choir & Cats...), key, chord progression (every Chord-Cat preset), chord length.
- **Dice** and the **Compose** tab write whole songs that sound good (see *The composer* below): pick a genre (Pop, Lo-fi, Trap, House, Ambient, Jazz, Rock, Cinematic) and a mood (Happy, Sad, Dark, Dreamy, Epic, Chill), press **Write a song!** and every part in the rack plays it: chords, bass line, drums, the melody (Lead) and a counter-melody. **Song** or **Endless** (it keeps writing new parts while it plays). **New: Chords / Bass / Drums / Melody / Counter-melody / Lyrics / Song form** renews one thing and keeps the rest. A map shows every section, chord and note with the playhead; the words scroll along, and Sing-Cat or Talk-Cat on the Lead part sings them. **Export**: the song as a WAV, one WAV per part (stems) or a MIDI file (*Music\\Cat Hub*). Every song has a number: the same number, genre and mood always give the same song.
- Every Cat's own window is a tab (centred, scaled to fit). **Settings**: 6 themes (Midnight, Ocean, Sunset, Forest, Candy, Paper), sound card, which Cats were found, the tutorial again.
- A short **tutorial** on the first start, **Rec** to a WAV in *Music\\Cat Hub*, `Space` = play / stop, `1`-`8` = hold a pad. Everything (incl. each Cat's settings) is remembered.

Every change pushed to `main` is built automatically on GitHub (see the **Actions** tab); the newest build replaces the "latest" release.


Six VST3 MIDI plug-ins and four instruments (a choir, a gliding synth, a vocaloid-style singer and a living ecosystem; also as standalone apps) (JUCE 8, C++20, CMake). They work on their own or chained:

| Plug-in | What it does |
|---|---|
| **Chord-Cat** | One-finger chord progressions: 16 pads, 80 chord types, 48 scales, 9 voicing models with voice leading, strum / humanise / groove, Markov progression generator, substitutions, MIDI capture, drag-and-drop MIDI, 48 factory progressions. |
| **Patt-Cat** | 4-layer step sequencer for held chords: 32 steps per layer with velocity / gate / tie / rest / ratchet / probability / voice select, Euclidean generator, polyrhythms, 80 preset patterns. **Bounce** page: every note bounces like a ball (shown as cats jumping over a keyboard) and re-plays on each bounce. |
| **Bend-Cat** | Per-voice pitch-bend expression over MPE: glide, 27-shape envelope editor, LFO, 16-step bend pattern, human detune and drift, Scala microtuning, live visualiser, 25 expression presets. |
| **Drum-Cat** | Drum pattern generator: 13 style models (Boom Bap, Dilla, Neo Soul, Lo-Fi, Funky, Trap, Detroit, House, Drill, Dembow, Afrobeats, Jungle, Rock) with hand-played micro-timing and ghost notes; 9 lanes, 20 variations per seed, editable groove grid, swing / human / velocity / chance / open hats, GM or sequential drum maps, drag-and-drop MIDI, 40 groove presets. |
| **Orbit-Cat** | Orbital-physics arpeggiator: every held note is a planet (Kepler orbits, eccentricity, mutual pull, drag, turbulence) and plays when it crosses an alignment spoke; per-note mass / radius / eccentricity / speed / octave / damping / gate / boost, quantize, 32 presets, animated orbit display. |
| **Bass-Cat** | Bass lines that follow the chord you hold (or Chord-Cat's MIDI Out): finds the root (also of inversions), 18 styles (Pop Roots, Walking, Neo Soul, Funk, Boom Bap, Dilla, 808 with slides, Reggae, House, Disco Octaves, Latin, Drone, Rock, Dub, Country, Afrobeats, Drill, Synth Arp), written in chord degrees so they follow every chord change; ghost notes, fills, slides, 20 variations per seed, editable degree grid, Follow Drum, 24 presets, drag-and-drop MIDI. |
| **Sing-Cat** | *Instrument (makes sound).* A physically modelled choir of people and cats, up to 64 singers. Each singer has a glottis (tension, breath, jitter, rasp) feeding a vocal tract and nasal cavity. Includes 14 casts (men, women, children, elders, kittens, tomcats, opera stars, families, mixed); Neck / Jaw / Lips / Nose / Chest body controls; 13 styles (Classical, Gospel, Pop, Folk, Jazz, Breathy, Pansori, Opera, Bulgarian, Soul, Robot, Lullaby, Chant); and Solo (legato) / Choir / Tutti modes with MPE. The vowel chart supports Wave / Envelope / Steps / drawn Path / Random movement. Singers stand on a stage you arrange, in a modelled room, and their faces sing with their mouths. Also: meows, purring, breathing, 48 presets. |
| **Glide-Cat** | *Instrument (makes sound).* Polyphonic synth (up to 16 voices) whose voices glide from chord to chord instead of restarting: Smart (smallest total movement) or Classic voice leading, extra notes Branch off a voice or start Fresh, spare voices merge, lines remembered after release. Glide by Time or Rate, 5 curves (incl. Elastic and Semitone Stairs), 5 paths (Arc, Zigzag, Swirl), start directions, stagger orders, release bends. Two layers per voice (detuned saws, PWM pulse, sine, noise, HP + resonant LP with initial / attack-level filter envelope, amp envelope, ring mod), auto swell / brighten / drift / delayed vibrato, MPE. 10 Autopilot modes (Octave Leaps, Scale Walk, Glide Arp, Weave, Mirror, 8-step Sequence...), 4 modulators + 8-slot matrix, Drive, Ensemble, Prism (cascading pitch-shifted echoes), tempo-synced ping-pong Echo, Tape (wow, flutter, age) and Rooms (incl. Shimmer and Bloom). Live Glide View; 58 sound presets and 41 glide presets, chosen separately, each with a dice. |
| **Talk-Cat** | *Instrument (makes sound).* A vocaloid-style lead singer, human or cat, with its own **piano roll**. Draw notes, type a lyric on each (double-click; Tab jumps to the next note; several words fill the following notes), or paste a whole song text into LYRICS. Sings **English** (dictionary + spelling rules), **Slovak** (č š ž ť ď ň ľ, ia/ie/iu/ô, syllabic r/l), **Japanese** romaji (ka shi tsu, kya, n, small tsu) and **Cat** (meow, mrrp, purr, hiss, nya, yowl, chirp, trill). Exact pronunciation as phonemes in brackets, e.g. [l ah v]. Consonants start before the beat so the vowel lands on it. 10 drawable curves (pitch bend, dynamics, breath, brightness, gender, tension, growl, vibrato, cat, mouth); per-note velocity, portamento, vibrato and consonant length. Voice designer (size, gender, age, human-to-cat morph, breath, tension, rasp, nasal, ring, jaw, lips, tongue, throat) with a live vocal-tract view; 15 singing styles; double track + 3 diatonic harmonies; tone, compressor, drive, delay, reverb. Live mode: MIDI notes sing the next syllable of the live lyrics, and can be recorded into the roll. MIDI import/export with lyrics, 6 demo songs, 33 voice presets. |
| **Muse-Cat** | *Instrument (makes sound) + MIDI out, also a standalone app.* Writes whole songs from a genre, a mood and a song number, or endless music (see *The composer* below). Plays them with its own sounds (pick one per part) and sends every part as MIDI: chords ch 1, bass ch 2, melody ch 3, counter-melody ch 4, drums ch 10, so other Cats or synths can play them. Follows Cubase's transport (or its own Play button); New buttons per part, a song map with the playhead, the words, Drag MIDI (all parts, with the words as MIDI lyric events), export WAV / stems / MIDI, 13 song presets. |
| **Life-Cat** | *Instrument (makes sound) + optional MIDI out, also a standalone app.* A sandbox ecosystem that makes music. **Ten kinds of animal**, each with its own behaviour and sound: grazers, hunters, drifters, pollinators (spread plants, plucks), scavengers (eat what dies, percussion), giants (deep drones), swarms (flock, arpeggios), **singers** (birds / dolphins / sirens: whole melodic phrases), **drummers** (woodpeckers / shrimps / pulsars: euclidean rhythms) and **builders** (beavers / coral crabs / bots: plant seeds, lay meadow trails, build nests). Every creature sings with a voice grown from its **29 genes**; babies blend both parents' genes plus a mutation, so the music evolves. Four worlds that change look and sound: **Garden**, **Ocean**, **Space**, **Cells**. Sandbox: drop food, add / remove animals, walls, warm / cold zones, currents, paint terrain (water, rock, meadow, sunlight, sand, lava), gravity, wind, day / night, seasons, weather you can hear, a Gene Lab to design creatures, per-species volume / octave / sound / speed / max, up to 128 creatures. Zoom with the wheel, drag to look around, follow a favourite. Notes from position, species chord tones or the chord you hold (or Chord-Cat's, via Cat Link), always in key (11 scales); grid with swing or free floating. Reverb, ping-pong delay, MIDI out on one channel or one per kind, Drag MIDI, 36 world presets, worlds are saved with the project. |

**Patt-Cat Bounce** (Steps / Bounce switch above the grid)
- *Bounce* = Off, Pattern Notes (every note Patt-Cat plays bounces) or Keys (every key you press bounces on its own, alongside the pattern; mute layer A to hear only the bounces).
- The first gap is Rate (a note value at the song tempo, R.Mode Sync) or Time (ms, R.Mode Free). Curve + makes each gap shorter like a real ball (up to x0.55 per bounce), Curve - slows the bounces down. Notes = number of bounces, Fade Out = how much quieter the last bounce is, Octave / Oct Rnd move the bounces, Dur = note length, Human = random timing.
- Play: One Shot plays every bounce; While Held stops the remaining bounces when you let go of the key.

**New in every Cat**
- **Preset bar**: star a preset to make it a favourite, step through all presets or favourites only, save your own presets (the plug-in's complete state) and open them from the menu. They live in `%APPDATA%\CatSuite\<Plug-in>\Presets`.
- **Chains** (preset bar menu): one name loads a matching preset in every Cat that is open, e.g. *Trap Night* sets Chord-Cat, Patt-Cat, Bend-Cat, Drum-Cat and Bass-Cat together. Ten chains.
- **Follow Drum**: Drum-Cat shares its swing and timing feel; Chord-Cat, Patt-Cat, Orbit-Cat and Bass-Cat follow it when Follow Drum is on.
- **Chord-Cat STRUM tab**: 10 directions (incl. root first, low-then-high, outside-in, guitar down/up strokes), 16 timing shapes + curve, octave stack, tilt or target velocity, Classic sliding window, Round Trip repeats, synced bursts with muted chops, strum on release, live strum view, 24 strum presets and a dice.
- **Sing-Cat**: Stage / Lips / 3D face views, a microphone pair you drag on the stage (level, timing and tone follow each singer's distance and angle), and lyrics (each note sings the next syllable).

Every plug-in has its own **music-reactive cat**: Chord-Cat wears headphones, Patt-Cat is a tabby, Bend-Cat has a pitch wave and a curly tail, Drum-Cat plays a snare, Orbit-Cat wears a space helmet with a planet circling it, Sing-Cat sings into a microphone, Bass-Cat plays a bass guitar and Glide-Cat is wrapped in coloured glide lines Talk-Cat wears an idol headset with music notes floating up and Life-Cat has a sprout on its head with tiny creatures swimming around it. Each cat nods to the beat, twitches its ears and flicks its tail on notes, its eyes glow with how hard you play, and it appears big on a short boot screen when the window opens (click to skip). Every plug-in also has **Surprise** (randomise within musical ranges) with a one-step **undo** arrow; Bend-Cat's Surprise never touches Bend Range / MPE settings, which must match your instrument.

### Cat Ocean (an endless YouTube live stream)
**Cat-Ocean.exe** (comes with Get-Cat-Sound-Apps.bat) is a relaxing pixel-art ocean that streams itself to YouTube in 1080p, 24/7:
1. Paste your **stream key** (YouTube Studio > Go live > Stream) and your **channel** (`@name`).
2. Press **Get ffmpeg** once (the free tool that sends the video), then **Go live**. *Record a 30 s test video* checks everything without going live.
3. **Copy the YouTube description** puts a ready description with every command on the clipboard (the same text is in `packaging/Cat-Ocean-YouTube-Description.txt`).

Viewers type commands in the live chat:

| Command | What it does |
|---|---|
| `!fish Miro blue stripes` | get your own fish (name + any looks) |
| `!color red yellow white` | body, pattern and fin colours (red, orange, yellow, lime, green, teal, cyan, blue, navy, purple, violet, pink, magenta, coral, white, gray, black, brown, peach, mint, lavender) |
| `!body` slim / round / long / puffer / flat / cat, `!size` small / medium / big | the body |
| `!pattern` plain / stripes / spots / gradient / belly / checker | the pattern |
| `!tail` fan / fork / round / veil, `!fin` none / small / sail / spiky, `!eyes` dot / big / sleepy / star | tail, fin, eyes |
| `!hat` none / hat / glasses / bow / scarf / headphones / lamp / crown | something to wear |
| `!mood` calm / shy / social / hyper / lazy, `!speed` slow / normal / fast | how it swims |
| `!voice` bubble / pluck / bell / flute / chip / marimba / whistle / pad, `!pitch` low / mid / high | how it sings |
| `!melody 1353` (1 = key note .. 8 = octave), `!rhythm` sparse / normal / busy, `!notes` short / long | what it sings |
| `!dance`, `!spin`, `!jump` | moves |
| `!feed`, `!vote 1`, `!name Miro`, `!random`, `!info`, `!level`, `!home`, `!release`, `!help` | food, votes, ... |

**Moderators** (and the host typing in the app): `!rename @name New Name`, `!resetname @name`, `!kick @name`, `!ban @name`, `!unban @name`, `!event storm/calm/feast/babyboom/rarehunt`, `!place reef/kelp/crystal/ship`, `!music aqua/lounge/crystal/bubble`, `!startvote`, `!endvote`.

- **Levels**: a viewer's fish earns XP while it swims (1 a minute) and when they chat (2, at most every 30 s); the name shows the level (*MIRO LV7*). Some looks unlock with levels: LV2 puffer, gradient, sail fin, sleepy eyes, scarf, chip voice; LV3 flat, belly, spiky fin, star eyes, headphones, marimba; LV4 checker, veil tail, whistle; LV5 cat body, lamp; LV6 pad voice; LV8 crown.
- Your fish swims in when you write in the chat; after 20 quiet minutes it naps in the cave (it is kept forever). A cat in a boat waves hello.
- **Genetics**: babies mix their parents' genes (babies of your fish are called *Miro Jr.*), sometimes mutate, and a few are born **Golden, Rainbow, Ghost, Glowing or Crystal**. After many generations evolution discovers **new species** with something no fish had before (whiskers, a glowing antenna, a twin tail, starry spots, a frill, a horn).
- **Music** is always relaxing: Aqua dream, Sky lounge, Crystal deep or Bubble pop (Frutiger Aero: bright, airy, glassy). The composer never stops; the fishes' genes slowly pull its **tempo and key**, and every fish sings its own melody (with a little fin wave).
- **Places**: Aqua reef, Emerald kelp, Crystal deep (glowing plants, fish glow), Sunken ship (with a treasure chest). Chat votes where the ocean goes, which music plays, and events (storm, feast, baby boom, rare hunt).
- New **subscribers** (the channel's count going up) start a confetti party. Leaderboards: oldest fish, biggest families, rare fish, most babies, species, highest levels.
- One fish per viewer, a word filter for names (English and Slovak), a cooldown against spam, at most 40 viewers' fish on screen at once.

### The composer (Cat Hub's Compose tab, Dice and Muse-Cat)
Songs are written with music theory, so they sound good:
- **Harmony**: a scale from the mood (major, minor, Dorian, Mixolydian, Lydian, harmonic minor, Phrygian), proven progressions per genre plus Markov progressions with cadences, borrowed chords, a dominant before the chorus, 7ths / 9ths where the genre likes them, smooth voice leading. Verse, chorus, pre-chorus and bridge each get their own progression.
- **Melody**: a short motif repeated and varied in 4-bar phrases (statement, answer), chord tones on the strong beats, mostly steps, leaps answered by a step back, phrases ending on chord tones and the song on the home note. The chorus **hook** is one catchy bar idea, repeated and sequenced, and comes back the same in every chorus.
- **Bass, drums, counter-melody** per genre: walking bass in jazz, 808s that follow the kick in trap, off-beat bass in house; grooves with fills before a chorus, snare-roll builds and a breath before the drop, crashes on big moments, trap hi-hat rolls, swing; arpeggios, string ostinatos, power chords or trap bells.
- **Form**: intro, verse, pre-chorus, chorus, bridge, build, drop, breakdown, outro, with parts coming in and out and the energy rising. Endless mode keeps the key, the chords and the hook and writes about 32 bars ahead.
- **Words**: rhyming lines with exactly one syllable per melody note (hyphenated, e.g. *to-night*), so Sing-Cat and Talk-Cat sing one syllable per note. The title comes from the chorus.

### Cat Link

Every Cat has a **LINK** button in its top bar (the two rings; the number says how many other Cats are open). Click it for the Link panel:

- **Share key & scale**: change the key or scale in Chord-Cat, Talk-Cat or Life-Cat and the others follow.
- **Chords**: Chord-Cat sends the chord it plays; Bass-Cat, Patt-Cat, Orbit-Cat, Life-Cat, Glide-Cat, Sing-Cat and Talk-Cat can follow it **without any MIDI routing in Cubase**.
- **Life-Cat events**: births and deaths in Life-Cat make Drum-Cat play extra hits.
- **Follow Drum-Cat's groove**: swing and human timing from Drum-Cat.
- **Share tempo & play / stop** (standalone apps): the Cat apps play in sync; Play / Stop and BPM in the panel control all of them.
- **Link all Cats** turns everything on in every open Cat at once.

The interface is flat and minimal (dark, one soft blue accent) and **scalable**: drag the bottom-right corner to resize it from 50 % to 250 %. The whole UI scales as one piece with the aspect ratio kept, and since everything is drawn as vector graphics it stays sharp at any size. The size is remembered per plug-in.

All of them run with zero latency, place events sample-accurately and lock to Cubase's transport through `AudioPlayHead`. Every control is an automatable parameter and supports right-click **MIDI Learn**.

---

## Using it in Cubase 13 (important)

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

### Pure MIDI-effect build
For hosts that support VST3 MIDI effects natively (e.g. Bitwig, Reaper), build with `-DCAT_CUBASE_INSTRUMENT_MODE=OFF`. This sets `isMidiEffect() == true` and removes the audio bus.

---

## Building

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

## Known limitations
- **Humanise timing can only delay notes.** A zero-latency plug-in cannot play notes early, so ±15 ms becomes 0 to +15 ms.
- **Chord Track:** the Cubase Chord Track has no plug-in API, hence the MIDI-based sync described above.
- **Scala scales other than 12 notes:** each degree is mapped to one consecutive MIDI key from the reference note. Retuning is limited by the bend range, so set the instrument's bend range to cover the largest offset.
- **Orbit-Cat is generative:** the system evolves continuously, so playing the same passage twice will not repeat note for note (Drum-Cat does repeat).
- **Sing-Cat** is the choir (vowels, simple words); for clear lyrics use **Talk-Cat**. 64 singers cost roughly a quarter of one CPU core (Eco); High quality doubles that.
- **Late chords in Patt-Cat:** with zero latency, a hit that lands a few milliseconds before a chord change starts with the old chord and switches when the new chord arrives. Shared notes are not re-struck.
- **Testing so far:** the suite is built and tested headless on Linux (JUCE 8.0.15) by loading the real VST3 binaries. `CatSuiteHostTest` includes regression tests for the multi-key, late-chord, legato-merge, velocity and bend-range bugs found in Cubase.


**Renamed:** the choir that used to be called Talk-Cat is now **Sing-Cat** (same plug-in ID, so old projects still load it), and the new lyric singer is **Talk-Cat**.
