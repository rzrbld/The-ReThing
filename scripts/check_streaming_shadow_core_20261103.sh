#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
b=build/streaming-shadow-20261103
e=docs/evidence/streaming-shadow-core-20261103
mkdir -p "$e"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/check_streaming_shadow_core.c -lm -o "$b/core-check.host"
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/$b:/out:rw" --workdir /src \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-gcc -static -O2 -mcpu=cortex-a53 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/check_streaming_shadow_core.c -lm -o /out/core-check.aarch64
: > "$e/check.log"
for f in build/streaming-occlusion-20261103/*.bin; do
 "$b/core-check.host" "$f" >> "$e/check.log"
 build/tools/qemu-user/usr/bin/qemu-aarch64 "$b/core-check.aarch64" "$f" >> "$e/check.log"
done
sha256sum "$b/core.h" "$b"/core-check.* probe/visibility/check_streaming_shadow_core.c > "$e/SHA256SUMS"
cat "$e/check.log"
