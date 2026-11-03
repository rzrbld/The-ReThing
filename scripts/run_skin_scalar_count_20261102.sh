#!/bin/bash
# Count fast/fallback use in saved gameplay. Diagnostic only, not a benchmark.
set -euo pipefail
cd "$(dirname "$0")/.."
for pair in on-1; do
 name="skin-scalar-count-live-20261102-$pair"
 binary="$PWD/build/game/thething.host"
 [[ $pair != on* ]] || binary="$PWD/build/skin-scalar-count-game-20261102/diagnostic.host"
 mkdir -p "build/$name/userdata/bin"
 cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
 env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$binary" \
 THING_INPUT_EVENTS='{"35":"tap dpdown","38":"tap a","45":"tap a"}' \
 THING_CAPTURE_START=55 THING_AUTO_TUTORIAL=0 THING_CPU_PROFILE=1 THING_PROFILE_SECONDS=95 \
 THING_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=30 THING_LOCAL_PRELOAD="$PWD/build/frame-cpu-exact-20261102/frame-cpu.host.so" THING_FPS_OVERLAY=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
 for file in frame-cpu-exact.csv profile.csv host-cpu-timeline.csv host-cpu-profile.csv host-profile-maps.txt cpu-profile-info.txt cpu-timeline-info.txt; do
  cp "build/$name/$file" "docs/evidence/$name/$file"
 done
 echo "Completed $pair"
done
