#!/bin/bash
# Inclusive main-thread CPU diagnostics; overlapping roots must not be summed.
set -euo pipefail
cd "$(dirname "$0")/.."
name=${THING_SECTIONS_RUN:-cpu-sections-route-periodic-20261102}
probe=${THING_SECTIONS_PROBE:-cpu-sections-probe}
mkdir -p "build/$name/userdata/bin"
cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/$probe/diagnostic.host" \
 THING_INPUT_EVENTS='{"35":"tap dpdown","38":"tap a","45":"tap a","65":"axis lefty -26000","80":"axis lefty 0","90":"axis rightx 18000","92":"axis rightx 0"}' \
 THING_CAPTURE_START=55 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=110 \
 THING_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=0 THING_FPS_OVERLAY=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
cp "build/$name/profile.csv" "docs/evidence/$name/profile.csv"
