#!/bin/bash
# Validate combined CPU/audio and batching candidate.
# Software GLES/QEMU only; NOT a hardware performance measurement.
set -euo pipefail
cd "$(dirname "$0")/.."
name=${THING_COMBINED_ARM_RUN:-facade-probe-arm-final-20261103}
mkdir -p "build/$name/userdata/bin"
cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
env -u THING_LOCAL_GDB -u THING_LOCAL_PRELOAD -u THING_LOCAL_CPU_OBSERVE \
 THING_LOCAL_RUN="$name" THING_LOCAL_QEMU=1 THING_LOCAL_BINARY="$PWD/build/facade-probe-aarch64-20261103/diagnostic.aarch64" \
 THING_INPUT_EVENTS='{"80":"tap dpdown","83":"tap a","90":"tap a","115":"axis lefty 26000","118":"axis lefty -26000","121":"axis lefty 26000","124":"axis lefty -26000","127":"axis lefty 26000","130":"axis lefty -26000","133":"axis lefty 26000","136":"axis lefty -26000","139":"axis lefty 26000","142":"axis lefty -26000","145":"axis lefty 0"}' \
 THING_CAPTURE_START=${THING_FACADE_CAPTURE_START:-150} THING_CAPTURE_INTERVAL=10 THING_LOCAL_WALL_TIMEOUT=360 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=${THING_COMBINED_ARM_SECONDS:-210} \
 THING_NATIVE_STREAM_PROBE="$PWD/build/native-vorbis-arm/probe.aarch64.so" THING_NATIVE_VORBIS_PROBE="$PWD/build/native-vorbis-arm/probe.aarch64.so" \
 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DECAL_BATCH=0 DS_DECAL_BATCH=0 THING_CPU_CLIP=0 THING_FACADE_PROBE=1 THING_CPU_PROFILE=1 THING_TRACE_RENDER=1 THING_DRAW_ERROR_CHECK_EVERY=16 \
 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_FPS_LIMIT=0 THING_FPS_OVERLAY=0 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py

for f in facade-calls.csv facade-frame.csv profile.csv render.csv; do cp "build/$name/$f" "docs/evidence/$name/$f"; done
