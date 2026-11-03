#!/usr/bin/env python3
import argparse,json,struct
from pathlib import Path
import numpy as np
from analyze_terrain_capture_20261103 import read_capture
from analyze_coarse_occlusion_20261103 import coverage_segments
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--capture',type=Path,default=root/'docs/evidence/queue-bounds-views-20261103');ap.add_argument('--streaming',action='store_true');ap.add_argument('--reference',default='coarse-mesh-budget-32-depth-safe.json');ap.add_argument('--out',type=Path,default=root/'build/occlusion-pipeline-20261103');args=ap.parse_args()
p=args.capture;out=args.out;out.mkdir(exist_ok=True)
reference=json.loads((p/args.reference).read_text());expected=reference['windows']
mesh_budget=16 if args.streaming else reference['max_meshes'];triangle_budget=4 if args.streaming else reference['max_triangles']
assert 1<=mesh_budget<=32 and 1<=triangle_budget<=16
for file in sorted(p.glob('geometry-*.bin')):
 frame,rows=read_capture(file);raw=(p/f'queue-bounds-{frame}.bin').read_bytes();ref=next(r for r in rows if r['target']==0x794310)
 rejected=set(next(w['rejected'] if args.streaming else w['rejected_draws'] for w in expected if w['file']==file.name and w.get('grid',[64,48])==[64,48]))
 material_file=p/f'material-{frame}.bin'
 segments=coverage_segments(rows,material_file.read_bytes() if material_file.exists() else None)
 data=bytearray(struct.pack('<4I',0x504c4332,len(rows),mesh_budget,triangle_budget))
 for i,r in enumerate(rows):
  draw,key,valid,flag=struct.unpack_from('<4I',raw,i*232);a=np.frombuffer(raw,dtype='<f4',count=54,offset=i*232+16).astype(float);rs=r['rs']
  rank=r['target']==0x794310 and valid==1 and np.isfinite(a).all() and np.all(a[:3]<=a[3:6])
  same=r['rt']==ref['rt'] and r['depth']==ref['depth'] and r['viewport']==ref['viewport']
  raster=same and np.array_equal(r['matrices'][1:],ref['matrices'][1:]) and r['target']==0x794310 and r['valid'] and r['primitive']==4 and r['fvf']&14==2 and len(r['xyz'])%3==0 and rs[7]==1 and rs[14]==1 and rs[23] in (2,4) and rs[15]==0 and rs[27]==0 and rs[52]==0 and rs[47]==0 and r['viewport'][4:]==(0,0x3f800000)
  query=rank and flag==0 and same and np.array_equal(a[22:].reshape(2,4,4),ref['matrices'][1:]) and rs[7]==1 and rs[23] in (2,4) and rs[52]==0 and rs[47]==0 and r['viewport'][4:]==(0,0x3f800000)
  if rank and raster:assert np.array_equal(a[6:].reshape(3,4,4),r['matrices'])
  bits=int(rank)|int(raster)*2|int(query)*4;n=len(r['xyz']) if raster else 0
  data.extend(struct.pack('<6I54d',draw,bits,int(rs[22]),n,int(draw in rejected),segments[draw],*a))
  if n:data.extend(r['xyz'].astype('<f8').tobytes())
 (out/(file.stem+'.bin')).write_bytes(data)
 print(file.name,len(rows),'expected rejected',len(rejected))
