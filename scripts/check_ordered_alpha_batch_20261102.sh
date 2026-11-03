#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/prepare_ordered_alpha_batch_20261102.py
mkdir -p build/ordered-alpha-batch-20261102 docs/evidence/ordered-alpha-batch-20261102
cc -std=gnu11 -O2 -g -Wall -Wextra -Wno-misleading-indentation -Iprobe/batch -Iruntime $(pkg-config --cflags sdl2 glesv2) \
 build/ordered-alpha-batch-20261102/selftest.c build/ordered-alpha-batch-20261102/adjacent_batch.c runtime/d3d8_renderer.c \
 $(pkg-config --libs sdl2 glesv2) -lm -lz -o build/ordered-alpha-batch-20261102/check
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a build/ordered-alpha-batch-20261102/check "$@" | tee "docs/evidence/ordered-alpha-batch-20261102/${THING_BATCH_RESULT_NAME:-results}.txt"
