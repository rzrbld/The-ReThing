#!/bin/bash
# Tuned settings + render trace: confirms the native buffer cache is used
# ([d3d8-buffer] native_vertex_draws) and gives per-frame submit_ms.
export DS_GPU_VERTEX=1
export DS_GPU_BUFFER_CACHE=1
export THING_BIND_CACHE=1
export THING_DRAW_ERROR_CHECK_EVERY=16
export THING_RING_VB=0
export THING_FPS_OVERLAY=0
export THING_TRACE_RENDER=1
export THING_GL_STATS=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
