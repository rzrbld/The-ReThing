#define _GNU_SOURCE
#include "profiler.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
uint32_t g_cur_func=0x00401000;
static double cpu(void){struct timespec t;assert(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t));return t.tv_sec+t.tv_nsec*1e-9;}
static void burn(double seconds){double end=cpu()+seconds;volatile unsigned n=1;while(cpu()<end)for(int i=0;i<1024;i++)n=n*1664525u+1013904223u;}
static void*worker(void*unused){(void)unused;thing_cpu_profiler_stop();burn(.35);return NULL;}
static unsigned samples(void){char path[1024];snprintf(path,sizeof path,"%s/cpu-profile.csv",getenv("THING_ROOT"));FILE*f=fopen(path,"r");assert(f);char line[256];assert(fgets(line,sizeof line,f));unsigned sum=0,key,n;while(fgets(line,sizeof line,f)){assert(sscanf(line,"%x,%u",&key,&n)==2);assert(key==g_cur_func);sum+=n;}fclose(f);return sum;}
static void custom(int sig){(void)sig;}
int main(void){
 assert(getenv("THING_ROOT"));unsetenv("THING_CPU_PROFILE");thing_cpu_profiler_start();thing_cpu_profiler_stop();
 setenv("THING_CPU_PROFILE","1",1);
 struct sigaction sa={0},got;sa.sa_handler=custom;sigemptyset(&sa.sa_mask);assert(!sigaction(SIGPROF,&sa,NULL));
 thing_cpu_profiler_start();assert(!sigaction(SIGPROF,NULL,&got));assert(got.sa_handler==custom);
 sa.sa_handler=SIG_DFL;assert(!sigaction(SIGPROF,&sa,NULL));
 thing_cpu_profiler_start();raise(SIGPROF);pthread_t t;assert(!pthread_create(&t,NULL,worker,NULL));assert(!pthread_join(t,NULL));
 thing_cpu_profiler_flush();unsigned worker_samples=samples();assert(worker_samples<=2);
 thing_cpu_profiler_frame(1234);burn(.35);thing_cpu_profiler_flush();unsigned main_samples=samples();assert(main_samples>=25&&main_samples<=45);
 thing_cpu_profiler_frame(5678);burn(.15);thing_cpu_profiler_stop();unsigned final_samples=samples();assert(final_samples>=main_samples+10);
 assert(!sigaction(SIGPROF,NULL,&got));assert(got.sa_handler==SIG_DFL);
 char path[1024];snprintf(path,sizeof path,"%s/host-cpu-profile.csv",getenv("THING_ROOT"));FILE*f=fopen(path,"r");assert(f);char line[128];assert(fgets(line,sizeof line,f));unsigned count;unsigned long pc;unsigned host=0;while(fgets(line,sizeof line,f)){assert(sscanf(line,"%lx,%u",&pc,&count)==2&&pc);host+=count;}fclose(f);assert(host==final_samples);
 snprintf(path,sizeof path,"%s/host-cpu-timeline.csv",getenv("THING_ROOT"));f=fopen(path,"r");assert(f&&fgets(line,sizeof line,f));unsigned frame,guest,first=0,second=0,all=0;unsigned long lr;
 while(fgets(line,sizeof line,f)){assert(sscanf(line,"%u,%lx,%lx,%x,%u",&frame,&pc,&lr,&guest,&count)==5);assert(pc&&guest==g_cur_func&&count==1);if(frame==1234)first++;else if(frame==5678)second++;else assert(frame==1);all++;}fclose(f);
 assert(all==final_samples&&first>=25&&second>=10);
 printf("PASS main-thread CPU sampling: worker-only=%u, main350ms=%u, final500ms=%u; timeline frames=%u/%u match total=%u; nonzero PCs, accumulated flush, wrong-thread stop ignored, SIGPROF conflict/restoration checked\n",worker_samples,main_samples,final_samples,first,second,all);
 return 0;
}
