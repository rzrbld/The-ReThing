#!/usr/bin/env python3
"""Generate an isolated differential probe; does not modify game translation."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'build/translated-local-regs/thething/functions_0235.c').read_text()
def extract(first,last):
 lines=s.splitlines();a=next(i for i,l in enumerate(lines) if f'0x{first}:' in l);b=next(i for i in range(a,len(lines)) if f'0x{last}:' in lines[i]);return '\n'.join(lines[a:b+1])+'\n'
def localize(code):
 depth=high=0;out=[]
 for line in code.splitlines():
  assert not any(k in line for k in ['RECOMP_CALL','goto ','L_','FP_SI','fp_xch'])
  if 'fp_push(' in line:
   m=re.fullmatch(r'\s*fp_push\((.*)\);(.*)',line);assert m,line
   line=f'    skin_fp{depth}=(double)({m[1]});{m[2]}';depth+=1;high=max(high,depth)
  elif 'fp_pop()' in line:
   assert depth>0;depth-=1;line=line.replace('fp_pop()',f'skin_fp{depth}')
  line=line.replace('FP_ST(0)',f'skin_fp{depth-1}')
  assert 'fp_pop' not in line and 'fp_push' not in line and 'FP_ST' not in line
  out.append(line)
 assert depth==0
 # Popped ring slots are cleared by the current exact helper; keep that state.
 return '    volatile double '+','.join(f'skin_fp{i}' for i in range(high))+';\n'+'\n'.join(out)+f'\n    for(unsigned i=1;i<={high};i++){{unsigned slot=(FP_TOS_SLOT-i)&7u;g_st[slot]=0.0;g_st_i64[slot]=0;}}\n'
blocks=[extract('007AEBBA','007AEC3E'),extract('007AEC65','007AECC1')]
# Position block has an internal conditional flag expression but no control transfer.
prefix='''#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include <assert.h>
#include <fenv.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_esi,g_ebx,g_edi,g_ebp;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;
ptrdiff_t g_mem_base;
static unsigned char mem[4096],initial[4096],expected[4096];
static uint32_t seed=0x12389abc;
static uint32_t next(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static uint32_t eax,ecx,edx,esp,esi,ebx,_cf,_flag_a,_flag_b,_flag_k;
'''
text=prefix+'''static int finite_inputs(uint32_t vector,uint32_t matrix,uint32_t stack){
 if((vector<stack+0x90&&vector+12>stack-4)||(matrix<stack+0x90&&matrix+64>stack-4))return 0;
 for(unsigned k=0;k<3;k++)if((MEM32(vector+k*4)&0x7f800000u)==0x7f800000u)return 0;
 for(unsigned k=0;k<16;k++)if((MEM32(matrix+k*4)&0x7f800000u)==0x7f800000u)return 0;
 return 1;
}
'''
for i,code in enumerate(blocks):
 text+=f'__attribute__((noinline)) static void reference{i}(void){{\n{code}}}\n'
 matrix='esi' if i else '((esi<<6)+MEM32(MEM32(esp+0x58)+0x3c))'
 text+=f'__attribute__((noinline)) static void candidate{i}(void){{\nif(!finite_inputs(eax,{matrix},esp)){{reference{i}();return;}}\n{localize(code)}}}\n'
text+='''static void setup(unsigned trial,int top,unsigned block){
 memcpy(mem,initial,sizeof mem);g_mem_base=(uintptr_t)mem;
 eax=256;esi=block?512:0;esp=2048;ebx=128;ecx=64;edx=96;
 /* Position block resolves its matrix via a signed bone index and pointer chain. */
 MEM16(ebx)=0;MEM32(esp+0x58)=768;MEM32(768+0x3c)=512;
 MEM32(esp+0x28)=1024;MEM32(esp+0x4c)=0x3f800000;
 /* Some output/input overlap variants exercise the original load/store ordering. */
 if(trial%5==0)eax=esp+0x5c;
 if(trial%7==0)MEM32(768+0x3c)=esp+0x60;
 for(unsigned k=0;k<8;k++){uint64_t bits=0x3ff0000000000000ULL+k;memcpy(g_st+k,&bits,8);g_st_i64[k]=0x123456789012345LL+k;}
 g_fp_top=top;_cf=1;_flag_a=13;_flag_b=17;_flag_k=19;
}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
 const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};unsigned cases=0;
 for(unsigned block=0;block<2;block++)for(unsigned mode=0;mode<4;mode++){
  assert(fesetround(modes[mode])==0);
  for(unsigned trial=0;trial<12000;trial++){
   for(unsigned k=0;k<sizeof initial;k+=4){uint32_t v=next();if(trial%2)v=(v&0x807fffff)|((115+next()%25)<<23);memcpy(initial+k,&v,4);}
   int top=(int)(next()%25)-8;setup(trial,top,block);feclearexcept(FE_ALL_EXCEPT);
   if(block)reference1();else reference0();
   memcpy(expected,mem,sizeof mem);double st[8];int64_t si[8];memcpy(st,g_st,sizeof st);memcpy(si,g_st_i64,sizeof si);
   uint32_t regs[]={eax,ecx,edx,esp,esi,ebx,_cf,_flag_a,_flag_b,_flag_k};int flags=fetestexcept(FE_ALL_EXCEPT),end_top=g_fp_top;
   setup(trial,top,block);feclearexcept(FE_ALL_EXCEPT);if(block)candidate1();else candidate0();
   uint32_t actual[]={eax,ecx,edx,esp,esi,ebx,_cf,_flag_a,_flag_b,_flag_k};
   if(memcmp(expected,mem,sizeof mem)||memcmp(st,g_st,sizeof st)||memcmp(si,g_st_i64,sizeof si)||memcmp(regs,actual,sizeof regs)||g_fp_top!=end_top||fetestexcept(FE_ALL_EXCEPT)!=flags){printf("FAIL block=%u mode=%u trial=%u memory=%d stack=%d shadows=%d regs=%d fpflags=%x/%x\\n",block,mode,trial,memcmp(expected,mem,sizeof mem),memcmp(st,g_st,sizeof st),memcmp(si,g_st_i64,sizeof si),memcmp(regs,actual,sizeof regs),flags,fetestexcept(FE_ALL_EXCEPT));for(unsigned j=0;j<sizeof mem;j+=4)if(memcmp(expected+j,mem+j,4)){uint32_t a,b;memcpy(&a,expected+j,4);memcpy(&b,mem+j,4);printf("offset=%x expected=%08x actual=%08x\\n",j,a,b);}return 1;}
   cases++;
  }
 }
 fesetround(FE_TONEAREST);printf("PASS %u exact comparisons: memory, registers/flags, x87 slots/shadows/top, FP exceptions; four rounding modes, exceptional floats and overlapping input/output\\n",cases);
 memset(initial,0,sizeof initial);for(unsigned k=0;k<sizeof initial;k+=4){float v=.125f;memcpy(initial+k,&v,4);}
 for(unsigned block=0;block<2;block++)for(unsigned candidate=0;candidate<2;candidate++){
  setup(1,0,block);void(*fn)(void)=block?(candidate?candidate1:reference1):(candidate?candidate0:reference0);
  double start=seconds();for(unsigned n=0;n<1000000;n++){eax=256;esi=block?512:0;esp=2048;fn();}
  printf("block=%u %s ns_per_call=%.3f\\n",block,candidate?"local":"ring",(seconds()-start)*1e9/1000000);
 }
 return 0;
}
'''
b=ROOT/'build/skin-locals-probe';b.mkdir(exist_ok=True);(b/'probe.c').write_text(text)
(ROOT/'docs/evidence/skin-locals/blocks.json').write_text(json.dumps({'scope':'Isolated candidate only; not applied to shipped game','source_block_sha256':[hashlib.sha256(x.encode()).hexdigest() for x in blocks]},indent=2)+'\n')
print(b/'probe.c')
