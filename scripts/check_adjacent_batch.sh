#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/adjacent-batch-check docs/evidence/adjacent-batch
cc -std=gnu11 -O2 -g -Wall -Wextra -Wno-misleading-indentation -Iprobe/batch -Iruntime $(pkg-config --cflags sdl2 glesv2) \
 probe/batch/adjacent_batch_selftest.c runtime/adjacent_batch.c runtime/d3d8_renderer.c \
 $(pkg-config --libs sdl2 glesv2) -lm -lz -o build/adjacent-batch-check/check
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a build/adjacent-batch-check/check "$@" | tee "docs/evidence/adjacent-batch/${THING_BATCH_RESULT_NAME:-results}.txt"
