#!/usr/bin/env python3
"""Generate exact state tests for local-top direct/indirect call boundaries."""
import json
from pathlib import Path
from probe_fp_local_top_20261102 import transform
root=Path(__file__).resolve().parents[1];out=root/'build/fp-local-top-20261102';out.mkdir(exist_ok=True)
base=root/'build/leaf-registers-20261102';s=(base/'probe.c').read_text()
s=s.replace('#include "recomp_types.h"','#include "recomp_types.h"\n#include "../../probe/fp_local_top/local_top.h"',1)
mark='__attribute__((noinline)) static void reference0'
stub='''static void boundary(void) {
 MEM32(128)=(uint32_t)g_fp_top;
 memcpy((void *)ADDR(136),g_st,sizeof g_st);
 fp_push(17.25);fp_xch(2);fp_pop();fp_push(-0.0);
 g_esp+=4;
}
recomp_func_t recomp_lookup_manual(uint32_t va){return va==0x1234?boundary:0;}
recomp_func_t recomp_lookup(uint32_t va){(void)va;return 0;}
recomp_func_t recomp_lookup_import(uint32_t va){(void)va;return 0;}
'''
assert mark in s;s=s.replace(mark,stub+'\n'+mark,1)
for i,p in enumerate(json.loads((base/'patches.json').read_text())):
 original=p['original']
 enter='RECOMP_ENTER(0x'+p['address']+'u);';assert original.count(enter)==1
 # Exercise a nonempty stack across direct and indirect transfers. The callee
 # observes global top, exchanges a slot, and changes top before returning.
 body=original.replace(enter,enter+'''\n fp_push(3.25);RECOMP_CALL(boundary);fp_pop();
 fp_push(7.5);RECOMP_ICALL(0x1234);fp_pop();fp_xch(1);''')
 for kind,old,new in [('reference',original,body),('candidate',p['candidate'],transform(body))]:
  rename='__attribute__((noinline)) static void '+kind+str(i)
  old=old.replace('void recomp_thething_'+p['address'],rename)
  new=new.replace('void recomp_thething_'+p['address'],rename)
  assert s.count(old)==1;s=s.replace(old,new,1)
s=s.replace('assert(!memcmp(st,g_st,sizeof st));', 'if(memcmp(st,g_st,sizeof st)){for(unsigned j=0;j<8;j++)if(memcmp(st+j,g_st+j,8)){uint64_t a,b;memcpy(&a,st+j,8);memcpy(&b,g_st+j,8);fprintf(stderr,"FP mismatch f=%u m=%u trial=%u slot=%u expected=%016llx actual=%016llx\\n",f,m,trial,j,(unsigned long long)a,(unsigned long long)b);break;}return 2;}')
s=s.replace('exact leaf register/memory/x87/flags comparisons','exact direct/indirect x87-boundary register/memory/flags comparisons')
s=s.replace('assert(!memcmp(expected,mem,sizeof mem));', 'if(memcmp(expected,mem,sizeof mem)){for(unsigned j=0;j<sizeof mem;j++)if(expected[j]!=mem[j]){fprintf(stderr,"memory mismatch f=%u m=%u trial=%u byte=%u expected=%02x actual=%02x\\n",f,m,trial,j,expected[j],mem[j]);break;}return 1;}')
(out/'probe.c').write_text(s)
print('Generated boundary differential test')
