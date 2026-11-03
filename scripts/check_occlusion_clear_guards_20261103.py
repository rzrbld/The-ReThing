#!/usr/bin/env python3
"""Adversarial replay: depth clears must invalidate all earlier occluders."""
import argparse,json,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/evidence/material-views-20261103'
ap=argparse.ArgumentParser();ap.add_argument('--native',action='store_true');ap.add_argument('--streaming-native',action='store_true');args=ap.parse_args()
with tempfile.TemporaryDirectory(prefix='thing-clear-guards-') as tmp:
 p=Path(tmp)
 for f in SOURCE.iterdir():
  if f.name.startswith('material-') and f.suffix=='.bin':
   data=bytearray(f.read_bytes());assert len(data)%196==0
   for i in range(len(data)//196):struct.pack_into('<I',data,i*196+12,i+1)
   (p/f.name).write_bytes(data)
  elif f.name.startswith(('geometry-','queue-bounds-')) or f.name=='depth.csv':
   (p/f.name).symlink_to(f)
 subprocess.run(['python3',str(ROOT/'scripts/analyze_coarse_occlusion_20261103.py'),str(p),'--max-meshes','32','--max-triangles','16','--selection','depth_area','--output','result.json'],check=True,stdout=subprocess.DEVNULL)
 result=json.loads((p/'result.json').read_text())
 assert len(result['windows'])==6
 for window in result['windows']:
  assert window['depth_clear_provenance']
  assert window['coverage_segments']>200
  assert window['rejected_queue_candidates']==0,window
 original=json.loads((SOURCE/'coarse-depth-safe.json').read_text())
 assert any(w['rejected_queue_candidates']>0 for w in original['windows'])
 native=[]
 if args.native or args.streaming_native:
  benchdir='streaming-occlusion-20261103' if args.streaming_native else 'occlusion-pipeline-20261103'
  fixtures=p/'fixtures'
  subprocess.run(['python3',str(ROOT/'scripts/export_occlusion_pipeline_20261103.py'),'--capture',str(p),'--reference','result.json','--out',str(fixtures)],check=True,stdout=subprocess.DEVNULL)
  for fixture in sorted(fixtures.glob('*.bin')):
   for name,command in [('host',[str(ROOT/'build'/benchdir/'bench.host')]),('arm-qemu',[str(ROOT/'build/tools/qemu-user/usr/bin/qemu-aarch64'),str(ROOT/'build'/benchdir/'bench.aarch64')])]:
    output=subprocess.check_output(command+[str(fixture),'1'],text=True).strip()
    assert 'rejected=0' in output
    native.append({'target':name,'fixture':fixture.name,'output':output})
 report={'test':'insert depth clear before every captured draw','windows':len(result['windows']),'rejections':0,'result':'PASS','note':'Validates invalidation in offline replay only; no deployed runtime occlusion.'}
 report['native']=native
 (SOURCE/('clear-guard-streaming-test.json' if args.streaming_native else 'clear-guard-test.json')).write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report))
