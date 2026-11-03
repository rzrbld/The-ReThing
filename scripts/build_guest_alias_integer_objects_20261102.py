#!/usr/bin/env python3
"""Compile isolated integer candidates, keeping an unannotated LLVM control."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/guest-alias-integer-20261102'
manifest=json.loads((out/'manifest.json').read_text())
llvm=root/'build/tools/llvm21/usr/bin'
includes=['-I'+str(root/'runtime'),'-I'+manifest['source_include'],'-I'+str(root/'third_party/pcrecomp/runtime/recomp32')]
for arch in ('host','arm'):
 d=out/arch;d.mkdir(exist_ok=True)
 target=[] if arch=='host' else ['--target=aarch64-linux-gnu','--sysroot='+str(root/'build/guest-alias-arm-20261102/sysroot'),'-mcpu=cortex-a53']
 def run(*args):subprocess.run([str(x) for x in args],check=True)
 run(llvm/'clang-21',*target,'-std=gnu11','-O1','-Xclang','-disable-llvm-passes','-fno-strict-aliasing','-fwrapv','-ffp-contract=off',*includes,'-S','-emit-llvm',out/'candidates.c','-o',d/'initial.ll')
 run(llvm/'opt-21','-passes=function(sroa),cgscc(inline),function(sroa),verify','-S',d/'initial.ll','-o',d/'raw.ll')
 run('python3',root/'scripts/probe_guest_alias_20261102.py',d,len(manifest['candidates']))
 for variant,source in [('plain','raw.ll'),('alias','annotated.ll')]:
  run(llvm/'opt-21','-passes=verify,default<O2>,verify','-S',d/source,'-o',d/(variant+'.ll'))
  run(llvm/'clang-21',*target,'-O2','-ffp-contract=off','-c',d/(variant+'.ll'),'-o',d/(variant+'.o'))
 files={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in d.glob('*') if p.is_file()}
 (d/'hashes.json').write_text(json.dumps(files,indent=2)+'\n')
print('Host/ARM alias and plain objects built; NOT integrated or validated')
