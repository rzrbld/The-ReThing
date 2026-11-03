#!/usr/bin/env bash
# Recreate the software-rendered AArch64 QEMU diagnostic runtime. It is not Mali.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tools build/arm64-qemu-sysroot build/arm64-qemu-shims
archive='qemu-user_1%3a10.2.1+ds-1ubuntu3.3_amd64.deb'
if [[ ! -f build/tools/$archive ]]; then
  (cd build/tools && apt-get download 'qemu-user=1:10.2.1+ds-1ubuntu3.3')
fi
printf '%s  %s\n' a00c7fdf34c913b799f8d1cc556d3fdd9f9fd695167a17baf86123a9cfaefb22 "build/tools/$archive" | sha256sum -c -
dpkg-deb -x "build/tools/$archive" build/tools/qemu-user
image=${CROSS_IMAGE:-rething-cross:bookworm}
docker run --rm --network host -v "$PWD/build/arm64-qemu-sysroot:/out:rw" "$image" sh -c '
 set -e; export DEBIAN_FRONTEND=noninteractive
 apt-get update > /tmp/update.log 2>&1
 apt-get install -y --no-install-recommends \
  libsdl2-2.0-0:arm64=2.26.5+dfsg-1 libsdl2-ttf-2.0-0:arm64=2.20.1+dfsg-2 \
  libgles2:arm64=1.6.0-1 libegl1:arm64=1.6.0-1 libgl1:arm64=1.6.0-1 \
  libglx-mesa0:arm64=22.3.6-1+deb12u2 libgl1-mesa-dri:arm64=22.3.6-1+deb12u2 > /tmp/install.log 2>&1
 dpkg-query -W > /out/package-manifest.txt
 tar -cf - -C / lib/aarch64-linux-gnu usr/lib/aarch64-linux-gnu usr/share/fonts 2>/dev/null | tar -xf - -C /out
 cp /tmp/update.log /tmp/install.log /out/'
ln -sfn "$PWD/build/arm64-qemu-sysroot/usr/lib/aarch64-linux-gnu/libGLESv2.so.2" build/arm64-qemu-shims/libmali.so.0
