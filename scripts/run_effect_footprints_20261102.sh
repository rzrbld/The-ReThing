#!/bin/bash
# Diagnostic backtracking route; verify visible footprints in captures.
# Inclusive timers overlap. Host software GLES is not H700 performance.
set -euo pipefail
cd "$(dirname "$0")/.."
name=${THING_SECTIONS_RUN:-effect-footprints-backtrack-20261102}
probe=${THING_SECTIONS_PROBE:-effect-sections-probe-20261102}
mkdir -p "build/$name/userdata/bin"
cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/$probe/diagnostic.host" \
 THING_INPUT_EVENTS='{"35": "tap dpdown", "38": "tap a", "45": "tap a", "55": "axis lefty 26000", "58": "axis lefty -26000", "61": "axis lefty 26000", "64": "axis lefty -26000", "67": "axis lefty 26000", "70": "axis lefty -26000", "73": "axis lefty 26000", "76": "axis lefty -26000", "79": "axis lefty 26000", "82": "axis lefty -26000", "85": "axis lefty 26000", "88": "axis lefty -26000", "91": "axis lefty 26000", "94": "axis lefty -26000", "97": "axis lefty 26000", "100": "axis lefty -26000", "103": "axis lefty 0"}' \
 THING_CAPTURE_START=50 THING_CAPTURE_INTERVAL=5 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=165 \
 THING_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=30 THING_FPS_OVERLAY=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
for file in profile.csv render.csv frame-cpu-exact.csv; do
 if [ -f "build/$name/$file" ]; then cp "build/$name/$file" "docs/evidence/$name/$file"; fi
done
