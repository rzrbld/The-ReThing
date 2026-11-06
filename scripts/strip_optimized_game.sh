#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# --fixed-regs strips the C1 build from build_optimized_game.py --fixed-regs.
out="$PWD/build/early-queue-combined-aarch64-20261103" name=thething-queue-bounds.aarch64
if [[ "${1:-}" == --fixed-regs ]]; then
  out="$PWD/build/c1-fixed-combined-aarch64-20261106" name=thething-c1-fixed.aarch64
fi
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$out:/work:rw" \
 "${CROSS_IMAGE:-rething-cross:bookworm}" \
 aarch64-linux-gnu-strip --strip-unneeded -o "/work/$name" /work/diagnostic.aarch64
sha256sum "$out/diagnostic.aarch64" "$out/$name"
