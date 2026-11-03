#!/usr/bin/env python3
"""Guarded scalarized skin block; restore baseline."""
import hashlib,json,shutil,subprocess,sys,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
target='aarch64' if '--target=aarch64' in sys.argv else 'host'
variant='skin-scalar-guarded-arm-game-20261102' if target=='aarch64' else 'skin-scalar-guarded-game-20261102'
out=root/'build'/variant;backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
unit=root/'build/translated-local-regs/thething/functions_0235.c'
paths=[unit,root/'build/game'/target/'thething/functions_0235.o',root/'build/game'/target/'thething/functions_0235.flags',root/'build/game'/target/'thething/lifted.a',root/'build/game'/('thething.'+target),root/'build/game'/('thething.'+target+'.sha256')]
paths+=sorted(p for p in (root/'build/game'/target).iterdir()if p.is_file())
hashes={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));hashes[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 s=unit.read_text()
 original=(root/'build/skin-fused-20261102/block.txt').read_text()
 scalar=(root/'build/skin-scalar-block-20261102/body.txt').read_text()
 a=s.index('void recomp_thething_007AE710(void) {');b=s.index('\nvoid recomp_',a+1)
 body=s[a:b]
 assert body.count(original)==1
 replacement='if(thing_skin_scalar_eligible(eax,esi,esp,ebp)){\n'+scalar+'\n}else{\n'+original+'\n}\n'
 s=s[:a]+body.replace(original,replacement,1)+s[b:]
 s=s.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/skin_scalar_guard.h"',1)
 unit.write_text(s)
 command=['python3','-u','scripts/build_game.py','--target',target,'--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4']
 if target=='aarch64':command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
 with (out/'build.log').open('w')as log:
  subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game'/('thething.'+target),out/('diagnostic.'+target))
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p.relative_to(root))]
e=root/'docs/evidence'/variant;e.mkdir(exist_ok=True)
(e/'build.json').write_text(json.dumps(dict(restored=hashes,candidate_sha256=hashlib.sha256((out/('diagnostic.'+target)).read_bytes()).hexdigest()),indent=2)+'\n')
print('Guarded scalar skin candidate built; baseline sources and artifacts restored')
