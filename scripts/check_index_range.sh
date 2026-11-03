#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/index-range docs/evidence/index-range-20261102
cc -O2 -Wall -Wextra -Werror -Iruntime probe/index_range_selftest.c -o build/index-range/check.host
build/index-range/check.host | tee docs/evidence/index-range-20261102/host-correctness.txt
# ARM reproduction (requires the existing local Docker build image):
# docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/index-range:/out:rw" --workdir /src sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 aarch64-linux-gnu-gcc -O2 -mcpu=cortex-a53 -static -Wall -Wextra -Werror -Iruntime probe/index_range_selftest.c -o /out/check.aarch64
# build/tools/qemu-user/usr/bin/qemu-aarch64 build/index-range/check.aarch64
# QEMU validates NEON results, not H700 performance.
