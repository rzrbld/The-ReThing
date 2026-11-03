#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Match production default contraction; set off to repeat the strict probe.
contract=${THING_LEAF_TEST_CONTRACT:-fast}
mkdir -p docs/evidence/leaf-calls-20261102
python3 scripts/probe_leaf_calls_20261102.py
cc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract="$contract" -Ithird_party/pcrecomp/runtime/recomp32 build/leaf-calls-20261102/probe.c -lm -o build/leaf-calls-20261102/check
build/leaf-calls-20261102/check | tee docs/evidence/leaf-calls-20261102/host-test.log
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$PWD:/src" -w /src sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 aarch64-linux-gnu-gcc -static -std=gnu11 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -ffp-contract="$contract" -Ithird_party/pcrecomp/runtime/recomp32 build/leaf-calls-20261102/probe.c -lm -o build/leaf-calls-20261102/check.aarch64
build/tools/qemu-user/usr/bin/qemu-aarch64 build/leaf-calls-20261102/check.aarch64 | tee docs/evidence/leaf-calls-20261102/arm-test.log
sha256sum build/leaf-calls-20261102/probe.c build/leaf-calls-20261102/check* > docs/evidence/leaf-calls-20261102/tests.sha256
