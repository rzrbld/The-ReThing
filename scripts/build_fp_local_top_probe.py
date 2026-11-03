#!/usr/bin/env python3
"""Build isolated audited leaf local-x87-top candidate; restore baseline."""
import hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/fp-local-top-probe';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
patches=json.loads((root/'build/fp-local-top-20261102/patches.json').read_text());units={}
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text();selected=[x for x in patches if x['original'] in s]
 if selected:units[p]=selected
assert sum(map(len,units.values()))==len(patches)>5
paths=[]
for p in units:paths += [p,root/'build/game/host/thething'/(p.stem+'.o'),root/'build/game/host/thething'/(p.stem+'.flags')]
paths += [root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 for p,selected in units.items():
  s=p.read_text()
  for x in selected:
   assert s.count(x['original'])==1;s=s.replace(x['original'],x['candidate'],1)
  s=s.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/fp_local_top/local_top.h"',1)
  p.write_text(s)
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
evidence=root/'docs/evidence/fp-local-top-20261102';evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('Leaf local-top candidate built; normal source and artifacts restored')
