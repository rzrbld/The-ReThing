#!/usr/bin/env python3
"""Isolated host/ARM differential checks; preserve each run and historical64."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--slots',type=int,default=512);ap.add_argument('--arch',choices=['host','arm','both'],default='both');a=ap.parse_args()
assert a.slots>0 and a.slots&(a.slots-1)==0
root=Path(__file__).resolve().parents[1];out=root/'build/skin-palette-memo-20261102'
subprocess.run(['python3',str(root/'scripts/prepare_skin_palette_memo_20261102.py')],check=True)
(out/'driver.c').write_text((root/'scripts/skin_palette_memo_driver_20261102.c').read_text())
run=out/('test-slots%d-%s'%(a.slots,time.strftime('%Y%m%d-%H%M%S')));run.mkdir()
for name in ('reference.c','candidate.c','driver.c','memo.h','fpstate.h'):(run/name).write_bytes((out/name).read_bytes())
base={'slots':a.slots,'source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in run.iterdir()}}
(run/'metadata.json').write_text(json.dumps(base,indent=2)+'\n')
for arch in (['host','arm'] if a.arch=='both' else [a.arch]):
 for fp in ['strict','game-default']:
  binary=run/('check-'+arch+'-'+fp)
  cmd=['gcc','-O2','-fno-strict-aliasing','-fwrapv','-DSKIN_MEMO_SLOTS='+str(a.slots),'-Ithird_party/pcrecomp/runtime/recomp32',*[str((run/(s+'.c')).relative_to(root))for s in ('reference','candidate','driver')],'-lm','-o',str(binary.relative_to(root))]
  if fp=='strict':cmd[2:2]=['-frounding-math','-fno-fast-math','-ffp-contract=off']
  if arch=='arm':
   cmd[0]='aarch64-linux-gnu-gcc';cmd[1:1]=['-static','-mcpu=cortex-a53']
   cmd=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{out}:/src/build/skin-palette-memo-20261102:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*cmd]
  metadata=dict(base,arch=arch,fp=fp,command=cmd)
  mp=run/(arch+'-'+fp+'.json');mp.write_text(json.dumps(metadata,indent=2)+'\n')
  subprocess.run(cmd,cwd=root,check=True)
  execute=[str(binary)] if arch=='host'else[str(root/'build/tools/qemu-user/usr/bin/qemu-aarch64'),str(binary)]
  p=subprocess.run(execute,capture_output=True,text=True);(run/(arch+'-'+fp+'.log')).write_text(p.stdout+p.stderr)
  metadata.update(exit=p.returncode,binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest());mp.write_text(json.dumps(metadata,indent=2)+'\n');print(arch,fp,p.stdout+p.stderr);p.check_returncode()
