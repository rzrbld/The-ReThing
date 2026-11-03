#!/bin/bash
# Tuned settings + main-thread CPU profile (10 ms samples) and counters, but no
# render tracing, so the FPS is close to the real tuned run. Auto-exits after
# THING_PROFILE_SECONDS (default 240). Bundle the files listed in README.
export DS_GPU_VERTEX=1
export THING_BIND_CACHE=1
export THING_DRAW_ERROR_CHECK_EVERY=16
export THING_RING_VB=0
export THING_FPS_OVERLAY=0
export THING_CPU_PROFILE=1
export THING_GL_STATS=1
export THING_COPY_STATS=1
export THING_PROFILE_SECONDS=${THING_PROFILE_SECONDS:-240}
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
