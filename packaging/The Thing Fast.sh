#!/bin/bash
# Re-enables the cached/batched renderer paths for the on-device A/B.
# The streaming vertex ring stays disabled: an H700 regression is being
# bisected, and cross-frame glBufferSubData is a suspected serialisation
# hazard on Mali. Set THING_RING_VB=1 explicitly only for that experiment.
export THING_BIND_CACHE=1
export THING_DRAW_ERROR_CHECK_EVERY=16
export THING_RING_VB=${THING_RING_VB:-0}
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
