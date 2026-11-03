#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/memmove-check
# Snapshot the checked recovered bodies; never modify final game build output.
cp build/game/recovered.c build/memmove-check/recovered.c
includes=(-Iruntime -Iprobe -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle)
flags=(-std=gnu11 -O1 -g -fno-strict-aliasing -fwrapv -Werror=implicit-function-declaration -DRECOMP_FP_STACK_EXACT=1 -DDS_HEAP_START=0x40000000u)
cc "${flags[@]}" "${includes[@]}" -c build/memmove-check/recovered.c -o build/memmove-check/recovered.o
cc "${flags[@]}" "${includes[@]}" -c runtime/memmove_selftest.c -o build/memmove-check/main.o
objects=()
for object in build/game/host/*.o; do
  case ${object##*/} in native_game_main.o|native_recovered.o|*_selftest.o) continue;; esac
  objects+=("$object")
done
archives=(build/game/host/*/lifted.a)
read -r -a libraries <<< "$(pkg-config --libs sdl2 glesv2)"
cc -g -rdynamic -o build/memmove-check/check \
  build/memmove-check/main.o build/memmove-check/recovered.o "${objects[@]}" \
  -Wl,--start-group "${archives[@]}" -Wl,--end-group \
  -L"$PWD/build/host-sdk/usr/lib/x86_64-linux-gnu" -Wl,-rpath,"$PWD/build/host-sdk/usr/lib/x86_64-linux-gnu" \
  -lSDL2_ttf "${libraries[@]}" -lm -lpthread -ldl
sha256sum build/memmove-check/recovered.c build/memmove-check/check > build/memmove-check/SHA256SUMS
THING_ROOT=${THING_ROOT:-"$PWD/build/active"} build/memmove-check/check "${1:---full}"
