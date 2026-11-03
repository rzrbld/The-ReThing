#!/usr/bin/env python3
"""Instrument original PVS returns and all aliased renderer gate sites; restore build."""
import hashlib,json,re,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/static-mesh-20261103';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=[]
for unit in ('0233',):
 paths += [root/f'build/translated-local-regs/thething/functions_{unit}.c',root/f'build/game/host/thething/functions_{unit}.o',root/f'build/game/host/thething/functions_{unit}.flags']
paths += [root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
paths += sorted(p for p in (root/'build/game/host').iterdir()if p.is_file())
paths=list(dict.fromkeys(paths));manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 for unit in ('0233',):
  p=root/f'build/translated-local-regs/thething/functions_{unit}.c';text=p.read_text()
  text=text.replace('#include "functions.h"','#include "functions.h"\n#include "'+str(root/'probe/visibility/static_mesh.h')+'"',1)
  m=re.search(r'void recomp_thething_007A3CE0\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S);assert m
  body=m[0].replace('{','{\n uint32_t probe_node=g_ecx;\n uint32_t prev_node=thing_probe_node,prev_mesh=thing_probe_mesh,prev_lod=thing_probe_lod,prev_count=thing_probe_lod_count;',1)
  assert body.count('L_007A3D76:')==1
  body=body.replace('L_007A3D76:','L_007A3D76:\n static_mesh_note(probe_node,eax,MEM32(ebp+0x10));')
  body=body.replace('RECOMP_REGS_OUT(); return;','thing_probe_node=prev_node;thing_probe_mesh=prev_mesh;thing_probe_lod=prev_lod;thing_probe_lod_count=prev_count; RECOMP_REGS_OUT(); return;')
  text=text[:m.start()]+body+text[m.end():];gates=1
  p.write_text(text)
 with (out/'build.log').open('w')as log:
  subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
ev=root/'docs/evidence/static-mesh-20261103';ev.mkdir(exist_ok=True)
(ev/'build.json').write_text(json.dumps({'restored':manifest,'diagnostic_sha256':hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest(),'gate_sites':gates},indent=2)+'\n')
print('Built diagnostic; sources and baseline artifacts restored by hash')
