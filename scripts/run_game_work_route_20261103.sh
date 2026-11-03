#!/bin/bash
# Work-count diagnostic: moving, turning, reload/fire; not an FPS benchmark.
set -euo pipefail
cd "$(dirname "$0")/.."
name=original-game-work-route-20261103
[[ ! -e "docs/evidence/$name/exit.txt" ]] || { echo 'Evidence exists' >&2; exit 1; }
mkdir -p "build/$name/userdata/bin"
cp build/test-fixtures/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU -u THING_LOCAL_PRELOAD \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/game-work-counts-host-20261103/diagnostic.host" \
 THING_INPUT_EVENTS='{"35":"tap dpdown","38":"tap a","45":"tap a","55":"tap x","65":"axis righttrigger 32767","70":"axis righttrigger 0","72":"axis lefty 26000","80":"axis lefty 0","82":"axis rightx 18000","84":"axis rightx 0","90":"axis lefty 26000","98":"axis lefty 0","100":"axis righttrigger 32767","105":"axis righttrigger 0","110":"axis rightx 18000","112":"axis rightx 0","115":"axis lefty 26000","120":"axis lefty 0"}' \
 THING_CAPTURE_START=55 THING_CAPTURE_INTERVAL=15 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=160 \
 THING_NATIVE_STREAM_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" THING_NATIVE_VORBIS_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" \
 THING_EARLY_QUEUE_BOUNDS=1 THING_DRAW_BATCH=0 THING_DECAL_BATCH=0 THING_CPU_CLIP=0 THING_DRAW_ERROR_CHECK_EVERY=16 \
 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 THING_FPS_LIMIT=30 THING_FPS_OVERLAY=0 \
 THING_CPU_PROFILE=1 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
cp "build/$name/original-game-work.csv" "docs/evidence/$name/original-game-work.csv"
