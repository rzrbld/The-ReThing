#!/usr/bin/env python3
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1];src=root/'build/skin-scalar-block-20261102';out=root/'build/skin-scalar-guarded-20261102';out.mkdir(exist_ok=True)
s=(src/'candidate.c').read_text().replace('void candidate0(void){','static void scalar0(void){')
s+='\n#include "../../probe/skin_scalar_guard.h"\nextern void reference0(void);\nunsigned skin_fast_hits,skin_fallback_hits;\nvoid candidate0(void){if(thing_skin_scalar_eligible(g_eax,g_esi,g_esp,g_ebp)){skin_fast_hits++;scalar0();}else{skin_fallback_hits++;reference0();}}\n'
(out/'candidate.c').write_text(s)
(out/'reference.c').write_text((src/'reference.c').read_text())
s=(src/'driver.c').read_text().replace('unsigned cases=0;','unsigned cases=0; extern unsigned skin_fast_hits,skin_fallback_hits;')
s=s.replace('printf("PASS %u exact', 'assert(skin_fast_hits>1000 && skin_fallback_hits>1000);printf("paths fast=%u fallback=%u\\n",skin_fast_hits,skin_fallback_hits);printf("PASS %u exact')
s=s.replace('115+rng()%25','rng()%159').replace('t<12000','t<50000')
s=s.replace('  uint32_t r[13],actual[13];', '  if(t%16==0){uint32_t special[]={0,0x80000000u,1,0x80000001u,0x4f800000u,0xcf800000u,0x4f800001u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u};uint32_t v=special[(t/16)%11];for(unsigned k=0;k<sizeof initial;k+=4)memcpy(initial+k,&v,4);}\n  uint32_t r[13],actual[13];')
(out/'driver.c').write_text(s)
print('Staged guarded scalar block and original fallback')
