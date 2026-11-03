#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/package
cp build/game/thething.aarch64 build/package/thething.aarch64
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD/build/package:/out:rw" "$image" \
  aarch64-linux-gnu-strip --strip-debug /out/thething.aarch64
python3 scripts/package_candidate.py
