#!/usr/bin/env python3
"""Instrument selected original functions; restore normal build by hash."""
import re,hashlib,json,shutil,subprocess,sys
from pathlib import Path
variant='cpu-sections-outer-probe' if '--outer-only' in sys.argv else 'cpu-sections-probe'
root=Path(__file__).resolve().parents[1];out=root/'build'/variant;backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
roots={'0072BB30':'sphere','0072B730':'cone','0072A890':'line','0070C1A0':'spatial_query','0072EDB0':'particle_tree','0063C590':'ai_brain','0075DC80':'render_75dc80','00759C20':'render_759c20','0076FC10':'render_76fc10','007AE710':'skin','005B7F80':'matrix'}
if '--outer-only' in sys.argv:
 for a in ('00759C20','0076FC10','007AE710','005B7F80'):roots.pop(a)
units={}
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 text=p.read_text();found=[a for a in roots if 'void recomp_thething_'+a+'(void) {' in text]
 if found:units[p]=found
assert sum(map(len,units.values()))==len(roots)
paths=[]
for p in units:paths += [p,root/'build/game/host/thething'/(p.stem+'.o'),root/'build/game/host/thething'/(p.stem+'.flags')]
paths += [root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 for p,addresses in units.items():
  text=p.read_text().replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/cpu_sections/timing.h"',1)
  for index,a in enumerate(addresses):
   match=re.search(r'void recomp_thething_'+a+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S);assert match
   body=match.group();body=body.replace('{','{\n    SectionTime _thing_scope_clock=section_begin('+str(index)+',"'+roots[a]+'_'+a+'");',1)
   marker='RECOMP_REGS_OUT(); return;';assert marker in body
   body=body.replace(marker,'section_end(_thing_scope_clock); '+marker)
   end='    RECOMP_REGS_OUT(); /* checked fallthrough flush */';assert body.count(end)==1
   body=body.replace(end,'    section_end(_thing_scope_clock);\n'+end)
   text=text[:match.start()]+body+text[match.end():]
  p.write_text(text)
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
evidence=root/'docs/evidence'/('cpu-sections-outer-20261102' if '--outer-only' in sys.argv else 'cpu-sections-20261102');evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(roots=roots,restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('CPU section diagnostic built; original artifacts restored by hash')
