#!/usr/bin/env bash
# GPU-vs-CPU equivalence selftest for the optional vertex-shader path
# (runtime/d3d8_renderer.c, DS_GPU_VERTEX / THING_GPU_VERTEX) against the CPU
# reference path, driven through identical D8State/raw-FVF geometry in one GLES
# context and compared by RGBA readback.
#
# HOST EVIDENCE ONLY: x86_64 Linux + Xvfb + Mesa llvmpipe GLES (software
# rasterization). This makes no H700/Mali device or performance claim.
#
# Host GL quirk: this Xvfb GLX stack routes through libEGL_nvidia and fails, and
# SDL's default GLX path rejects the ES profile (GLXBadProfileARB). SDL is
# therefore forced onto EGL (SDL_VIDEO_X11_FORCE_EGL=1) while the actual driver
# stays Mesa llvmpipe software via the LIBGL/GALLIUM/EGL vendor variables.
set -euo pipefail
cd "$(dirname "$0")/.."

EVIDENCE=${THING_GPU_TEST_EVIDENCE:-docs/evidence/gpu-equivalence}
BUILD=${THING_GPU_TEST_BUILD:-build/gpu-equivalence}
mkdir -p "$BUILD" "$EVIDENCE/images"

cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -Iruntime -Iprobe \
  $(pkg-config --cflags sdl2 glesv2) \
  probe/d3d8_gpu_equivalence_selftest.c "${THING_GPU_TEST_RENDERER:-runtime/d3d8_renderer.c}" \
  -Wl,--gc-sections -Wl,--wrap=glUniformMatrix3fv -Wl,--wrap=glUniform3fv -o "$BUILD/check" $(pkg-config --libs sdl2 glesv2) -lm -lz

set +e
env -u DS_GPU_VERTEX -u THING_GPU_VERTEX -u DS_GPU_TRIM_VB -u THING_GPU_TRIM_VB \
  SDL_VIDEO_X11_FORCE_EGL=1 \
  LIBGL_ALWAYS_SOFTWARE=1 \
  GALLIUM_DRIVER=llvmpipe \
  __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
  timeout 110 xvfb-run -a -s '-screen 0 800x600x24' \
  "$BUILD/check" "$EVIDENCE" 2>&1 | tee "$EVIDENCE/log.txt"
status=${PIPESTATUS[0]}
set -e
if [ "$status" -ne 0 ]; then
  echo "check_gpu_renderer_equivalence: selftest exited $status" >&2
  exit "$status"
fi
grep -q '^overall=PASS$' "$EVIDENCE/log.txt" || {
  echo "check_gpu_renderer_equivalence: overall verdict is not PASS" >&2
  exit 1
}
python3 - "$EVIDENCE/result.json" <<'PY'
import json, sys
with open(sys.argv[1], "r", encoding="utf-8") as handle:
    data = json.load(handle)
assert data["overall"] == "PASS", data["overall"]
assert data["required_cases"].split("/")[0] == data["required_cases"].split("/")[1]
print("result.json: valid; overall=%s required_cases=%s diagnostic_cases=%s"
      % (data["overall"], data["required_cases"], data["diagnostic_cases"]))
PY
echo "check_gpu_renderer_equivalence: PASS (host Mesa llvmpipe evidence only; no H700/Mali claim)"
echo "evidence: $EVIDENCE/result.json $EVIDENCE/log.txt $EVIDENCE/findings.md $EVIDENCE/images/"
grep -E '^(case=|required_cases=|overall=)' "$EVIDENCE/log.txt"
