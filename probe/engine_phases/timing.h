#ifndef THING_ENGINE_PHASES_H
#define THING_ENGINE_PHASES_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
uint64_t ep_begin(unsigned id,uint32_t target);
void ep_end(unsigned id,uint64_t start);
#ifdef THING_ENGINE_PHASE_IMPLEMENTATION
/* Defined in translated unit0165 only; original callsites are coarse phases.
 * No per-particle, per-bone or per-draw timing. Main guest thread only. */
#define EP_N 22
#define EP_TARGETS 16
static const char *ep_names[EP_N]={"update_v88","update_v84","update_v8c","update_68fd60","terrain_prepare","decals_prepare","entities_prepare","effects_first_prepare","shadows_prepare","effects_second_prepare","flush_68aad8","flush_68ac90","flush_68ad47","flush_68adaa","flush_68ae91","flush_68aeb9","flush_68af15","server_pump","server_tick","server_entities_first","server_entities_second","server_output"};
static struct {uint64_t calls,ns,max,active;uint32_t targets[EP_TARGETS];uint64_t counts[EP_TARGETS],overflow;} ep[EP_N];
static FILE *ep_file;static int ep_init,ep_on;
static uint64_t ep_clock(void){struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
uint64_t ep_begin(unsigned id,uint32_t target){
 if(!ep_init){ep_init=1;const char *v=getenv("THING_ENGINE_PHASES"),*root=getenv("THING_ROOT");if(v&&!strcmp(v,"1")&&root){char p[4096];snprintf(p,sizeof p,"%s/engine-phases.csv",root);ep_file=fopen(p,"w");if(ep_file){setvbuf(ep_file,NULL,_IOFBF,262144);fputs("frame,phase,target,calls,cpu_ns,max_ns,target_calls,overflow,incomplete\n",ep_file);ep_on=1;}}}
 if(!ep_on)return 0;
 unsigned i;for(i=0;i<EP_TARGETS;i++)if(!ep[id].targets[i]||ep[id].targets[i]==target){ep[id].targets[i]=target;ep[id].counts[i]++;break;}
 if(i==EP_TARGETS)ep[id].overflow++;
 ep[id].calls++;ep[id].active=1;return ep_clock();
}
void ep_end(unsigned id,uint64_t start){if(!start||!ep[id].active)return;ep[id].active=0;uint64_t n=ep_clock()-start;ep[id].ns+=n;if(n>ep[id].max)ep[id].max=n;}
void thing_engine_phase_present(unsigned frame){
 if(!ep_on)return;
 for(unsigned id=0;id<EP_N;id++){
  if(!ep[id].calls)continue;
  if(ep[id].active)ep[id].ns=ep[id].max=0;
  for(unsigned j=0;j<EP_TARGETS&&ep[id].targets[j];j++)fprintf(ep_file,"%u,%s,0x%08x,%llu,%llu,%llu,%llu,%llu,%llu\n",frame,ep_names[id],ep[id].targets[j],(unsigned long long)ep[id].calls,(unsigned long long)ep[id].ns,(unsigned long long)ep[id].max,(unsigned long long)ep[id].counts[j],(unsigned long long)ep[id].overflow,(unsigned long long)ep[id].active);
  memset(&ep[id],0,sizeof ep[id]);
 }
 if(frame%120==0)fflush(ep_file);
}
void thing_engine_phase_finish(void){if(ep_file){fclose(ep_file);ep_file=NULL;ep_on=0;fprintf(stderr,"[engine-phases] closed; coarse callsite phases; target rows repeat phase totals\n");}}
#endif
#endif
