#!/bin/bash
# 30 FPS lock: half of a 60 Hz refresh, immediate present + 33.33 ms pacer.
# Only waits when the scene is already faster than 30; it cannot lift slower scenes.
export THING_FPS_LIMIT=30
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
