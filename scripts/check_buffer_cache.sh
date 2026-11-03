#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/buffer-cache-check
cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -DRECOMP_FP_STACK_EXACT=1 \
 -Iruntime -Iprobe -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
 $(pkg-config --cflags sdl2 glesv2) runtime/d3d8_buffer_cache_selftest.c runtime/d3d8_renderer.c runtime/adjacent_batch.c \
 -Wl,--gc-sections -o build/buffer-cache-check/check $(pkg-config --libs sdl2 glesv2) -lm
build/buffer-cache-check/check
