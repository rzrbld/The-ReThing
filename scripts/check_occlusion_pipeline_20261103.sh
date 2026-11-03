#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
b=build/occlusion-pipeline-20261103
e=docs/evidence/occlusion-pipeline-20261103
mkdir -p "$b" "$e"
python3 scripts/analyze_coarse_occlusion_20261103.py docs/evidence/queue-bounds-views-20261103 --max-meshes 32 --selection depth_area --max-triangles 16 --output coarse-mesh-budget-32-depth-safe.json > "$e/reference.log"
python3 scripts/export_occlusion_pipeline_20261103.py > "$e/fixtures.log"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/occlusion_pipeline_bench.c -lm -o "$b/bench.host"
: > "$e/host.log"
for f in "$b"/*.bin; do
 printf '%s\n' "$f" >> "$e/host.log"
 "$b/bench.host" "$f" 3000 >> "$e/host.log"
done
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/$b:/out:rw" --workdir /src \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-gcc -static -O2 -mcpu=cortex-a53 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/occlusion_pipeline_bench.c -lm -o /out/bench.aarch64
: > "$e/arm-qemu.log"
for f in "$b"/*.bin; do
 printf '%s\n' "$f" >> "$e/arm-qemu.log"
 build/tools/qemu-user/usr/bin/qemu-aarch64 "$b/bench.aarch64" "$f" 10 >> "$e/arm-qemu.log"
done
python3 scripts/check_occlusion_clear_guards_20261103.py --native > "$e/clear-guards.log"
b4=build/occlusion-budget4-20261103
python3 scripts/export_occlusion_pipeline_20261103.py --capture docs/evidence/material-views-20261103 --reference coarse-budget-4-4.json --out "$b4" > "$e/budget4-fixtures.log"
: > "$e/budget4-host.log"
: > "$e/budget4-arm-qemu.log"
for f in "$b4"/*.bin; do
 "$b/bench.host" "$f" 3000 >> "$e/budget4-host.log"
 build/tools/qemu-user/usr/bin/qemu-aarch64 "$b/bench.aarch64" "$f" 10 >> "$e/budget4-arm-qemu.log"
done
sha256sum probe/visibility/occlusion_pipeline_bench.c probe/visibility/coarse_occlusion_bench.c "$b"/bench.* "$b"/*.bin > "$e/SHA256SUMS"
cat "$e/host.log" "$e/arm-qemu.log"
