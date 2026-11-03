#!/usr/bin/env python3
"""Build the tested five-leaf x87-top candidate; preserve baseline ARM files."""
import hashlib,json,os,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/fp-local-top-arm-20261102';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
patches=json.loads((root/'build/fp-local-top-20261102/patches.json').read_text())
units={}
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text(); selected=[x for x in patches if x['original'] in s]
 if selected: units[p]=selected
assert sum(map(len,units.values()))>5
paths=[]
for p in units: paths += [p,root/'build/game/aarch64/thething'/(p.stem+'.o'),root/'build/game/aarch64/thething'/(p.stem+'.flags')]
paths += [root/p for p in ['build/game/aarch64/thething/lifted.a','build/game/thething.aarch64','build/game/thething.aarch64.sha256','build/game/aarch64/elf-header.txt','build/game/aarch64/dynamic.txt','build/game/aarch64/link.log']]
manifest={}
for i,p in enumerate(paths):
 shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
assert manifest['build/game/thething.aarch64']=='fd314856d177c4ad616d74fee89a6978557c7a122ee99d91d6d486103fa19bba'
try:
 for p,selected in units.items():
  s=p.read_text()
  for x in selected:
   assert s.count(x['original'])==1;s=s.replace(x['original'],x['candidate'],1)
  assert s.count('#include "functions.h"')==1
  s=s.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/fp_local_top/local_top.h"',1);p.write_text(s)
 command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49','python3','-u','scripts/build_game.py','--target','aarch64','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4']
 with (out/'build.log').open('w') as log: subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.aarch64',out/'thething-fp-local-top.debug.aarch64')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
result=dict(restored=manifest,candidate_sha256=hashlib.sha256((out/'thething-fp-local-top.debug.aarch64').read_bytes()).hexdigest())
(out/'build-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
print('ARM leaf-top candidate built; baseline restored by hash')
