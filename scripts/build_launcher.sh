#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/launcher
case ${1:-host} in
host)
 cc -O2 -c launcher/thething_launcher.c $(pkg-config --cflags sdl2) -o build/launcher/ui.host.o
 c++ -std=c++17 -O2 launcher/native_setup.cpp build/launcher/ui.host.o $(pkg-config --libs sdl2) -o build/launcher/thething-launcher.host ;;

aarch64)
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build/launcher:/out:rw" --workdir /src \
  "${CROSS_IMAGE:-rething-cross:bookworm}" \
  sh -ec 'aarch64-linux-gnu-gcc -O2 -mcpu=cortex-a53 -I/src/build/sdk-include -I/src/build/sdk-include/SDL2 -I/usr/include/SDL2 \
  -c launcher/thething_launcher.c -o /out/ui.aarch64.o
  aarch64-linux-gnu-g++ -std=c++17 -O2 -mcpu=cortex-a53 -static-libstdc++ -static-libgcc launcher/native_setup.cpp /out/ui.aarch64.o -L/src/build/target-libs -Wl,--allow-shlib-undefined -lSDL2 -o /out/thething-launcher.aarch64' ;;
*) exit 2 ;;
esac
