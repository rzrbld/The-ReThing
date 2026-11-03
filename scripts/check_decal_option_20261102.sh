#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/prepare_decal_option_20261102.py
python3 scripts/prepare_small_decal_batch_20261102.py
mkdir -p docs/evidence/decal-option-20261102
for mode in opaque decals; do
 source=probe/batch/adjacent_batch_selftest.c
 [[ $mode != decals ]] || source=build/small-decal-batch-20261102/selftest.c
 cc -std=gnu11 -O2 -g -Iruntime $(pkg-config --cflags sdl2 glesv2) "$source" build/decal-option-20261102/adjacent_batch.c runtime/d3d8_renderer.c $(pkg-config --libs sdl2 glesv2) -lm -lz -o "build/decal-option-20261102/check-$mode"
done
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a sh -c 'THING_DECAL_BATCH=0 build/decal-option-20261102/check-opaque && THING_DECAL_BATCH=1 build/decal-option-20261102/check-decals; rc=$?; printf "executable_exit=%s\n" "$rc"; exit "$rc"' \
 > docs/evidence/decal-option-20261102/results.txt 2>&1
