#!/bin/bash
# Bounded host API attribution, NOT an H700 performance benchmark.
set -euo pipefail
cd "$(dirname "$0")/.."
name=queue-phase-views-20261103
test ! -e "docs/evidence/$name/exit.txt" || { echo "Evidence exists" >&2; exit 1; }
mkdir -p "build/$name/userdata/bin" "docs/evidence/$name"
cp build/test-fixtures/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
rm -f "build/$name/gl-cost.csv" "build/$name/render.control"
env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU -u LD_PRELOAD \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/queue-phase-capture-20261103/diagnostic.host" \
 THING_INPUT_EVENTS='{"60":"tap dpdown","63":"tap a","70":"tap a","110":"axis rightx 18000","112":"axis rightx 0"}'  \
 THING_RENDER_EVENTS='{"100":"geometry","135":"geometry"}' \
 THING_CAPTURE_START=98 THING_CAPTURE_INTERVAL=12 THING_AUTO_TUTORIAL=0 \
 THING_CPU_PROFILE=1 THING_PROFILE_SECONDS=152 \
 THING_NATIVE_STREAM_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" \
 THING_NATIVE_VORBIS_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" \
 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=30 THING_FPS_OVERLAY=0 \
 THING_LOCAL_PRELOAD="$PWD/build/depth-probe-20261103/depth-probe.so" \
 THING_DEPTH_FILE="$PWD/build/$name/depth.csv" \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
for file in depth.csv profile.csv host-cpu-profile.csv host-profile-maps.txt; do
 cp "build/$name/$file" "docs/evidence/$name/$file"
done

cp "build/$name"/geometry-*.bin "docs/evidence/$name/"

cp "build/$name"/queue-bounds-*.bin "docs/evidence/$name/"

cp "build/$name"/material-*.bin "docs/evidence/$name/"

cp "build/$name"/queue-phase-*.bin "docs/evidence/$name/"
