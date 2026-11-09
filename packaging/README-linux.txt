Cat Suite for Linux (x86_64)
============================

What is in here
  VST3/        every Cat Suite plug-in (19), as VST3 for Linux
  Apps/        the apps: Sing-Cat, Talk-Cat, Glide-Cat, Life-Cat, Muse-Cat, Negative,
               the Cat Hub, Cat Ocean and Cat Box
  Hub Cats/    the Cats the Cat Hub plays (its own copies)
  install.sh   installs everything into your home folder (no root needed)

Install
  1. Unpack:     tar xzf Cat-Suite-Linux.tar.gz
  2. Install:    cd Cat-Suite && ./install.sh
  3. In your DAW, rescan the plug-ins (they are in ~/.vst3).
  To update: unpack the new download and run ./install.sh again.
  To remove everything: ./install.sh --remove

Which programs
  Cubase does not run on Linux. The plug-ins work in Linux DAWs that load VST3:
  Reaper, Bitwig Studio, Ardour, Renoise, Tracktion Waveform, Qtractor, Carla ...
  The apps run on their own (sound through ALSA or JACK: choose it in the app's Options / settings).

Needs
  A 64-bit Intel / AMD Linux from about 2022 on (glibc 2.35+), with ALSA, freetype and fontconfig
  (they are there on every normal desktop). Built and tested on Ubuntu 22.04.

    Plug-ins           ~/.vst3/*.vst3
    Apps               ~/.local/share/cat-suite/
    Menu entries       ~/.local/share/applications/cat-suite-*.desktop
    Terminal names     ~/.local/bin/<app>   (e.g. Negative, Cat-Hub)

The Cat Suite is free software (GPLv3): see LICENSE.txt and THIRD-PARTY-NOTICES.txt.
