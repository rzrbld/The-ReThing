#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/depth-probe-20261103
for variant in depth_probe depth_static_probe depth_stacks_probe depth_queued_probe; do
 case "$variant" in depth_probe) dest=depth-probe;; depth_static_probe) dest=depth-static;; depth_stacks_probe) dest=depth-stacks;; *) dest=depth-queued;; esac
 cc -std=c11 -O2 -Wall -Wextra -Werror -shared -fPIC "probe/visibility/$variant.c" -ldl -lGLESv2 -o "build/depth-probe-20261103/$dest.so"
done
