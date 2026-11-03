#!/bin/bash
# Conservative CPU-reference renderer (pre-2026-10-31 defaults) for rollback or
# A/B when the GPU path or fragment specialisation is suspect.
export DS_GPU_VERTEX=0
export THING_BIND_CACHE=0
export THING_DRAW_ERROR_CHECK_EVERY=1
export THING_RING_VB=0
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
