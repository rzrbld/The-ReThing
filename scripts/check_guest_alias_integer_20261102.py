#!/usr/bin/env python3
"""Narrow call/ABI differential test; does not validate all 70 routines."""
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[1];out=root/'build/guest-alias-integer-20261102'
s=(out/'candidates.c').read_text();a=s.index('void candidate2(');b=s.index('\nvoid candidate3(',a)
body=s[a:b].replace('void candidate2(', 'static __attribute__((noinline)) void reference2(',1)
(out/'test.c').write_text('#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'+body+'\n#include "../../probe/guest_alias_integer/state_call_test.inc"\n')
llvm=root/'build/tools/llvm21/usr/bin'
for arch in ('host','arm'):
 d=out/arch
 for variant in ('plain','alias'):
  subprocess.run([str(llvm/'llvm-extract-21'),'-S','--func=candidate2','--glob=.str',str(d/(variant+'.ll')),'-o',str(d/(variant+'-test.ll'))],check=True)
  flags=[] if arch=='host'else ['--target=aarch64-linux-gnu','-mcpu=cortex-a53']
  subprocess.run([str(llvm/'clang-21'),*flags,'-O2','-c',str(d/(variant+'-test.ll')),'-o',str(d/(variant+'-test.o'))],check=True)
  binary=d/(variant+'-test')
  command=['gcc','-O2','-fno-strict-aliasing','-fwrapv','-Ithird_party/pcrecomp/runtime/recomp32',str((out/'test.c').relative_to(root)),str((d/(variant+'-test.o')).relative_to(root)),'-o',str(binary.relative_to(root)),'-lm']
  if arch=='arm':
   command[0]='aarch64-linux-gnu-gcc';command.insert(1,'-static')
   command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{out}:/src/build/guest-alias-integer-20261102:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
  subprocess.run(command,cwd=root,check=True)
  command=[str(binary)] if arch=='host'else [str(root/'build/tools/qemu-user/usr/bin/qemu-aarch64'),str(binary)]
  result=subprocess.run(command,capture_output=True,text=True)
  (d/(variant+'-test.log')).write_text(result.stdout+result.stderr)
  print(arch,variant,result.returncode,result.stdout.strip());result.check_returncode()
