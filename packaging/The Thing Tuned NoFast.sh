#!/bin/bash
# A/B control for the fragment-shader specialisation: identical to The Thing
# Tuned but keeps the generic fragment program (DS_FAST_SHADER=0).
export DS_FAST_SHADER=0
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing Tuned.sh" "$@"
