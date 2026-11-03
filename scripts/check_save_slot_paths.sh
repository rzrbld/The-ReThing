#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/save-fix-check
cc -std=gnu11 -O1 -ffunction-sections -fdata-sections \
 -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
 runtime/save_slots.c runtime/save_slots_selftest.c third_party/pcrecomp/runtime/win32hle/hle_path.c \
 -Wl,--gc-sections -o build/save-fix-check/paths
build/save-fix-check/paths
