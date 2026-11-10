# Cat Link

The Cats talk to each other without any MIDI routing.

Every Cat has a **LINK** button in its top bar (the two rings; the number says how many other Cats are open). Click it for the Link panel:

- **Share key & scale**: change the key or scale in Chord-Cat, Talk-Cat or Life-Cat and the others follow.
- **Chords**: Chord-Cat sends the chord it plays; Bass-Cat, Patt-Cat, Orbit-Cat, Life-Cat, Glide-Cat, Sing-Cat and Talk-Cat can follow it **without any MIDI routing in the DAW**.
- **Life-Cat events**: births and deaths in Life-Cat make Drum-Cat play extra hits.
- **Follow Drum-Cat's groove**: swing and human timing from Drum-Cat.
- **Share tempo & play / stop** (standalone apps): the Cat apps play in sync; Play / Stop and BPM in the panel control all of them.
- **Link all Cats** turns everything on in every open Cat at once.

The interface is flat and minimal (dark, one soft blue accent) and **scalable**: drag the bottom-right corner to resize it from 50 % to 250 %. The whole UI scales as one piece with the aspect ratio kept, and since everything is drawn as vector graphics it stays sharp at any size. The size is remembered per plug-in.

All of them run with zero latency, place events sample-accurately and lock to the DAW's transport through `AudioPlayHead`. Every control is an automatable parameter and supports right-click **MIDI Learn**.
