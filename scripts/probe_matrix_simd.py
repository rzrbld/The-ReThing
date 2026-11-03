#!/usr/bin/env python3
"""Isolated SIMD experiment from the actual translated matrix instruction order."""
import hashlib,json,re
from pathlib import Path
p=Path(__file__).resolve().parents[1]
s=(p/'build/translated-local-regs/thething/functions_0120.c').read_text()
lines=s.splitlines();first=next(i for i,l in enumerate(lines) if '0x005B7F80: mov' in l);last=next(i for i in range(first,len(lines)) if '0x005B8188:' in lines[i]);code='\n'.join(lines[first:last+1])+'\n'
stack=[];outputs=[];peak=0
for line in code.splitlines()[1:]:
 if 'fp_push(' in line:
  m=re.search(r'MEM32\(([^)]+)\)',line);assert m,line;stack.append(('load',m[1]));peak=max(peak,len(stack))
 elif 'FP_ST(0) *=' in line:
  m=re.search(r'ADDR\(([^)]+)\)',line);assert m,line;stack[-1]=('mul',stack[-1],('load',m[1]))
 elif 'FP_ST(0) +=' in line:
  b=stack.pop();a=stack.pop();stack.append(('add',a,b))
 elif 'fstp' in line:
  m=re.search(r'ADDR\(([^)]+)\)',line);assert m,line;outputs.append((m[1],stack.pop()))
 else:raise AssertionError(line)
assert len(outputs)==16 and peak==2 and not stack
prefix='''#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include <assert.h>
#include <fenv.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#if defined(__aarch64__)
#include <arm_neon.h>
typedef float64x2_t Pair;
static Pair pair(double a,double b){double v[2]={a,b};return vld1q_f64(v);}
#define MUL(a,b) vmulq_f64(a,b)
#define ADD(a,b) vaddq_f64(a,b)
static void unpack(double *out,Pair v){vst1q_f64(out,v);}
#else
typedef double Pair __attribute__((vector_size(16)));
static Pair pair(double a,double b){return (Pair){a,b};}
#define MUL(a,b) ((a)*(b))
#define ADD(a,b) ((a)+(b))
static void unpack(double *out,Pair v){memcpy(out,&v,16);}
#endif
uint32_t g_eax,g_ecx,g_edx,g_esp,g_esi,g_ebx,g_edi,g_ebp;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;
ptrdiff_t g_mem_base;
static unsigned char mem[4096],initial[4096],expected[4096];
static uint32_t eax,ecx,edx,esp;
static unsigned fallback_count,fast_count;
static uint32_t seed=0x12389abc;
static uint32_t next(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
'''
text=prefix+'__attribute__((noinline)) static void reference(void){\n'+code+'}\n'
text+='''static int eligible(void){
 if(((uint64_t)ecx<(uint64_t)eax+64&&(uint64_t)eax<(uint64_t)ecx+64)||((uint64_t)ecx<(uint64_t)edx+64&&(uint64_t)edx<(uint64_t)ecx+64))return 0;
 for(unsigned i=0;i<16;i++)if((MEM32(eax+i*4)&0x7f800000u)==0x7f800000u||(MEM32(edx+i*4)&0x7f800000u)==0x7f800000u)return 0;
 return 1;
}
__attribute__((noinline)) static void candidate(void){
 eax=MEM32(esp+4);
 if(!eligible()){fallback_count++;reference();return;}fast_count++;
'''
def expr(a,b):
 assert a[0]==b[0]
 if a[0]=='load':return f'pair((double)*(float*)ADDR({a[1]}),(double)*(float*)ADDR({b[1]}))'
 return f'{a[0].upper()}({expr(a[1],b[1])},{expr(a[2],b[2])})'
for i in range(0,16,2):
 a,b=outputs[i:i+2]
 text+=f'    {{Pair v={expr(a[1],b[1])};double out[2];unpack(out,v);*(float*)ADDR({a[0]})=(float)out[0];*(float*)ADDR({b[0]})=(float)out[1];}}\n'
