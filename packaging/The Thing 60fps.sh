#!/bin/bash
# 60 FPS lock (vsync). Matches the guest's default presentation interval.
export THING_FPS_LIMIT=60
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
