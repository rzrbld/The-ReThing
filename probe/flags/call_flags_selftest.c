#define RECOMP_GENERATED_CODE
#define RECOMP_LOCAL_REGS
#include "recomp_types.h"
#include "proven_call.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
ptrdiff_t g_mem_base;
static unsigned char mem[4096];
static uint32_t seed=12389;
static uint32_t next(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
/* Exact lazy-flag emission pattern for xor/sub/test. Entry flag loads are
 * deliberately retained, as they are in the real staged candidate. */
static unsigned mode;
static void callee(void){
 uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=FK_NONE;
 RECOMP_REGS_LOCALS
 RECOMP_ENTER(0x00401000u);
 uint32_t in=MEM32(esp+4);
 if(mode==0){_flag_a=0;_flag_b=0;eax=0;_cf=0;_flag_k=FK_TEST;}
 else if(mode==1){_flag_a=eax;_flag_b=in;_cf=CMP_B(_flag_a,_flag_b);eax-=in;_flag_k=FK_CMP;}
 else{_flag_a=eax;_flag_b=in;_cf=0;_flag_k=FK_TEST;}
 ecx=TEST_Z(_flag_a,_flag_b)?17:29;
 MEM32(esp+8)=ecx;
 RECOMP_FLAGS_OUT();esp+=4;RECOMP_REGS_OUT();
}
static void caller(int fast,uint32_t a,uint32_t b,uint32_t k,uint32_t cf){
 uint32_t _cf=cf,_flag_a=a,_flag_b=b,_flag_k=k;
 RECOMP_REGS_LOCALS
 if(fast)RECOMP_CALL_FLAGS_KILLED(callee);else RECOMP_CALL(callee);
 RECOMP_FLAGS_IN();
 /* Observe returned flags before overwriting them. */
 edi=recomp_carry(_flag_k,_flag_a,_flag_b,_cf);
 RECOMP_FLAGS_OUT();RECOMP_REGS_OUT();
}
static void setup(uint32_t x,uint32_t y){
 memset(mem,0,sizeof mem);g_mem_base=(ptrdiff_t)mem;
 g_eax=x;g_ecx=1;g_edx=2;g_ebx=3;g_esi=4;g_edi=5;g_ebp=6;g_esp=2048;g_cur_func=0x00501000;
 MEM32(g_esp)=y;
 /* Different stale global flags must not affect the candidate. */
 g_flag_a=0x01234567;g_flag_b=0x89abcdef;g_flag_k=FK_ADD;g_flag_cf=1;
}
int main(void){unsigned cases=0;
 for(mode=0;mode<3;mode++)for(unsigned i=0;i<100000;i++){
  uint32_t x=next(),y=i%8?next():x,a=next(),b=next(),k=i%2?FK_TEST:FK_CMP,cf=i%2;
  setup(x,y);caller(0,a,b,k,cf);unsigned char expected[sizeof mem];memcpy(expected,mem,sizeof mem);
  uint32_t regs[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_a,g_flag_b,g_flag_k,g_flag_cf};
  setup(x,y);caller(1,a,b,k,cf);
  uint32_t actual[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_a,g_flag_b,g_flag_k,g_flag_cf};
  assert(!memcmp(expected,mem,sizeof mem)&&!memcmp(regs,actual,sizeof regs));cases++;
 }
 printf("PASS %u call-transfer cases: full guest memory/GPR/stack/returned flags, stale entry globals, restored caller identity\n",cases);
}
