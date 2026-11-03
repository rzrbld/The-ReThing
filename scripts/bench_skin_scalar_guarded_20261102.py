#!/usr/bin/env python3
import json,re,statistics,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=[]
for v in (0,1,1,0):
 s=subprocess.check_output([str(root/'build/skin-scalar-guarded-20261102/check-host'),str(v),'2000000'],text=True).strip();print(s)
 rows.append(dict(variant=v,ns=float(re.search(r'\bns=([0-9.]+)',s)[1]),output=s))
a=statistics.mean(r['ns']for r in rows if r['variant']==0);b=statistics.mean(r['ns']for r in rows if r['variant']==1)
d=dict(runs=rows,baseline_ns=a,candidate_ns=b,change_percent=100*(b/a-1),note='Finite block microbenchmark including guard overhead. Not full-game or H700 performance. Candidate counters included.')
(root/'docs/evidence/skin-scalar-guarded-20261102/microbench.json').write_text(json.dumps(d,indent=2)+'\n');print(d['change_percent'])
