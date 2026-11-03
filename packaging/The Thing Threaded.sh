#!/bin/bash
# Experimental render-worker thread: GL submission runs on a worker thread and
# overlaps guest execution (desert_storm-style; the main lever for CPU-bound
# gameplay). Needs the threaded binary; harmless on the normal binary because
# DS_RENDER_THREAD is only read by the proxy build.
export DS_RENDER_THREAD=1
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
