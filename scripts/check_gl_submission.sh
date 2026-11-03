#!/usr/bin/env bash
# Host-only A/B of the D3D8 renderer's per-draw GL submission path.
#
# Runs the same short menu-scene capture twice (binding caches on/off) with
# DS_GL_STATS=1 and writes driver-call counters + profile/render CSVs under
# docs/evidence/gl-submission/. This measures host call counts and wall submit
# time; it is not H700 performance evidence. Usage: check_gl_submission.sh
set -euo pipefail
cd "$(dirname "$0")/.."
run() {
  local tag=$1 cache=$2 check_every=${3:-1} ring=${4:-1}
  local root="build/gl-submission/$tag" out="docs/evidence/gl-submission/$tag"
  rm -rf "$root" "$out"; mkdir -p "$root/bin" "$root/data" "$root/config" "$out"
  cp build/active/bin/thething.exe "$root/bin/thething.exe"
  cp build/font-candidate/config/registry.txt "$root/config/registry.txt"
  for s in build/active/data/*; do ln -s "$(realpath "$s")" "$root/data/$(basename "$s")"; done
  : > "$root/input.commands"
  LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
  __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
  THING_ROOT="$PWD/$root" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
  THING_WINDOWED=1 THING_TEST_INPUT=1 THING_TRACE_RENDER=1 THING_GL_STATS=1 \
  THING_PROFILE_SECONDS=45 THING_BIND_CACHE="$cache" \
  THING_DRAW_ERROR_CHECK_EVERY="$check_every" THING_RING_VB="$ring" \
  xvfb-run -a -s '-screen 0 800x600x24' build/game/thething.host > "$out/run.log" 2>&1 || true
  grep -o "\[d3d8-gl\].*" "$out/run.log" | tail -1 > "$out/gl-stats.txt" || true
  cp "$root/profile.csv" "$out/profile.csv" 2>/dev/null || true
  cp "$root/render.csv" "$out/render.csv" 2>/dev/null || true
  printf 'tag=%s bind_cache=%s error_check_every=%s ring_vb=%s\n' "$tag" "$cache" "$check_every" "$ring" > "$out/settings.txt"
  echo "== $tag (bind_cache=$cache error_check_every=$check_every)"; cat "$out/gl-stats.txt"
}
run cache_on 1
run cache_off 0
run cache_on_ec16 1 16
run ring_off 1 16 0
