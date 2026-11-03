/* Isolated experiment: exact-input memo only for 7AE710 palette pair. */
#include <fenv.h>
#include "fpstate.h"
#ifndef SKIN_MEMO_SLOTS
#define SKIN_MEMO_SLOTS 512
#endif
#if SKIN_MEMO_SLOTS < 1 || (SKIN_MEMO_SLOTS & (SKIN_MEMO_SLOTS-1))
#error SKIN_MEMO_SLOTS_must_be_power_of_two
#endif
struct skin_memo_entry {uint32_t valid,record,index;uint64_t control,status;uint16_t cw;uint32_t input[48],mid[16],result[16];};
static struct skin_memo_entry skin_memo[SKIN_MEMO_SLOTS];
static uint64_t skin_memo_hits,skin_memo_misses,skin_memo_fallbacks;
static uint64_t skin_memo_cw_fallbacks,skin_memo_alias_fallbacks,skin_memo_nonfinite_fallbacks,skin_memo_hostfp_fallbacks;
static unsigned skin_memo_first_cw,skin_memo_seen_cw;
/* Optional SDL clock: standalone differential tests need no SDL link. */
extern uint32_t SDL_GetTicks(void) __attribute__((weak));
static int skin_memo_report_enabled,skin_memo_tick_started;
static uint32_t skin_memo_last_tick;
static void skin_memo_report(void){fprintf(stderr,"[skin-palette-memo] attempts=%llu hits=%llu misses=%llu fallbacks=%llu bytes=%zu cw_fallback=%llu alias_fallback=%llu nonfinite_fallback=%llu first_cw=0x%x hostfp_fallback=%llu\n",(unsigned long long)(skin_memo_hits+skin_memo_misses+skin_memo_fallbacks),(unsigned long long)skin_memo_hits,(unsigned long long)skin_memo_misses,(unsigned long long)skin_memo_fallbacks,sizeof skin_memo,(unsigned long long)skin_memo_cw_fallbacks,(unsigned long long)skin_memo_alias_fallbacks,(unsigned long long)skin_memo_nonfinite_fallbacks,skin_memo_first_cw,(unsigned long long)skin_memo_hostfp_fallbacks);}
static int skin_memo_report_initialized;
static void skin_memo_periodic(void){
 uint64_t completed=skin_memo_hits+skin_memo_misses+skin_memo_fallbacks;
 if(!skin_memo_report_enabled || (completed&1023u) || !SDL_GetTicks)return;
 uint32_t now=SDL_GetTicks();
 if(!skin_memo_tick_started){skin_memo_tick_started=1;skin_memo_last_tick=now;return;}
 if((uint32_t)(now-skin_memo_last_tick)<5000u)return;
 skin_memo_last_tick=now;skin_memo_report();
}
void skin_memo_reset(void){memset(skin_memo,0,sizeof skin_memo);skin_memo_hits=skin_memo_misses=skin_memo_fallbacks=0;skin_memo_cw_fallbacks=skin_memo_alias_fallbacks=skin_memo_nonfinite_fallbacks=skin_memo_hostfp_fallbacks=0;skin_memo_first_cw=skin_memo_seen_cw=0;skin_memo_tick_started=0;skin_memo_last_tick=0;}
void skin_memo_stats(uint64_t *v){v[0]=skin_memo_hits;v[1]=skin_memo_misses;v[2]=skin_memo_fallbacks;}
static int skin_overlap(uint32_t a,unsigned n,uint32_t b,unsigned m){return (uint64_t)a < (uint64_t)b+m && (uint64_t)b < (uint64_t)a+n;}
static int skin_memo_begin(uint32_t record,uint32_t index,uint32_t bone,uint32_t bind,uint32_t sp,struct skin_memo_entry **pending){
 if(!skin_memo_report_initialized){skin_memo_report_initialized=1;const char *v=getenv("THING_SKIN_PALETTE_MEMO_STATS");if(v&&atoi(v)){skin_memo_report_enabled=1;atexit(skin_memo_report);}}
 skin_memo_periodic();
 *pending=0;
 if(!skin_memo_seen_cw){skin_memo_seen_cw=1;skin_memo_first_cw=g_fpu_cw;}
 if(!skin_guest_cw_supported(g_fpu_cw)){skin_memo_fallbacks++;skin_memo_cw_fallbacks++;return 0;}
 /* Scratch writes include call argument/return and contiguous matrix slots. */
 if(skin_overlap(bone,64,sp-8,8) || skin_overlap(bind,64,sp-8,8) || skin_overlap(bone,64,sp+0x90,0x100) || skin_overlap(bind,64,sp+0x90,0x100)) {skin_memo_fallbacks++;skin_memo_alias_fallbacks++;return 0;}
 uint32_t input[48];memcpy(input,(void*)ADDR(bone),64);memcpy(input+16,(void*)ADDR(bind),64);memcpy(input+32,(void*)ADDR(sp+0xD0),64);
 for(unsigned i=0;i<48;i++)if((input[i]&0x7f800000)==0x7f800000){skin_memo_fallbacks++;skin_memo_nonfinite_fallbacks++;return 0;}
 uint64_t control,status;if(!skin_host_fp(&control,&status)){skin_memo_fallbacks++;skin_memo_hostfp_fallbacks++;return 0;}
 struct skin_memo_entry *e=&skin_memo[((record>>3)^(index>>6))&(SKIN_MEMO_SLOTS-1)];
 if(e->valid && e->record==record && e->index==index && e->control==control && e->status==status && e->cw==g_fpu_cw && !memcmp(e->input,input,sizeof input)){
  memcpy((void*)ADDR(sp+0x150),e->mid,64);memcpy((void*)ADDR(sp+0x110),e->mid,64);memcpy((void*)ADDR(sp+0x90),e->result,64);
  for(unsigned n=1;n<=2;n++){unsigned k=(8u-(unsigned)g_fp_top-n)&7u;g_st[k]=0;g_st_i64[k]=0;}
  skin_memo_hits++;return 1;
 }
 e->valid=0;e->record=record;e->index=index;e->control=control;e->status=status;e->cw=g_fpu_cw;memcpy(e->input,input,sizeof input);*pending=e;skin_memo_misses++;return 0;
}
static void skin_memo_end(struct skin_memo_entry *e,uint32_t sp){if(!e)return;memcpy(e->mid,(void*)ADDR(sp+0x150),64);memcpy(e->result,(void*)ADDR(sp+0x90),64);uint64_t control,status;int supported=skin_host_fp(&control,&status);e->valid=(supported && control==e->control && status==e->status);}
