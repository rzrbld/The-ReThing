#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/depth-probe-20261103 docs/evidence/depth-probe-20261103
cc -std=c11 -O2 -Wall -Wextra -Werror -shared -fPIC probe/visibility/depth_static_probe.c -ldl -lGLESv2 -o build/depth-probe-20261103/selftest-probe.so
cc -O2 -Wall -Wextra -Werror probe/visibility/depth_probe_test.c $(pkg-config --cflags --libs sdl2 glesv2) -ldl -o build/depth-probe-20261103/test
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json xvfb-run -a env LD_PRELOAD="$PWD/build/depth-probe-20261103/selftest-probe.so" THING_DEPTH_FILE="$PWD/docs/evidence/depth-probe-20261103/selftest-indexed.csv" build/depth-probe-20261103/test
python3 - <<'PY'
import csv
r=list(csv.DictReader(open('docs/evidence/depth-probe-20261103/selftest-indexed.csv')))
assert [(x['normal_pass'],x['no_depth_pass'])for x in r]==[('1','1'),('0','1'),('0','0')]
assert [x['indexed']for x in r]==['0','1','0']
print('PASS visible / depth-hidden / offscreen classifications')
PY
