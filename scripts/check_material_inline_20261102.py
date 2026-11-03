#!/usr/bin/env python3
import hashlib,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/material-inline-20261102';e=root/'docs/evidence/material-inline-20261102';e.mkdir(exist_ok=True)
for arch in ('host','arm'):
 binary=out/('check-'+arch)
 command=['gcc','-O2','-fno-strict-aliasing','-fwrapv','-Ithird_party/pcrecomp/runtime/recomp32',*[str((out/(name+'.c')).relative_to(root))for name in ('reference','candidate','driver')],'-lm','-o',str(binary.relative_to(root))]
 if arch=='arm':
  command[0]='aarch64-linux-gnu-gcc';command[1:1]=['-static','-mcpu=cortex-a53']
  command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{out}:/src/build/material-inline-20261102:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
 subprocess.run(command,cwd=root,check=True)
 command=[str(binary)] if arch=='host'else[str(root/'build/tools/qemu-user/usr/bin/qemu-aarch64'),str(binary)]
 r=subprocess.run(command,capture_output=True,text=True);(e/(arch+'.log')).write_text(r.stdout+r.stderr)
 (e/(arch+'.json')).write_text(json.dumps(dict(exit=r.returncode,sha256=hashlib.sha256(binary.read_bytes()).hexdigest()),indent=2)+'\n');print(arch,r.returncode,r.stdout+r.stderr);r.check_returncode()
