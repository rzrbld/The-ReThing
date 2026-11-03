#!/usr/bin/env python3
"""Bounded offline budget sweep. Does not measure live game performance."""
import argparse,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('capture',type=Path);a=ap.parse_args();results=[]
for meshes,triangles in [(4,4),(8,8),(16,16),(32,32),(64,32),(64,64)]:
 name=f'coarse-budget-{meshes}-{triangles}.json'
 subprocess.run(['python3',str(root/'scripts/analyze_coarse_occlusion_20261103.py'),str(a.capture),'--max-meshes',str(meshes),'--max-triangles',str(triangles),'--selection','depth_area','--output',name],check=True,stdout=subprocess.DEVNULL)
 for w in json.loads((a.capture/name).read_text())['windows']:
  if w['grid']!=[64,48]:continue
  r={k:w[k] for k in ['file','scanned_references','triangle_tile_tests','rejected_queue_candidates','overlap_with_frustum_rejections','gpu_visible_contradictions']}
  r.update(mesh_budget=meshes,triangle_budget=triangles);results.append(r)
(a.capture/'occlusion-budget-sweep.json').write_text(json.dumps(results,indent=2)+'\n')
for r in results:print(r)
