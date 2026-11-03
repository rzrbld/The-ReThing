#!/usr/bin/env python3
"""Stage exact entry-count diagnostics, not a performance candidate."""
import collections,hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/cadence-counts-20261102';out.mkdir(exist_ok=True)
d=json.loads((root/'docs/evidence/device-skin-simd-20261102/native-gameplay.json').read_text())
names=[]
for name,n in d['functions']:
 name=name.rsplit('!',1)[-1]
 if re.fullmatch('recomp_thething_[0-9A-F]{8}',name):names.append(name)
 if len(names)==50:break
c=json.loads((root/'docs/evidence/cpu-cadence-20261102/comparison.json').read_text());delta=collections.defaultdict(lambda:[0,0])
for run in c['runs']:
 for f in run['guest_functions']:delta[f['name']][bool(run['cap'])]+=f['self_cpu_ms_per_frame']/2
names+= [n for n,v in sorted(delta.items(),key=lambda p:p[1][1]-p[1][0],reverse=True)[:20]]
names=list(dict.fromkeys(names));patches=[]
for path in sorted((root/'build/translated-local-regs/thething').glob('functions_*.c')):
 s=path.read_text()
 for name in names:
  marker='void '+name+'(void) {'
  if marker not in s:continue
  a=s.index(marker);b=s.find('\nvoid recomp_',a+1)
  body=s[a:b if b>=0 else len(s)];index=names.index(name)
  enter='RECOMP_ENTER(0x'+name.rsplit('_',1)[1]+'u);'
  assert body.count(enter)==1,name
  patches.append(dict(name=name,index=index,path=str(path.relative_to(root)),original=body,replacement=body.replace(enter,enter+f'\n    thing_cadence_counts[{index}]++;',1)))
assert len(patches)==len(names)
(out/'patches.json').write_text(json.dumps(patches,indent=2)+'\n')
(out/'counter.c').write_text('''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
'''+f'uint64_t thing_cadence_counts[{len(names)}];\nstatic uint64_t begin_counts[{len(names)}];\nstatic const char *names[]={{'+','.join(json.dumps(n)for n in names)+'''};
void thing_cadence_frame(unsigned frame,double elapsed){
 static int phase;static unsigned first;static double start;
 if(!phase&&elapsed>=55){memcpy(begin_counts,thing_cadence_counts,sizeof begin_counts);first=frame;start=elapsed;phase=1;}
 if(phase==1&&elapsed>=93){
  const char *root=getenv("THING_ROOT");if(!root)abort();char path[PATH_MAX];
  if(snprintf(path,sizeof path,"%s/cadence-counts.csv",root)>=(int)sizeof path)abort();
  FILE*f=fopen(path,"w");if(!f)abort();
  fputs("function,begin_frame,end_frame,begin_seconds,end_seconds,begin_calls,end_calls,delta_calls\\n",f);
  for(unsigned i=0;i<sizeof names/sizeof names[0];i++)fprintf(f,"%s,%u,%u,%.9f,%.9f,%llu,%llu,%llu\\n",names[i],first,frame,start,elapsed,(unsigned long long)begin_counts[i],(unsigned long long)thing_cadence_counts[i],(unsigned long long)(thing_cadence_counts[i]-begin_counts[i]));
  if(fclose(f))abort();phase=2;
 }
}
''')
(out/'manifest.json').write_text(json.dumps(dict(names=names,note='Entry counts only; no loop-iteration measurement; diagnostic changes execution cost, not a performance candidate'),indent=2)+'\n')
print('Staged',len(names),'routine counters; no game sources modified')
