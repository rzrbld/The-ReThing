#!/bin/bash
# UI regression diagnostic; QEMU/software GLES results are not H700 performance.
set -euo pipefail
cd "$(dirname "$0")/.."
arch=${1:-host}
slots=${2:-empty}
case "$arch:$slots" in host:empty|host:valid|arm:empty|arm:valid) ;; *) echo 'Usage: check_load_menu.sh host|arm empty|valid' >&2; exit 2;; esac
name="load-menu-${arch}-${slots}-$(date +%Y%m%d-%H%M%S)-$$"
if [[ $slots == valid ]]; then
    mkdir -p "build/$name/userdata/bin"
    cp build/save-writer-corrected/userdata/bin/Slot1.sav "build/$name/userdata/bin/Slot1.sav"
fi
if [[ $arch == arm ]]; then
    binary="$PWD/scripts/run_qemu_aarch64.sh"
    events='{"65":"tap dpdown","68":"tap a"}'
    duration=85
    start=62
else
    binary="$PWD/build/game/thething.host"
    events='{"50":"tap dpdown","53":"tap a"}'
    duration=65
    start=49
fi
env THING_LOCAL_RUN="$name" THING_LOCAL_BINARY="$binary" THING_INPUT_EVENTS="$events" \
 THING_CAPTURE_START="$start" THING_AUTO_TUTORIAL=0 THING_PROFILE_SECONDS="$duration" \
 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_GPU_BUFFER_CACHE=0 DS_RENDER_THREAD=0 \
 THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 SDL_VIDEO_X11_FORCE_EGL=1 \
 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a python3 scripts/capture_local_gameplay.py
printf 'Inspect menu captures and log under docs/evidence/%s\n' "$name"
test "$(cat "docs/evidence/$name/exit.txt")" = 0
