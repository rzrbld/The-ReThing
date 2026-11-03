#!/bin/bash
# Fifteen-minute warm-up followed by three-minute measured gameplay interval.
export THING_PROFILE_SECONDS=1080
export THING_TRACE_RENDER=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
