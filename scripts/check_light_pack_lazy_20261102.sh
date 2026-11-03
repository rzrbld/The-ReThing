#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/prepare_light_pack_lazy_20261102.py
out=build/light-pack-lazy-20261102
sanitize=();if [ "${THING_TEST_SANITIZE:-0}" = 1 ]; then sanitize+=(-fsanitize=address,undefined -fno-omit-frame-pointer); export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"; fi
for mode in baseline candidate; do
 extra=();if [ "$mode" = candidate ]; then extra+=(-DTHING_LIGHT_PACK_COUNTERS=1);fi
 cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -DRECOMP_FP_STACK_EXACT=1 \
  "${extra[@]}" "${sanitize[@]}" -DTHING_TEST_D3D8_SOURCE="\"$PWD/$out/d3d8.$mode.c\"" \
  -Iruntime -Iprobe -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
  $(pkg-config --cflags sdl2 glesv2) probe/light_pack_lazy_selftest.c \
  runtime/d3d8_renderer.c runtime/d3d8_lights.c runtime/adjacent_batch.c \
  -Wl,--gc-sections -o "$out/check-$mode" $(pkg-config --libs sdl2 glesv2) -lm
 "$out/check-$mode" > "$out/check-$mode.txt" 2> "$out/check-$mode.err"
done
diff -u "$out/check-baseline.txt" "$out/check-candidate.txt"
cat "$out/check-candidate.txt" "$out/check-candidate.err"
