#!/bin/bash
# Diagnostic backtracking route; verify visible footprints in captures.
# Inclusive timers overlap. Host software GLES is not H700 performance.
set -euo pipefail
cd "$(dirname "$0")/.."
name=${THING_SECTIONS_RUN:-effect-footprints-replay-capture-20261102}
probe=${THING_SECTIONS_PROBE:-effects-decal-capture-probe-20261102}
mkdir -p "build/$name/userdata/bin"
cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$PWD/build/$probe/diagnostic.host" \
 THING_INPUT_EVENTS='{"60":"tap dpdown","63":"tap a","70":"tap a","80":"axis lefty 26000","83":"axis lefty -26000","86":"axis lefty 26000","89":"axis lefty -26000","92":"axis lefty 26000","95":"axis lefty -26000","98":"axis lefty 26000","101":"axis lefty -26000","104":"axis lefty 26000","107":"axis lefty -26000","110":"axis lefty 26000","113":"axis lefty -26000","116":"axis lefty 26000","119":"axis lefty -26000","122":"axis lefty 26000","125":"axis lefty 0"}' \
 THING_CAPTURE_START=75 THING_CAPTURE_INTERVAL=5 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=145 \
 DS_TRACE_DRAW_LIMIT=4096 THING_RENDER_EVENTS='{"135":"trace"}' DS_TRACE_RENDER=1 THING_LOCAL_PRELOAD="$PWD/build/frame-cpu-lines-20261102/frame-cpu.host.so" \
 THING_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=30 THING_FPS_OVERLAY=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
for file in profile.csv render.csv frame-cpu-exact.csv render-frame.txt; do
 if [ -f "build/$name/$file" ]; then cp "build/$name/$file" "docs/evidence/$name/$file"; fi
done

cp "build/$name"/decal-*.bin "docs/evidence/$name/"
