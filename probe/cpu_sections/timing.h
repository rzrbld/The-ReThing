#ifndef THING_CPU_SECTION_TIMING_H
#define THING_CPU_SECTION_TIMING_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
extern uint32_t SDL_GetTicks(void);
#define SECTION_SLOTS 16
#define SECTION_BINS 64
typedef struct{uint64_t calls,cpu,max;} SectionBin;
static SectionBin section_stats[SECTION_SLOTS][SECTION_BINS];
static const char*section_names[SECTION_SLOTS];
static __thread unsigned section_depth[SECTION_SLOTS];
static __thread int section_main=-1;
static int section_registered;
static unsigned section_reported_bin;
typedef struct{uint64_t cpu;unsigned id,bin;int active;} SectionTime;
static uint64_t section_cpu(void){struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void section_report_until(unsigned end){
 for(unsigned b=section_reported_bin;b<end&&b<SECTION_BINS;b++)for(unsigned i=0;i<SECTION_SLOTS;i++)if(section_names[i]){
 SectionBin*s=&section_stats[i][b];if(s->calls)fprintf(stderr,"[cpu-section] name=%s sdl_bin_start_ms=%u calls=%llu cpu_ns=%llu max_ns=%llu\n",section_names[i],b*5000,(unsigned long long)s->calls,(unsigned long long)s->cpu,(unsigned long long)s->max);
 }section_reported_bin=end;
}
static void section_report(void){section_report_until(SECTION_BINS);}
static SectionTime section_begin(unsigned id,const char*name){
 SectionTime t={0,id,0,0};if(section_main<0)section_main=(pid_t)syscall(SYS_gettid)==getpid();if(!section_main)return t;
 if(!section_registered){section_registered=1;atexit(section_report);}section_names[id]=name;
 if(section_depth[id]++)return t;
 t.bin=SDL_GetTicks()/5000;section_report_until(t.bin);if(t.bin>=SECTION_BINS)t.bin=SECTION_BINS-1;t.active=1;t.cpu=section_cpu();return t;
}
static void section_end(SectionTime t){if(!section_main)return;--section_depth[t.id];if(!t.active)return;uint64_t dt=section_cpu()-t.cpu;SectionBin*s=&section_stats[t.id][t.bin];s->calls++;s->cpu+=dt;if(dt>s->max)s->max=dt;}
#endif
