#!/bin/bash
# Same as Diagnose but enables the optional GPU vertex-shader path so the
# device can A/B CPU vertex transform vs Mali vertex shading.
export DS_GPU_VERTEX=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing Diagnose.sh" "$@"
