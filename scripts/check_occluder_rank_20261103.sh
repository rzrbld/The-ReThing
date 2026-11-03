#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
b=build/occluder-rank-20261103
e=docs/evidence/occluder-rank-20261103
mkdir -p "$b" "$e"
python3 scripts/export_occluder_rank_20261103.py > "$e/fixtures.log"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/occluder_rank_bench.c -lm -o "$b/bench.host"
: > "$e/host.log"
for f in "$b"/*.bin; do
 printf '%s\n' "$f" >> "$e/host.log"
 "$b/bench.host" "$f" 3000 >> "$e/host.log"
done
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/$b:/out:rw" --workdir /src \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-gcc -static -O2 -mcpu=cortex-a53 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/occluder_rank_bench.c -lm -o /out/bench.aarch64
: > "$e/arm-qemu.log"
for f in "$b"/*.bin; do
 printf '%s\n' "$f" >> "$e/arm-qemu.log"
 build/tools/qemu-user/usr/bin/qemu-aarch64 "$b/bench.aarch64" "$f" 10 >> "$e/arm-qemu.log"
done
sha256sum probe/visibility/occluder_rank_bench.c "$b"/bench.* "$b"/*.bin > "$e/SHA256SUMS"
cat "$e/host.log" "$e/arm-qemu.log"
