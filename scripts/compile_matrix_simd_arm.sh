#!/bin/bash
set -euo pipefail
flags=(-fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Ithird_party/pcrecomp/runtime/recomp32 -Iruntime)
aarch64-linux-gnu-gcc -O1 "${flags[@]}" -c build/matrix-simd/helper.c -o /out/helper.aarch64.o
aarch64-linux-gnu-gcc -static -O2 "${flags[@]}" build/matrix-simd/external-probe.c /out/helper.aarch64.o -lm -o /out/external.aarch64
aarch64-linux-gnu-objdump -d /out/helper.aarch64.o > /out/helper.aarch64.asm
