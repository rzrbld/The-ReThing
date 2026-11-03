#!/usr/bin/env python3
"""Build isolated ordered-alpha batching host; restore normal native artifacts."""
import hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/ordered-alpha-batch-20261102'
backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=[root/'build/render-thread-src/runtime/adjacent_batch.c']
paths+=sorted(p for p in (root/'build/game/host').iterdir()if p.is_file())
paths +=[root/'build/game/thething.host',root/'build/game/thething.host.sha256']
hashes={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));hashes[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 shutil.copy2(out/'adjacent_batch.c',paths[0])
 with (out/'build.log').open('w')as log:
  subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p.relative_to(root))]
e=root/'docs/evidence/ordered-alpha-batch-20261102';e.mkdir(exist_ok=True)
(e/'build.json').write_text(json.dumps(dict(restored=hashes,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('Isolated alpha batch host built; all normal artifacts restored')
