#!/usr/bin/env bash
# Build and run the Cat Box Pocket tests on the PC:  host/build.sh [out folder]
set -e
cd "$(dirname "$0")/.."
out="${1:-host/out}"; mkdir -p "$out"
g++ -std=c++17 -O2 -Wall -Wextra -Wno-misleading-indentation -o "$out/pocket_test" engine/engine.cpp engine/ui.cpp engine/store.cpp host/test.cpp
"$out/pocket_test" "$out"
