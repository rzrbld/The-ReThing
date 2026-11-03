#!/bin/bash
# Uncapped: immediate present, no vsync wait and no frame pacer (may tear).
export THING_FPS_LIMIT=0
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
