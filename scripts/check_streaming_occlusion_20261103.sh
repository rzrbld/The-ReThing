#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
b=build/streaming-occlusion-20261103
e=docs/evidence/streaming-occlusion-20261103
mkdir -p "$b" "$e"
python3 scripts/analyze_streaming_occlusion_20261103.py docs/evidence/queue-phase-views-20261103 --min-score 0.1 --output streaming-score-0.1.json > "$e/reference.log"
python3 scripts/export_occlusion_pipeline_20261103.py --capture docs/evidence/queue-phase-views-20261103 --reference streaming-score-0.1.json --streaming --out "$b" > "$e/fixtures.log"
cc -O2 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/streaming_occlusion_bench.c -lm -o "$b/bench.host"
: > "$e/host.log"
for f in "$b"/*.bin; do "$b/bench.host" "$f" 3000 >> "$e/host.log"; done
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/$b:/out:rw" --workdir /src \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-gcc -static -O2 -mcpu=cortex-a53 -ffp-contract=off -Wall -Wextra -Werror probe/visibility/streaming_occlusion_bench.c -lm -o /out/bench.aarch64
: > "$e/arm-qemu.log"
for f in "$b"/*.bin; do build/tools/qemu-user/usr/bin/qemu-aarch64 "$b/bench.aarch64" "$f" 10 >> "$e/arm-qemu.log"; done
python3 scripts/check_occlusion_clear_guards_20261103.py --streaming-native > "$e/clear-guards.log"
sha256sum probe/visibility/streaming_occlusion_bench.c probe/visibility/occlusion_pipeline_bench.c probe/visibility/coarse_occlusion_bench.c "$b"/bench.* "$b"/*.bin > "$e/SHA256SUMS"
cat "$e/host.log" "$e/arm-qemu.log"
