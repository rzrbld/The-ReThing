#!/bin/bash
# Observe an unskipped intro with existing coarse original-engine timers.
set -euo pipefail
cd "$(dirname "$0")/.."
name=cutscene-phase-lead-20261103
[[ ! -e docs/evidence/$name/exit.txt ]] || exit 1
env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/simulation-phases-host-20261103/diagnostic.host" \
 THING_INPUT_EVENTS='{"35":"tap a","42":"tap a"}' \
 THING_CAPTURE_START=45 THING_CAPTURE_INTERVAL=15 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=150 \
 THING_ENGINE_PHASES=1 THING_CPU_PROFILE=1 DS_TRACE_RENDER=1 \
 THING_NATIVE_STREAM_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" THING_NATIVE_VORBIS_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" \
 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DECAL_BATCH=0 THING_CPU_CLIP=0 THING_DRAW_ERROR_CHECK_EVERY=16 \
 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 THING_FPS_LIMIT=0 THING_FPS_OVERLAY=0 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
for f in engine-phases.csv profile.csv render.csv host-cpu-timeline.csv host-profile-maps.txt; do cp "build/$name/$f" "docs/evidence/$name/$f"; done
