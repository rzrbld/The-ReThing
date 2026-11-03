/* Isolated experiment: logging happens after the measured interval. */
#include <time.h>
#include <inttypes.h>
#include <stdio.h>
extern __thread unsigned audio_probe_whole_depth;
typedef struct { uint64_t cpu,wall; } AudioProbeTime;
static uint64_t audio_probe_ns(clockid_t id){struct timespec ts;clock_gettime(id,&ts);return (uint64_t)ts.tv_sec*1000000000ull+ts.tv_nsec;}
static AudioProbeTime audio_probe_begin(void){AudioProbeTime t={audio_probe_ns(CLOCK_THREAD_CPUTIME_ID),audio_probe_ns(CLOCK_MONOTONIC)};return t;}
static void audio_probe_end(const char*kind,uint32_t ctx,uint32_t size,AudioProbeTime t){
 uint64_t cpu=audio_probe_ns(CLOCK_THREAD_CPUTIME_ID)-t.cpu,wall=audio_probe_ns(CLOCK_MONOTONIC)-t.wall;
 fprintf(stderr,"[audio-cpu] kind=%s context=%#x bytes=%u cpu_ns=%"PRIu64" wall_ns=%"PRIu64"\n",kind,ctx,size,cpu,wall);
}
