#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="$PWD/build/gl-cost-preload-20261102"
mkdir -p "$out"
cc -std=c11 -O2 -Wall -Wextra -Werror -shared -fPIC probe/rt/gl_cost_preload.c \
 -ldl -o "$out/libthing-glcost.host.so"
if [[ ${1:-host} == aarch64 ]]; then
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$out:/out:rw" --workdir /src \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Werror -shared -fPIC \
 probe/rt/gl_cost_preload.c -ldl -o /out/libthing-glcost.aarch64.so
fi
sha256sum "$out"/*.so
