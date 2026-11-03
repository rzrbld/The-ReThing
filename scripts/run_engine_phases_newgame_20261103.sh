#!/bin/bash
# Validate combined CPU/audio and batching candidate.
# Software GLES/QEMU only; NOT a hardware performance measurement.
set -euo pipefail
cd "$(dirname "$0")/.."
name=${THING_COMBINED_ARM_RUN:-simulation-phases-newgame-host-20261103}
mkdir -p "build/$name/userdata/bin"
test ! -e "docs/evidence/$name/exit.txt" || { echo "Choose a fresh THING_COMBINED_ARM_RUN for this transition test" >&2; exit 1; }
THING_COMBINED_TRANSITION_RUN="$name" python3 scripts/drive_combined_arm_transitions_20261102.py &
driver_pid=$!
trap 'kill "$driver_pid" 2>/dev/null || true' EXIT
env -u THING_LOCAL_GDB -u THING_LOCAL_PRELOAD -u THING_LOCAL_CPU_OBSERVE \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/simulation-phases-host-20261103/diagnostic.host" \
 THING_INPUT_EVENTS='{"35":"tap a","42":"tap a","50":"tap a","57":"tap a","155":"axis rightx 18000","157":"axis rightx 0"}' \
 THING_CAPTURE_START=45 THING_CAPTURE_INTERVAL=3 THING_LOCAL_WALL_TIMEOUT=360 THING_AUTO_TUTORIAL=1 THING_AUTO_MOVE_SECONDS=2 THING_FACADE_PROBE=0 THING_ENGINE_PHASES=1 THING_PERF_PROBE=1 THING_CPU_PROFILE=1 THING_TRACE_RENDER=1 THING_GL_STATS=1 THING_PROFILE_SECONDS=${THING_COMBINED_ARM_SECONDS:-200} \
 THING_NATIVE_STREAM_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" THING_NATIVE_VORBIS_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" \
 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DECAL_BATCH=0 THING_CPU_CLIP=0 THING_DRAW_ERROR_CHECK_EVERY=16 \
 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_FPS_LIMIT=${THING_ENGINE_TEST_FPS:-0} THING_FPS_OVERLAY=0 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py

for f in engine-phases.csv profile.csv render.csv host-cpu-timeline.csv host-profile-maps.txt; do cp "build/$name/$f" "docs/evidence/$name/$f"; done
