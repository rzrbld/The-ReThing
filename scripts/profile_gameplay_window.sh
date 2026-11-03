#!/usr/bin/env bash
# Host-only: profile a bounded *gameplay* window via SIGUSR1 interval snapshots.
#
# The game reaches level1 through the intro, tutorials are dismissed and forward
# movement runs; two SIGUSR1 flushes bracket a gameplay-only interval whose
# cumulative main-thread CPU samples are subtracted offline. Not H700 evidence.
set -euo pipefail
cd "$(dirname "$0")/.."
run=build/gameplay-profile
out=${OUT:-docs/evidence/gameplay-profile}
rm -rf "$run" "$out"; mkdir -p "$out"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
export __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
export THING_CPU_PROFILE=1 THING_GL_STATS=1
THING_LOCAL_RUN=gameplay-profile timeout 300 \
  xvfb-run -a -s '-screen 0 800x600x24' python3 scripts/capture_local_gameplay.py \
  > build/gameplay-profile.out 2>&1 &
driver=$!
start=$(date +%s)
# Find the real game process by exact executable name. Do NOT use pgrep -f on
# a path: the harness' own wrapper command line can contain that string and a
# signal would then kill the driver instead of the game.
game_pid=""
for _ in $(seq 1 120); do
  for candidate in $(pidof thething.host 2>/dev/null || true); do
    if [[ -r /proc/$candidate/comm && $(cat /proc/$candidate/comm) == thething.host ]]; then
      game_pid=$candidate; break
    fi
  done
  [[ -n $game_pid ]] && break
  sleep 1
done
[[ -n $game_pid ]] || { echo "game did not start" >&2; exit 1; }
[[ $game_pid != "$$" ]] || { echo "refusing to signal the driver" >&2; exit 1; }
snapshot() {
  local tag=$1 dir="$out/$1"
  mkdir -p "$dir"
  for f in host-cpu-profile.csv host-call-profile.csv cpu-profile.csv host-profile-maps.txt cpu-profile-info.txt profile.csv render.csv; do
    cp "$run/$f" "$dir/$f" 2>/dev/null || true
  done
  date +%s > "$dir/wall_epoch.txt"
}
wait_until() { local target=$1 now; while :; do now=$(date +%s); (( now - start >= target )) && break; sleep 1; done; }
wait_until 190
kill -USR1 "$game_pid"
sleep 2; snapshot before
wait_until 246
kill -USR1 "$game_pid"
sleep 2; snapshot after
wait "$driver" || true
printf '%s\n' "$game_pid" > "$out/game_pid.txt"
echo "gameplay-window snapshots written to $out"
