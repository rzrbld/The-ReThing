#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p docs/evidence/batch-range-20261102
cc -DTHING_BATCH_RANGE_TEST -std=gnu11 -O2 -g -Wall -Wextra -Wno-misleading-indentation -Ibuild/batch-range-20261102/runtime -Iprobe/batch -Iruntime $(pkg-config --cflags sdl2 glesv2) \
 build/batch-range-20261102/selftest.c build/batch-range-20261102/runtime/adjacent_batch.c build/batch-range-20261102/runtime/d3d8_renderer.c \
 $(pkg-config --libs sdl2 glesv2) -lm -lz -o build/batch-range-20261102/check
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a build/batch-range-20261102/check | tee docs/evidence/batch-range-20261102/results.txt
