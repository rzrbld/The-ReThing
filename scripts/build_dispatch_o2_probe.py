#!/usr/bin/env python3
"""Isolated optimization-level experiment; restores normal artifacts by hash."""
import hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/dispatch-o2-probe';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=[root/p for p in ['third_party/pcrecomp/runtime/win32hle/host_lite.c','build/game/host/hle_host_lite.o','build/game/host/hle_host_lite.flags','build/game/thething.host','build/game/thething.host.sha256']]
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 source=paths[0];source.write_text('#pragma GCC optimize ("O2")\n'+source.read_text())
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
evidence=root/'docs/evidence/dispatch-o2-20261102';evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('Dispatch O2 candidate built; source and normal artifacts restored')
