#!/bin/bash
# Experimental GPU vertex-shader backend for the static port.
#
# The GPU path moves per-vertex transform/lighting off the CPU and onto the
# Mali-G31 GPU. Host x86 Mesa equivalence tests pass 13/13 realistic state
# cases and unsupported draws fall back to the CPU reference path, but H700
# visuals/performance are NOT device-verified. Keep the default "The Thing.sh"
# launcher for normal play; use this one only to A/B the renderer on a device
# that is available for testing.
export DS_GPU_VERTEX=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
