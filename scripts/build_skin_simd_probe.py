#!/usr/bin/env python3
"""Build an isolated inline skin SIMD candidate, restoring production by hash."""
import hashlib,json,pathlib,shutil,subprocess,sys
p=pathlib.Path(__file__).resolve().parents[1];variant='skin-simd-inline' if '--inline-guards' in sys.argv else 'skin-simd';b=p/'build'/variant;backup=b/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=['build/translated-local-regs/thething/functions_0235.c','build/game/host/thething/functions_0235.o','build/game/host/thething/functions_0235.flags','build/game/host/thething/lifted.a','build/game/thething.host','build/game/thething.host.sha256']
manifest={}
for i,n in enumerate(paths):
 src=p/n;shutil.copy2(src,backup/str(i));manifest[n]=hashlib.sha256(src.read_bytes()).hexdigest()
try:
 src=p/paths[0];s=src.read_text();s=s.replace('#include "functions.h"',f'#include "functions.h"\n#include "../../{variant}/inline.h"',1)
 for i,patch in enumerate(json.loads((b/'patches.json').read_text())):
  old=patch['original'];assert s.count(old)==14, 'expected14 lifted entry-point copies'
  label=f'L_SKIN_SIMD_DONE_{i}'
  replacement='    if(thing_skin_simd_enabled()&&'+patch['condition']+') {\n'+patch['body']+'\n        goto '+label+';\n    }\n'+old+label+':;\n'
  s=s.replace(old,replacement)
 src.write_text(s)
 with (b/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=p,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(p/'build/game/thething.host',b/'diagnostic.host')
finally:
 for i,n in enumerate(paths):shutil.copy2(backup/str(i),p/n)
for n,h in manifest.items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
(p/'docs/evidence'/variant/'build-manifest.json').write_text(json.dumps({'restored_files':manifest,'diagnostic_sha256':hashlib.sha256((b/'diagnostic.host').read_bytes()).hexdigest()},indent=2)+'\n')
print('Skin SIMD experiment built; normal source/object/archive/executable restored by hash')
