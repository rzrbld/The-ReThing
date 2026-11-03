#!/usr/bin/env python3
import ctypes as c,json,csv,subprocess
from pathlib import Path
import numpy as np
from analyze_terrain_capture_20261103 import read_capture
root=Path(__file__).resolve().parents[1];out=root/'build/precomputed-bounds-20261103';out.mkdir(exist_ok=True)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC','probe/visibility/precomputed_bounds_bridge.c','-lm','-o',str(out/'bounds.so')],cwd=root,check=True)
class Planes(c.Structure):_fields_=[('plane',c.c_double*24),('w',c.c_double*4),('magnitude',c.c_double*4),('valid',c.c_int)]
lib=c.CDLL(str(out/'bounds.so'));fp=c.POINTER(c.c_float);lib.bounds_prepare.argtypes=[c.POINTER(Planes),fp,fp,fp];lib.bounds_outside.argtypes=[c.POINTER(Planes),fp,fp];lib.bounds_outside.restype=c.c_int
source=root/'docs/evidence/terrain-capture-views-20261103';queries=list(csv.DictReader((source/'depth.csv').open()));summary=[]
for w,file in enumerate(sorted(source.glob('geometry-*.bin')),1):
 _,rows=read_capture(file);q=[x for x in queries if int(x['window'])==w];assert len(q)==len(rows);rejected=[]
 for r,d in zip(rows,q):
  if not r['valid'] or r['primitive']!=4 or r['fvf']&14!=2 or not len(r['xyz']):continue
  arrays=[np.ascontiguousarray(x,dtype=np.float32)for x in r['matrices']];lo=np.ascontiguousarray(r['xyz'].min(axis=0),dtype=np.float32);hi=np.ascontiguousarray(r['xyz'].max(axis=0),dtype=np.float32);planes=Planes()
  lib.bounds_prepare(c.byref(planes),*[x.ctypes.data_as(fp)for x in arrays])
  if lib.bounds_outside(c.byref(planes),lo.ctypes.data_as(fp),hi.ctypes.data_as(fp)):
   assert d['normal_pass']=='0' and d['no_depth_pass']!='1',(file,r['draw'],d)
   rejected.append(r['draw'])
 summary.append({'file':file.name,'draws':len(rows),'bounds_rejected':len(rejected),'rejected_ordinals':rejected})
e=root/'docs/evidence/precomputed-bounds-20261103';e.mkdir(exist_ok=True);(e/'captured-validation.json').write_text(json.dumps(summary,indent=2)+'\n');print([(x['file'],x['bounds_rejected'])for x in summary])
