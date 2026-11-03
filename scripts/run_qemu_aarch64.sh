#!/usr/bin/env bash
# AArch64 Linux diagnostic under QEMU + Debian Mesa; not Mali/H700 emulation.
set -euo pipefail
cd "$(dirname "$0")/.."
project=$PWD
qemu="$project/build/tools/qemu-user/usr/bin/qemu-aarch64"
sysroot="$project/build/arm64-qemu-sysroot"
loader="$sysroot/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1"
libs="$project/build/arm64-qemu-shims:$sysroot/lib/aarch64-linux-gnu:$sysroot/usr/lib/aarch64-linux-gnu:$sysroot/usr/lib/aarch64-linux-gnu/pulseaudio"
test -x "$qemu" && test -f "$loader" && test -e "$project/build/arm64-qemu-shims/libmali.so.0"
export LIBGL_ALWAYS_SOFTWARE=1
export LIBGL_DRIVERS_PATH="$sysroot/usr/lib/aarch64-linux-gnu/dri"
export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy}
binary=${THING_QEMU_BINARY:-"$project/build/game/thething.aarch64"}
exec "$qemu" "$loader" --library-path "$libs" "$binary" "$@"
