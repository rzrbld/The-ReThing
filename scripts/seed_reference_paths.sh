#!/usr/bin/env bash
# Installer registry paths for only the isolated Wine reference prefix.
set -euo pipefail
cd "$(dirname "$0")/.."
unset WAYLAND_DISPLAY
if [[ ${THING_REFERENCE_REGISTRY_SEED:-0} != 1 ]]; then
  exec xvfb-run -a env THING_REFERENCE_REGISTRY_SEED=1 bash scripts/seed_reference_paths.sh
fi
export WINEPREFIX="$PWD/build/wine-v12"
export WINEDEBUG=-all
trap 'WINEPREFIX="$WINEPREFIX" /usr/lib/x86_64-linux-gnu/wine/wineserver -k || true' EXIT
key='HKLM\Software\Computer Artworks\The Thing\1.0'
windows_root=$(python3 - <<'PY'
import os
print('Z:'+os.getcwd().replace('/', '\\')+'\\build\\active')
PY
)
resource="$windows_root\data;$windows_root\data\mainui.pak"
fmv="$windows_root\data\FMV"
level="$windows_root\data\Levels"
if [[ ${THING_REFERENCE_RESOURCES_NO_FMV:-0} == 1 ]]; then
  resource_root="$windows_root\..\reference-resources-no-fmv"
  resource="$resource_root;$resource_root\mainui.pak"
  level="$resource_root\Levels"
  fmv="$resource_root\FMV"
fi
if [[ ${THING_REFERENCE_SKIP_MOVIES:-0} == 1 ]]; then
  fmv="$windows_root\data\__reference_movies_skipped__"
fi
if [[ ${THING_RESOURCE_PACK_LIST:-0} == 1 ]]; then
  resource="$windows_root\data\mainui.pak;$windows_root\data\resources.pak;$windows_root\data\english.pak"
fi
wine reg add "$key" /v ResourceDir /t REG_SZ /d "$resource" /f /reg:32
wine reg add "$key" /v LevelDir /t REG_SZ /d "$level" /f /reg:32
wine reg add "$key" /v FMVDir /t REG_SZ /d "$fmv" /f /reg:32
wine reg query "$key" /reg:32
