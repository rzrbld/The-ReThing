#include <assert.h>
#include <string.h>
#include <stdlib.h>
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
ptrdiff_t g_mem_base;
static unsigned route,seen,mode;
static uint32_t trace[32];
static unsigned char arena[65536],initial[65536],expected[65536];
static uint32_t seed=1;
static uint32_t rng(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void state(uint32_t *s){
 uint32_t v[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_a,g_flag_b,g_flag_cf,g_flag_k};
 memcpy(s,v,sizeof v);
}
static void callee(void){
 state(trace+4);trace[17]=MEM32(g_esp);trace[18]=MEM32(g_esp+4);
 trace[19]=MEM32(g_esp+8);trace[20]=MEM32(g_esp+12);
 g_eax=0xabcdef00+mode;g_ecx=0x12456789;g_edx=0xfedcba98;
 g_ebx^=mode;g_esi+=mode;g_edi-=mode;g_ebp^=0x55555555;
 g_flag_a=0x80000000+mode;g_flag_b=mode;g_flag_cf=mode&1;g_flag_k=FK_CMP;
 MEM32(0x9000)=mode;g_cur_func=0xf00d;g_esp+=16;
}
static recomp_func_t lookup(uint32_t va,unsigned stage){trace[seen++]=stage;trace[3]=va;return route==stage?callee:NULL;}
recomp_func_t recomp_lookup_manual(uint32_t va){return lookup(va,1);}
recomp_func_t recomp_lookup(uint32_t va){return lookup(va,2);}
recomp_func_t recomp_lookup_import(uint32_t va){return lookup(va,3);}
static void setup(unsigned n){
 seed=n+1;mode=n;route=1+n%3;seen=0;memset(trace,0,sizeof trace);
 memcpy(arena,initial,sizeof arena);
 g_eax=rng();g_ecx=0x1000;g_edx=rng();g_ebx=rng();g_esp=0x8000;
 g_esi=rng();g_edi=rng();g_ebp=rng();g_cur_func=rng();
 g_flag_a=rng();g_flag_b=rng();g_flag_cf=rng()&1;g_flag_k=FK_CMP;
 MEM32(0x1538)=n%4?0x3000:0;MEM32(0x3000)=0x4000;MEM32(0x40c8)=0x12345678;
 MEM32(0x8004)=rng();MEM32(0x8008)=rng();
}
extern void candidate2(void);
int main(void){
 g_mem_base=(ptrdiff_t)arena;
 for(unsigned i=0;i<sizeof initial;i++)initial[i]=(unsigned char)rng();
 for(unsigned n=0;n<50000;n++){
  uint32_t before[32]={0},after[32]={0},calls[32];
  setup(n);reference2();state(before);memcpy(expected,arena,sizeof arena);memcpy(calls,trace,sizeof calls);unsigned count=seen;
  setup(n);candidate2();state(after);
  if(memcmp(before,after,sizeof before)||memcmp(expected,arena,sizeof arena)||memcmp(calls,trace,sizeof calls)||count!=seen){fprintf(stderr,"Mismatch trial %u\n",n);return 1;}
 }
 puts("PASS 50000 exact SetRenderState-wrapper cases: memory, GPRs, flags, lookup order and callee-entry state; null/manual/translated/import paths");
 return 0;
}
