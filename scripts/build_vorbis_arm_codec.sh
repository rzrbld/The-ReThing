#!/bin/bash
# Run inside the pinned cross compiler container, /src mounted and build writable.
set -euo pipefail
cd /src
source_root=/src/build/vorbis-arm-sources
out=/src/build/native-vorbis-arm
prefix=$out/prefix
mkdir -p "$out/ogg" "$out/vorbis" "$prefix"
export CC=aarch64-linux-gnu-gcc AR=aarch64-linux-gnu-ar RANLIB=aarch64-linux-gnu-ranlib
export CFLAGS='-O2 -mcpu=cortex-a53 -fPIC -ffp-contract=off'
(cd "$out/ogg" && "$source_root/libogg-1.3.6/configure" --host=aarch64-linux-gnu --prefix="$prefix" --disable-shared --enable-static && make -j4 && make install)
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
(cd "$out/vorbis" && "$source_root/libvorbis-1.3.7/configure" --host=aarch64-linux-gnu --prefix="$prefix" --disable-shared --enable-static --with-ogg="$prefix" && make -j4 && make install)
"$CC" $CFLAGS -shared -I"$prefix/include" probe/audio/vorbis_decode_probe.c probe/audio/vorbis_stream.c \
 "$prefix/lib/libvorbisfile.a" "$prefix/lib/libvorbis.a" "$prefix/lib/libogg.a" \
 -Wl,--exclude-libs,ALL -lm -o "$out/probe.aarch64.so"
"$CC" $CFLAGS -I"$prefix/include" probe/audio/vorbis_decode_probe.c probe/audio/vorbis_stream.c probe/audio/vorbis_corpus.c \
 "$prefix/lib/libvorbisfile.a" "$prefix/lib/libvorbis.a" "$prefix/lib/libogg.a" \
 -lm -o "$out/corpus.aarch64"
cp "$source_root/libogg-1.3.6/COPYING" "$out/LICENSE.ogg"
cp "$source_root/libvorbis-1.3.7/COPYING" "$out/LICENSE.vorbis"
aarch64-linux-gnu-readelf -d "$out/probe.aarch64.so" > "$out/dynamic.txt"
aarch64-linux-gnu-readelf -V "$out/probe.aarch64.so" > "$out/versions.txt"
sha256sum "$out/probe.aarch64.so" "$out/corpus.aarch64" > "$out/SHA256SUMS"
"$CC" $CFLAGS -I"$prefix/include" probe/audio/vorbis_stream.c probe/audio/vorbis_stream_selftest.c \
 "$prefix/lib/libvorbisfile.a" "$prefix/lib/libvorbis.a" "$prefix/lib/libogg.a" \
 -lm -o "$out/stream-selftest.aarch64"
"$CC" $CFLAGS -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
 probe/audio/guest_stream_selftest.c -ldl -o "$out/guest-stream-selftest.aarch64"
