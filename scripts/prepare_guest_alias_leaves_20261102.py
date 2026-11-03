#!/usr/bin/env python3
"""Build public candidate bodies independently of the test harness."""
import json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];strict='--strict-fp' in sys.argv;out=root/('build/guest-alias-leaves-strict-20261102' if strict else 'build/guest-alias-leaves-20261102');out.mkdir(exist_ok=True)
patches=json.loads((root/'build/leaf-registers-20261102/patches.json').read_text())
s='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'
driver=(root/'build/guest-alias-20261102/probe.c').read_text()
for i,p in enumerate(patches):
 s+=p['original'].replace('void recomp_thething_'+p['address'],'void candidate'+str(i))
 body=p['original'].replace('void recomp_thething_'+p['address'],'__attribute__((noinline)) static void candidate'+str(i))
 assert driver.count(body)==1;driver=driver.replace(body,'extern void candidate'+str(i)+'(void);\n',1)
driver=driver.replace('assert(!memcmp(st,g_st,sizeof st));', 'if(memcmp(st,g_st,sizeof st)){for(unsigned j=0;j<8;j++)if(memcmp(st+j,g_st+j,8)){uint64_t a,b;memcpy(&a,st+j,8);memcpy(&b,g_st+j,8);fprintf(stderr,"FP mismatch f=%u m=%u trial=%u slot=%u expected=%016llx actual=%016llx\\n",f,m,trial,j,(unsigned long long)a,(unsigned long long)b);break;}return 2;}')
(out/'leaves.c').write_text(s);(out/'driver.c').write_text(driver)
clang=root/'build/tools/llvm21/usr/bin/clang-21';opt=root/'build/tools/llvm21/usr/bin/opt-21';extract=root/'build/tools/llvm21/usr/bin/llvm-extract-21'
for arch in ('host','arm'):
 directory=out/arch;directory.mkdir(exist_ok=True)
 target=[] if arch=='host' else ['--target=aarch64-linux-gnu','--sysroot='+str(root/'build/guest-alias-arm-20261102/sysroot'),'-mcpu=cortex-a53']
 subprocess.run([str(clang),*target,'-std=gnu11','-O1','-Xclang','-disable-llvm-passes','-fno-strict-aliasing','-fwrapv',('-ffp-model=strict' if strict else '-ffp-contract=off'),'-I'+str(root/'third_party/pcrecomp/runtime/recomp32'),'-S','-emit-llvm',str(out/'leaves.c'),'-o',str(directory/'initial.ll')],check=True)
 subprocess.run([str(opt),'-passes=function(sroa),cgscc(inline),function(sroa),verify','-S',str(directory/'initial.ll'),'-o',str(directory/'raw.ll')],check=True)
 subprocess.run(['python3',str(root/'scripts/probe_guest_alias_20261102.py'),str(directory)],check=True)
 subprocess.run([str(opt),'-passes=verify,default<O2>,verify','-S',str(directory/'annotated.ll'),'-o',str(directory/'optimized.ll')],check=True)
 subprocess.run([str(extract),'-S',*['--func=candidate'+str(i) for i in range(5)],str(directory/'optimized.ll'),'-o',str(directory/'leaves.ll')],check=True)
 subprocess.run([str(clang),*target,'-O2','-ffp-contract=off','-c',str(directory/'leaves.ll'),'-o',str(directory/'leaves.o')],check=True)
print('Built isolated public host/ARM leaf objects; no test-context specialization')
