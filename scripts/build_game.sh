#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
target=${1:-host}
shift || true
python3 scripts/patch_fpu_env.py --apply
python3 scripts/patch_font_diagnostics.py --apply
python3 scripts/patch_save_slots.py
PYTHONDONTWRITEBYTECODE=1 python3 scripts/recover_targets.py
python3 scripts/generate_modules.py
# Circular-x87-stack staging: rewrite the lifter's direct _st[i]/g_st_i64[i]
# accesses and the checked environment calls onto logical slot accessors.
python3 scripts/stage_x87_ring.py --tree build/translated build/translated-x87
python3 scripts/stage_x87_ring.py --file build/game/recovered.c
# Optional render-worker variant: generate the typed GL proxy and stage the
# bounded runtime hooks before compiling.
render_thread=0
for arg in "$@"; do [[ "$arg" == --render-thread ]] && render_thread=1; done
if (( render_thread )); then
  python3 probe/rt/generate_render_proxy.py
  python3 scripts/stage_render_thread.py
fi
# --local-regs needs the fail-closed staged source tree (see stage_local_regs.py).
local_regs=0
for arg in "$@"; do [[ "$arg" == --local-regs ]] && local_regs=1; done
if (( local_regs )); then
  mkdir -p build/translated-local-regs
  python3 scripts/stage_local_regs.py build/translated-x87/*/functions_*.c \
    --output build/translated-local-regs
fi
if [[ "$target" == host ]]; then
  exec python3 -u scripts/build_game.py --target host "$@"
elif [[ "$target" == aarch64 ]]; then
  mkdir -p build/game
  image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
  exec docker run --rm --network none --read-only --tmpfs /tmp \
    --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/game:/src/build/game:rw" \
    --workdir /src "$image" python3 -u scripts/build_game.py --target aarch64 "$@"
else
  echo 'target must be host or aarch64' >&2
  exit 2
fi
