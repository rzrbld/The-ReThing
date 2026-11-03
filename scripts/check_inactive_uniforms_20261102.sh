#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Validate the promoted source directly; historical candidate artifacts stay intact.
out=build/inactive-uniforms-source-check-20261102
mkdir -p "$out"
cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer \
 -DTHING_TEST_RENDERER_SOURCE="\"$PWD/runtime/d3d8_renderer.c\"" \
 -Iruntime -Iprobe $(pkg-config --cflags sdl2 glesv2) probe/inactive_uniforms_selftest.c \
 -Wl,--gc-sections -o "$out/check" $(pkg-config --libs sdl2 glesv2) -lm
ASAN_OPTIONS=detect_leaks=0 "$out/check"
