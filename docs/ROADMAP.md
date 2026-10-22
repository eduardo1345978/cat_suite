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

## 10. Cat Box (hardware groovebox) - TOP PRIORITY this month (questionnaire 4, 22 Oct 2026)
User's words: Cat Box must be FUN to play, not "click and it generates music for me". No generators in the box.
Plan doc: Claude Doc "Cat Box - modularny syntetizator". Same program on the PC (CatBox/, build_catbox.bat).
- [x] Step 1: engine on the PC + CI for Raspberry Pi (ARM64) and Windows, presets, memory, --kiosk, --selftest
- [x] Cat Box 2: generators out; drum step sequencer (16 drums x 16/32/64 steps) + live Rec; 303-style bass
      step sequencer (note / slide / accent) + live Rec; own Acid synth; English screen; light / dark screen; tempo 40-240
- [ ] NEXT: get it running on the Pi 5 with sound (user has the Pi 5 8 GB; ask what else is at home)
- [ ] Pico firmware: encoders WITH push, RGB pads, buttons -> USB MIDI ch 16 (map in CatBox/README.md)
- [ ] Sounds: more instruments, up to 8 at once (Glide-Cat pad, Sing-Cat choir, Talk-Cat voice, piano, sampler, lead synth)
      and Cat Box's own synth engine; Arp sounds: supersaw, acid 303, strings (pluck, chip, glass bell, e-piano exist)
- [ ] Master FX: reverb, delay, sidechain pump, vinyl, tape, filter sweep, bitcrush
- [ ] Performance FX on the pads (stutter, roll, sweep while held)
- [ ] Scenes: one pad saves / recalls all instruments at once (song mode: NO, a loop is enough)
- [ ] Bass follows the Arp's chord (with a switch); bass chord pads with all 80 Chord-Cat chord types
- [ ] Key from Chord-Cat over Cat Link when at the PC; tempo sync with Cubase; presets PC <-> box; record the jam to WAV on a USB stick
- [ ] Updates over WiFi
- [ ] Cat Box as a VST3 in Cubase; bigger PC window with more on screen
- [ ] Velocity pads (FSR) right away in the hardware
- Hardware answers: flat groovebox (not a cube), 3D printed, black, no cat ears, 5" display, encoders with push,
  RGB pads, mains power (battery: maybe later), small built-in speaker, 2x jack L/R + headphones + mic/line in + MIDI DIN in/out,
  soldering together with someone, budget 200 EUR -> cheap first version (sound, display, knobs, pads), mic / MIDI DIN /
  speaker in a second wave; cheap parts rather than ready modules; first playable version by Christmas
- Screen: knob values, sound picture, pattern steps; English; dark + light; no dancing cat; no phone control; not a Cat Hub controller

## 11. Other answers from questionnaire 4
- Default theme in all Cats: Dark. No animated cat mascot in the plug-ins. Installer .exe with checkboxes: yes, now.
  Auto update: yes. Linux version: yes. Website, YouTube tutorials: later.
- Working style: always test everything before saying done; medium-length reports; ask at big decisions.
- Cat Hub must not become a soulless quick-music generator either.
- Cat Ocean, Cat Hub, new Cats sections were left unanswered (no preference now).

## Answers that mean "no"
- Vertical Shorts picture: no, YouTube Auto is fine
- Chat games, fish pets/houses/friends, seasons other than Halloween/Christmas: no preference / not now
