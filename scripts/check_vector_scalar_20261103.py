#!/usr/bin/env python3
from pathlib import Path
import subprocess,os,json,hashlib,sys
root=Path(__file__).resolve().parents[1];out=root/'build/vector-scalar-20261103';e=root/'docs/evidence/vector-scalar-20261103';e.mkdir(exist_ok=True)
subprocess.run(['python3','scripts/prepare_vector_scalar_20261103.py'],cwd=root,check=True)
for arch in ('host','arm'):
 binary=out/('check-'+arch)
 command=['gcc','-O2','-fno-strict-aliasing','-fwrapv','-Ithird_party/pcrecomp/runtime/recomp32',*[str((out/(name+'.c')).relative_to(root)) for name in ('reference0','reference1','candidate0','candidate1')],'probe/vector_scalar/test.c','-lm','-o',str(binary.relative_to(root))]
 if arch=='arm':
  command[0]='aarch64-linux-gnu-gcc';command[1:1]=['-static','-mcpu=cortex-a53']
  command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{out}:/src/build/vector-scalar-20261103:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
 subprocess.run(command,cwd=root,check=True)
 command=[str(binary)] if arch=='host' else [str(root/'build/tools/qemu-user/usr/bin/qemu-aarch64'),str(binary)]
 r=subprocess.run(command,capture_output=True,text=True);(e/(arch+'.log')).write_text(r.stdout+r.stderr);print(arch,r.stdout+r.stderr);r.check_returncode()
 (e/(arch+'.json')).write_text(json.dumps({'exit':r.returncode,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'driver_sha256':hashlib.sha256((root/'probe/vector_scalar/test.c').read_bytes()).hexdigest()},indent=2)+'\n')
 if arch=='host':
  runs=[]
  for kind in (0,1):
   for variant in (0,1,1,0):runs.append(subprocess.check_output(command+[str(kind),str(variant),'2000000'],text=True).strip())
  (e/'bench-host.json').write_text(json.dumps(runs,indent=2)+'\n');print(runs)
