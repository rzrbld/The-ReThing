#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Headers downloaded only; no global installation.
mkdir -p build/vorbis-probe-deps
(cd build/vorbis-probe-deps && apt-get download libvorbis-dev=1.3.7-3build2 libogg-dev=1.3.6-2)
for archive in build/vorbis-probe-deps/*.deb; do
    dpkg-deb -x "$archive" build/vorbis-probe-deps/root
done
cc -O2 -Wall -Wextra -Werror -shared -fPIC \
    -isystem build/vorbis-probe-deps/root/usr/include probe/audio/vorbis_decode_probe.c probe/audio/vorbis_stream.c \
    -Wl,-l:libvorbisfile.so.3 -o build/vorbis-probe-deps/probe.so
sha256sum build/vorbis-probe-deps/*.deb build/vorbis-probe-deps/probe.so
