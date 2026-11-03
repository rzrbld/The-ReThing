#!/bin/bash
# 30 FPS ABBA: compare exact CPU/frame, not capped FPS improvement.
set -euo pipefail
cd "$(dirname "$0")/.."
for pair in simple-1; do
 name="config-shadow-quality-20261103-$pair"
 binary="$PWD/build/occlusion-benefit-20261103/diagnostic.host"
 mode=0; [[ $pair != on* ]] || mode=1
 test ! -e "docs/evidence/$name/exit.txt" || { echo "Refusing to overwrite existing evidence" >&2; exit 1; }
 mkdir -p "build/$name/userdata/bin" "docs/evidence/$name"
 printf "%s\n" "$mode" > "docs/evidence/$name/culling-mode.txt"
 cp build/test-fixtures/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
 env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU \
 THING_OCCLUSION_BENEFIT=0 THING_LOCAL_REGISTRY="$PWD/docs/evidence/config-quality-20261103/registry-simple-shadows.txt" THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$binary" \
 THING_INPUT_EVENTS='{"35":"tap dpdown","38":"tap a","45":"tap a","80":"axis rightx 18000","82":"axis rightx 0"}' \
 THING_CAPTURE_START=55 THING_CAPTURE_INTERVAL=15 THING_AUTO_TUTORIAL=0 THING_CPU_PROFILE=0 DS_TRACE_RENDER=1 THING_PROFILE_SECONDS=115 \
 THING_NATIVE_STREAM_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" THING_NATIVE_VORBIS_PROBE="$PWD/build/vorbis-probe-deps/stream-probe.so" \
 THING_DRAW_BATCH=0 THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=30 THING_LOCAL_PRELOAD="$PWD/build/frame-cpu-exact-20261102/frame-cpu.host.so" THING_FPS_OVERLAY=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
 for file in frame-cpu-exact.csv profile.csv render.csv; do
  cp "build/$name/$file" "docs/evidence/$name/$file"
 done
 echo "Completed $pair"
done
