#!/bin/bash
# 30 FPS ABBA: compare exact CPU/frame, not capped FPS improvement.
set -euo pipefail
cd "$(dirname "$0")/.."
for pair in on-1; do
 name="small-decal-gated-smoke-20261102-$pair"
 binary="$PWD/build/game/thething.host"
 [[ $pair != on* ]] || binary="$PWD/build/small-decal-gated-host-20261102/diagnostic.host"
 batch=0; [[ $pair != on* ]] || batch=1; native=""

 mkdir -p "build/$name/userdata/bin"
 cp build/audio-decode-trace-20261102/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
 env -u THING_LOCAL_GDB -u THING_LOCAL_QEMU \
 THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$binary" \
 THING_INPUT_EVENTS='{"60":"tap dpdown","63":"tap a","70":"tap a","80":"axis lefty 26000","83":"axis lefty -26000","86":"axis lefty 26000","89":"axis lefty -26000","92":"axis lefty 26000","95":"axis lefty -26000","98":"axis lefty 26000","101":"axis lefty -26000","104":"axis lefty 26000","107":"axis lefty -26000","110":"axis lefty 26000","113":"axis lefty -26000","116":"axis lefty 26000","119":"axis lefty -26000","122":"axis lefty 26000","125":"axis lefty 0"}' \
 THING_LOCAL_CPU_OBSERVE=1 THING_CAPTURE_START=125 THING_CAPTURE_INTERVAL=19 THING_AUTO_TUTORIAL=0 THING_CPU_PROFILE=1 THING_PROFILE_SECONDS=147 \
 THING_NATIVE_STREAM_PROBE="$native" THING_NATIVE_VORBIS_PROBE="$native" THING_DRAW_BATCH="$batch" DS_DRAW_BATCH="$batch" THING_DRAW_ERROR_CHECK_EVERY=16 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 \
 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 THING_FPS_LIMIT=30 THING_LOCAL_PRELOAD="$PWD/build/frame-cpu-lines-20261102/frame-cpu.host.so" THING_FPS_OVERLAY=0 \
 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 \
 SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
 for file in frame-cpu-exact.csv profile.csv host-cpu-timeline.csv host-cpu-profile.csv host-profile-maps.txt cpu-profile-info.txt cpu-timeline-info.txt; do
  cp "build/$name/$file" "docs/evidence/$name/$file"
 done
 echo "Completed $pair"
done