text+=''' for(unsigned i=1;i<=2;i++){unsigned slot=(FP_TOS_SLOT-i)&7u;g_st[slot]=0.0;g_st_i64[slot]=0;}
}
static void setup(unsigned trial,int top){
 memcpy(mem,initial,sizeof mem);g_mem_base=(ptrdiff_t)mem;eax=123;edx=512;ecx=1024;esp=2048;MEM32(esp+4)=256;
 switch(trial%8){case 0:ecx=256;break;case 1:ecx=512;break;case 2:ecx=260;break;case 3:ecx=508;break;case 4:edx=256;break;}
 for(unsigned i=0;i<8;i++){uint64_t x=0x3ff0000000000000ULL+i;memcpy(g_st+i,&x,8);g_st_i64[i]=0x123456789012345LL+i;}
 g_fp_top=top;
}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
 const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};unsigned cases=0;
 const uint32_t specials[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345};
 for(unsigned mode=0;mode<4;mode++){assert(!fesetround(modes[mode]));
 for(unsigned trial=0;trial<24000;trial++){
  for(unsigned k=0;k<sizeof initial;k+=4){uint32_t v=next();if(trial%4==0)v=(v&0x807fffff)|((115+next()%25)<<23);else if(trial%4==1)v=specials[next()%(sizeof specials/sizeof specials[0])];else if(trial%4==2)v=specials[next()%8];memcpy(initial+k,&v,4);}
  int top=(int)(next()%25)-8;setup(trial,top);feclearexcept(FE_ALL_EXCEPT);reference();
  memcpy(expected,mem,sizeof mem);double st[8];int64_t si[8];memcpy(st,g_st,sizeof st);memcpy(si,g_st_i64,sizeof si);
  uint32_t regs[]={eax,ecx,edx,esp};int flags=fetestexcept(FE_ALL_EXCEPT),end_top=g_fp_top;
  setup(trial,top);feclearexcept(FE_ALL_EXCEPT);candidate();uint32_t actual[]={eax,ecx,edx,esp};
  if(memcmp(expected,mem,sizeof mem)||memcmp(st,g_st,sizeof st)||memcmp(si,g_st_i64,sizeof si)||memcmp(regs,actual,sizeof regs)||g_fp_top!=end_top||fetestexcept(FE_ALL_EXCEPT)!=flags){
   printf("FAIL mode=%u trial=%u memory=%d stack=%d shadows=%d regs=%d exceptions=%x/%x\\n",mode,trial,memcmp(expected,mem,sizeof mem),memcmp(st,g_st,sizeof st),memcmp(si,g_st_i64,sizeof si),memcmp(regs,actual,sizeof regs),flags,fetestexcept(FE_ALL_EXCEPT));return 1;
  }cases++;
 }}
 printf("PASS %u exact cases; fast=%u fallback=%u; four rounding modes, full memory/register/x87 state and FP exception checks\\n",cases,fast_count,fallback_count);
 fesetround(FE_TONEAREST);for(unsigned i=0;i<sizeof initial;i+=4){float v=.125f;memcpy(initial+i,&v,4);}
 for(unsigned pass=0;pass<4;pass++){int simd=pass==1||pass==2;setup(5,0);void(*fn)(void)=simd?candidate:reference;double start=seconds();for(unsigned i=0;i<1000000;i++)fn();printf("pass=%u %s ns_per_call=%.3f\\n",pass,simd?"SIMD":"reference",(seconds()-start)*1e3);}
 return 0;
}
'''
b=p/'build/matrix-simd';b.mkdir(exist_ok=True);(b/'probe.c').write_text(text)
e=p/'docs/evidence/matrix-simd';e.mkdir(exist_ok=True);(e/'source.json').write_text(json.dumps({'source_block_sha256':hashlib.sha256(code.encode()).hexdigest(),'outputs':outputs,'max_stack_depth':peak},indent=2)+'\n')
# Build a helper with exactly the tested arithmetic; opt-in and isolated only.
helper=text[:text.index('uint32_t g_eax')].replace('#include <assert.h>','#include <assert.h>\n#include <stdlib.h>')
elig=text[text.index('static int eligible'):text.index('__attribute__((noinline)) static void candidate')]
elig=elig.replace('eligible(void)','eligible(uint32_t eax,uint32_t edx,uint32_t ecx)')
body=text[text.index(' eax=MEM32(esp+4);',text.index('static void candidate')):text.index('static void setup')]
body=body.replace(' eax=MEM32(esp+4);','').replace(' if(!eligible()){fallback_count++;reference();return;}fast_count++;',' if(!eligible(eax,edx,ecx))return 0;')
body=body.rsplit('}',1)[0]+' return 1;\n}\n'
helper+=elig+"""__attribute__((optimize("O2"))) int thing_matrix_simd(uint32_t eax,uint32_t edx,uint32_t ecx){
 static int enabled=-1;if(enabled<0){const char *v=getenv("THING_MATRIX_SIMD");enabled=v&&!strcmp(v,"1");if(enabled)fprintf(stderr,"[matrix-simd] experimental two-lane double arithmetic enabled\\n");}
 if(!enabled)return 0;
"""+body
(b/'helper.c').write_text(helper)
start=text.index('__attribute__((noinline)) static void candidate')
end=text.index('static void setup',start)
external=text[:start]+"""extern int thing_matrix_simd(uint32_t,uint32_t,uint32_t);
__attribute__((noinline)) static void candidate(void){
 eax=MEM32(esp+4);if(thing_matrix_simd(eax,edx,ecx))fast_count++;else{fallback_count++;reference();}
}
"""+text[end:]
external=external.replace('#include <assert.h>','#include <assert.h>\n#include <stdlib.h>').replace('int main(void){','int main(void){setenv("THING_MATRIX_SIMD","1",1);')
(b/'external-probe.c').write_text(external)
print(b/'probe.c')
