#!/usr/bin/env bash
# Copies Cat Box with everything it needs (and nothing else from the suite) into a folder: the standalone
# repository github.com/eduardo1345978/cat-box. The workflow sync-catbox.yml runs it after every push to main.
#     bash tools/export_catbox.sh <folder>
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)"
DST="${1:?usage: export_catbox.sh <folder>}"
mkdir -p "$DST"
# everything that came from here is replaced (the folder's .git stays)
find "$DST" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +

copy() { mkdir -p "$DST/$(dirname "$1")"; cp -R "$SRC/$1" "$DST/$1"; }
copy CatBox
rm -rf "$DST/CatBox/library"
for f in Shared/Source/CatSounds.cpp Shared/Source/CatSounds.h Shared/Source/SafetyLimiter.h \
         instruments/GlideCat/Source/GlideSynth.cpp instruments/GlideCat/Source/GlideSynth.h instruments/GlideCat/Source/GlideFX.cpp instruments/GlideCat/Source/GlideFX.h \
         instruments/GlideCat/Source/GlideData.cpp instruments/GlideCat/Source/GlideData.h \
         midi/OrbitCat/Source/OrbitEngine.cpp midi/OrbitCat/Source/OrbitEngine.h \
         instruments/SingCat/Source/ChoirEngine.cpp instruments/SingCat/Source/ChoirEngine.h instruments/SingCat/Source/VoiceModel.cpp instruments/SingCat/Source/VoiceModel.h \
         instruments/SingCat/Source/Lyrics.cpp instruments/SingCat/Source/Lyrics.h \
         apps/CatHub/Resources/icon.png apps/CatHub/Resources/icon_small.png \
         cmake/JuceSetup.cmake cmake/CatPlugin.cmake get_libraries.bat build_catbox.bat; do
    copy "$f"
done
cp -R "$SRC/tools/catbox-repo/." "$DST/"
echo "Cat Box exported to $DST"
