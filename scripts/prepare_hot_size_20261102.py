#!/usr/bin/env python3
"""Extract measured hot routines with existing FP-top fixes for code-size audit."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/hot-size-20261102';out.mkdir(parents=True,exist_ok=True)
evidence=root/'docs/evidence/hot-size-20261102';evidence.mkdir(parents=True,exist_ok=True)
profile=root/'docs/evidence/device-batch-ab-20261102/20261102-162503-cpucombo-batch0-cache0-scan0-14554/native-60-150.json'
d=json.loads(profile.read_text())
names=[]
for key,count in d['functions']:
 name=key.rsplit('!',1)[-1]
 if re.fullmatch(r'recomp_thething_[0-9A-F]{8}',name):names.append((name,count))
 if len(names)==40:break
assert len(names)==40
wanted=dict(names);found={};sources={}
for p in sorted((root/'build/translated-local-regs/thething').glob('functions_*.c')):
 text=p.read_text()
 for m in re.finditer(r'void (recomp_thething_[0-9A-F]{8})\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S):
  if m[1] in wanted:found[m[1]]=m[0];sources[m[1]]=str(p.relative_to(root))
assert set(found)==set(wanted)
patches=json.loads((root/'build/fp-local-top-20261102/patches.json').read_text())
changed=[]
for patch in patches:
 name='recomp_thething_'+patch['address']
 if name in found:
  assert found[name]==patch['original'],name
  found[name]=patch['candidate'];changed.append(name)
preamble='''/* Derived proprietary translated code: keep local, do not distribute. */
#define RECOMP_GENERATED_CODE
#include "functions.h"
#include "../../probe/fp_local_top/local_top.h"
'''
(out/'hot.c').write_text(preamble+'\n'.join(found[name] for name,_ in names))
# Compile intact original translation units: aggregation would change inlining
# opportunities and would not represent the actual game build.
unit_manifest=[]
for source in sorted(set(sources.values())):
 text=(root/source).read_text()
 for patch in patches:
  if patch['original'] in text:
   text=text.replace(patch['original'],patch['candidate'],1)
 text=text.replace('#include "functions.h"', '#include "functions.h"\n#include "local_top.h"',1)
 selected=[name for name in wanted if sources[name]==source]
 for mode in ('baseline','candidate'):
  directory=out/mode;directory.mkdir(exist_ok=True)
  content=text
  if mode=='candidate':
   for name in selected:
    marker='void '+name+'(void) {'
    assert content.count(marker)==1
    content=content.replace(marker,'__attribute__((optimize("Os")))\n'+marker,1)
  (directory/Path(source).name).write_text(content)
 unit_manifest.append({'source':source,'selected':selected})
manifest={'profile':str(profile.relative_to(root)),'profile_sha256':hashlib.sha256(profile.read_bytes()).hexdigest(),
 'profile_total_samples':d['total_interval_samples'],
 'selected_samples':sum(wanted.values()),'units':unit_manifest,'fp_top_preserved':changed,
 'routines':[{'name':n,'samples':v,'source':sources[n],
              'body_sha256':hashlib.sha256(found[n].encode()).hexdigest()} for n,v in names],
 'source_sha256':hashlib.sha256((out/'hot.c').read_bytes()).hexdigest()}
(evidence/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Selected',len(names),'routines;',sum(wanted.values()),'of',d['total_interval_samples'],'samples;',len(changed),'existing FP-top substitutions preserved')
