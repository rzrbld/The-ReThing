#define _GNU_SOURCE
#include "win32hle.h"
#include "profiler.h"
#include <signal.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <ucontext.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Adapted from desert_storm/runtime/profiler.c, replacing its process-wide
 * ITIMER_PROF with a timer charged to, and delivered only on, the main thread.
 * Thread targeting also follows desert_storm/probe/main_thread_profiler.c.
 * No DS game addresses or overrides. Handler: fixed integer work only; no
 * allocation, stdio, GL, SDL, unwinding, locks or symbol resolution. */
#if defined(__linux__) && (defined(__aarch64__) || defined(__x86_64__))
#define GUEST_BUCKETS 32768u
#define PC_BUCKETS 65536u
#define TIMELINE_SAMPLES 65536u
_Static_assert(__atomic_always_lock_free(sizeof(uintptr_t),0),"sampling needs lock-free uintptr loads/stores");
_Static_assert(__atomic_always_lock_free(sizeof(uint32_t),0),"sampling needs lock-free guest context load");
static volatile sig_atomic_t guest_address[GUEST_BUCKETS],guest_count[GUEST_BUCKETS];
static _Atomic(uintptr_t) pc_address[PC_BUCKETS],lr_address[PC_BUCKETS];
static volatile sig_atomic_t pc_count[PC_BUCKETS],guest_lost,pc_lost,total;
/* Fixed storage: no allocation, clocks or I/O in the signal handler. About
 * 1.5 MiB, touched only while profiling; 10.9 minutes of main-thread CPU. */
