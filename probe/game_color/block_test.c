#include <time.h>
#include "block_reference.inc"
#include "block_candidate.inc"
static unsigned char memory[0x860000];
typedef struct{uint32_t regs[13],flags[4];double st[8];int64_t si[8];uint16_t cw;int top,exceptions;unsigned char mem[384];} Snapshot;
static void snap(Snapshot *s){memset(s,0,sizeof *s);uint32_t regs[]={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func,g_flag_k,g_flag_a,g_flag_b,g_flag_cf};memcpy(s->regs,regs,sizeof regs);memcpy(s->flags,end_flags,sizeof end_flags);memcpy(s->st,g_st,sizeof g_st);memcpy(s->si,g_st_i64,sizeof g_st_i64);s->cw=g_fpu_cw;s->top=g_fp_top;s->exceptions=fetestexcept(FE_ALL_EXCEPT);memcpy(s->mem,memory+0xf80,384);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec*1e-9;}
static uint32_t rng=0x811984;static unsigned random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void setup(unsigned color,unsigned cw,unsigned top,unsigned seed,unsigned prior){
 rng=seed;for(unsigned i=0;i<384;i++)memory[0xf80+i]=random32();
 g_esp=0x1000;g_edi=0x1080;g_eax=random32();g_ecx=random32();g_edx=random32();g_ebx=random32();g_esi=random32();g_ebp=random32();g_cur_func=0x75cef0;
 g_flag_k=FK_TEST;g_flag_a=random32();g_flag_b=random32();g_flag_cf=1;
 for(unsigned i=0;i<8;i++){g_st[i]=random32()*.125;g_st_i64[i]=(int64_t)random32()<<16;}
 g_fp_top=top;g_fpu_cw=cw;MEM32(g_edi+0x44)=color;feclearexcept(FE_ALL_EXCEPT);if(prior)feraiseexcept(FE_DIVBYZERO);
}
int main(void){g_mem_base=(ptrdiff_t)memory;MEM32(0x844aa0)=0x3b808081;MEM32(0x84177c)=0x3f000000;MEM32(0x851db8)=0x437f0000;
 for(unsigned x=0;x<256;x++){setup(x*0x10101,0x37f,0,1,0);original();if(color_byte[x]!=(g_eax&255)||red_root_bits[x]!=MEM32(g_esp+0x10))return 6;if(((g_eax>>8)&255)!=color_byte[x]||((g_eax>>16)&255)!=color_byte[x])return 4;}
 FILE *f=fopen("/tmp/game_color_lut.h","w");fprintf(f,"static const unsigned char game_color_lut[256]={");for(unsigned x=0;x<256;x++)fprintf(f,"%s%u",x?",":"",color_byte[x]);fprintf(f,"};\nstatic const uint32_t game_color_red_root[256]={");for(unsigned x=0;x<256;x++)fprintf(f,"%s0x%08x",x?",":"",red_root_bits[x]);fprintf(f,"};\n");fclose(f);
 unsigned cases=0;int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
 for(unsigned host=0;host<4;host++)for(unsigned pc=0;pc<3;pc++)for(unsigned rc=0;rc<4;rc++)for(unsigned x=0;x<1024;x++){
 unsigned color=x<256?x*0x10101:random32(),seed=random32(),cw=0x7f|((pc==0?0:pc==1?2:3)<<8)|(rc<<10),top=x%6,prior=x&1;Snapshot a,b;
 fesetround(modes[host]);setup(color,cw,top,seed,prior);original();snap(&a);
 setup(color,cw,top,seed,prior);candidate();snap(&b);
 if(memcmp(&a,&b,sizeof a)){unsigned char *p=(void*)&a,*q=(void*)&b;for(unsigned i=0;i<sizeof a;i++)if(p[i]!=q[i]){printf("FAIL host=%u rc=%u color=%08x byte=%u ref=%u cand=%u exceptions=%x/%x\n",host,rc,color,i,p[i],q[i],a.exceptions,b.exceptions);break;}return 1;}cases++;}
 fesetround(FE_TONEAREST);
 for(unsigned mask=0;mask<63;mask++){Snapshot a,b;setup(0x81644723,0x340|mask,2,999,1);original();snap(&a);setup(0x81644723,0x340|mask,2,999,1);candidate();snap(&b);if(memcmp(&a,&b,sizeof a))return 5;cases++;}
 printf("full_state_cases=%u fast=%llu fallback=%llu PASS\n",cases,(unsigned long long)fast_count,(unsigned long long)fallback_count);
 puts("Compared registers, local/global lazy flags, all8 x87 value/shadow slots, depth/CW,384 scratch+packet bytes, and hostFE flags; includes priorDIVBYZERO,4hostRC,4guestRC,3guestPC,occupiedstack,63unmaskedguestCW fallbacks.");for(unsigned pass=0;pass<4;pass++){
 setup(0x123456,0x37f,0,1,0);double t=now();for(unsigned i=0;i<200000;i++){MEM32(g_edi+0x44)=i*7919u;original();}double ref=now()-t;
 setup(0x123456,0x37f,0,1,0);t=now();for(unsigned i=0;i<200000;i++){MEM32(g_edi+0x44)=i*7919u;candidate();}double cand=now()-t;
 printf("isolated_host pass=%u calls=200000 reference_ns=%.1f candidate_ns=%.1f (NOT gameplay or H700 timing)\n",pass,ref*5000,cand*5000);
 }
 return 0;
}
