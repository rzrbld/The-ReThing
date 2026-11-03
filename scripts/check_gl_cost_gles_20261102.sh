#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=build/gl-cost-preload-20261102
evidence=docs/evidence/gl-cost-preload-20261102
mkdir -p "$out" "$evidence"
cat > "$out/replay.c" <<'SRC'
#define main replay_main
#include "../../probe/effects/decal_replay.c"
#undef main
#include <dlfcn.h>
int main(int argc,char **argv) {
 int (*begin)(unsigned)=dlsym(RTLD_DEFAULT,"ds_gl_cost_begin");
 void (*present)(void)=dlsym(RTLD_DEFAULT,"ds_gl_cost_present");
 assert(begin && present && begin(1));
 int result=replay_main(argc,argv);
 present();
 return result;
}
SRC
cc -std=gnu11 -O2 -g -Wall -Wextra -Wno-misleading-indentation -Iruntime \
 $(pkg-config --cflags sdl2 glesv2) "$out/replay.c" \
 build/small-decal-batch-20261102/adjacent_batch.c runtime/d3d8_renderer.c \
 $(pkg-config --libs sdl2 glesv2) -ldl -lm -lz -o "$out/replay"
rm -f "$evidence/replay-gl-cost.csv"
SDL_VIDEO_X11_FORCE_EGL=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
 xvfb-run -a env LD_PRELOAD="$PWD/$out/libthing-glcost.host.so" \
 THING_GLCOST_FILE="$PWD/$evidence/replay-gl-cost.csv" \
 sh -c '"$@"; rc=$?; printf "executable_exit=%s\n" "$rc"; exit "$rc"' \
 sh "$out/replay" docs/evidence/effect-footprints-replay-capture-20261102/decal-*.bin \
 | tee "$evidence/replay.txt"
