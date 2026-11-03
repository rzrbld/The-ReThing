#!/bin/bash
# Validate the isolated skin helper inlining ARM candidate.
# Software GLES/QEMU only; NOT a hardware performance measurement.
set -euo pipefail
cd "$(dirname "$0")/.."
name=${THING_SKIN_FUSED_ARM_RUN:-skin-fused-arm-play-20261102}
mkdir -p "build/$name/userdata/bin"
cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
env -u THING_LOCAL_GDB \
 THING_LOCAL_RUN="$name" THING_LOCAL_QEMU=1 THING_LOCAL_BINARY="$PWD/build/skin-fused-arm-game-20261102/diagnostic.aarch64" \
 THING_INPUT_EVENTS='{"50":"tap dpdown","53":"tap a","60":"tap a","112":"axis rightx 18000","114":"axis rightx 0","120":"axis lefty -26000","126":"axis lefty 0"}' \
 THING_CAPTURE_START=100 THING_LOCAL_WALL_TIMEOUT=360 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=${THING_SKIN_FUSED_ARM_SECONDS:-140} \
 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 \
 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_FPS_LIMIT=0 THING_FPS_OVERLAY=0 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
