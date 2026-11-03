#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/batch-range-20261102 docs/evidence/batch-range-20261102
cc -std=gnu11 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iruntime \
 probe/batch/owned_range_selftest.c runtime/adjacent_batch.c -o build/batch-range-20261102/owned-range-host
build/batch-range-20261102/owned-range-host | tee docs/evidence/batch-range-20261102/owned-range-host.txt
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/build/batch-range-20261102:/src/build/batch-range-20261102:rw" --workdir /src \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -static -mcpu=cortex-a53 -Iruntime \
 probe/batch/owned_range_selftest.c runtime/adjacent_batch.c -o build/batch-range-20261102/owned-range-arm
build/tools/qemu-user/usr/bin/qemu-aarch64 build/batch-range-20261102/owned-range-arm | tee docs/evidence/batch-range-20261102/owned-range-arm.txt
