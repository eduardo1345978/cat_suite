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
         GlideCat/Source/GlideSynth.cpp GlideCat/Source/GlideSynth.h GlideCat/Source/GlideFX.cpp GlideCat/Source/GlideFX.h \
         GlideCat/Source/GlideData.cpp GlideCat/Source/GlideData.h \
         OrbitCat/Source/OrbitEngine.cpp OrbitCat/Source/OrbitEngine.h \
         SingCat/Source/ChoirEngine.cpp SingCat/Source/ChoirEngine.h SingCat/Source/VoiceModel.cpp SingCat/Source/VoiceModel.h \
         SingCat/Source/Lyrics.cpp SingCat/Source/Lyrics.h \
         CatHub/Resources/icon.png CatHub/Resources/icon_small.png \
         cmake/JuceSetup.cmake cmake/CatPlugin.cmake get_libraries.bat build_catbox.bat; do
    copy "$f"
done
cp -R "$SRC/tools/catbox-repo/." "$DST/"
echo "Cat Box exported to $DST"
