#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/package_first_run.py stage
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD/build/first-run-distro:/out:rw" \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-strip --strip-unneeded /out/ports/the-thing/thething.aarch64 /out/ports/the-thing/thething-launcher
python3 scripts/package_first_run.py pack
