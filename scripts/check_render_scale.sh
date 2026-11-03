#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/scale-check
cc -std=gnu11 -O1 -ffunction-sections -fdata-sections -DRECOMP_FP_STACK_EXACT=1 \
 -Iruntime -Iprobe -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
 $(pkg-config --cflags sdl2 glesv2) runtime/d3d8_scale_selftest.c runtime/d3d8_renderer.c runtime/adjacent_batch.c \
 -Wl,--gc-sections -o build/scale-check/check $(pkg-config --libs sdl2 glesv2) -lm -lz
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a build/scale-check/check
