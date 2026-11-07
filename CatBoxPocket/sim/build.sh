#!/usr/bin/env bash
# Build the simulator page: the engine as WebAssembly inside one HTML file.
#   needs Zig (pip install ziglang) or any clang + wasi-libc;  sim/build.sh [out.html]
set -e
cd "$(dirname "$0")/.."
out="${1:-sim/out/CatBoxPocketSim.html}"; mkdir -p "$(dirname "$out")"
python3 -m ziglang c++ -target wasm32-wasi -O2 -std=c++17 -fno-exceptions -mexec-model=reactor \
  -Wl,--export-dynamic -Wl,--strip-all -Wl,--initial-memory=67108864 \
  -o sim/out/pocket.wasm sim/api.cpp engine/engine.cpp engine/ui.cpp engine/store.cpp engine/factory.cpp 2> sim/out/build.log || { cat sim/out/build.log; exit 1; }
python3 - "$out" <<'PY'
import base64, sys
page = open('sim/page.html').read()
wasm = base64.b64encode(open('sim/out/pocket.wasm', 'rb').read()).decode()
open(sys.argv[1], 'w').write(page.replace('__WASM__', wasm))
PY
echo "$out"
