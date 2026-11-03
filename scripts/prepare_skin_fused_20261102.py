#!/usr/bin/env python3
"""Isolated full transform/weight/accumulate block; no game mutation."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/skin-fused-20261102';out.mkdir(exist_ok=True)
lines=(root/'build/translated-local-regs/thething/functions_0235.c').read_text().splitlines()
a=next(i for i,l in enumerate(lines)if '0x007AEBBA:'in l);b=next(i for i in range(a,len(lines))if '0x007AECD7:'in lines[i]);block='\n'.join(lines[a:b+1])+'\n    RECOMP_FLAGS_IN();\n'
assert 'goto 'not in block
leaves=json.loads((root/'build/leaf-registers-20261102/patches.json').read_text())
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'
helpers={}
for kind,attrs in [('reference','static __attribute__((noinline,noclone))'),('inline','static __attribute__((always_inline)) inline')]:
 helpers[kind]=''
 for i in (0,1):
  p=leaves[i];helpers[kind]+=p['original'].replace('void recomp_thething_'+p['address'],attrs+' void '+kind+'_'+p['address'],1)+'\n'
def body(name,kind):
 s=block
 for address in ('00414210','00414230'):s=s.replace('recomp_thething_'+address,kind+'_'+address)
 return 'void '+name+'(void){\nuint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=0; RECOMP_REGS_LOCALS\nRECOMP_ENTER(0x007AE710u);\n'+s+'RECOMP_FLAGS_OUT();RECOMP_REGS_OUT();\n}\n'
(out/'candidate.c').write_text(prefix+helpers['inline']+body('candidate0','inline'))
(out/'reference.c').write_text(prefix+helpers['reference']+body('reference0','reference'))
(out/'driver.c').write_text(prefix+'''#include <assert.h>
#include <fenv.h>
#include <time.h>
#include <string.h>
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
static unsigned char memory[4096],initial[4096],expected[4096];
static uint32_t seed=1234567;
static uint32_t rng(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
extern void reference0(void),candidate0(void);
static void setup(unsigned trial){
 memcpy(memory,initial,sizeof memory);g_mem_base=(ptrdiff_t)memory;
 g_eax=256;g_ecx=64;g_edx=96;g_esp=2048;g_esi=0;g_ebx=128;g_edi=trial;g_ebp=1536;g_cur_func=0x777777;
 if(trial%5==0)g_ebp=256;if(trial%7==0)g_ebp=512;if(trial%11==0)g_eax=2100;
 MEM32(g_esp+0x58)=768;MEM32(768+0x3c)=512;MEM32(g_esp+0x28)=1024;MEM32(g_esp+0x88)=256;
 g_flag_a=trial;g_flag_b=~trial;g_flag_cf=trial&1;g_flag_k=FK_CMP;
 g_fp_top=(int)(trial%25)-8;g_fpu_cw=0x37f;
 for(unsigned i=0;i<8;i++){g_st[i]=.125*i;g_st_i64[i]=trial+i;}
}
static void regs(uint32_t*r){uint32_t v[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_a,g_flag_b,g_flag_cf,g_flag_k};memcpy(r,v,sizeof v);}
int main(int argc,char **argv){
 if(argc==3){
  for(unsigned i=0;i<sizeof initial;i+=4){float v=.125f;memcpy(initial+i,&v,4);}setup(1);
  unsigned n=(unsigned)strtoul(argv[2],0,10);void(*fn)(void)=atoi(argv[1])?candidate0:reference0;
  struct timespec a,b;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&a);
  for(unsigned i=0;i<n;i++){g_eax=256;g_esi=0;g_esp=2048;g_ebp=1536;g_fp_top=3;fn();}
  clock_gettime(CLOCK_THREAD_CPUTIME_ID,&b);
  double ns=(b.tv_sec-a.tv_sec)*1e9+b.tv_nsec-a.tv_nsec;
  printf("bench variant=%s iterations=%u ns=%.6f checksum=%08x,%08x\\n",argv[1],n,ns/n,MEM32(1536),MEM32(1548));return 0;
 }
 int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};unsigned cases=0;
 for(unsigned m=0;m<4;m++){assert(!fesetround(modes[m]));for(unsigned t=0;t<12000;t++){
  for(unsigned i=0;i<sizeof initial;i+=4){uint32_t v=rng();if(t%2)v=(v&0x807fffff)|((115+rng()%25)<<23);memcpy(initial+i,&v,4);}
  uint32_t r[13],actual[13];double st[8];int64_t si[8];
  setup(t);feclearexcept(FE_ALL_EXCEPT);reference0();regs(r);memcpy(expected,memory,sizeof memory);memcpy(st,g_st,sizeof st);memcpy(si,g_st_i64,sizeof si);int top=g_fp_top,flags=fetestexcept(FE_ALL_EXCEPT);
  setup(t);feclearexcept(FE_ALL_EXCEPT);candidate0();regs(actual);
  int memdiff=memcmp(expected,memory,sizeof memory),regdiff=memcmp(r,actual,sizeof r),stdiff=memcmp(st,g_st,sizeof st),sidiff=memcmp(si,g_st_i64,sizeof si);
  if(memdiff||regdiff||stdiff||sidiff||top!=g_fp_top||flags!=fetestexcept(FE_ALL_EXCEPT)){fprintf(stderr,"FAIL mode=%u trial=%u memory=%d regs=%d st=%d shadow=%d top=%d/%d flags=%x/%x\\n",m,t,memdiff,regdiff,stdiff,sidiff,top,g_fp_top,flags,fetestexcept(FE_ALL_EXCEPT));return 1;}
  cases++;
 }}printf("PASS %u exact fused-block comparisons including four original helper calls\\n",cases);return 0;
}
''')
(out/'block.txt').write_text(block)
print('Staged full skin block and separate reference/candidate/driver')
