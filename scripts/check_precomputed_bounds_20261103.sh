#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/precomputed-bounds-20261103 docs/evidence/precomputed-bounds-20261103
cc -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -ffp-contract=off probe/visibility/precomputed_bounds_test.c -lm -o build/precomputed-bounds-20261103/check-host
build/precomputed-bounds-20261103/check-host > docs/evidence/precomputed-bounds-20261103/host.log
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/precomputed-bounds-20261103:/src/build/precomputed-bounds-20261103:rw" --workdir /src sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 aarch64-linux-gnu-gcc -static -mcpu=cortex-a53 -std=c11 -O2 -ffp-contract=off -Wall -Wextra -Werror -Wno-misleading-indentation probe/visibility/precomputed_bounds_test.c -lm -o build/precomputed-bounds-20261103/check-arm
build/tools/qemu-user/usr/bin/qemu-aarch64 build/precomputed-bounds-20261103/check-arm > docs/evidence/precomputed-bounds-20261103/arm.log
python3 scripts/check_bounds_capture_20261103.py
sha256sum probe/visibility/precomputed_bounds.h probe/visibility/precomputed_bounds_test.c build/precomputed-bounds-20261103/check-host build/precomputed-bounds-20261103/check-arm > docs/evidence/precomputed-bounds-20261103/checksums.txt
