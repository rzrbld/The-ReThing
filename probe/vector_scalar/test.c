#define RECOMP_GENERATED_CODE 1
#define RECOMP_LOCAL_REGS 1
#include "recomp_types.h"
#include <stdio.h>
#include <string.h>
#include <fenv.h>
#include <time.h>
#include <assert.h>
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
static unsigned char memory[4096],initial[4096],expected[4096];
static unsigned seed=711;
static unsigned rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
extern void reference0(void),reference1(void),candidate0(void),candidate1(void);
static void setup(unsigned t,unsigned kind){
 memcpy(memory,initial,sizeof memory);g_mem_base=(ptrdiff_t)memory;
 g_eax=32;g_ecx=1536;g_edx=512;g_esp=2048;g_esi=t;g_edi=17;g_ebx=128;g_ebp=256;g_cur_func=123;
 if(t%3==0)g_ecx=512;if(t%5==0)g_ecx=516;if(t%7==0)g_ecx=260;if(t%11==0)g_ecx=2048;
 if(kind==0)MEM32(g_esp+4)=256;g_fp_top=(int)(t%25)-8;g_fpu_cw=0x23f;
 g_flag_a=t;g_flag_b=~t;g_flag_cf=t&1;g_flag_k=FK_CMP;
 for(unsigned i=0;i<8;i++){g_st[i]=i*.125;g_st_i64[i]=t+i;}
}
static void regs(uint32_t*r){uint32_t v[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_a,g_flag_b,g_flag_cf,g_flag_k};memcpy(r,v,sizeof v);}
int main(int argc,char**argv){
 void(*ref[2])(void)={reference0,reference1},(*cand[2])(void)={candidate0,candidate1};
 if(argc==4){unsigned kind=atoi(argv[1]),variant=atoi(argv[2]),n=atoi(argv[3]);for(unsigned i=0;i<4096;i+=4){float f=.125;memcpy(initial+i,&f,4);}setup(1,kind);struct timespec a,b;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&a);for(unsigned i=0;i<n;i++){g_esp=2048;(variant?cand[kind]:ref[kind])();}clock_gettime(CLOCK_THREAD_CPUTIME_ID,&b);printf("%u %u %.4f %08x\n",kind,variant,((b.tv_sec-a.tv_sec)*1e9+b.tv_nsec-a.tv_nsec)/n,MEM32(1536));return 0;}
 int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};unsigned edges[]={0,0x80000000,1,0x80000001,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0x7fa12345};
 for(unsigned kind=0;kind<2;kind++)for(unsigned m=0;m<4;m++){assert(!fesetround(modes[m]));for(unsigned t=0;t<50000;t++){
  for(unsigned i=0;i<4096;i+=4){unsigned v=rnd();if(t%3==0)v=edges[rnd()%10];else if(t%3==1)v=(v&0x807fffff)|((100+rnd()%50)<<23);memcpy(initial+i,&v,4);}
  uint32_t r[13],actual[13];double st[8];int64_t si[8];setup(t,kind);feclearexcept(FE_ALL_EXCEPT);ref[kind]();regs(r);memcpy(expected,memory,4096);memcpy(st,g_st,64);memcpy(si,g_st_i64,64);int flags=fetestexcept(FE_ALL_EXCEPT),top=g_fp_top;
  setup(t,kind);feclearexcept(FE_ALL_EXCEPT);cand[kind]();regs(actual);
  if(memcmp(expected,memory,4096)||memcmp(r,actual,sizeof r)||memcmp(st,g_st,64)||memcmp(si,g_st_i64,64)||top!=g_fp_top||flags!=fetestexcept(FE_ALL_EXCEPT)){printf("FAIL kind=%u mode=%u trial=%u mem=%d regs=%d st=%d shadow=%d flags=%x/%x\n",kind,m,t,memcmp(expected,memory,4096),memcmp(r,actual,sizeof r),memcmp(st,g_st,64),memcmp(si,g_st_i64,64),flags,fetestexcept(FE_ALL_EXCEPT));return 1;}
 }}puts("PASS 400000 exact comparisons: memory, registers, flags, FP stack/shadows, exceptions");return 0;
}
