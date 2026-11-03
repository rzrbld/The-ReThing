#!/bin/bash
# Requires the rotation-ring binary (experiments/thething-ring-rotation.aarch64)
# installed as thething.aarch64. Conservative settings + caches + rotation ring.
export THING_BIND_CACHE=1
export THING_DRAW_ERROR_CHECK_EVERY=16
export THING_RING_VB=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing Diagnose.sh" "$@"
