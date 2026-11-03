#!/usr/bin/env python3
"""Instrument original PVS returns and all aliased renderer gate sites; restore build."""
import hashlib,json,re,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/pvs-probe-20261103';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=[]
for unit in ('0165','0186'):
 paths += [root/f'build/translated-local-regs/thething/functions_{unit}.c',root/f'build/game/host/thething/functions_{unit}.o',root/f'build/game/host/thething/functions_{unit}.flags']
paths += [root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
paths += sorted(p for p in (root/'build/game/host').iterdir()if p.is_file())
paths=list(dict.fromkeys(paths));manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 for unit in ('0165','0186'):
  p=root/f'build/translated-local-regs/thething/functions_{unit}.c';text=p.read_text()
  inc=('#define THING_DEFINE_PVS_COUNTS\n'if unit=='0186'else'')+'#include "'+str(root/'probe/visibility/pvs_counts.h')+'"'
  text=text.replace('#include "functions.h"','#include "functions.h"\n'+inc,1)
  if unit=='0186':
   m=re.search(r'void recomp_thething_006E8BF0\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S);assert m
   body=m[0].replace('{','{\n    pv_calls++; pv_missing+=(MEM32(g_esp+4)==UINT32_MAX);',1)
   assert body.count('RECOMP_REGS_OUT(); return;')==3
   body=body.replace('RECOMP_REGS_OUT(); return;','pv_pass+=(LO8(eax)!=0); pv_reject+=(LO8(eax)==0); RECOMP_REGS_OUT(); return;')
   text=text[:m.start()]+body+text[m.end():]
  else:
   marker='L_0068B6CE:';gates=text.count(marker);assert gates==13
   text=text.replace(marker,marker+'''
    pv_gate++;
    if(!MEM8(esp+0x13))pv_disabled++;
    else if(MEM8(ebx+0x56)&4)pv_special++;
    else if(MEM32(esp+0x54)==UINT32_MAX)pv_no_cell++;
    else pv_expected++;
''')
  p.write_text(text)
 with (out/'build.log').open('w')as log:
  subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
ev=root/'docs/evidence/pvs-probe-20261103';ev.mkdir(exist_ok=True)
(ev/'build.json').write_text(json.dumps({'restored':manifest,'diagnostic_sha256':hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest(),'gate_sites':gates},indent=2)+'\n')
print('Built diagnostic; sources and baseline artifacts restored by hash')
