/* Diagnostic only. Geometry identities tagged within one guest Present interval.
 * Render timings include called bridge/driver CPU, not asynchronous GPU time. */
#ifndef THING_EFFECT_ATTRIBUTION_H
#define THING_EFFECT_ATTRIBUTION_H
#include <stdint.h>
#include <time.h>
#include <stdio.h>
extern uint32_t SDL_GetTicks(void);
enum {EA_OTHER,EA_WEATHER,EA_PARTICLE,EA_DECAL,EA_SHADOW,EA_AMBIGUOUS,EA_COUNT};
extern unsigned ea_context;
void ea_present(void);
void ea_tag(uint32_t geometry);
unsigned ea_lookup(uint32_t geometry);
uint64_t ea_clock(void);
void ea_record(unsigned category,uint64_t elapsed);
#ifdef THING_EFFECT_ATTRIBUTION_DEFINE
unsigned ea_context;
static uint64_t ea_generation=1;
typedef struct {uint32_t key,category;uint64_t generation;} EASlot;
static EASlot ea_map[65536];
static uint64_t ea_calls[EA_COUNT],ea_ns[EA_COUNT],ea_tags,ea_conflicts,ea_overflow;
static unsigned ea_bin;
uint64_t ea_clock(void){struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void ea_report(void){
 unsigned bin=SDL_GetTicks()/5000;if(bin==ea_bin)return;
 static const char *names[]={"other","weather","particle","decal","shadow","ambiguous"};
 for(unsigned i=0;i<EA_COUNT;i++)if(ea_calls[i])fprintf(stderr,"[effect-render] bin_ms=%u category=%s calls=%llu cpu_ns=%llu\n",ea_bin*5000,names[i],(unsigned long long)ea_calls[i],(unsigned long long)ea_ns[i]);
 fprintf(stderr,"[effect-tags] bin_ms=%u tags=%llu conflicts=%llu overflow=%llu\n",ea_bin*5000,(unsigned long long)ea_tags,(unsigned long long)ea_conflicts,(unsigned long long)ea_overflow);
 for(unsigned i=0;i<EA_COUNT;i++)ea_calls[i]=ea_ns[i]=0;
 ea_tags=ea_conflicts=ea_overflow=0;ea_bin=bin;
}
void ea_present(void){ea_report();++ea_generation;}
static EASlot *ea_slot(uint32_t key){
 unsigned h=(key*2654435761u)>>16;
 for(unsigned n=0;n<65536;n++,h=(h+1)&65535){EASlot*s=ea_map+h;if(s->generation!=ea_generation||s->key==key)return s;}
 return NULL;
}
void ea_tag(uint32_t key){
 if(!key)return;EASlot*s=ea_slot(key);if(!s){++ea_overflow;return;}
 ++ea_tags;
 if(s->generation!=ea_generation){s->key=key;s->category=ea_context;s->generation=ea_generation;}
 else if(s->category!=ea_context&&s->category!=EA_AMBIGUOUS){s->category=EA_AMBIGUOUS;++ea_conflicts;}
}
unsigned ea_lookup(uint32_t key){EASlot*s=key?ea_slot(key):NULL;return s&&s->generation==ea_generation?s->category:EA_OTHER;}
void ea_record(unsigned c,uint64_t elapsed){if(c>=EA_COUNT)c=EA_OTHER;++ea_calls[c];ea_ns[c]+=elapsed;}
#endif
#endif
