#!/usr/bin/env bash
# Reproducible release-candidate build: translated guest code at -O2 with the
# RECOMP_LOCAL_REGS register model, plus the runtime/translation performance
# fixes. The resulting binaries are diagnostic candidates, not device-qualified.
#
# Usage:
#   bash scripts/build_release.sh host
#   bash scripts/build_release.sh aarch64
#   bash scripts/build_release.sh all          # host + aarch64
#   bash scripts/build_release.sh package      # aarch64 + local candidate tarball
#
# JOBS overrides the compile parallelism (default 8). CROSS_IMAGE selects the
# pinned AArch64 container (see README.md).
set -euo pipefail
cd "$(dirname "$0")/.."
jobs=${JOBS:-8}
target=${1:-all}

build() {
  local t=$1
  echo "=== build $t: -O2 translated code + RECOMP_LOCAL_REGS (fail-closed staging)"
  bash scripts/build_game.sh "$t" --allow-traps --local-regs --translated-opt=-O2 --jobs "$jobs"
}

check_host() {
  THING_ROOT="$PWD/build/active" build/game/thething.host --check-imports
}

check_aarch64() {
  local bin=build/game/thething.aarch64
  readelf -h "$bin" | grep -E 'Class|Data|Type|Machine|Entry'
  readelf -d "$bin" | grep -E 'NEEDED'
  readelf -V "$bin" | grep -o 'GLIBC_2\.[0-9]*' | sort -u | tail -1
  THING_ROOT="$PWD/build/active" bash scripts/run_qemu_aarch64.sh --check-imports
}

case "$target" in
  host)    build host; check_host ;;
  aarch64) build aarch64; check_aarch64 ;;
  all)     build host; check_host; build aarch64; check_aarch64 ;;
  package) build aarch64; check_aarch64; bash scripts/package_candidate.sh ;;
  *) echo 'usage: build_release.sh host|aarch64|all|package' >&2; exit 2 ;;
esac

echo "=== done. Host: $(cat build/game/thething.host.sha256 2>/dev/null || true)"
echo "=== done. ARM:  $(cat build/game/thething.aarch64.sha256 2>/dev/null || true)"
