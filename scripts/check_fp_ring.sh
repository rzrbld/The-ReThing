#!/usr/bin/env bash
# Bit-exact reference test for the circular x87 stack (recomp_types.h).
set -euo pipefail
cd "$(dirname "$0")/.."
cc=${CC:-gcc}
out=build/fp-ring-test
mkdir -p "$out"
$cc -std=gnu11 -O2 -ffp-contract=off -Wall -Wextra \
  -Ithird_party/pcrecomp/runtime/recomp32 -Iruntime \
  probe/fp_ring_selftest.c runtime/fpu_env.c -o "$out/fp_ring_selftest" -lm
"$out/fp_ring_selftest"