static volatile struct {uintptr_t pc,lr;uint32_t guest,frame;} timeline[TIMELINE_SAMPLES];
static volatile sig_atomic_t timeline_count,timeline_lost,current_frame=1;
static timer_t timer_id;
static pid_t owner_tid;
static struct sigaction previous_action;
static volatile sig_atomic_t active;
static int exit_registered;
static char output_root[PATH_MAX];
static const struct itimerspec interval={{0,10000000},{0,10000000}};
static void increment(volatile sig_atomic_t *v){if(*v<INT_MAX)++*v;}
static void sample(int sig,siginfo_t *info,void *opaque){
    (void)sig;
    if(!active||!info||info->si_code!=SI_TIMER||info->si_value.sival_ptr!=&timer_id)return;
    uint32_t guest=__atomic_load_n(&g_cur_func,__ATOMIC_RELAXED);
    increment(&total);
    unsigned slot=(guest>>2)&(GUEST_BUCKETS-1),i;
    for(i=0;i<16;i++,slot=(slot+1)&(GUEST_BUCKETS-1)){
        if(!guest_count[slot]||(uint32_t)guest_address[slot]==guest){guest_address[slot]=(sig_atomic_t)guest;increment(&guest_count[slot]);break;}
    }
    if(i==16)increment(&guest_lost);
    uintptr_t pc=0,lr=0;
#if defined(__aarch64__)
    pc=((ucontext_t*)opaque)->uc_mcontext.pc;
    lr=((ucontext_t*)opaque)->uc_mcontext.regs[30];
#elif defined(__x86_64__)
    pc=((ucontext_t*)opaque)->uc_mcontext.gregs[REG_RIP];
#endif
    if(!pc)return;
    if((unsigned)timeline_count<TIMELINE_SAMPLES){
        unsigned at=(unsigned)timeline_count;
        timeline[at].pc=pc;timeline[at].lr=lr;
        timeline[at].guest=guest;timeline[at].frame=(uint32_t)current_frame;
        timeline_count++;
    }else increment(&timeline_lost);
    uintptr_t hash=((pc>>2)^(pc>>19)^(lr>>3)^(lr>>21))*0x9e3779b1u;
    slot=(unsigned)hash&(PC_BUCKETS-1);
    for(i=0;i<32;i++,slot=(slot+1)&(PC_BUCKETS-1)){
        if(!pc_count[slot]||(pc_address[slot]==pc&&lr_address[slot]==lr)){
            pc_address[slot]=pc;lr_address[slot]=lr;increment(&pc_count[slot]);return;
        }
    }
    increment(&pc_lost);
}
static FILE *output(const char *name){
    char path[PATH_MAX];if(snprintf(path,sizeof path,"%s/%s",output_root,name)>=(int)sizeof path)return NULL;
    FILE *f=fopen(path,"w");if(!f)fprintf(stderr,"[cpu-profile] cannot open %s: %s\n",path,strerror(errno));return f;
}
static void write_samples(void){
    FILE*f=output("cpu-profile.csv");if(f){fputs("guest_function,cpu_samples\n",f);for(unsigned i=0;i<GUEST_BUCKETS;i++)if(guest_count[i])fprintf(f,"0x%08x,%d\n",(uint32_t)guest_address[i],guest_count[i]);fclose(f);}
    /* PC rows can repeat for different LR values; analyzers must sum by PC. */
    f=output("host-cpu-profile.csv");if(f){fputs("host_pc,cpu_samples\n",f);for(unsigned i=0;i<PC_BUCKETS;i++)if(pc_count[i])fprintf(f,"0x%lx,%d\n",(unsigned long)pc_address[i],pc_count[i]);fclose(f);}
    f=output("host-call-profile.csv");if(f){fputs("host_pc,host_lr,cpu_samples\n",f);for(unsigned i=0;i<PC_BUCKETS;i++)if(pc_count[i])fprintf(f,"0x%lx,0x%lx,%d\n",(unsigned long)pc_address[i],(unsigned long)lr_address[i],pc_count[i]);fclose(f);}
    f=output("host-cpu-timeline.csv");if(f){fputs("frame,host_pc,host_lr,guest_function,cpu_samples\n",f);for(unsigned i=0;i<(unsigned)timeline_count;i++)fprintf(f,"%u,0x%lx,0x%lx,0x%08x,1\n",timeline[i].frame,(unsigned long)timeline[i].pc,(unsigned long)timeline[i].lr,timeline[i].guest);fclose(f);}
    f=output("cpu-timeline-info.txt");if(f){fprintf(f,"samples=%d\nlost=%d\ncapacity=%u\nframe=work-after-previous-Present-before-current-Present-return\n",timeline_count,timeline_lost,TIMELINE_SAMPLES);fclose(f);}
    f=output("host-profile-maps.txt");FILE*maps=fopen("/proc/self/maps","r");if(f&&maps){char line[1024];while(fgets(line,sizeof line,maps))fputs(line,f);}if(f)fclose(f);if(maps)fclose(maps);
    f=output("cpu-profile-info.txt");if(f){fprintf(f,"clock=CLOCK_THREAD_CPUTIME_ID\ndelivery=SIGEV_THREAD_ID\nthread=%ld\ninterval_ns=10000000\nsamples=%d\nguest_lost=%d\npc_lr_lost=%d\nguest_context=last-entered-label-not-unwound-stack\n",(long)owner_tid,total,guest_lost,pc_lost);fclose(f);}
    fprintf(stderr,"[cpu-profile] main_tid=%ld samples=%d guest_lost=%d pc_lr_lost=%d interval_ms=10 root=%s\n",(long)owner_tid,total,guest_lost,pc_lost,output_root);
}
/* Drain a pending coalesced SIGPROF before restoring the previous disposition. */
static void disarm(sigset_t *old){
    sigset_t set;sigemptyset(&set);sigaddset(&set,SIGPROF);sigprocmask(SIG_BLOCK,&set,old);
    const struct itimerspec zero={{0,0},{0,0}};timer_settime(timer_id,0,&zero,NULL);
    struct timespec wait={0,0};while(sigtimedwait(&set,NULL,&wait)>=0){}
}
void thing_cpu_profiler_flush(void){
    if(!active||(pid_t)syscall(SYS_gettid)!=owner_tid)return;
    sigset_t old;disarm(&old);write_samples();
    if(timer_settime(timer_id,0,&interval,NULL))fprintf(stderr,"[cpu-profile] rearm failed: %s\n",strerror(errno));
    sigprocmask(SIG_SETMASK,&old,NULL);
}
void thing_cpu_profiler_stop(void){
    if(!active||(pid_t)syscall(SYS_gettid)!=owner_tid)return;
    sigset_t old;disarm(&old);active=0;timer_delete(timer_id);write_samples();sigaction(SIGPROF,&previous_action,NULL);sigprocmask(SIG_SETMASK,&old,NULL);
}
void thing_cpu_profiler_frame(unsigned next_frame){current_frame=(sig_atomic_t)next_frame;}
void thing_cpu_profiler_start(void){
    const char *enabled=getenv("THING_CPU_PROFILE");if(active||!enabled||strcmp(enabled,"1"))return;
    const char *root=getenv("THING_ROOT");if(!root||snprintf(output_root,sizeof output_root,"%s",root)>=(int)sizeof output_root){fprintf(stderr,"[cpu-profile] THING_ROOT missing/too long\n");return;}
    struct sigaction current;if(sigaction(SIGPROF,NULL,&current)||current.sa_handler!=SIG_DFL){fprintf(stderr,"[cpu-profile] SIGPROF already owned; sampling disabled\n");return;}
    sigset_t mask;sigprocmask(SIG_SETMASK,NULL,&mask);if(sigismember(&mask,SIGPROF)){fprintf(stderr,"[cpu-profile] SIGPROF blocked; sampling disabled\n");return;}
    owner_tid=(pid_t)syscall(SYS_gettid);
    struct sigevent event;memset(&event,0,sizeof event);event.sigev_notify=SIGEV_THREAD_ID;event.sigev_signo=SIGPROF;event.sigev_value.sival_ptr=&timer_id;event._sigev_un._tid=owner_tid;
    if(timer_create(CLOCK_THREAD_CPUTIME_ID,&event,&timer_id)){fprintf(stderr,"[cpu-profile] thread timer unavailable: %s\n",strerror(errno));return;}
    struct sigaction action;memset(&action,0,sizeof action);action.sa_sigaction=sample;action.sa_flags=SA_SIGINFO|SA_RESTART;sigemptyset(&action.sa_mask);
    if(sigaction(SIGPROF,&action,&previous_action)){timer_delete(timer_id);return;}
    active=1;
    if(timer_settime(timer_id,0,&interval,NULL)){active=0;fprintf(stderr,"[cpu-profile] arm failed: %s\n",strerror(errno));sigaction(SIGPROF,&previous_action,NULL);timer_delete(timer_id);return;}
    if(!exit_registered){atexit(thing_cpu_profiler_stop);exit_registered=1;}
    fprintf(stderr,"[cpu-profile] enabled main-thread CPU clock tid=%ld interval_ms=10\n",(long)owner_tid);
}
#else
void thing_cpu_profiler_start(void){if(getenv("THING_CPU_PROFILE"))fprintf(stderr,"[cpu-profile] sampling supported only on Linux AArch64/x86-64\n");}
void thing_cpu_profiler_flush(void){}
void thing_cpu_profiler_stop(void){}
void thing_cpu_profiler_frame(unsigned next_frame){(void)next_frame;}
#endif
