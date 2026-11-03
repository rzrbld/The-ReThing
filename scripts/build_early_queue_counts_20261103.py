#!/usr/bin/env python3
import re,hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/early-queue-counts-20261103';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
roots={'00794310':'scope','0075C850':'queue','00758AA0':'present'};units={}
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text();found=[a for a in roots if 'void recomp_thething_'+a+'(void) {' in s]
 if found:units[p]=found
assert sum(map(len,units.values()))==3
paths=[]
for p in units:paths +=[p,root/'build/game/host/thething'/(p.stem+'.o'),root/'build/game/host/thething'/(p.stem+'.flags')]
paths +=[root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
stage=root/'build/render-thread-src/runtime/d3d8.c';paths +=[stage]+sorted(p for p in (root/'build/game/host').iterdir() if p.is_file());paths=list(dict.fromkeys(paths));manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 s=stage.read_text();needle='static D8State state;';assert s.count(needle)==1
 s=s.replace(needle,needle+'\n#include "../../../probe/visibility/early_queue_counts_renderer.h"')
 stage.write_text(s)
 for i,(p,addresses) in enumerate(units.items()):
  s=p.read_text().replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/visibility/early_queue_bounds.h"',1)
  for a in addresses:
   m=re.search(r'void recomp_thething_'+a+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',s,re.S);assert m;b=m.group()
   if roots[a]=='present':
    b=b.replace('{','{\n    extern void thing_queue_bounds_report(void);thing_queue_bounds_report();',1)
   elif roots[a]=='scope':
    b=b.replace('{','{\n    unsigned _bounds_old_scope=thing_terrain_scope;thing_terrain_scope=1;',1)
    b=b.replace('RECOMP_REGS_OUT(); return;','thing_terrain_scope=_bounds_old_scope; RECOMP_REGS_OUT(); return;')
    b=b.replace('    RECOMP_REGS_OUT(); /* checked fallthrough flush */','    thing_terrain_scope=_bounds_old_scope;\n    RECOMP_REGS_OUT(); /* checked fallthrough flush */')
   else:
    needle='L_0075CABB:\n';assert b.count(needle)==1
    # CABB has the original subset record in ESI and resource in EAX.
    # Only queued, ordinary (flag=0) geometry; preserve all initialization,
    # temporary reference acquisition/release and the original return value.
    hook='    if (thing_terrain_scope && esi && MEM8(eax+0x10)==0 &&\n        thing_queue_bounds_outside((const float*)ADDR(esi+0x1c),\n                                   (const float*)ADDR(MEM32(esp+0x1b0))))\n        goto L_0075CE97;\n'
    b=b.replace(needle,needle+hook)
   s=s[:m.start()]+b+s[m.end():]
  p.write_text(s)
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
e=root/'docs/evidence/early-queue-counts-20261103';e.mkdir(exist_ok=True);(e/'build.json').write_text(json.dumps({'restored':manifest,'diagnostic_sha256':hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()},indent=2)+'\n');shutil.rmtree(backup)
print('Early queue bounds candidate built; baseline restored and rollback copies removed')
