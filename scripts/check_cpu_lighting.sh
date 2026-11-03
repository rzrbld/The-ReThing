#!/usr/bin/env bash
# Isolated host CPU arithmetic/conformance and microbenchmark; no game build.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/cpu-lighting-check docs/evidence/cpu-lighting-check
cc -std=gnu11 -O2 -ffunction-sections -fdata-sections -Iprobe -Iruntime \
 -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
 $(pkg-config --cflags sdl2 glesv2) probe/d3d8_light_selftest.c \
 -Wl,--gc-sections -o build/cpu-lighting-check/check $(pkg-config --libs sdl2 glesv2) -lm
build/cpu-lighting-check/check | tee docs/evidence/cpu-lighting-check/results.txt
