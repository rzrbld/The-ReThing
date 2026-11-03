#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/prepare_tiny_index_upload_20261102.py
out=build/tiny-index-upload-20261102
evidence=docs/evidence/tiny-index-upload-20261102
mkdir -p "$evidence"
cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-omit-frame-pointer \
 -DTHING_TEST_RENDERER_SOURCE="\"$PWD/$out/d3d8_renderer.c\"" \
 -I"$out" -Iruntime $(pkg-config --cflags sdl2 glesv2) \
 probe/tiny_index_upload_selftest.c -Wl,--gc-sections \
 $(pkg-config --libs sdl2 glesv2) -lm -o "$out/selftest"
ASAN_OPTIONS=detect_leaks=0 "$out/selftest" | tee "$evidence/selftest.txt"
cc -std=gnu11 -O2 -g -Wall -Wextra -Wno-misleading-indentation \
 -I"$out" -Iruntime $(pkg-config --cflags sdl2 glesv2) \
 "$out/replay.c" "$out/adjacent_batch.c" "$out/d3d8_renderer.c" \
 $(pkg-config --libs sdl2 glesv2) -lm -lz -o "$out/replay"
sha256sum "$out/replay" "$out/d3d8_renderer.c" > "$evidence/inputs.sha256"
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a sh -c '"$@"; rc=$?; printf "executable_exit=%s\n" "$rc"; exit "$rc"' \
 sh "$out/replay" docs/evidence/effect-footprints-replay-capture-20261102/decal-*.bin \
 | tee "$evidence/replay.txt"
