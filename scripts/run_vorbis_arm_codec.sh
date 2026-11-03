#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/fetch_vorbis_sources.py
mkdir -p build/native-vorbis-arm
image=sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49
docker run --rm --network none --read-only --tmpfs /tmp \
 --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
 -v "$PWD/build/native-vorbis-arm:/src/build/native-vorbis-arm:rw" \
 --workdir /src "$image" bash scripts/build_vorbis_arm_codec.sh \
 > build/native-vorbis-arm/build.log 2>&1
mkdir -p docs/evidence/native-vorbis-arm-corpus-20261102
THING_QEMU_BINARY="$PWD/build/native-vorbis-arm/corpus.aarch64" \
 bash scripts/run_qemu_aarch64.sh docs/evidence/audio-cpu-20261102 \
 docs/evidence/native-vorbis-arm-corpus-20261102 \
 > docs/evidence/native-vorbis-arm-corpus-20261102/decoded.csv \
 2> docs/evidence/native-vorbis-arm-corpus-20261102/run.log
python3 scripts/check_vorbis_arm_corpus.py
THING_QEMU_BINARY="$PWD/build/native-vorbis-arm/stream-selftest.aarch64" \
 bash scripts/run_qemu_aarch64.sh docs/evidence/audio-cpu-20261102 \
 > docs/evidence/native-vorbis-arm-corpus-20261102/stream-selftest.log 2>&1
THING_QEMU_BINARY="$PWD/build/native-vorbis-arm/guest-stream-selftest.aarch64" \
 bash scripts/run_qemu_aarch64.sh "$PWD/build/native-vorbis-arm/probe.aarch64.so" \
 docs/evidence/audio-cpu-20261102/decode-0001.ogg \
 > docs/evidence/native-vorbis-arm-corpus-20261102/guest-stream-selftest.log 2>&1
