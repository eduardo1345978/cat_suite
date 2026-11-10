# Install

## Windows: Cat Suite Setup (easiest)

1. Open **[Releases > latest](https://github.com/eduardo1345978/cat_suite/releases/tag/latest)** and download **Cat-Suite-Setup.exe**.
2. Run it. If Windows warns, click **More info > Run anyway** (the Cats are not signed by a big company).
3. Tick the Cats and apps you want and press **Install**. Close your DAW first.
4. Later, open **Cat Suite Setup** from the Start menu: it shows what has an update and updates it with one click.


## Windows: by hand

1. Open **[Releases > latest](https://github.com/eduardo1345978/cat_suite/releases/tag/latest)** and download **Cat-Suite-Windows.zip**.
2. Unzip it, close your DAW and double-click **Install-Cat-Suite.bat**.
3. Rescan the plug-ins in your DAW (Cubase: **Studio > VST Plug-in Manager > Rescan**).

**Only the instruments that make sound, as apps** (Sing-Cat, Talk-Cat, Glide-Cat, Life-Cat): download **[Get-Cat-Sound-Apps.bat](https://github.com/eduardo1345978/cat_suite/releases/download/latest/Get-Cat-Sound-Apps.bat)** and double-click it (if Windows warns, click *More info > Run anyway*). It downloads just those four, installs them as standalone apps with Start menu and Desktop shortcuts, and also as VST3 plug-ins. In the apps you can play with a MIDI keyboard or your **computer keyboard**: `A S D F G H J K L` = white keys, `W E T Y U O P` = black keys (Z works as Y on Slovak / German keyboards), Left / Right arrow = octave, Down / Up arrow = velocity.

## Mac

Every plug-in also comes for **macOS 11 or newer**, on **Apple Silicon (M1-M4) and Intel** Macs.

1. Open **[Releases > latest](https://github.com/eduardo1345978/cat_suite/releases/tag/latest)** and download **Cat-Suite-Mac.zip**, then unzip it.
2. Quit your DAW. **Right-click** *Install Cat Suite.command* > **Open** > **Open**. macOS asks the first time because the Cats aren't from the App Store. It copies the Cats into `~/Library/Audio/Plug-Ins/VST3`.
3. Rescan the plug-ins in your DAW (Cubase: **Studio > VST Plug-in Manager > Rescan**).

The Mac download has all 20 VST3 plug-ins. The apps (Cat Hub, Cat Ocean ...) are on Windows and Linux.

## Linux

Every plug-in and every app also comes for **64-bit Linux** (Ubuntu 22.04 or newer, Fedora, Arch, Mint ... from about 2022 on). Cubase doesn't run on Linux, so use a Linux DAW that loads VST3: **Reaper, Bitwig Studio, Ardour, Renoise, Tracktion Waveform, Carla**.

1. Open **[Releases > latest](https://github.com/eduardo1345978/cat_suite/releases/tag/latest)** and download **Cat-Suite-Linux.tar.gz**.
2. In a terminal: `tar xzf Cat-Suite-Linux.tar.gz && cd Cat-Suite && ./install.sh` (no root needed). The plug-ins go to `~/.vst3`, the apps (Sing-Cat, Talk-Cat, Glide-Cat, Life-Cat, Muse-Cat, Negative, Cat Hub, Cat Ocean, Cat Box) into your applications menu.
3. Rescan the plug-ins in your DAW. To update, run `./install.sh` from the new download; `./install.sh --remove` takes everything out.

## After installing

- Rescan the plug-ins in your DAW (Cubase: **Studio > VST Plug-in Manager > Rescan**).
- MIDI Cats (Chord, Patt, Bend, Drum, Orbit, Bass, Micro, Hum) load as **instruments with a MIDI output**: see [Using it in your DAW](DAW.md).
- Something wrong? See the [FAQ](FAQ.md).
