#!/usr/bin/env python3
"""Temporarily instrument one translated entry, build, then restore production files."""
import hashlib,json,pathlib,shutil,subprocess
p=pathlib.Path(__file__).resolve().parents[1];b=p/'build/matrix-reuse';backup=b/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=['build/translated-local-regs/thething/functions_0120.c','build/game/host/thething/functions_0120.o','build/game/host/thething/functions_0120.flags','build/game/host/thething/lifted.a','build/game/thething.host','build/game/thething.host.sha256']
helper=p/'build/game/recovered_matrix_reuse_probe.c';assert not helper.exists()
manifest={}
for i,n in enumerate(paths):
 src=p/n;shutil.copy2(src,backup/str(i));manifest[n]=hashlib.sha256(src.read_bytes()).hexdigest()
try:
 src=p/paths[0];s=src.read_text();needle='    fp_push(*(float*)&MEM32(eax + 0x20)); /* 0x005B7F84: fld dword ptr [eax + 0x20] */';assert s.count(needle)==1
 s=s.replace('#include "functions.h"','#include "functions.h"\nextern void thing_matrix_reuse_probe(uint32_t,uint32_t,uint32_t);',1).replace(needle,'    thing_matrix_reuse_probe(eax,edx,ecx);\n'+needle);src.write_text(s)
 shutil.copy2(b/'helper.c',helper)
 with (b/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=p,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(p/'build/game/thething.host',b/'diagnostic.host')
finally:
 for i,n in enumerate(paths):shutil.copy2(backup/str(i),p/n)
 helper.unlink(missing_ok=True)
for n,h in manifest.items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
(p/'docs/evidence/matrix-reuse/build-manifest.json').write_text(json.dumps({'restored_files':manifest,'diagnostic_sha256':hashlib.sha256((b/'diagnostic.host').read_bytes()).hexdigest()},indent=2)+'\n')
print('Diagnostic built; production source/object/archive/executable hashes restored')
