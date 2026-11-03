#!/usr/bin/env python3
"""Isolated exact identity-matrix shortcut against shipped local-TOP body."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/matrix-identity-20261103';out.mkdir(exist_ok=True)
p=next(p for p in json.loads((root/'build/fp-local-top-20261102/patches.json').read_text()) if p['address']=='005B7F80')
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n#include "../../probe/fp_local_top/local_top.h"\n#include <fenv.h>\n'
helper=r'''
unsigned skin_fast_hits,skin_fallback_hits;
static int matrix_identity(uint32_t a,uint32_t b,uint32_t d){
 if(a>UINT32_MAX-64 || b>UINT32_MAX-64 || d>UINT32_MAX-64)return 0;
 if((d<a+64 && a<d+64)||(d<b+64 && b<d+64))return 0;
 for(unsigned i=0;i<16;i++)if(MEM32(b+4*i)!=(i%5==0?0x3f800000u:0))return 0;
 uint32_t v[16];
 for(unsigned i=0;i<16;i++){
  v[i]=MEM32(a+4*i);uint32_t exp=v[i]&0x7f800000u;
  if(exp==0x7f800000u || (exp==0 && (v[i]&0x7fffffffu)))return 0;
 }
 if(fegetround()!=FE_TONEAREST)return 0;
 for(unsigned j=0;j<4;j++){
  uint32_t sign=(v[j]&v[4+j]&v[8+j]&v[12+j])&0x80000000u;
  for(unsigned i=j;i<16;i+=4)if(!(v[i]&0x7fffffffu))v[i]=sign;
 }
 for(unsigned i=0;i<16;i++)MEM32(d+4*i)=v[i];
 unsigned slot=(8u-(unsigned)g_fp_top-1u)&7u;
 g_st[slot]=0;g_st_i64[slot]=0;slot=(slot-1u)&7u;g_st[slot]=0;g_st_i64[slot]=0;
 return 1;
}
'''
body=p['candidate'];needle='    thing_fp_push(&_thing_fp_top,*(float*)&MEM32(eax + 0x20));'
assert body.count(needle)>=1
fast='''    if(matrix_identity(eax,edx,ecx)) { skin_fast_hits++; RECOMP_FLAGS_OUT(); esp+=8; RECOMP_REGS_OUT(); return; }
    skin_fallback_hits++;
'''
(out/'reference.c').write_text(prefix+body.replace('recomp_thething_005B7F80','reference0'))
(out/'candidate.c').write_text(prefix+helper+body.replace(needle,fast+needle,1).replace('recomp_thething_005B7F80','candidate0'))
driver=(root/'build/matrix-scalar-guarded-20261102/driver.c').read_text()
# Include identity cases, random nonidentity, adversarial exceptional/zero inputs,
# overlaps, all rounding modes and nonzero pre-existing exception flags.
driver=driver.replace('  uint32_t r[13],actual[13];', '''  if(t%4!=0)for(unsigned i=0;i<16;i++){uint32_t v=i%5==0?0x3f800000u:0;memcpy(initial+512+4*i,&v,4);}
  if(t%8==1)for(unsigned i=0;i<16;i++){uint32_t v=(rng()&1)?0x80000000u:0;memcpy(initial+256+4*i,&v,4);}
  uint32_t r[13],actual[13];''')
driver=driver.replace('feclearexcept(FE_ALL_EXCEPT);','feclearexcept(FE_ALL_EXCEPT);if(t%3==0)feraiseexcept(FE_INVALID|FE_INEXACT);')
# Mixed benchmark approximates observed ~49% identity incidence; no fenv changes timed.
driver=driver.replace('setup(1);\n  unsigned n=', 'setup(1);\n  unsigned n=')
driver=driver.replace('g_fp_top=3;fn();','g_fp_top=3;for(unsigned k=0;k<16;k++)MEM32(512+4*k)=(i%2 && k%5==0)?0x3f800000u:0;fn();')
(out/'driver.c').write_text(driver)
(out/'patch.json').write_text(json.dumps({'address':p['address'],'unit':p['unit'],'baseline':body,'candidate':body.replace(needle,fast+needle,1).replace('skin_fast_hits++; ','').replace('    skin_fallback_hits++;\n',''),'helper':'#include <fenv.h>\n'+helper.replace('unsigned skin_fast_hits,skin_fallback_hits;','')},indent=2)+'\n')
print(out)
