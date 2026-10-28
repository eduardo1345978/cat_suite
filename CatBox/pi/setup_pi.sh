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
# a small launcher: waits for the screen, starts Cat Box full screen, starts it again if it ever closes or crashes
cat > "$HOME/.local/bin/cat-box-run" <<EOF
#!/usr/bin/env bash
# Cat Box on the box: one copy only, started again if it stops (to stop it: pkill -f cat-box-run; pkill -if "cat-box --kiosk")
pgrep -if "cat-box --kiosk" >/dev/null && exit 0
sleep 3
command -v xset >/dev/null && xset s off -dpms 2>/dev/null || true   # never blank the screen (X11)
while true; do
    "$HOME/.local/bin/cat-box" --kiosk
    sleep 2
done
EOF
chmod +x "$HOME/.local/bin/cat-box-run"
# the desktop starts everything in ~/.config/autostart after logging in
mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/cat-box.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Cat Box
Exec=$HOME/.local/bin/cat-box-run
X-GNOME-Autostart-enabled=true
EOF
# log in by itself to the desktop and never blank the screen (Raspberry Pi OS)
if command -v raspi-config >/dev/null; then
    sudo raspi-config nonint do_boot_behaviour B4 || true   # desktop, auto-login
    sudo raspi-config nonint do_blanking 1 || true          # screen blanking off
fi

cat <<'EOF'

Done. Cat Box starts full screen by itself after the next reboot (sudo reboot), and again if it ever closes.
Try it now from the desktop:   ~/.local/bin/cat-box --kiosk     (right-click = sound card / MIDI)
Stop it (Ctrl + Alt + T for a terminal, or over ssh):   pkill -f cat-box-run; pkill -if "cat-box --kiosk"
Free sample libraries (optional, a few GB):   bash CatBox/pi/get_libraries.sh

HiFiBerry: add the line for your board to /boot/firmware/config.txt (see HiFiBerry's guide for
the exact overlay name, e.g. dtoverlay=hifiberry-dacplus-std), reboot, and Cat Box picks the
HiFiBerry by itself on its first start. The Pico controller is switched on as soon as it is plugged in.
EOF
