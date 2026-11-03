#!/usr/bin/env bash
# Full fresh-checkout build; the only game input is the user's supported ISO.
set -euo pipefail
unset PYTHONOPTIMIZE
cd "$(dirname "$0")/.."
if [[ $# != 2 ]]; then
  echo 'Usage: bash scripts/build_distribution.sh /path/to/the_thing.iso /path/to/output.zip' >&2
  exit 2
fi
iso=$(realpath "$1")
output=$(realpath -m "$2")
[[ -f "$iso" ]] || { echo 'ISO not found' >&2; exit 1; }
[[ ! -e "$output" ]] || { echo 'Output exists; choose a new filename' >&2; exit 1; }
[[ ! -e build/translated && ! -e build/game ]] || {
  echo 'Use a fresh checkout for the full build. See BUILDING.md for individual stages.' >&2; exit 1;
}
for tool in docker python3 cc c++ pkg-config dpkg-deb readelf; do
  command -v "$tool" >/dev/null || { echo "Missing prerequisite: $tool" >&2; exit 1; }
done
pkg-config --exists sdl2
export CROSS_IMAGE=${CROSS_IMAGE:-rething-cross:bookworm}
export JOBS=${JOBS:-4}
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || { echo 'JOBS must be a positive integer' >&2; exit 1; }
mkdir -p build docs/evidence
python3 -m venv .venv
.venv/bin/pip install -r requirements-audit.txt
export PATH="$PWD/.venv/bin:$PATH"
bash scripts/prepare_cross.sh
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build:/src/build:rw" -v "$PWD/docs/evidence:/src/docs/evidence:rw" \
  -v "$iso:/input/game.iso:ro" -e THING_ISO=/input/game.iso -e THING_SYSTEM_TOOLS=1 \
  --workdir /src "$CROSS_IMAGE" python3 scripts/prepare_v12.py
python3 -u scripts/translate_game.py --all-modules
bash scripts/build_game.sh aarch64 --allow-traps --local-regs --translated-opt=-O2 --render-thread --jobs "$JOBS"
python3 scripts/build_optimized_game.py --target=aarch64
bash scripts/strip_optimized_game.sh
bash scripts/build_launcher.sh aarch64
bash scripts/build_launcher.sh host
python3 scripts/fetch_audio_sources.py
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build:/src/build:rw" --workdir /src \
  "$CROSS_IMAGE" bash scripts/build_vorbis_arm_codec.sh
python3 scripts/prepare_installer_tools.py
python3 scripts/check_launcher_settings.py
python3 scripts/check_portmaster_launch.py
python3 scripts/package_portmaster.py \
  --game build/early-queue-combined-aarch64-20261103/thething-queue-bounds.aarch64 --output "$output"
python3 scripts/check_portmaster_package.py --archive "$output"
echo "Built and checked $output. Hardware gameplay still requires testing."
