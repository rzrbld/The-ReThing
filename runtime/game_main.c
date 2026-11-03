#define _GNU_SOURCE
#include "win32hle.h"
#include "host.h"
#include "paths.h"
#include "directshow.h"
#include "profiler.h"
#include <SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <execinfo.h>

extern const win32hle_shim ds_d3d8_shims[] __attribute__((weak));
extern const win32hle_shim ds_dinput8_shims[] __attribute__((weak));
extern const win32hle_shim ds_dsound8_shims[] __attribute__((weak));
extern const win32hle_shim ds_win32_extra_shims[] __attribute__((weak));
extern void ds_graphics_profile_flush(void) __attribute__((weak));
static volatile sig_atomic_t stop_requested,snapshot_requested;
static void game_exit(int code) {
    (void)code;
    thing_cpu_profiler_stop();
    if(ds_graphics_profile_flush)ds_graphics_profile_flush();
    SDL_Quit();
}
static void runtime_signal(int sig) {
    if(sig==SIGUSR1)snapshot_requested=1;else stop_requested=sig;
}
/* Input/event bridges call this at ordinary pump boundaries. */
void ds_runtime_poll(void) {
    if(snapshot_requested) {
        snapshot_requested=0;
        if(ds_graphics_profile_flush)ds_graphics_profile_flush();
        thing_cpu_profiler_flush();
    }
    if(stop_requested) {
        fprintf(stderr,"[host] requested stop signal=%d\n",stop_requested);
        hle_exit(0);
    }
}
void ds_guard_dispatch(uint32_t va) {
    if(va==RECOMP_RETADDR || recomp_lookup(va) || hle_name(va))return;
    hle_fatal("unresolved guest dispatch target %#x from %#x",va,g_cur_func);
}
static void crashed(int sig,siginfo_t *info,void *context) {
    (void)context;
    dprintf(2,"\nGAME FAULT signal=%d address=%p guest_base=%p guest=%#x import=%s\n"
            "eax=%#x ebx=%#x ecx=%#x edx=%#x esp=%#x ebp=%#x esi=%#x edi=%#x fs=%#x\n",
            sig,info->si_addr,(void*)g_mem_base,g_cur_func,g_cur_import?g_cur_import:"(none)",
            g_eax,g_ebx,g_ecx,g_edx,g_esp,g_ebp,g_esi,g_edi,g_fs_base);
    void *trace[32];int n=backtrace(trace,32);backtrace_symbols_fd(trace,n,2);
    _exit(128+sig);
}
int main(int argc,char **argv) {
    int permissive=0,check=0,trace=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--permissive"))permissive=1;
        else if(!strcmp(argv[i],"--check-imports"))check=1;
        else if(!strcmp(argv[i],"--trace-imports"))trace=1;
        else {
            fprintf(stderr,"usage: THING_ROOT=/path/to/install %s [--check-imports] [--trace-imports] [--permissive]\n",argv[0]);
            return !strcmp(argv[i],"--help")?0:64;
        }
    }
    setvbuf(stderr,NULL,_IONBF,0);setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS,"1");
    struct sigaction sa;memset(&sa,0,sizeof sa);sa.sa_sigaction=crashed;sa.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGFPE,&sa,NULL);sigaction(SIGBUS,&sa,NULL);
    struct sigaction control;memset(&control,0,sizeof control);control.sa_handler=runtime_signal;control.sa_flags=SA_RESTART;
    sigaction(SIGTERM,&control,NULL);sigaction(SIGINT,&control,NULL);sigaction(SIGUSR1,&control,NULL);
    if(ds_paths_init()){perror("The Thing paths (set THING_ROOT to root containing bin/ and data/)");return 1;}
    const char *root=getenv("THING_ROOT");char bin[PATH_MAX],reg[PATH_MAX],userdata[PATH_MAX];
    snprintf(bin,sizeof bin,"%s/bin",root);snprintf(reg,sizeof reg,"%s/config/registry.txt",root);
    snprintf(userdata,sizeof userdata,"%s/userdata",root);
    if(chdir(bin)){perror(bin);return 1;}
    hle_set_drive('C',root);hle_set_drive('S',getenv("THING_SAVE_DIR"));
    hle_set_command_line("C:\\bin\\thething.exe");
    hle_set_overlay(root,userdata);
    /* First registered shim wins; game bridges override generic implementations. */
    if(ds_d3d8_shims)win32hle_register(ds_d3d8_shims);
    if(ds_dinput8_shims)win32hle_register(ds_dinput8_shims);
    if(ds_dsound8_shims)win32hle_register(ds_dsound8_shims);
    if(ds_win32_extra_shims)win32hle_register(ds_win32_extra_shims);
    recomp_host_init();
    thing_directshow_register();
    hle_reg_load(reg);hle_on_exit=game_exit;
    extern void thing_seed_installation_paths(void);
    thing_seed_installation_paths();
    win32hle_trace=trace || getenv("THING_TRACE_IMPORTS")!=NULL;
    fprintf(stderr,"[host] The Thing static translation; build=2026-10-29-input-path-diagnostics-1 root=%s strict=%d\n",root,!permissive);
    int rc=permissive?recomp_host_boot_permissive("thething.exe"):recomp_host_boot("thething.exe");
    if(rc)return rc;
    if(check) {fprintf(stderr,"[host] import check only; guest code not executed\n");return 0;}
    fprintf(stderr,"[host] calling PE entry %#x\n",recomp_host_entry());
    thing_cpu_profiler_start();
    int result=(int)recomp_host_run();
    thing_cpu_profiler_stop();
    return result;
}
