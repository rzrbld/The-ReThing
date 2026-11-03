#!/usr/bin/env python3
"""Isolated selective-call ABI experiment for five audited leaf functions."""
import hashlib,json,shutil,subprocess,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/leaf-calls-probe';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
patches=json.loads((root/'build/leaf-registers-20261102/patches.json').read_text());units={};counts={p['address']:0 for p in patches};definitions=0
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text();original=s;changed=False
 for x in patches:
  if x['original'] in s:
   assert s.count(x['original'])==1;s=s.replace(x['original'],x['candidate'],1);definitions+=1;changed=True
  call='RECOMP_CALL(recomp_thething_'+x['address']+');';n=s.count(call)
  if n:
   # Generated callers access guest GPRs through local aliases, not raw globals.
   assert not re.search(r'\bg_(?:eax|ecx|edx|ebx|esp|esi|edi|ebp)\b',original)
   s=s.replace(call,'THING_LEAF_CALL_'+x['address']+'(recomp_thething_'+x['address']+');');counts[x['address']]+=n;changed=True
 if changed:
  assert s.count('#include "functions.h"')==1
  s=s.replace('#include "functions.h"','#include "functions.h"\n#include "../../leaf-calls-20261102/leaf_calls.h"',1)
  units[p]=s
assert definitions==5 and sum(counts.values())==1739,(definitions,counts)
if '--audit-only' in sys.argv:
 print(json.dumps(dict(definitions=definitions,call_counts=counts,units=len(units)),indent=2));raise SystemExit(0)
paths=[]
for p in units:paths += [p,root/'build/game/host/thething'/(p.stem+'.o'),root/'build/game/host/thething'/(p.stem+'.flags')]
paths += [root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 for p,s in units.items():p.write_text(s)
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
evidence=root/'docs/evidence/leaf-calls-20261102';evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(call_counts=counts,restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('Selective-call candidate built; normal source and artifacts restored')
