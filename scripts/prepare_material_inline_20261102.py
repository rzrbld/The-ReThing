#!/usr/bin/env python3
"""Stage exact original SetRenderState wrapper with/without native inlining."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/material-inline-20261102';out.mkdir(exist_ok=True)
name='void recomp_thething_00759C20(void) {'
found=[]
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text()
 if name in s:
  a=s.index(name);b=s.index('\nvoid recomp_',a+1);found.append(s[a:b])
assert len(found)==1
original=found[0];prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'
for kind,attrs in [('reference','static __attribute__((noinline,noclone))'),('candidate','static __attribute__((always_inline)) inline')]:
 helper=original.replace('void recomp_thething_00759C20',attrs+' void '+kind+'_helper',1)
 wrapper='void '+kind+'2(void){uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=0;RECOMP_REGS_LOCALS\nRECOMP_ENTER(0x0076FC10u);RECOMP_CALL('+kind+'_helper);RECOMP_FLAGS_IN();RECOMP_FLAGS_OUT();RECOMP_REGS_OUT();}\n'
 (out/(kind+'.c')).write_text(prefix+helper+'\n'+wrapper)
driver=(root/'probe/guest_alias_integer/state_call_test.inc').read_text().replace('extern void candidate2(void);','extern void candidate2(void),reference2(void);')
driver=driver.replace('PASS 50000 exact SetRenderState-wrapper cases','PASS 50000 exact parent-to-SetRenderState wrapper inline cases')
(out/'driver.c').write_text(prefix+driver)
print('Staged original wrapper call comparison; no game source changed')
