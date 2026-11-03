#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
clang="$PWD/build/tools/llvm21/usr/bin/clang-21"
opt="$PWD/build/tools/llvm21/usr/bin/opt-21"
host=build/guest-alias-20261102
arm=build/guest-alias-arm-20261102
evidence=docs/evidence/guest-alias-20261102
mkdir -p "$evidence" "$arm/sysroot/usr"
python3 scripts/prepare_guest_alias_probe_20261102.py
common=(-std=gnu11 -O1 -Xclang -disable-llvm-passes -fno-strict-aliasing -fwrapv -ffp-contract=off -Ithird_party/pcrecomp/runtime/recomp32 -S -emit-llvm)
"$clang" "${common[@]}" "$host/probe.c" -o "$host/raw.ll"
python3 scripts/probe_guest_alias_20261102.py "$host"
"$opt" -passes='verify,default<O2>,verify' -S "$host/annotated.ll" -o "$host/optimized.ll"
"$clang" -O2 -ffp-contract=off "$host/optimized.ll" -lm -o "$host/check"
"$host/check" | tee "$evidence/host-test.log"
image=sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49
if [[ ! -f "$host/arm-headers.tar" ]]; then
 docker run --rm --network none --read-only --user "$(id -u):$(id -g)" -v "$PWD/$host:/out" "$image" tar -C /usr/aarch64-linux-gnu -cf /out/arm-headers.tar include
fi
tar -xf "$host/arm-headers.tar" -C "$arm/sysroot/usr"
"$clang" --target=aarch64-linux-gnu --sysroot="$arm/sysroot" -mcpu=cortex-a53 "${common[@]}" "$host/probe.c" -o "$arm/raw.ll"
python3 scripts/probe_guest_alias_20261102.py "$arm"
"$opt" -passes='verify,default<O2>,verify' -S "$arm/annotated.ll" -o "$arm/optimized.ll"
"$clang" --target=aarch64-linux-gnu -mcpu=cortex-a53 -O2 -ffp-contract=off -c "$arm/optimized.ll" -o "$arm/check.o"
docker run --rm --network none --read-only --tmpfs /tmp --workdir /out --user "$(id -u):$(id -g)" -v "$PWD/$arm:/out" "$image" aarch64-linux-gnu-gcc -static /out/check.o -lm -o /out/check.aarch64
build/tools/qemu-user/usr/bin/qemu-aarch64 "$arm/check.aarch64" | tee "$evidence/arm-test.log"
sha256sum "$host/probe.c" "$host/check" "$arm/check.aarch64" "$host/arm-headers.tar" > "$evidence/tests.sha256"
