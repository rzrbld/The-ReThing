#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="$PWD/build/early-queue-combined-aarch64-20261103"
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$out:/work:rw" \
 "${CROSS_IMAGE:-rething-cross:bookworm}" \
 aarch64-linux-gnu-strip --strip-unneeded -o /work/thething-queue-bounds.aarch64 /work/diagnostic.aarch64
sha256sum "$out/diagnostic.aarch64" "$out/thething-queue-bounds.aarch64"
