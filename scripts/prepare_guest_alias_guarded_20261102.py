#!/usr/bin/env python3
"""Prepare GCC fallbacks around independently compiled LLVM math objects."""
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];source=root/'build/guest-alias-leaves-20261102';out=root/'build/guest-alias-guarded-20261102';out.mkdir(exist_ok=True)
driver=(source/'driver.c').read_text();wrappers=['#include "../../../probe/guest_alias_guards.h"', '#ifdef THING_ALIAS_TEST_COUNTS', 'static unsigned long fast_hits[5],fallback_hits[5];', '#define ALIAS_HIT(kind,i) kind##_hits[i]++', '#else', '#define ALIAS_HIT(kind,i) ((void)0)', '#endif']
for i in range(5):
 wrappers += [f'extern void fast_candidate{i}(void);',f'void candidate{i}(void){{if(thing_alias_eligible({i})){{ALIAS_HIT(fast,{i});fast_candidate{i}();}}else{{ALIAS_HIT(fallback,{i});reference{i}();}}}}']
marker='static unsigned char mem[4096]';assert marker in driver
driver=driver.replace(marker,'\n'.join(wrappers)+'\n'+marker,1)
# Include is relative to generated driver's directory, two levels below root.
driver=driver.replace('../../../probe/guest_alias_guards.h','../../probe/guest_alias_guards.h')
driver=driver.replace('}}printf("PASS', '}}\n#ifdef THING_ALIAS_TEST_COUNTS\n for(unsigned i=0;i<5;i++){printf("paths function=%u fast=%lu fallback=%lu\\n",i,fast_hits[i],fallback_hits[i]);assert(fast_hits[i]&&fallback_hits[i]);}\n#endif\nprintf("PASS')
(out/'driver.c').write_text(driver)
for arch in ('host','arm'):
 directory=out/arch;directory.mkdir(exist_ok=True)
 s=(source/arch/'leaves.ll').read_text()
 for i in range(5):s=s.replace('@candidate'+str(i)+'(', '@fast_candidate'+str(i)+'(')
 (directory/'leaves.ll').write_text(s)
 target=[]if arch=='host'else['--target=aarch64-linux-gnu','-mcpu=cortex-a53']
 subprocess.run([str(root/'build/tools/llvm21/usr/bin/clang-21'),*target,'-O2','-ffp-contract=off','-c',str(directory/'leaves.ll'),'-o',str(directory/'leaves.o')],check=True)
print('Prepared guarded mixed GCC/LLVM probe')
