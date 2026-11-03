#define _GNU_SOURCE
/* Linux AArch64/x86-64 external PC sampler. SEIZE does not stop the task;
 * each requested sample INTERRUPTs, reads NT_PRSTATUS, then resumes it.
 * No memory writes, injected calls, register changes, or EXITKILL option.
 * Diagnostic stops perturb timing: these samples are not a performance test. */
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <elf.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__aarch64__)
#include <asm/ptrace.h>
#endif
static volatile sig_atomic_t interrupted;
static void stop_request(int sig){interrupted=sig;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static int wait_stopped(pid_t tid,int *status){
    double end=now()+2.;
    do{pid_t got=waitpid(tid,status,__WALL|WNOHANG);if(got==tid){if(WIFSTOPPED(*status))return 0;fprintf(stderr,"target exited status=%#x\n",*status);return -1;}if(got<0&&errno!=EINTR){perror("waitpid");return -1;}struct timespec pause={0,1000000};nanosleep(&pause,NULL);}while(now()<end);
    fprintf(stderr,"timed out awaiting ptrace stop\n");return -1;
}
static int request_stop(pid_t tid,int *status){if(ptrace(PTRACE_INTERRUPT,tid,0,0)){perror("PTRACE_INTERRUPT");return -1;}return wait_stopped(tid,status);}
static long number(const char*s,long max){char*end;errno=0;long n=strtol(s,&end,10);if(errno||*end||end==s||n<1||n>max)return -1;return n;}
static int pending_signal(int status){
    unsigned event=(unsigned)status>>16;int sig=WSTOPSIG(status);
    return event==PTRACE_EVENT_STOP&&sig==SIGTRAP?0:sig;
}
int main(int argc,char**argv){
    if(argc<2||argc>4||!strcmp(argv[1],"--help")){fprintf(stderr,"usage: %s TID [samples=20 [interval_ms=50]]\n",argv[0]);return argc==2&&!strcmp(argv[1],"--help")?0:64;}
    long value=number(argv[1],INT_MAX),count=argc>2?number(argv[2],1000):20,interval=argc>3?number(argv[3],60000):50;
    if(value<2||value==getpid()||count<1||interval<1){fprintf(stderr,"invalid TID/count/interval\n");return 64;}
    pid_t tid=(pid_t)value;struct sigaction sa={0};sa.sa_handler=stop_request;sigemptyset(&sa.sa_mask);sigaction(SIGINT,&sa,NULL);sigaction(SIGTERM,&sa,NULL);
    if(ptrace(PTRACE_SEIZE,tid,0,0)){perror("PTRACE_SEIZE");return 1;}
    int stopped=0,status=0,result=0;double begin=now();
    puts("sample,elapsed_ms,tid,host_pc,host_lr,host_sp,stop_signal,stop_event");fflush(stdout);
    for(long n=0;n<count&&!interrupted;n++){
        if(request_stop(tid,&status)){result=1;break;}stopped=1;
#if defined(__aarch64__)
        struct user_pt_regs regs;struct iovec iov={&regs,sizeof regs};
#elif defined(__x86_64__)
        struct user_regs_struct regs;struct iovec iov={&regs,sizeof regs};
#else
#error "sample_thread supports Linux AArch64 and x86-64 only"
#endif
        if(ptrace(PTRACE_GETREGSET,tid,(void*)(uintptr_t)NT_PRSTATUS,&iov)||iov.iov_len!=sizeof regs){perror("PTRACE_GETREGSET");result=1;break;}
#if defined(__aarch64__)
        uintptr_t pc=regs.pc,lr=regs.regs[30],sp=regs.sp;
#else
        uintptr_t pc=regs.rip,lr=0,sp=regs.rsp;
#endif
        printf("%ld,%.3f,%ld,0x%lx,0x%lx,0x%lx,%d,%u\n",n,(now()-begin)*1000,(long)tid,(unsigned long)pc,(unsigned long)lr,(unsigned long)sp,WSTOPSIG(status),(unsigned)status>>16);fflush(stdout);
        /* Preserve unexpected delivery/group-stop signals instead of hiding
         * them. End sampling so root can inspect a target already stopping. */
        if(pending_signal(status)){fprintf(stderr,"unexpected target stop; detaching with signal %d\n",pending_signal(status));break;}
        if(n+1==count||interrupted)break;
        if(ptrace(PTRACE_CONT,tid,0,0)){perror("PTRACE_CONT");result=1;break;}stopped=0;
        struct timespec delay={interval/1000,(interval%1000)*1000000};while(nanosleep(&delay,&delay)&&errno==EINTR&&!interrupted){}
    }
    if(!stopped){if(!request_stop(tid,&status))stopped=1;else result=1;}
    if(stopped&&ptrace(PTRACE_DETACH,tid,0,(void*)(uintptr_t)pending_signal(status))){perror("PTRACE_DETACH");result=1;}
    fprintf(stderr,"sampler detached tid=%ld result=%d interrupted=%d\n",(long)tid,result,(int)interrupted);
    return interrupted?128+interrupted:result;
}
