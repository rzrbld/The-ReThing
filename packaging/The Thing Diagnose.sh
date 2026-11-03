#!/bin/bash
# Bounded profiling run: main-thread CPU samples, render.csv, GL/copy/perf counters.
# Auto-exits after THING_PROFILE_SECONDS presentation seconds (default 240).
export THING_CPU_PROFILE=1
export THING_TRACE_RENDER=1
export THING_GL_STATS=1
export THING_COPY_STATS=1
export THING_PERF_PROBE=1
export THING_PROFILE_SECONDS=${THING_PROFILE_SECONDS:-240}
exec "$(dirname -- "${BASH_SOURCE[0]}")/The Thing.sh" "$@"
