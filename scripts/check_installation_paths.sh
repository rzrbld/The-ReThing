#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/installation-selftest
cc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections \
 -Iruntime -Iprobe -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
 $(pkg-config --cflags sdl2) runtime/installation_selftest.c runtime/installation.c \
 third_party/pcrecomp/runtime/win32hle/advapi32.c -Wl,--gc-sections -o build/installation-selftest/check
for mode in fresh old-hklm old-hkcu custom empty both; do
 printf 'CASE %s\n' "$mode"
 build/installation-selftest/check "$mode"
done
