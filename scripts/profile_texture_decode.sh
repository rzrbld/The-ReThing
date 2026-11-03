#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
name=texture-decode640
mkdir -p "build/$name/userdata/bin"
cp build/save-writer-corrected/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
env -u THING_BRIGHTNESS -u DS_BRIGHTNESS \
 THING_LOCAL_RUN="$name" THING_TRACE_RENDER=1 THING_DRAW_BATCH=0 \
 THING_DRAW_ERROR_CHECK_EVERY=16 \
 THING_INPUT_EVENTS='{"50":"tap dpdown","53":"tap a","60":"tap a","95":"axis lefty -26000","105":"axis lefty 0"}' \
 THING_CAPTURE_START=112 THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS=120 \
 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 \
 THING_FPS_LIMIT=0 THING_FPS_OVERLAY=0 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
cp "build/$name/profile.csv" "build/$name/render.csv" "docs/evidence/$name/"
