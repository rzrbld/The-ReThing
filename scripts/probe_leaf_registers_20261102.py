#!/usr/bin/env python3
"""Differential test: omit writeback of unchanged GPRs in five audited leaf routines."""
import re,json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/leaf-registers-20261102';out.mkdir(exist_ok=True)
source=(root/'build/translated-local-regs/thething/functions_0005.c').read_text();patches=[]
text='''#define RECOMP_GENERATED_CODE 1
#define RECOMP_LOCAL_REGS 1
#include "recomp_types.h"
#include <fenv.h>
#include <assert.h>
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;
ptrdiff_t g_mem_base;
'''
for index,(a,unit) in enumerate((('00414210','0005'),('00414230','0005'),('0048BB90','0040'),('00412F00','0004'),('005B7F80','0120'))):
 source=(root/('build/translated-local-regs/thething/functions_'+unit+'.c')).read_text()
 body=re.search(r'void recomp_thething_'+a+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',source,re.S).group()
 clean=re.sub(r'/\*.*?\*/','',body,flags=re.S)
 assert not re.search(r'RECOMP_(?:CALL|ICALL|ITAIL|TAIL|EH_)',clean)
 # Only known helper/macro calls in these two reviewed arithmetic leaves.
 calls=set(re.findall(r'\b(\w+)\s*\(',clean))
 assert calls<= {'recomp_thething_'+a,'RECOMP_ENTER','fp_push','MEM32','FP_ST','ADDR','fp_pop','RECOMP_FLAGS_OUT','RECOMP_REGS_OUT'},calls
 regs=[r for r in ('eax','ecx','edx','ebx','esp','esi','edi','ebp') if re.search(r'\b'+r+r'\b',clean)]
 written=[r for r in regs if re.search(r'\b'+r+r'\s*(?:=(?!=)|\+=|-=|\*=|/=|\|=|&=|\^=|<<=|>>=)',clean)]
 assert set(written)<= {'eax','esp'} and 'esp' in written
 replacement='('+','.join('g_'+r+'='+r for r in written)+')'
 optimized=body.replace('RECOMP_REGS_OUT()',replacement)
 text+=body.replace('void recomp_thething_'+a,'__attribute__((noinline)) static void reference'+str(index))
 text+=optimized.replace('void recomp_thething_'+a,'__attribute__((noinline)) static void candidate'+str(index))
 patches.append(dict(address=a,used=regs,written=written,omitted=[r for r in ('eax','ecx','edx','ebx','esp','esi','edi','ebp')if r not in written],original=body,candidate=optimized,source_sha256=hashlib.sha256(body.encode()).hexdigest()))
text+='''
static unsigned char mem[4096],initial[4096],expected[4096];
static uint32_t seed=123567;
static uint32_t next(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void setup(unsigned trial){
 memcpy(mem,initial,sizeof mem);g_mem_base=(ptrdiff_t)mem;
 g_eax=0x12345678;g_ebx=trial*13417u;g_esi=trial^0xfefefefe;g_edi=trial^0x89abcdef;g_ebp=trial+534;
 g_esp=2048;g_edx=512;g_ecx=768;
 if(trial%3==0)g_ecx=g_edx+4;if(trial%5==0)g_ecx=g_esp+4;
 MEM32(2052)=1024;
 g_flag_a=trial;g_flag_b=~trial;g_flag_cf=trial&1;g_flag_k=trial%5;
 g_fp_top=trial%8;g_fpu_cw=0x37f;
 for(unsigned i=0;i<8;i++){uint64_t bits=0x3ff0000000000000ull+i;memcpy(g_st+i,&bits,8);g_st_i64[i]=trial+i;}
}
static void regs(uint32_t out[13]){uint32_t r[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_flag_a,g_flag_b,g_flag_cf,g_flag_k,g_cur_func};memcpy(out,r,sizeof r);}
int main(void){
 int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};unsigned cases=0;
 void(*refs[])(void)={reference0,reference1,reference2,reference3,reference4};
 void(*opts[])(void)={candidate0,candidate1,candidate2,candidate3,candidate4};
 for(unsigned f=0;f<5;f++)for(unsigned m=0;m<4;m++){
 assert(!fesetround(modes[m]));
 for(unsigned trial=0;trial<5000;trial++){
 for(unsigned i=0;i<sizeof initial;i+=4){uint32_t b=next();memcpy(initial+i,&b,4);}
 setup(trial);feclearexcept(FE_ALL_EXCEPT);refs[f]();
 uint32_t r[13],actual[13];regs(r);memcpy(expected,mem,sizeof mem);
 double st[8];int64_t shadows[8];memcpy(st,g_st,sizeof st);memcpy(shadows,g_st_i64,sizeof shadows);int top=g_fp_top,flags=fetestexcept(FE_ALL_EXCEPT);
 setup(trial);feclearexcept(FE_ALL_EXCEPT);opts[f]();regs(actual);
 assert(!memcmp(r,actual,sizeof r));assert(!memcmp(expected,mem,sizeof mem));assert(!memcmp(st,g_st,sizeof st));assert(!memcmp(shadows,g_st_i64,sizeof shadows));assert(top==g_fp_top);assert(flags==fetestexcept(FE_ALL_EXCEPT));cases++;
 }}printf("PASS %u exact leaf register/memory/x87/flags comparisons\\n",cases);return 0;
}
'''
(out/'probe.c').write_text(text);(out/'patches.json').write_text(json.dumps(patches,indent=2)+'\n')
print('Generated audited leaf candidates; game sources unchanged')
