#!/bin/sh
# Cat Suite for Linux: installs the plug-ins (VST3) for you and the apps with menu entries.
#   ./install.sh            install (or update)
#   ./install.sh --remove   take everything out again
# Nothing needs root: everything goes into your home folder.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
vst="$HOME/.vst3"
apps="$HOME/.local/share/cat-suite"
menu="$HOME/.local/share/applications"
bin="$HOME/.local/bin"
names="Sing-Cat Talk-Cat Glide-Cat Life-Cat Muse-Cat Negative Cat-Hub Cat-Ocean Cat-Box"

if [ "$1" = "--remove" ]; then
    for b in "$here"/VST3/*.vst3; do rm -rf "$vst/$(basename "$b")"; done
    for n in $names; do rm -f "$menu/cat-suite-$n.desktop" "$bin/$n"; done
    rm -rf "$apps"
    echo "Cat Suite is removed."
    exit 0
fi

for n in $names; do
    if pgrep -x "$n" > /dev/null 2>&1; then echo "$n is still open - close it, then run this again."; exit 1; fi
done

mkdir -p "$vst" "$apps" "$menu" "$bin"
echo "Installing the plug-ins into $vst ..."
for b in "$here"/VST3/*.vst3; do
    rm -rf "$vst/$(basename "$b")"
    cp -r "$b" "$vst/"
    echo "  $(basename "$b" .vst3)"
done

echo "Installing the apps into $apps ..."
rm -rf "$apps/Hub Cats"
cp -r "$here/Apps/." "$apps/"
cp -r "$here/Hub Cats" "$apps/Hub Cats"
cp -r "$here/licenses" "$here/LICENSE.txt" "$here/THIRD-PARTY-NOTICES.txt" "$apps/" 2>/dev/null || true
for n in $names; do
    [ -f "$apps/$n" ] || continue
    chmod +x "$apps/$n"
    ln -sf "$apps/$n" "$bin/$n"
    cat > "$menu/cat-suite-$n.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=$n
Comment=Cat Suite
Exec="$apps/$n"
Terminal=false
Categories=AudioVideo;Audio;Music;
DESK
done
command -v update-desktop-database > /dev/null 2>&1 && update-desktop-database "$menu" > /dev/null 2>&1 || true

echo
echo "Done! The plug-ins are in $vst (Reaper, Bitwig, Ardour, Renoise, Waveform ... find them there - rescan in your DAW)."
echo "The apps are in your applications menu, and in a terminal: $(echo $names)"
case ":$PATH:" in *":$bin:"*) ;; *) echo "(For the terminal names, add $bin to your PATH.)";; esac
