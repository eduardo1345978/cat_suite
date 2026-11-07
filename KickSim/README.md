# KICK Simulator

The Behringer KICK is open source (AGPL-3.0, https://github.com/MusicTribeOpenSource/0700-AAM).
`.github/workflows/build-kick.yml` builds its desktop simulator (KT_KickEM) for Windows from that
repository. This folder only holds what the build needs on top of it:

- `juce_serialport/`: a stand-in for a module the release left out (the simulator has no serial port)
- `patches/`: small fixes applied to the KICK sources before building
