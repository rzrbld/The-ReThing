#define RECOMP_GENERATED_CODE 1
#define RECOMP_LOCAL_REGS 1
#include "recomp_types.h"
#include <assert.h>
#include <fenv.h>
#include "fpstate.h"
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
static unsigned char memory[32768],initial[32768],expected[32768];
static uint32_t seed=1234567;
static uint32_t rng(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
extern void reference0(void),candidate0(void),skin_memo_reset(void),skin_memo_stats(uint64_t*);
static void test_fp(unsigned trial,unsigned variant){
 feclearexcept(FE_ALL_EXCEPT);
#if defined(__aarch64__)
 uint64_t cr,sr=(trial&1)?0x9f:0;
 __asm__ volatile("mrs %0, fpcr":"=r"(cr));
 cr &= ~((1ull<<24)|(1ull<<25));if(variant==9)cr|=(1ull<<24)|(1ull<<25);
 __asm__ volatile("msr fpcr, %0"::"r"(cr));__asm__ volatile("msr fpsr, %0"::"r"(sr));
#else
 uint32_t mx;__asm__ volatile("stmxcsr %0":"=m"(mx));
 mx &= ~((1u<<15)|(1u<<6)|63u);if(variant==9)mx|=(1u<<15)|(1u<<6);if(trial&1)mx|=63;
 __asm__ volatile("ldmxcsr %0"::"m"(mx));
#endif
}
static void setup(unsigned trial,unsigned variant){
 memcpy(memory,initial,sizeof memory);g_mem_base=(ptrdiff_t)memory;
 g_eax=256;g_ecx=64;g_edx=96;g_esp=24000;g_esi=512;g_ebx=8192+(trial%32)*128+((trial/512)%2)*4096;g_edi=trial;g_ebp=((trial/32)%16)*64;g_cur_func=0x777777;
 if(trial%11==0)g_eax=g_esp+0x150;
 if(trial%13==0)g_esi=g_esp-8;
 MEM32(g_ebx+0x3c)=2048;MEM32(g_esp+0x18)=0;MEM8(g_esp+0x13)=1;
 /* Later variants mutate one independently keyed input, or output palette only. */
 if(trial%19==0)MEM32(g_eax)=1;
 if(variant==2)MEM32(g_eax)^=1;
 if(variant==3)MEM32(g_esi+g_ebp)^=1;
 if(variant==4)MEM32(g_esp+0xD0)^=1;
 if(variant==5)MEM32(2048+g_ebp)^=0x123;
 if(variant==6)MEM32(g_eax)=0x7fc00001;
 if(variant==7)MEM32(g_eax)=0x7f800000;
 if(variant==8)g_ebp=((g_ebp+64)%1024);
 g_flag_a=trial;g_flag_b=~trial;g_flag_cf=trial&1;g_flag_k=FK_CMP;
 g_fp_top=(int)(trial%7);
 static const uint16_t pc[]={0,0x200,0x300};
 g_fpu_cw=(uint16_t)(0x3f|pc[(trial/2)%3]|((trial/6)%4)*0x400|((trial%2)*0x40));
 if(trial%17==0)g_fpu_cw=0x023e; /* unmasked invalid exception */
 if(trial%29==0)g_fpu_cw=0x013f; /* reserved precision */
 for(unsigned i=0;i<8;i++){g_st[i]=.125*i;g_st_i64[i]=trial+i;}
}
static void regs(uint32_t*r){uint32_t v[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_a,g_flag_b,g_flag_cf,g_flag_k};memcpy(r,v,sizeof v);}
static void test_control_predicate(void){
 assert(skin_guest_cw_supported(0x023f));assert(skin_guest_cw_supported(0x027f));
 assert(skin_guest_cw_supported(0x033f));assert(skin_guest_cw_supported(0x037f));
 const unsigned precision[]={0,0x200,0x300};
 for(unsigned r=0;r<4;r++)for(unsigned p=0;p<3;p++)for(unsigned b=0;b<2;b++)assert(skin_guest_cw_supported(0x3f|precision[p]|(r<<10)|(b<<6)));
 for(unsigned b=0;b<6;b++)assert(!skin_guest_cw_supported(0x023f&~(1u<<b)));
 assert(!skin_guest_cw_supported(0x013f));
#if defined(__aarch64__)
 const uint64_t traps=0x9f00;
 assert(skin_host_fp_masked_control(0));
 assert(skin_host_fp_masked_control((3ull<<22)|(3ull<<24)));
 for(unsigned bit=0;bit<32;bit++)if(traps&(1ull<<bit))assert(!skin_host_fp_masked_control(1ull<<bit));
#else
 const uint64_t masked=((uint64_t)0x37f<<32)|0x1f80;
 assert(skin_host_fp_masked_control(masked));
 assert(skin_host_fp_masked_control(masked|(3u<<13)|(1u<<15)|(1u<<6)));
 for(unsigned bit=7;bit<=12;bit++)assert(!skin_host_fp_masked_control(masked&~(1ull<<bit)));
 for(unsigned bit=32;bit<38;bit++)assert(!skin_host_fp_masked_control(masked&~(1ull<<bit)));
#endif
}
int main(void){
 test_control_predicate();
 int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};unsigned cases=0;uint64_t hits=0,misses=0,fallbacks=0;
 for(unsigned m=0;m<4;m++){assert(!fesetround(modes[m]));skin_memo_reset();for(unsigned t=0;t<1200;t++){
  for(unsigned i=0;i<sizeof initial;i+=4){uint32_t v=rng();v=(v&0x807fffff)|((115+rng()%25)<<23);memcpy(initial+i,&v,4);}
  for(unsigned variant=0;variant<10;variant++)for(unsigned repeat=0;repeat<2;repeat++){
   uint32_t r[13],actual[13];double st[8];int64_t si[8];
   setup(t,variant);test_fp(t,variant);reference0();regs(r);memcpy(expected,memory,sizeof memory);memcpy(st,g_st,sizeof st);memcpy(si,g_st_i64,sizeof si);int top=g_fp_top,flags=fetestexcept(FE_ALL_EXCEPT);
   uint64_t cr,sr,ac,as;skin_host_fp(&cr,&sr);
   setup(t,variant);test_fp(t,variant);candidate0();regs(actual);
   skin_host_fp(&ac,&as);
   int md=memcmp(expected,memory,sizeof memory),rd=memcmp(r,actual,sizeof r),sd=memcmp(st,g_st,sizeof st),id=memcmp(si,g_st_i64,sizeof si);
   if(cr!=ac||sr!=as||md||rd||sd||id||top!=g_fp_top||flags!=fetestexcept(FE_ALL_EXCEPT)){
    fprintf(stderr,"native control=%llx/%llx status=%llx/%llx\n",(unsigned long long)cr,(unsigned long long)ac,(unsigned long long)sr,(unsigned long long)as);
    fprintf(stderr,"FAIL mode=%u trial=%u variant=%u repeat=%u mem=%d regs=%d st=%d i64=%d flags=%x/%x\n",m,t,variant,repeat,md,rd,sd,id,flags,fetestexcept(FE_ALL_EXCEPT));
    for(unsigned i=0;i<sizeof memory;i+=4)if(memcmp(expected+i,memory+i,4))fprintf(stderr,"mem %x: %08x/%08x\n",i,*(uint32_t*)(expected+i),*(uint32_t*)(memory+i));
    for(unsigned i=0;i<13;i++)if(r[i]!=actual[i])fprintf(stderr,"reg %u: %x/%x\n",i,r[i],actual[i]);return 1;
   }cases++;
  }
 }uint64_t counts[3];skin_memo_stats(counts);hits+=counts[0];misses+=counts[1];fallbacks+=counts[2];
 }assert(hits&&misses&&fallbacks);printf("PASS %u exact span comparisons: hits=%llu misses=%llu fallbacks=%llu; four rounding modes, changed inputs/palette, aliases, NaN/Inf, nondefault CW,512colliding keys and record reuse\n",cases,(unsigned long long)hits,(unsigned long long)misses,(unsigned long long)fallbacks);return 0;
}
