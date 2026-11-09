# FAQ and known limits

## Something doesn't work

- **A Cat doesn't show up in Cubase:** close Cubase, install again, then *Studio > VST Plug-in Manager > Rescan*. The VST3 files are in `C:\Program Files\Common Files\VST3`.
- **Windows warns about Cat-Suite-Setup.exe:** click *More info > Run anyway*. The files are built in the open on GitHub (Actions tab).
- **A MIDI Cat makes no sound:** it sends MIDI. Route its *MIDI Out* to a synth track ([Using it in Cubase](Cubase.md)), or use its own sound where it has one (Micro-Cat, Hum-Cat).
- **Micro-Cat plays normal notes:** pick a tuning other than *12-EDO (normal)*. Another synth needs MPE (bend range = Micro-Cat's) or MTS-ESP.
- **Cubase crashed with a Cat:** send the newest file from `Documents\Steinberg\CrashDumps` with a short description (an Issue on GitHub).

## Known limits

- **Humanise timing can only delay notes.** A zero-latency plug-in cannot play notes early, so ±15 ms becomes 0 to +15 ms.
- **Chord Track:** the Cubase Chord Track has no plug-in API, hence the MIDI-based sync described above.
- **Scala scales other than 12 notes:** each degree is mapped to one consecutive MIDI key from the reference note. Retuning is limited by the bend range, so set the instrument's bend range to cover the largest offset.
- **Orbit-Cat is generative:** the system evolves continuously, so playing the same passage twice will not repeat note for note (Drum-Cat does repeat).
- **Sing-Cat** is the choir (vowels, simple words); for clear lyrics use **Talk-Cat**. 64 singers cost roughly a quarter of one CPU core (Eco); High quality doubles that.
- **Late chords in Patt-Cat:** with zero latency, a hit that lands a few milliseconds before a chord change starts with the old chord and switches when the new chord arrives. Shared notes are not re-struck.
- **Testing so far:** the suite is built and tested headless on Linux (JUCE 8.0.15) by loading the real VST3 binaries. `CatSuiteHostTest` includes regression tests for the multi-key, late-chord, legato-merge, velocity and bend-range bugs found in Cubase.


**Renamed:** the choir that used to be called Talk-Cat is now **Sing-Cat** (same plug-in ID, so old projects still load it), and the new lyric singer is **Talk-Cat**.
