#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/lighting-delivery/{the-thing,Ports} dist
python3 - <<'PY'
from pathlib import Path
import shutil
out=Path('build/lighting-delivery')
shutil.copy2('build/game/thething.aarch64',out/'the-thing/thething.aarch64')
shutil.copy2('build/launcher/thething-launcher.aarch64',out/'the-thing/thething-launcher')
for n in ['the-thing-env.sh','cpu-session.sh','muos-session.sh']:shutil.copy2(Path('packaging')/n,out/'the-thing'/n)
for n in ['The Thing.sh','The ReThing.sh','The Thing Measure.sh','The Thing Measure Cache.sh']:shutil.copy2(Path('packaging')/n,out/'Ports'/n)
for p in (out/'Ports').iterdir():p.chmod(0o755)
for p in (out/'the-thing').iterdir():p.chmod(0o755)
shutil.copy2('docs/LIGHTING_UPDATE_INSTALL.txt',out/'READ-ME.txt')
shutil.copy2('docs/LIGHTING_FIX_2026-11-01.txt',out/'TECHNICAL-NOTES.txt')
PY
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD/build/lighting-delivery:/out:rw" \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 \
 aarch64-linux-gnu-strip --strip-unneeded /out/the-thing/thething.aarch64 /out/the-thing/thething-launcher
python3 - <<'PY'
from pathlib import Path
import hashlib,json,zipfile
out=Path('build/lighting-delivery');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest={'purpose':'Save-slot recovery, scaled presentation, restored light IDs501–511, active-light lookup optimization; brightness overrides removed',
 'arm_unstripped_sha256':sha(Path('build/game/thething.aarch64')),'host_sha256':sha(Path('build/game/thething.host')),
 'files':{str(p.relative_to(out)):sha(p) for p in sorted(out.rglob('*')) if p.is_file() and p.name!='MANIFEST.json'},
 'hardware_validation':'Pending; no stable30FPS claim','local_validation':'Original save callback/restart-load; missing-light correction; CPU/GPU and scaling regression checks'}
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
path=Path('dist')/f'the-thing-save-light-scale-20261101-{sha(out/"the-thing/thething.aarch64")[:8]}-update.zip'
with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(out.rglob('*')):
  if p.is_file():z.write(p,p.relative_to(out))
with zipfile.ZipFile(path) as z:assert z.testzip() is None
print(path);print('sha256',sha(path))
PY
