#!/usr/bin/env bash
# Builds translation archives only. These are NOT a linked/runnable game.
set -euo pipefail
cd "$(dirname "$0")/.."
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
module=${1:?Usage: bash scripts/build_lifted.sh MODULE_NAME}
[[ "$module" =~ ^[a-z0-9_]+$ ]] || exit 2
test -f "build/translated/$module/functions.h"
python3 - "$module" <<'PY'
import json,sys
from pathlib import Path
report=json.loads(Path(f'docs/evidence/translation-{sys.argv[1]}.json').read_text())
if report['unsupported'] or report['lift_exceptions']:
    sys.exit('Refusing build: unresolved translation failures; see translation ledger')
PY
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -e THING_MODULE="$module" \
  -v "$PWD:/src:ro" -v "$PWD/build/translated/$module:/out:rw" \
  --workdir /src "$image" bash -eu -o pipefail -c '
    for file in /out/functions_*.c /out/dispatch.c; do
      aarch64-linux-gnu-gcc -std=gnu11 -O1 -fno-strict-aliasing -fwrapv \
        -mcpu=cortex-a53 -Werror=implicit-function-declaration \
        -I/src/third_party/pcrecomp/runtime/recomp32 -I/src/runtime \
        -c "$file" -o "${file%.c}.aarch64.o"
    done
    aarch64-linux-gnu-ar rcs /out/lifted.aarch64.a /out/*.aarch64.o
    aarch64-linux-gnu-readelf -h /out/functions_0000.aarch64.o > /out/elf-header.txt
    sha256sum /out/lifted.aarch64.a > /out/SHA256SUMS
  '
