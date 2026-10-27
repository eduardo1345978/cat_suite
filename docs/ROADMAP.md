# Cat Suite roadmap (questionnaire 3, 21 Oct 2026)

When the user says "continue": work down this list in order, tick items off here (`[x]`), commit and push,
then copy changed files to the PC (D:\Projekty\vst_plugin) and run build_and_install.bat.

## 0. In progress
- [x] Cat Ocean: black text in the key / channel / host / console boxes (were white on white)
- [x] Cat Ocean: underwater reverb over the whole mix, slider + `/reverb 0-100`
- [x] Reverb default changes with the place (cave / ice / trench = big, lagoon / sky = small); the slider is the amount on top

## 1. FIRST: Cat Ocean chat + going live (user: "chat never connected", stream didn't show in Studio)
- [ ] Check the chat reader end to end against a real live stream; clearer status lines (why it isn't connected)
- [x] (part) key check (spaces removed, URL refused) + after 2 min sending with no live video: tells what to change in Studio
- [ ] Going live: Cat Ocean said "streaming" but Studio showed nothing -> show ffmpeg's real output / RTMP errors,
      check the key (trim spaces, warn on wrong length), explain Dual stream must be Off/Auto (user keeps YouTube Auto for Shorts)
- [ ] Polish every chat command (!fish, emotes, !pet, !feedcat ...) - test each one through the real chat path
- [x] Fish emotes: !dance (notes circle it) !spin (really turns round) !jump (swims up, leaps out, splash)

## 2. Cat Ocean content
- [ ] Stream text and panels SMALLER, more ocean (English stays)
- [ ] Creatures: octopus & crabs, whales & dolphins visiting, mermaid cat (VERY rare, sings a solo, everyone gets XP)
- [ ] Places: underwater city, deep trench (dark, glowing), coral castle, sky ocean (floating islands)
- [ ] Halloween (automatic all October): pumpkins & candles on the sea floor
- [ ] Christmas (automatic all December): snow and a Christmas tree
- [ ] Music: styles Ambient piano, Synthwave night drive; song voting every song (3 choices on screen, chat types 1/2/3)
- [ ] Daily summary on screen
- [ ] Moderator panel inside the app (viewer list with buttons)
- [ ] Discord bot (webhook): when live, rare fish & new species, daily summary, Super Chats / members

## 3. Fixes
- [ ] Cat Hub: the Cats' windows (tabs) don't resize with the Hub window -> scale them to fit
- [ ] Crashes: hunt them (stress tests, Hub + Muse-Cat PresetBar menu under no display)

## 4. Echo-Cat
- [ ] More presets, reverse echo, pitch echo (each echo higher / lower), echo steps (draw a rhythm of echoes)

## 5. New plug-ins
- [ ] Tune-Cat: vocal tuner - natural correction, hard T-Pain effect, follows Chord-Cat's key (Cat Link), harmony voices
- [ ] Compressor / loudness Cat
- [ ] Filter / wobble Cat (synced sweeps)
- [ ] Lo-fi / tape Cat (vinyl, wobble, crunch)
- [ ] Piano-Cat (full piano), Guitar-Cat (strummed), 808 / drum machine (plays Drum-Cat's beats)
- [ ] Sampler-Cat: drag in a sound and play it, chop a loop into pads, record from the mic, cat meow samples included

## 6. Voices
- [ ] Talk-Cat / Sing-Cat: clearer words, more voices / languages, rap mode

## 7. Cat Hub
- [ ] Echo-Cat as an effect on parts
- [ ] Arrangement timeline (record the jam into a song view)
- [ ] Mixer page (all volumes + meters)
- [ ] Export to a Cubase project (MIDI + stems)
- [ ] Piano roll: velocity bars, copy / paste / select many, scale highlight, drum grid

## 8. Muse-Cat / composer
- [ ] Sing the lyrics (built-in voice), melody editor, more genres (K-pop, drum & bass, metal ...), rhyme help
- [ ] Songs longer, more surprising, catchier hooks

## 9. Look and sharing
- [ ] Theme switch in every Cat: Dark / Frutiger Aero / Light
- [ ] Animated cat mascot in every plug-in that reacts to the music
- [ ] One-click installer .exe where you tick what you want - MODERN and nice to use (user asked): a real window
      with the Cat look, cards with icons for each Cat / app, a progress bar, "Open Cubase" / "Start Cat Hub" at the end, also uninstall
- [ ] Auto update: the apps tell you a new version is out, you click Update
- [ ] Mac version: later
- [ ] Linux: publish Linux builds too (VST3 + apps) on GitHub (user asked 21 Oct; everything already builds and is tested on Linux)

## 10. Cat Box (hardware groovebox) - TOP PRIORITY (questionnaires 4 + 5, 22 Oct 2026)
User's words: FUN, a "toy-engineering" instrument, not a soulless music-spitting machine. No generators.
Loops are SECONDARY; the heart is SOUND DESIGN: turning knobs live, sampling with a mic, modifying, playing
with sound, making chord progressions like the Telepathic Instruments Orchid. Plays it outside / at a friend's.
Plan doc: Claude Doc "Cat Box - modularny syntetizator". Same program on the PC (CatBox/, build_catbox.bat).
- [x] Step 1: engine on PC + CI for Raspberry Pi (ARM64) and Windows, presets, memory, --kiosk, --selftest
- [x] Cat Box 2: step sequencers (drums 16 x 64, 303 bass), live Rec, Acid synth, English screen, light/dark
- [x] Cat Box 3: FX page (reverb sends, room, echo, pump + target, DJ filter, tape, vinyl, crush, glue, width),
      16 performance pads, Scene button (16 scenes, at the next bar), Arp Supersaw + Acid, Arp on/off (plain synth),
      Bass follows Arp, Steps per page (fixed "arp doesn't work"), short arp taps sound at once, simpler screen
      with spectrum, PC pads 8 x 2, buttons: Play(Shift=Tap) Steps(Shift=Clear) Shift Scene Instr Oct- Oct+ Rec
- [x] Chaos button (user asked 22 Oct): random sound settings for the page, musical ranges, patterns stay; Shift + Chaos = undo
- [x] Chaos only when held 0.6 s, Chaos alone in the corner of the case layout (user asked 22 Oct)
- [x] Simple 5" box screen (user asked 23 Oct): page + preset name + tempo, big oscilloscope, knob card with name /
      big number / bar, preset & scene grid with names (Shift / Scene), step strip
- [x] PC app that connects to the box (user asked 23 Oct): network link (TCP 50650, discovery UDP 50651), PC plays the
      box and mirrors its screen, preset library with names: load, rename, copy PC <-> box
- [x] Piano pads on Arp and Bass (user asked 23 Oct): bottom row white keys, top row black keys; drums unchanged; Shift + knob 8 = Piano / Scale
- [x] Polish (23 Oct): knob rings + value on hover, double-click = default, Chaos hold bar, F1 help, pad velocity
      by click height, window remembers its place, plain pad colours on Arp/Bass; hold Instr + turn a knob = flip pages
- Cat Suite in the box (questionnaire 6, 23 Oct): Teenage Engineering style, NOT cat-branded, professional (no meows):
      pages pick ENGINES, engines have factory presets that can be edited or started from Init; lots of features but
      easy to find with buttons; big Cats = pages, small = sounds / effects; as many pages as needed, switched with
      Instr hold + knob; each Cat can be switched off in settings; Pi gets fewer voices, PC full. Order: Cats now.
      - [x] Engine system + Sound button (V) + GLIDE engine (all Glide-Cat sounds, 12 knob pages, Init) on Arp and Bass
      - [x] Chords page: 16 pads = 16 chords (presets AND own: pad + knob chord type), voice-leading knob, strum
            (direction + speed), chords feed Arp / Choir / Glide / own chord sound; play it like ORCHID
            (one hand picks the chord type, pads / keys the root); NO Markov generator
      - [x] Orbit page with animation (his favourite) - planets play the notes you hold (40 systems, planet edit, Hold, Arp or Bass sound)
      - [ ] Choir engine (Sing-Cat) as an engine with presets; no meows, no Talk-Cat
      - [ ] Effects TE style, as many as possible: echo with freeze / duck / ping-pong / tape wobble / shimmer,
            phaser, tremolo, tone, warmth, prism, shimmer/bloom rooms, ensemble
      - [ ] Arp: Bounce as an arp mode, Euclidean rhythms, pattern length as you set it (no Patt-Cat page)
      - [ ] Drums: Drum-Cat groove only (swing, human timing); Bass-Cat: Follow arp is enough
      - [ ] Joystick: bend (curves, LFO) + more targets; microtuning if it fits
      - [ ] Engines also: piano, nylon guitar, strings, e-piano, chip / glass bells (Cat Hub instruments)
      - Life-Cat: no. Talk-Cat: no. Patt-Cat page: no.
- [x] 26 Oct: user decided the box is just a box (Pi + screen + sound) played with his MIDI keyboard
      (M-Audio Oxygen Pro Mini: 32 keys, 8 pads x 2 banks, 4 knobs, 4 faders, transport). Done: MIDI learn wizard
      (knobs 1-12, buttons, 16 pads, knob bank), absolute knobs move by delta (no jumps), touch buttons on the 5" screen.
      No Pico controller, no encoders / FSR pads to buy now.
- CAT BOX 2 (questionnaire 7, 26 Oct): user unhappy: hard to use, too few sounds, cheap / boring sounds, Shift confusing,
      unclear screen, drums "totally don't work", arp broken and not adjustable, acid doesn't sound like acid, slow,
      not professional. Wants: big rebuild of controls + sounds (keep the rest), SOUND DESIGN first (like OP-1),
      knobs = sound by default, Sound button = everything about sound, Shift = 2nd page of sound, tempo / key / master
      on a PROJECT page, 2 knob pages max, all 12 knob labels always on screen, page switching with own ◀ ▶ buttons,
      HUNDREDS of presets in categories, heard while browsing, ALL engines (analog, FM, wavetable, sampler for melody +
      drums, physical piano/guitar, drum synth 808/909, sample kits "few but good", granular, Glide, Choir, proper Acid
      with MEGA customization), 8 equal tracks each picking an engine, Arp / Chords / Orbit as NOTE FX on any track,
      steps + live rec, one loop + scenes, separate MIXER page, send FX (reverb, delay) + master, clean minimal TE look,
      big readable font, subtle animation; likes OP-1, OP-XY, SP-404, MPC, Volca. Wants a MOCKUP first (done:
      Claude Design canvas "Cat Box 2 nákres": SOUND, SOUND+Shift, sound browser, SEQ, NOTE FX, MIX, PROJECT, panel with
      8 track buttons + SOUND/SEQ/NOTE FX/MIX/PROJECT + ◀ ▶ + SHIFT/SCENE/REC/PLAY/CHAOS, joystick, slider).
- [x] 26 Oct: CAT BOX 2 built (user: "less colourful, clearer, go"): new program from scratch (old Drums/Arp/Bass
      pages gone). 8 equal tracks, engines Analog (64 params, 39 sounds), Acid (303 model, 24 params incl. accent "wow",
      soft attack, Tube/Fuzz/Rat, 17 sounds), Drums (16 synthesized drums, 808/909/Lo-fi/Acoustic, tune/decay/tone/level
      per drum, 15 kits), Keys (piano, e-piano, nylon, pads, strings, bell, chip, pluck + filter/drive/tremolo/chorus,
      25 sounds), Glide (all 164 Glide-Cat params, 67 sounds) = 163 sounds; browser (engine / category / sound, heard at
      once, User sounds); note FX Arp (euclid, ratchet, chance, bounce ...), Chords (in key, voicing, strum), Orbit; SEQ
      (16-step bars up to 64, hold step + key/knobs, chords per step, slide/accent/chance/nudge, all 16 drum lanes on
      screen); Rec; MIX (vol/pan/2 sends/mute/solo, reverb + tempo delay, glue/tape/width, DJ filter on the slider);
      PROJECT page; scenes at the bar; Chaos + undo; MIDI learn; 12 knobs always on screen; grey look, one accent.
      Hardware map: 12 encoders CC 20-31, 20 buttons CC 102-121, slider CC 8, joystick CC 16/17 (Pico firmware TODO).
- [x] 26 Oct (user: "work until the credit runs out"): clicks fixed (drums fade on retrigger / choke, smooth
      transients, Analog keeps phase on retrigger + per-sample filter sweep, Acid no gap); new engines FM (29 sounds),
      Wavetable (22), Choir = Sing-Cat (67), Sampler (mic recording, Keys / Chop / One shot, 8 starter samples rendered
      by the box, 16 sounds), Granular (12) = 311 sounds; Glide Octave knob; Pico firmware + wiring (CatBox/pico)
- [x] Undo with 40 steps (Shift + Chaos), copy / paste a bar (Shift + Prev / Next in SEQ), knob moves recorded into
      steps (Rec + knob), ratchet per step + drum rolls, 28 drum kits (324 sounds)
- [x] 27 Oct: factory reset (PROJECT knob 12 / right-click, undoable); SIMPLER (user: "complicated, simpler + more options"):
      SEQ tap = on + pick, knobs edit the picked step (no holding), keys fill steps when stopped, hit a drum to pick it;
      MENU (Project twice): save / load 8 projects, new, clear, copy track, 15 drum patterns, 12 bass lines, help pages,
      help line always on screen
- [x] 27 Oct ("super ultra mega customization", "nice delay and send effects", "lots of effects and presets, polish"):
      4 insert FX per track (22 effects, presets, 25 whole chains), 8 macros, 2 LFOs + envelope + random, 16-slot mod
      matrix (any sound / FX / mix knob), MY knob page, sends: reverb types (hall, room, plate, shimmer, cloud, spring, dark)
      + pre-delay + ducking, delay types (clean, tape, dub, ping-pong, multi-tap, diffused, reverse) + wow + duck + into
      reverb, FX send (any effect); LOOK page: 8 themes, 9 colours, big text, ring knobs, track colours; ~80 FX Lab sounds
- [x] 27 Oct ("free preset libraries? put them all in, all samples"): VSCO 2 CE (CC0 orchestra) + Dirt-Samples
      (TidalCycles) via get_libraries.bat / pi/get_libraries.sh into CatBox/library (~6 GB, not in git), indexed at start
      and read on demand (5231 samples, 313 folders), every folder in the browser (LIB ...), new KIT engine (16 samples on
      pads), 15 library kits, 69 sampler + 12 granular library sounds (499 sounds); shimmer reverb runaway fixed
- [ ] NEXT (user): a better Sampler ("add sampler if credit is left") - multi-sample zones, then MIDI .mid import, DX7 .syx import
- [ ] Ask the user how the new drums / arp / acid / FM / sampler feel; physical-model guitar? more kits
- [ ] NEXT hardware: get it running on the Pi 5 (user HAS: Pi 5 8 GB, 27 W supply, micro SD, HDMI monitor,
      keyboard + mouse, a USB sound card; wants HiFiBerry DAC later) - walk him through setup_pi.sh
- [ ] SOUND DESIGN (most important next software):
      - Lead synth (first new instrument), then Sampler (record with a mic in the box; play it in tones AND chop to pads)
      - Cat Box synth engine "everything possible": oscillators, filters, envelopes, LFOs, mod matrix; designable Acid
      - Chord pads like Orchid: one finger = a chord, all 80 Chord-Cat chord types, progressions
      - Arp: melodic step sequencer, 32 steps, glide, Rec, cutoff + resonance; sounds: vocal/choir, kalimba, flute, guitar, pad
      - Bass: Sub 808, Reese, Slap, Double bass, FM, Wobble + tempo-synced wobble LFO; live transpose with a pad; bass as a lead
      - Up to 8 instruments (Glide-Cat pad, Sing-Cat choir, piano, sampler, lead synth, 2nd drums, percussion; NO Talk-Cat);
        own button per instrument (and an encoder to switch)
- [ ] Drums: some sounds are bad (ask which), 16+ kits, 808 with slide, snap, conga/bongo, tambourine, cat sounds,
      own samples; per-drum pitch + decay; choke groups; roll/ratchet per step; per-drum loop length (polyrhythm);
      step velocity by knob; Fill button (recorded fill); 16 drum patterns switched with pads at the end of the bar;
      microtiming later
- [ ] Playing: quantize knob (only when switched on), metronome click + 1-bar count-in when recording,
      multi-step Undo, copy / paste pattern or bar, "mutate" button (small changes to MY pattern), sleep after 30 min
- [ ] FX: knob automation recorded into the pattern; mixer page (volume, pan, mute)
- [ ] Presets: banks A, B, C...; names typed on the PC, sent to the box (box <-> PC link); backups by hand
- [ ] Screen: still simpler; big preset name, spectrum, mixer, gentle animations, modern clean font, no touch
- [ ] PC: tempo follows Cubase; Cat Box as a VST3 used as a link between box and Cubase; preset library window
- [ ] Hardware: pads 8 x 2 (big rubber, need advice which), 12 encoders with push (push action: decide later),
      joystick (pitch / mod), slider (crossfader / DJ filter), NO knob LEDs, Mute button, fewer buttons (more via Shift),
      flat 25 cm tilted case, 3D printed by the user (he has a printer, designs it in Tinkercad / Fusion), black;
      needs advice where to buy parts; first playable version by Christmas; budget 200 EUR first wave
- Name: user wants name ideas, does not have to be about cats
- Not wanted now: WAV recording (records through the jack), MIDI export, phone control, Talk-Cat in the box

## 11. Other answers from questionnaire 4
- Default theme in all Cats: Dark. No animated cat mascot in the plug-ins. Installer .exe with checkboxes: yes, now.
  Auto update: yes. Linux version: yes. Website, YouTube tutorials: later.
- Working style: always test everything before saying done; medium-length reports; ask at big decisions.
- Cat Hub must not become a soulless quick-music generator either.
- Cat Ocean, Cat Hub, new Cats sections were left unanswered (no preference now).

## Answers that mean "no"
- Vertical Shorts picture: no, YouTube Auto is fine
- Chat games, fish pets/houses/friends, seasons other than Halloween/Christmas: no preference / not now
