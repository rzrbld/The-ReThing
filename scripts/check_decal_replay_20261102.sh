#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
fixture=docs/evidence/effect-footprints-replay-capture-20261102
out=build/decal-replay-20261102
evidence=docs/evidence/decal-replay-20261102
test "$(cat "$fixture/exit.txt")" = 0
if rg -q 'intro.dll' "$fixture/run.log"; then
 echo 'Wrong route: captured New Game intro' >&2; exit 1
fi
mkdir -p "$out" "$evidence"
python3 - "$fixture" <<'PY'
import pathlib, re, sys
p = pathlib.Path(sys.argv[1])
records = sorted(p.glob('decal-*.bin'))
trace = (p/'render-frame.txt').read_text()
assert records and len({r.name.split('-')[1] for r in records}) == 1, 'Mixed or missing frames'
logged = re.findall(r'decal-record file=(\S+)', trace)
assert {pathlib.Path(n).name for n in logged} == {r.name for r in records}, 'Record/trace mismatch'
categories = re.findall(r'^effect category=(\d+)$', trace, re.M)
positions = [i for i, c in enumerate(categories) if c == '3']
assert len(positions) == len(records)
assert positions == list(range(positions[0], positions[-1]+1)), 'Decals interleaved with other draws'
PY
cc -std=gnu11 -O2 -g -Wall -Wextra -Wno-misleading-indentation -Iruntime $(pkg-config --cflags sdl2 glesv2) \
 probe/effects/decal_replay.c build/small-decal-batch-20261102/adjacent_batch.c runtime/d3d8_renderer.c \
 $(pkg-config --libs sdl2 glesv2) -lm -lz -o "$out/check"
sha256sum "$out/check" "$fixture"/decal-*.bin > "$evidence/inputs.sha256"
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a sh -c '"$@"; rc=$?; printf "executable_exit=%s\n" "$rc"; exit "$rc"' \
 sh "$out/check" "$fixture"/decal-*.bin | tee "$evidence/results.txt"
