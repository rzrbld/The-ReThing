#!/bin/bash
# Best-known settings without rebuilding: GPU vertex path + cached/batched
# renderer + no FPS overlay. Native buffer cache is NOT used: the device A/B
# showed it neutral to slightly worse than the plain GPU path on Mali.
export DS_GPU_VERTEX=1
export THING_BIND_CACHE=1
export THING_DRAW_ERROR_CHECK_EVERY=16
export THING_RING_VB=0
export THING_FPS_OVERLAY=0
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
