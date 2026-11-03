#!/usr/bin/env python3
"""Export current original bounds and independently computed Python ranking."""
import json,struct
from pathlib import Path
import numpy as np
from analyze_terrain_capture_20261103 import read_capture
from analyze_coarse_occlusion_20261103 import projected
root=Path(__file__).resolve().parents[1];p=root/'docs/evidence/queue-bounds-views-20261103';out=root/'build/occluder-rank-20261103';out.mkdir(exist_ok=True)
for file in sorted(p.glob('geometry-*.bin')):
 frame,rows=read_capture(file);raw=(p/f'queue-bounds-{frame}.bin').read_bytes();entries=[];ranking=[]
 for i,r in enumerate(rows):
  draw,key,valid,flag=struct.unpack_from('<4I',raw,i*232)
  if r['target']!=0x794310 or valid!=1:continue
  a=np.frombuffer(raw,dtype='<f4',count=54,offset=i*232+16).astype(float);lo=a[:3];hi=a[3:6]
  if not np.isfinite(a).all() or np.any(lo>hi):continue
  vertices=np.array([[hi[j] if mask&(1<<j) else lo[j] for j in range(3)]for mask in range(8)])
  v=projected(vertices,a[6:].reshape(3,4,4));score=-1.
  if v is not None:
   low=np.maximum(-1,v[:,:2].min(axis=0));high=np.minimum(1,v[:,:2].max(axis=0));span=np.maximum(0,high-low)
   score=float(span.prod())*max(1e-6,1-float(v[:,2].min()));ranking.append((score,draw))
  entries.append((draw,a,score))
 for budget in (4,32):
  expected=[draw for score,draw in sorted(ranking,reverse=True)[:budget]]
  data=bytearray(struct.pack('<4I',0x4b4e4152,len(entries),budget,len(expected)))
  for draw,a,score in entries:data.extend(struct.pack('<I55d',draw,*a,score))
  data.extend(struct.pack('<'+'I'*len(expected),*expected));(out/f'{file.stem}-top{budget}.bin').write_bytes(data)
 print(file.name,'bounds',len(entries),'valid projections',len(ranking))
