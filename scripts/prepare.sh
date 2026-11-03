#!/usr/bin/env bash
# Original ISO -> official 1.1 -> official 1.2 -> supplied clear 1.2 executable.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tools docs/evidence
ensure_tool() {
  local name=$1 package=$2 library=$3
  if [[ ! -x "build/tools/$name/usr/bin/$name" ]]; then
    (cd build/tools && apt-get download "$package" "$library")
    case "$name" in
      unshield)
        printf '%s\n' \
          'dd3d3a80e09f26e8f685981f5da7d7822e465542011a126caa0843c70f5b9830  build/tools/libunshield1_1.6.2-2_amd64.deb' \
          'c9f63cbc9ae86896eaff7dab5fbd3fb87dc4286d6e1333f6cdcb7d93f17d6104  build/tools/unshield_1.6.2-2_amd64.deb' | sha256sum -c -
        ;;
      xdelta)
        printf '%s\n' \
          '99133aec9c8c716f1109b7883d51dbf253aa5384b215584cea8cfd56de47467d  build/tools/libxdelta2t64_1.1.3-10.8_amd64.deb' \
          '12367bade0b7452d22aec4fb818f7e1f44c97f0aaf4565089a0ec66cfe3cbdde  build/tools/xdelta_1.1.3-10.8_amd64.deb' | sha256sum -c -
        ;;
    esac
    for specification in "$package" "$library"; do
      dpkg-deb -x "build/tools/${specification%%=*}_${specification#*=}_amd64.deb" "build/tools/$name"
    done
  fi
}
ensure_tool unshield unshield=1.6.2-2 libunshield1=1.6.2-2
ensure_tool xdelta xdelta=1.1.3-10.8 libxdelta2t64=1.1.3-10.8
python3 scripts/prepare_v12.py
