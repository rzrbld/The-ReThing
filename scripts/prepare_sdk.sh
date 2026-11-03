#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
case ${1:-host} in
host)
  if [[ ! -f build/host-sdk/usr/include/SDL2/SDL_ttf.h ]]; then
    mkdir -p build/tools
    (cd build/tools && apt-get download libsdl2-ttf-dev=2.24.0+dfsg-3 libsdl2-ttf-2.0-0=2.24.0+dfsg-3)
    printf '%s\n' \
      '5a486796caa798061dd9f8d29b2aa963206d146197e52d6e5d8deb7b571cf495  build/tools/libsdl2-ttf-2.0-0_2.24.0+dfsg-3_amd64.deb' \
      'd14328bf98b1175e8fd43d32d6e8e98255fed53a3d676603234835a31209ab7b  build/tools/libsdl2-ttf-dev_2.24.0+dfsg-3_amd64.deb' | sha256sum -c -
    dpkg-deb -x build/tools/libsdl2-ttf-dev_2.24.0+dfsg-3_amd64.deb build/host-sdk
    dpkg-deb -x build/tools/libsdl2-ttf-2.0-0_2.24.0+dfsg-3_amd64.deb build/host-sdk
  fi
  pkg-config --exists sdl2 glesv2
  ;;
aarch64)
  if [[ ! -f build/target-libs/libSDL2.so || ! -f build/sdk-include/SDL2/SDL_config.h ]]; then
    source_sdk=${DESERT_STORM_SDK:-/home/rzrbld/repos/desert_storm/build}
    test -f "$source_sdk/target-libs/libSDL2.so"
    test -f "$source_sdk/sdk-include/SDL2/SDL_config.h"
    mkdir -p build/target-libs build/sdk-include
    cp -a "$source_sdk/target-libs/." build/target-libs/
    cp -a "$source_sdk/sdk-include/." build/sdk-include/
  fi
  ;;
*) echo 'Usage: prepare_sdk.sh host|aarch64' >&2; exit 2;;
esac
