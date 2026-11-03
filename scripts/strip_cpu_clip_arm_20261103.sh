#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="$PWD/build/cpu-clip-aarch64-20261103"
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$out:/work:rw" \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-strip --strip-unneeded -o /work/thething-cpu-clip.aarch64 /work/diagnostic.aarch64
sha256sum "$out/diagnostic.aarch64" "$out/thething-cpu-clip.aarch64"
