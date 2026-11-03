#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/cpu-profiler-check/output
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Iruntime \
  -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
  runtime/profiler.c runtime/profiler_selftest.c -o build/cpu-profiler-check/check -pthread -lrt
THING_ROOT="$PWD/build/cpu-profiler-check/output" build/cpu-profiler-check/check
