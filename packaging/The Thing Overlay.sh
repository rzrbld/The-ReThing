#!/bin/bash
# Default settings plus the FPS counter overlay (diagnostic; costs some FPS).
export THING_FPS_OVERLAY=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
