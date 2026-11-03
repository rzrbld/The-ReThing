#!/usr/bin/env python3
"""Build isolated count-only host diagnostic, restoring all baseline artifacts."""
import hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/cadence-callers-20261102'
backup=out/'backup';backup.mkdir(exist_ok=True)
patches=json.loads((out/'patches.json').read_text());units={}
for p in patches:units.setdefault(root/p['path'],[]).append(p)
subprocess.run(['gcc','-O2','-c',str(out/'counter.c'),'-o',str(out/'counter.o')],check=True)
paths=[]
for p in units:paths +=[p,root/'build/game/host/thething'/(p.stem+'.o'),root/'build/game/host/thething'/(p.stem+'.flags')]
native=root/'build/render-thread-src/runtime/d3d8.c'
paths +=[native,root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
paths+=sorted(p for p in (root/'build/game/host').iterdir()if p.is_file())
hashes={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));hashes[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
(backup/'manifest.json').write_text(json.dumps(dict(files=[str(p.relative_to(root))for p in paths],hashes=hashes),indent=2)+'\n')
try:
 for p,items in units.items():
  s=p.read_text()
  for item in items:
   assert s.count(item['original'])==1,item['name'];s=s.replace(item['original'],item['replacement'],1)
  s=s.replace('#include "functions.h"','#include "functions.h"\nextern uint64_t thing_cadence_counts[];\nextern void thing_cadence_call(unsigned,uint32_t);',1);p.write_text(s)
 s=native.read_text();hook='double elapsed=(now-profile_start)/(double)freq;'
 assert s.count(hook)==1
 s=s.replace('static uint64_t profile_start,profile_last;', 'extern void thing_cadence_frame(unsigned,double);\nstatic uint64_t profile_start,profile_last;',1)
 native.write_text(s.replace(hook,hook+'thing_cadence_frame(frames,elapsed);',1))
 with (out/'build.log').open('w')as log:
  subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4','--extra-object',str(out/'counter.o')],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p.relative_to(root))]
e=root/'docs/evidence/cadence-callers-20261102';e.mkdir(exist_ok=True)
(e/'build.json').write_text(json.dumps(dict(restored=hashes,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('Built count-only diagnostic; all normal files restored')
