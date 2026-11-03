#include <time.h>
/* Included after g_shim_n: the guest machine lock serializes these counters.
 * No allocation, string processing, timestamps or I/O on the per-call path. */
int thing_facade_enabled;
static uint64_t facade_calls[MAX_SHIMS],facade_lookups,facade_hits;
static uint64_t facade_same[6],facade_emission_ns,facade_frames;
static FILE *facade_csv,*facade_summary;
static int facade_finished;
static uint64_t facade_clock(void){struct timespec ts;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts);return (uint64_t)ts.tv_sec*1000000000+ts.tv_nsec;}
void thing_facade_finish(void){
 if(facade_finished)return;facade_finished=1;
 if(facade_csv)fclose(facade_csv);if(facade_summary)fclose(facade_summary);
 if(thing_facade_enabled)fprintf(stderr,"[facade-probe] frames=%llu emission_cpu_ms=%.3f per_frame_us=%.3f; counters only, no per-call timing; partial final frame omitted\n",(unsigned long long)facade_frames,facade_emission_ns/1e6,facade_frames?facade_emission_ns/1e3/facade_frames:0);
}
static void facade_init(void){
 static int initialized;if(initialized)return;initialized=1;
 const char *v=getenv("THING_FACADE_PROBE"),*root=getenv("THING_ROOT");
 if(!v||strcmp(v,"1")||!root)return;
 char path[4096];if(snprintf(path,sizeof path,"%s/facade-calls.csv",root)>=(int)sizeof path)return;
 facade_csv=fopen(path,"w");snprintf(path,sizeof path,"%s/facade-frame.csv",root);facade_summary=fopen(path,"w");
 if(!facade_csv||!facade_summary){if(facade_csv)fclose(facade_csv);if(facade_summary)fclose(facade_summary);facade_csv=facade_summary=NULL;fprintf(stderr,"[facade-probe] cannot open counters\n");return;}
 setvbuf(facade_csv,NULL,_IOFBF,1024*1024);setvbuf(facade_summary,NULL,_IOFBF,65536);
 fputs("frame,shim_index,method,calls\n",facade_csv);
 fputs("frame,lookup_attempts,lookup_hits,shim_entries,d3d8_entries,rs_same,tss_same,texture_same,stream_same,indices_same,fvf_same\n",facade_summary);
 thing_facade_enabled=1;atexit(thing_facade_finish);fprintf(stderr,"[facade-probe] enabled: exact shim entries per Present, all serialized guest threads; state same-value counters do not skip work\n");
}
void thing_facade_same(unsigned id){if(id<6)facade_same[id]++;}
void thing_facade_present(unsigned frame){
 if(!thing_facade_enabled)return;
 uint64_t start=facade_clock(),total=0,d3d=0;
 for(int i=0;i<g_shim_n;i++){uint64_t n=facade_calls[i];if(!n)continue;total+=n;if(!strncmp(g_shim[i].name,"D3D8.",5))d3d+=n;
 fprintf(facade_csv,"%u,%d,%s,%llu\n",frame,i,g_shim[i].name,(unsigned long long)n);facade_calls[i]=0;}
 fprintf(facade_summary,"%u,%llu,%llu,%llu,%llu",frame,(unsigned long long)facade_lookups,(unsigned long long)facade_hits,(unsigned long long)total,(unsigned long long)d3d);
 for(unsigned i=0;i<6;i++){fprintf(facade_summary,",%llu",(unsigned long long)facade_same[i]);facade_same[i]=0;}fputc('\n',facade_summary);
 facade_lookups=facade_hits=0;facade_frames++;
 if(frame%120==0){fflush(facade_csv);fflush(facade_summary);}facade_emission_ns+=facade_clock()-start;
}
