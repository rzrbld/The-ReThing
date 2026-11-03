#!/usr/bin/env python3
"""Exact last-result reuse; matches full native arithmetic control/status state."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/matrix-last-result-20261103';out.mkdir(exist_ok=True)
p=next(p for p in json.loads((root/'build/fp-local-top-20261102/patches.json').read_text()) if p['address']=='005B7F80')
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n#include "../../probe/fp_local_top/local_top.h"\n#include <string.h>\n'
helper=r'''
unsigned skin_fast_hits,skin_fallback_hits;
static _Thread_local struct {uint32_t inputs[32],out[16];uint64_t control,status;int valid;} last_matrix;
static int matrix_environment(uint64_t *control,uint64_t *status){
#if defined(__aarch64__)
 __asm__ volatile("mrs %0, fpcr":"=r"(*control)::"memory");
 __asm__ volatile("mrs %0, fpsr":"=r"(*status)::"memory");
 return !(*control & ((0x1full<<8)|(1ull<<15)));
#elif defined(__x86_64__) && defined(__SSE2_MATH__)
 uint32_t mxcsr;__asm__ volatile("stmxcsr %0":"=m"(mxcsr)::"memory");
 *control=mxcsr & ~0x3fu;*status=mxcsr & 0x3fu;
 return (mxcsr&0x1f80u)==0x1f80u;
#else
 *control=*status=0;return 0;
#endif
}
static int matrix_last_lookup(uint32_t a,uint32_t b,uint32_t d,uint32_t *inputs){
 if(a>UINT32_MAX-64 || b>UINT32_MAX-64 || d>UINT32_MAX-64)return -1;
 if((d<a+64 && a<d+64)||(d<b+64 && b<d+64))return -1;
 uint64_t control,status;if(!matrix_environment(&control,&status))return -1;
 memcpy(inputs,(void*)ADDR(a),64);memcpy(inputs+16,(void*)ADDR(b),64);
 if(!last_matrix.valid || last_matrix.control!=control || last_matrix.status!=status || memcmp(inputs,last_matrix.inputs,128))return 0;
 memcpy((void*)ADDR(d),last_matrix.out,64);
 unsigned slot=(8u-(unsigned)g_fp_top-1u)&7u;
 g_st[slot]=0;g_st_i64[slot]=0;slot=(slot-1u)&7u;g_st[slot]=0;g_st_i64[slot]=0;
 return 1;
}
static void matrix_last_store(uint32_t d,const uint32_t *inputs){
 uint64_t control,status;if(!matrix_environment(&control,&status))return;
 memcpy(last_matrix.inputs,inputs,128);memcpy(last_matrix.out,(void*)ADDR(d),64);
 last_matrix.control=control;last_matrix.status=status;last_matrix.valid=1;
}
'''
body=p['candidate'];needle='    thing_fp_push(&_thing_fp_top,*(float*)&MEM32(eax + 0x20));'
fast='''    uint32_t matrix_inputs[32]; int matrix_reuse=matrix_last_lookup(eax,edx,ecx,matrix_inputs);
    if(matrix_reuse==1){skin_fast_hits++;RECOMP_FLAGS_OUT();esp+=8;RECOMP_REGS_OUT();return;}
    skin_fallback_hits++;
'''
ret='    RECOMP_FLAGS_OUT(); esp += 8;'
assert body.count(ret)==1
candidate=body.replace(needle,fast+needle,1).replace(ret,'    if(matrix_reuse==0)matrix_last_store(ecx,matrix_inputs);\n'+ret,1)
(out/'reference.c').write_text(prefix+body.replace('recomp_thething_005B7F80','reference0'))
(out/'candidate.c').write_text(prefix+helper+candidate.replace('recomp_thething_005B7F80','candidate0'))
driver=(root/'build/matrix-scalar-guarded-20261102/driver.c').read_text()
driver=driver.replace('  uint32_t r[13],actual[13];','  fenv_t priorenv;for(unsigned repeat=0;repeat<2;repeat++){fenv_t startenv; if(repeat)fesetenv(&priorenv);else{feclearexcept(FE_ALL_EXCEPT);if(t%3==0)feraiseexcept(FE_INVALID|FE_INEXACT);}fegetenv(&startenv);\n  uint32_t r[13],actual[13];')
driver=driver.replace('setup(t);feclearexcept(FE_ALL_EXCEPT);','setup(t);fesetenv(&startenv);')
driver=driver.replace('  cases++;','  fegetenv(&priorenv);cases++;}')
driver=driver.replace('int top=g_fp_top,flags=fetestexcept(FE_ALL_EXCEPT);','int top=g_fp_top,flags=fetestexcept(FE_ALL_EXCEPT);fenv_t expected_env={0},actual_env={0};fegetenv(&expected_env);')
driver=driver.replace('candidate0();regs(actual);','candidate0();regs(actual);fegetenv(&actual_env);')
driver=driver.replace('if(memdiff||regdiff||stdiff||sidiff||','if(memcmp(&expected_env,&actual_env,sizeof expected_env)||memdiff||regdiff||stdiff||sidiff||')
driver=driver.replace('g_fp_top=3;fn();','g_fp_top=3;MEM32(256)=0x3e000000u+((i/2)%64);fn();')
driver=driver.replace('guarded matrix multiplication','last-result matrix reuse')
(out/'driver.c').write_text(driver)
(out/'patch.json').write_text(json.dumps({'unit':p['unit'],'baseline':body,'candidate':candidate.replace('skin_fast_hits++;','').replace('    skin_fallback_hits++;\n',''),'helper':helper.replace('unsigned skin_fast_hits,skin_fallback_hits;','')},indent=2)+'\n')
print(out)
