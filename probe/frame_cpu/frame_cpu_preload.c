/* Optional SINGLE-renderer measurement interposer, not a game optimization.
 * Main-thread CPU between swaps includes frame-pacer CPU, excludes sleep and
 * other threads. Shader/driver worker CPU is outside this clock's scope. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
static void (*real_swap)(void *);
static FILE *output;
static pthread_t owner;
static uint64_t last_cpu,start_wall;
static unsigned frame;
static uint64_t ns(clockid_t clock){struct timespec t;if(clock_gettime(clock,&t))abort();return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static void close_output(void){if(output){fclose(output);output=NULL;}}
void SDL_GL_SwapWindow(void *window){
 if(!real_swap){
  *(void **)(&real_swap)=dlsym(RTLD_NEXT,"SDL_GL_SwapWindow");if(!real_swap)abort();
  owner=pthread_self();const char *root=getenv("THING_ROOT");if(!root)abort();
  char path[PATH_MAX];if(snprintf(path,sizeof path,"%s/frame-cpu-exact.csv",root)>=(int)sizeof path)abort();
  output=fopen(path,"w");if(!output)abort();
#ifdef THING_CPU_CSV_LINE_BUFFERED
  setvbuf(output,NULL,_IOLBF,2048);
#else
  setvbuf(output,NULL,_IOFBF,65536);
#endif
  fputs("swap,elapsed_seconds,thread_cpu_ms,swap_thread_cpu_ms,swap_wall_ms\n",output);atexit(close_output);
 }
 /* Fail rather than silently mix renderer-worker/main-thread clock origins. */
 if(!pthread_equal(owner,pthread_self()))abort();
 uint64_t cpu=ns(CLOCK_THREAD_CPUTIME_ID),wall=ns(CLOCK_MONOTONIC);
 if(!frame){start_wall=wall;last_cpu=cpu;}
 real_swap(window);
 uint64_t after_cpu=ns(CLOCK_THREAD_CPUTIME_ID),after_wall=ns(CLOCK_MONOTONIC);
 fprintf(output,"%u,%.9f,%.9f,%.9f,%.9f\n",++frame,(wall-start_wall)*1e-9,(cpu-last_cpu)*1e-6,(after_cpu-cpu)*1e-6,(after_wall-wall)*1e-6);
 /* Before-swap to before-swap: includes the previous swap's CPU exactly once. */
 last_cpu=cpu;
}
