#!/usr/bin/env bash
# Cat Box on a Raspberry Pi 5 (Raspberry Pi OS Bookworm, 64-bit, with desktop).
#
# Run once on the Pi, inside the cat_suite folder:
#     bash CatBox/pi/setup_pi.sh
#
# It installs the build tools, builds Cat Box (about 15-30 minutes on a Pi 5) and makes it
# start full screen by itself every time the Pi boots. Run it again after an update
# (git pull) to rebuild.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "== 1/3 Libraries"
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git \
    libasound2-dev libfreetype-dev libfontconfig1-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libxext-dev

echo "== 2/3 Building Cat Box"
cmake -B build-pi -G Ninja -DCAT_BOX_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-pi --target CatBox
EXE="$ROOT/build-pi/CatBox/CatBox_artefacts/Release/Cat-Box"
test -x "$EXE"
mkdir -p "$HOME/.local/bin"
ln -sf "$EXE" "$HOME/.local/bin/cat-box"

echo "== Quick check (plays everything offline, no sound card needed)"
if [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    "$EXE" --selftest /tmp/catbox-test.wav && echo "Self test OK"
else
    echo "(skipped: run this script from the Pi's desktop to also run the self test)"
fi

echo "== 3/3 Start with the Pi"
# the desktop starts everything in ~/.config/autostart after logging in (enable auto-login in raspi-config)
mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/cat-box.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Cat Box
Exec=$HOME/.local/bin/cat-box --kiosk
X-GNOME-Autostart-enabled=true
EOF

cat <<'EOF'

Done. Cat Box starts full screen after the next reboot (sudo reboot).
Try it now from the desktop:   ~/.local/bin/cat-box --kiosk     (right-click = sound card / MIDI)

HiFiBerry: add the line for your board to /boot/firmware/config.txt (see HiFiBerry's guide for
the exact overlay name, e.g. dtoverlay=hifiberry-dacplus-std), reboot, and Cat Box picks the
HiFiBerry by itself on its first start. The Pico controller is switched on as soon as it is plugged in.
EOF
