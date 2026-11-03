#!/usr/bin/env python3
"""Inline original SetRenderState wrapper at material setup sites; restore baseline."""
import hashlib,json,shutil,subprocess,sys,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
target='aarch64' if '--target=aarch64' in sys.argv else 'host'
variant='material-inline-arm-game-20261102' if target=='aarch64' else 'material-inline-game-20261102'
out=root/'build'/variant;backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
unit=root/'build/translated-local-regs/thething/functions_0221.c'
paths=[unit,root/'build/game'/target/'thething/functions_0221.o',root/'build/game'/target/'thething/functions_0221.flags',root/'build/game'/target/'thething/lifted.a',root/'build/game'/('thething.'+target),root/'build/game'/('thething.'+target+'.sha256')]
paths+=sorted(p for p in (root/'build/game'/target).iterdir()if p.is_file())
hashes={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));hashes[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 s=unit.read_text();a=s.index('void recomp_thething_0076FC10(void) {');b=s.index('\nvoid recomp_',a+1);body=s[a:b]
 helper=(root/'build/material-inline-20261102/candidate.c').read_text().split('#include "recomp_types.h"\n',1)[1].split('void candidate2(void)',1)[0]
 helper=helper.replace('candidate_helper','thing_material_inline_00759C20')
 old='RECOMP_CALL(recomp_thething_00759C20)';new='RECOMP_CALL(thing_material_inline_00759C20)'
 assert body.count(old)==23,body.count(old)
 body=body.replace(old,new)
 s=s[:a]+body+s[b:];s=s.replace('#include "functions.h"','#include "functions.h"\n'+helper,1);unit.write_text(s)
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
print('Material inline candidate built; baseline sources and artifacts restored')
