#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/controller-edges
cflags=(-std=gnu11 -O1 -g -Werror=implicit-function-declaration -Iruntime -Iprobe
 -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle)
read -r -a sdl_cflags <<< "$(pkg-config --cflags sdl2)"
read -r -a sdl_libs <<< "$(pkg-config --libs sdl2)"
for name in controller_edges controller_keyboard dinput8_buffer dinput8; do
 cc "${cflags[@]}" "${sdl_cflags[@]}" "probe/${name}_selftest.c" runtime/dinput8.c \
  runtime/test_input.c probe/desert_pad.c -o "build/controller-edges/$name" "${sdl_libs[@]}" -lm
done
build/controller-edges/controller_edges
build/controller-edges/controller_keyboard
build/controller-edges/dinput8_buffer
build/controller-edges/dinput8_buffer --commands
THING_PAD_MODE=native build/controller-edges/dinput8
THING_PAD_MODE=native build/controller-edges/dinput8 --commands
