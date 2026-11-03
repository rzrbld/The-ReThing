#!/bin/bash
# Bounded host API attribution, NOT an H700 performance benchmark.
set -euo pipefail
cd "$(dirname "$0")/.."
name=pvs-reasons-gameplay-20261103
mkdir -p "build/$name/userdata/bin" "docs/evidence/$name"
cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
rm -f "build/$name/gl-cost.csv" "build/$name/render.control"
env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU -u LD_PRELOAD \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/pvs-reasons-20261103/diagnostic.host" \
 THING_INPUT_EVENTS='{"60":"tap dpdown","63":"tap a","70":"tap a","80":"axis lefty 26000","83":"axis lefty -26000","86":"axis lefty 26000","89":"axis lefty -26000","92":"axis lefty 26000","95":"axis lefty -26000","98":"axis lefty 26000","101":"axis lefty -26000","104":"axis lefty 26000","107":"axis lefty -26000","110":"axis lefty 26000","113":"axis lefty -26000","116":"axis lefty 26000","119":"axis lefty -26000","122":"axis lefty 26000","125":"axis lefty 0"}' \
 THING_CAPTURE_START=125 THING_CAPTURE_INTERVAL=24 THING_AUTO_TUTORIAL=0 \
 THING_CPU_PROFILE=1 THING_PROFILE_SECONDS=152 \
 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=30 THING_FPS_OVERLAY=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
for file in pvs-cells.csv pvs-counts.csv profile.csv host-cpu-profile.csv host-profile-maps.txt; do
 cp "build/$name/$file" "docs/evidence/$name/$file"
done
