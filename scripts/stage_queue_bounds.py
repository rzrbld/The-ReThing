#!/usr/bin/env python3
"""Stage early queue culling into a caller-backed-up build tree."""
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
roots={'00794310':'scope','0075C850':'queue'};units={}
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text();found=[a for a in roots if 'void recomp_thething_'+a+'(void) {' in s]
 if found:units[p]=found
assert sum(map(len,units.values()))==2
stage=root/'build/render-thread-src/runtime/d3d8.c'
s=stage.read_text();needle='static D8State state;';assert s.count(needle)==1
s=s.replace(needle,needle+'\n#include "../../../probe/visibility/early_queue_bounds_renderer.h"')
stage.write_text(s)
for i,(p,addresses) in enumerate(units.items()):
 s=p.read_text().replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/visibility/early_queue_bounds.h"',1)
 for a in addresses:
  m=re.search(r'void recomp_thething_'+a+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',s,re.S);assert m;b=m.group()
  if roots[a]=='scope':
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
