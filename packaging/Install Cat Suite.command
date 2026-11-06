#!/bin/bash
# Cat Suite installer for macOS: copies every .vst3 next to this file into your own VST3 folder
# (~/Library/Audio/Plug-Ins/VST3, no password needed) and lets macOS open them (removes the
# "downloaded from the internet" flag, because the Cats are not signed by Apple).
cd "$(dirname "$0")" || exit 1
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
echo
echo "  =====  Cat Suite installer  ====="
echo
while pgrep -xq "Cubase.*|Nuendo.*" 2>/dev/null || pgrep -fq "/Cubase [0-9]*.app/" 2>/dev/null; do
    echo "  Cubase is still open. Please quit Cubase, then press Return."
    read -r _
done
mkdir -p "$DEST"
failed=0
for p in VST3/*.vst3; do
    [ -d "$p" ] || continue
    name="$(basename "$p")"
    echo "  Installing ${name%.vst3} ..."
    rm -rf "$DEST/$name"
    if cp -R "$p" "$DEST/"; then
        xattr -dr com.apple.quarantine "$DEST/$name" 2>/dev/null
    else
        echo "    COULD NOT COPY ${name%.vst3}"
        failed=1
    fi
done
echo
if [ "$failed" = 1 ]; then
    echo "  Some plug-ins could not be installed. Quit Cubase and run this again."
else
    echo "  Done! All Cats are installed in:"
    echo "  $DEST"
    echo
    echo "  In Cubase: Studio > VST Plug-in Manager > Rescan."
fi

# Micro-Cat's MTS-ESP mode needs ODDSound's free MTS-ESP library (one tuning for every MTS-ESP synth)
if [ ! -f "/Library/Application Support/MTS-ESP/libMTS.dylib" ] && [ -f "MTS-ESP/libMTSMac_v1.03.pkg" ]; then
    echo
    echo "  Micro-Cat can tune every MTS-ESP synth at once (Surge XT, Vital, Pigments, Serum 2 ...)."
    echo "  For that it needs the free MTS-ESP library by ODDSound."
    printf "  Install the MTS-ESP library now? [y/N] "
    read -r answer
    case "$answer" in
        [yY]*) open -W "MTS-ESP/libMTSMac_v1.03.pkg" ;;
    esac
fi
echo
echo "  You can close this window."
