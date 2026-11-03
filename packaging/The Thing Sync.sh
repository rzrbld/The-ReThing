#!/bin/bash
# Synchronous renderer (pre-threading) for A/B or rollback.
export DS_RENDER_THREAD=0
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
