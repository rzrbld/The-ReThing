#!/usr/bin/env python3
"""Instrument selected original functions; restore normal build by hash."""
import re,hashlib,json,shutil,subprocess,sys
from pathlib import Path
variant='effects-wide-probe-20261102'
root=Path(__file__).resolve().parents[1];out=root/'build'/variant;backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
roots=json.loads((root/'docs/evidence/effects-wide-20261102/roots.json').read_text())
# RECOMP_ITAIL in current header invokes the callee and returns to the generated
# explicit return below; section_end covers it and includes descendant work.
hdr=(root/'third_party/pcrecomp/runtime/recomp32/recomp_types.h').read_text()
itail=hdr.split('#define RECOMP_ITAIL(target_va)',1)[1].split('} while(0)',1)[0]
assert '_fn(); RECOMP_REGS_IN();' in itail and 'return;' not in itail
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
  assert len(addresses)<=16
  for index,a in enumerate(addresses):
   match=re.search(r'void recomp_thething_'+a+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S);assert match
   body=match.group();assert 'RECOMP_TAIL(' not in body,a;body=body.replace('{','{\n    SectionTime _thing_scope_clock=section_begin('+str(index)+',"'+roots[a]+'_'+a+'");',1)
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
evidence=root/'docs/evidence'/variant;evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(roots=roots,restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('CPU section diagnostic built; original artifacts restored by hash')
