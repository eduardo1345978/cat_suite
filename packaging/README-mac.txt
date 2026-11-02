CAT SUITE - Mac plug-ins (VST3)
===============================

Works on every Mac with macOS 11 (Big Sur) or newer: Apple Silicon (M1, M2, M3, M4) and Intel.

How to install
1. Quit Cubase.
2. Right-click (or Control-click) "Install Cat Suite.command" and choose Open, then Open again.
   (The first time macOS asks because the Cats are not from the App Store. Double-clicking
   only works after this.)
3. Open Cubase and rescan: Studio > VST Plug-in Manager > Rescan.

The plug-ins are copied to your own VST3 folder (no password needed):
    ~/Library/Audio/Plug-Ins/VST3

If macOS still says a Cat "is damaged" or "can't be opened", open Terminal and run:
    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/*-Cat.vst3

Install by hand instead: copy the .vst3 folders from VST3 into ~/Library/Audio/Plug-Ins/VST3
(in Finder: Go > Go to Folder..., paste that path).

What is inside (folder VST3):
    Chord-Cat, Patt-Cat, Bend-Cat, Drum-Cat, Orbit-Cat, Bass-Cat   - MIDI generators
    Sing-Cat   - choir of cats and people
    Glide-Cat  - gliding poly synth
    Talk-Cat   - singer with lyrics (English, Slovak, Japanese, Cat)
    Muse-Cat   - writes whole songs (your own words and chords too)
    Life-Cat   - a living ecosystem that makes music
    Echo-Cat   - echo and shimmer space for vocals and instruments (an audio effect)
    Micro-Cat  - microtonal tunings (37 + your own) for any MPE synth
