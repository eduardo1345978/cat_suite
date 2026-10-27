#!/bin/bash
# Cat Box on the Pi: the free sample libraries into CatBox/library (about 6 GB, once).
#   VSCO 2 Community Edition (CC0) and Dirt-Samples (TidalCycles)
set -e
cd "$(dirname "$0")/.."
mkdir -p library
cd library
[ -d Dirt-Samples ] || git clone --depth 1 https://github.com/tidalcycles/Dirt-Samples Dirt-Samples
[ -d VSCO-2-CE ] || git clone --depth 1 https://github.com/sgossner/VSCO-2-CE VSCO-2-CE
rm -rf Dirt-Samples/.git VSCO-2-CE/.git
echo "Done: $(find . -iname '*.wav' | wc -l) samples in $(pwd)"
