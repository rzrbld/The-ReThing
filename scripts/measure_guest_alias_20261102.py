#!/usr/bin/env python3
"""Same-compiler host microbench ABBA; no hardware/gameplay FPS projection."""
import hashlib,json,re,statistics,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];variant=sys.argv[1] if len(sys.argv)>1 else 'guest-alias-20261102';binary=root/'build'/variant/'check';rows=[]
for index in range(5):
 for mode in (0,1,1,0):
  output=subprocess.check_output([str(binary),str(mode),str(index),'10000000'],text=True).strip()
  fields=dict(re.findall(r'(\w+)=([\d.]+)',output));rows.append(dict(function=index,mode=mode,cpu_ns_per_call=float(fields['cpu_ns_per_call']),checksum=int(fields['checksum'])))
summary=[]
for index in range(5):
 selected=[r for r in rows if r['function']==index];assert len({r['checksum'] for r in selected})==1
 off=statistics.mean(r['cpu_ns_per_call']for r in selected if r['mode']==0)
 on=statistics.mean(r['cpu_ns_per_call']for r in selected if r['mode']==1)
 summary.append(dict(function=index,off_ns=off,on_ns=on,change_percent=100*(on/off-1)))
report=dict(note=('GCC production-fast reference versus guarded independent LLVM contract-off object. ' if 'guarded' in variant else 'Same-compiler comparison. ')+'Host CPU-clock microbench, finite fixed inputs, guest ESP/top reset each call. Only candidate guest accesses annotated; see variant build flags. Not gameplay or H700 performance.',binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),runs=rows,summary=summary)
p=root/'docs/evidence'/variant/'microbench.json';p.parent.mkdir(exist_ok=True);p.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(summary,indent=2))
