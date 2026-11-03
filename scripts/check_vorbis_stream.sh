#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -O2 -Wall -Wextra -Werror -shared -fPIC \
 -isystem build/vorbis-probe-deps/root/usr/include \
 probe/audio/vorbis_decode_probe.c probe/audio/vorbis_stream.c \
 -Wl,-l:libvorbisfile.so.3 -o build/vorbis-probe-deps/stream-probe.so
cc -O2 -Wall -Wextra -Werror -isystem build/vorbis-probe-deps/root/usr/include \
 probe/audio/vorbis_stream.c probe/audio/vorbis_stream_selftest.c \
 -Wl,-l:libvorbisfile.so.3 -o build/vorbis-probe-deps/stream-selftest
build/vorbis-probe-deps/stream-selftest docs/evidence/audio-cpu-20261102
cc -O2 -Wall -Wextra -Wno-misleading-indentation \
 -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
 probe/audio/guest_stream_selftest.c -ldl -o build/vorbis-probe-deps/guest-stream-selftest
build/vorbis-probe-deps/guest-stream-selftest "$PWD/build/vorbis-probe-deps/stream-probe.so" \
 docs/evidence/audio-cpu-20261102/decode-0001.ogg
