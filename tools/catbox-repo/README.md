# Cat Box 3

A groovebox you play yourself, for a Raspberry Pi 5 in a box (and for the PC): a "digital analog" synthesizer with
10 models, a drum machine, 8 tracks with steps, note FX and a mixer, a sampler with free libraries, and
**SCOPE** - pictures on an XY oscilloscope that you can hear. Nothing is generated: you play it.

![HOME](CatBox/docs/home.png)

| SYNTH | SCOPE |
|---|---|
| ![SYNTH](CatBox/docs/synth.png) | ![SCOPE](CatBox/docs/scope.png) |

- **The full guide** (apps, engines, keys, the box's hardware): [CatBox/README.md](CatBox/README.md)
- **New in Cat Box 3:** song mode (scenes in a row), WAV export and USB backup, a keyboard on the screen, patterns up to
  64 steps, Shift layer on the pots, encoder click = back to default, slider (DJ filter) and joystick (bend / mod) on the
  panel, touch for the FX chain and the mixer, Braindance sounds
- **Older versions:** every change since the first Cat Box is in the history (`git log`); the branches `cat-box-1` and `cat-box-2` are the last Cat Box 1 and Cat Box 2
- **The box's panel** (28 pots, 12 encoders, 16 steps, HOME, slider, joystick) - firmware for a Raspberry Pi Pico, wiring and parts:
  [CatBox/pico/catbox_panel_v2](CatBox/pico/catbox_panel_v2), the drawing [CatBox/pico/panel_v2.svg](CatBox/pico/panel_v2.svg)

## Build

- **Windows:** Visual Studio 2022 or newer with C++, then `build_catbox.bat` (it builds, runs the self test and starts Cat Box).
- **Raspberry Pi 5** (Raspberry Pi OS 64-bit with desktop):
  ```
  git clone https://github.com/eduardo1345978/cat-box.git
  cd cat-box
  bash CatBox/pi/setup_pi.sh
  ```
  It builds Cat Box (15-30 minutes), sets the Pi up for quick sound and starts Cat Box full screen at every boot.
- Anywhere with CMake 3.22+: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target CatBox`
  (JUCE 8 is downloaded by itself).

Free sample libraries (optional, about 6 GB): `get_libraries.bat` on Windows, `bash CatBox/pi/get_libraries.sh` on the Pi.

Made with [JUCE](https://juce.com). This repository is a copy of the Cat Box part of
[cat_suite](https://github.com/eduardo1345978/cat_suite), updated by itself after every change there.
