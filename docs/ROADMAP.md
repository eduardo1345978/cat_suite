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
