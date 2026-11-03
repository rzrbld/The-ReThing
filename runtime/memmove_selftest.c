/* Exercise the original translated CRT memmove and its recovered dispatch
 * aliases through the real guest ABI, comparing all guard bytes to libc. */
#define _GNU_SOURCE
#include "win32hle.h"
#include "host.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern const win32hle_shim ds_d3d8_shims[],ds_dinput8_shims[],ds_dsound8_shims[],ds_win32_extra_shims[];
static unsigned current_length,current_alignment;
static int current_delta;
void ds_runtime_poll(void) {}
void ds_guard_dispatch(uint32_t va) {
    if(va==RECOMP_RETADDR || recomp_lookup(va) || hle_name(va))return;
    hle_fatal("memmove check unresolved target %#x from %#x (n=%u src_align=%u delta=%d)",
        va,g_cur_func,current_length,current_alignment,current_delta);
}
static uint32_t random32(uint32_t *state) {
    uint32_t v=*state;v^=v<<13;v^=v>>17;v^=v<<5;return *state=v;
}
static double seconds(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv) {
    int quick=argc>1 && !strcmp(argv[1],"--quick");
    if(argc>1 && !quick && strcmp(argv[1],"--full"))return 64;
    const char *requested=getenv("THING_ROOT");char root[PATH_MAX],bin[PATH_MAX];
    if(!requested || !realpath(requested,root)) {fputs("Set THING_ROOT to a prepared game install\n",stderr);return 64;}
    assert(snprintf(bin,sizeof bin,"%s/bin",root)<(int)sizeof bin);
    assert(!chdir(bin));hle_set_drive('C',root);hle_set_command_line("C:\\bin\\thething.exe");
    win32hle_register(ds_d3d8_shims);win32hle_register(ds_dinput8_shims);
    win32hle_register(ds_dsound8_shims);win32hle_register(ds_win32_extra_shims);
    recomp_host_init();assert(!recomp_host_boot("thething.exe"));
    enum {BYTES=4096,SOURCE=1536,MAX_LENGTH=1024};
    uint8_t pattern[BYTES+256],expected[BYTES];uint32_t seed=0x53c2a19fu;
    for(unsigned i=0;i<sizeof pattern;i++)pattern[i]=(uint8_t)random32(&seed);
    uint8_t *memory=hle_alloc(BYTES);assert(memory);uint32_t guest=hle_va(memory);
    const int distance=quick?32:MAX_LENGTH;
    uint64_t cases=0;double start=seconds();
    for(unsigned length=0;length<=MAX_LENGTH;length++)for(unsigned alignment=0;alignment<4;alignment++)
        for(int delta=-distance;delta<=distance;delta++) {
            current_length=length;current_alignment=alignment;current_delta=delta;
            unsigned source=SOURCE+alignment,destination=(unsigned)((int)source+delta);
            assert(source+length<=BYTES && destination+length<=BYTES);
            unsigned salt=random32(&seed)&255u;
            memcpy(memory,pattern+salt,BYTES);memcpy(expected,pattern+salt,BYTES);
            memmove(expected+destination,expected+source,length);
            uint32_t args[3]={guest+destination,guest+source,length};
            uint32_t answer=hle_call_guest(0x7b34b0,3,args);
            if(answer!=args[0] || memcmp(memory,expected,BYTES)) {
                fprintf(stderr,"FAIL n=%u source_align=%u delta=%d returned=%#x expected=%#x\n",length,alignment,delta,answer,args[0]);
                for(unsigned i=0;i<BYTES;i++)if(memory[i]!=expected[i]) {fprintf(stderr,"first differing buffer byte=%u got=%02x expected=%02x\n",i,memory[i],expected[i]);break;}
                return 1;
            }
            cases++;
        }
    /* hle_call_guest intentionally restores its caller's saved registers, so
     * check the callee's own cdecl stack/nonvolatile preservation directly. */
    const unsigned boundaries[]={0,1,2,3,4,7,8,15,16,31,32,33,63,64,65,127,128,255,256,511,512,1023,1024};
    mach_enter();recomp_func_t function=recomp_lookup(0x7b34b0);assert(function);
    uint32_t stack=g_esp,fs=g_fs_base;
    for(unsigned i=0;i<sizeof boundaries/sizeof *boundaries;i++)for(unsigned alignment=0;alignment<4;alignment++)for(int direction=-1;direction<=1;direction+=2) {
        unsigned n=boundaries[i],source=SOURCE+alignment,destination=(unsigned)((int)source+direction);
        memcpy(memory,pattern,BYTES);memcpy(expected,pattern,BYTES);memmove(expected+destination,expected+source,n);
        g_esp=stack;g_ebx=0x12345678u;g_esi=0x23456789u;g_edi=0x3456789au;g_ebp=0x456789abu;
        PUSH32(g_esp,n);PUSH32(g_esp,guest+source);PUSH32(g_esp,guest+destination);PUSH32(g_esp,RECOMP_RETADDR);
        function();
        if(g_esp!=stack-12 || g_fs_base!=fs || g_ebx!=0x12345678u || g_esi!=0x23456789u || g_edi!=0x3456789au || g_ebp!=0x456789abu || g_eax!=guest+destination || memcmp(memory,expected,BYTES)) {
            fprintf(stderr,"FAIL cdecl frame n=%u align=%u direction=%d esp=%#x expected=%#x\n",n,alignment,direction,g_esp,stack-12);return 1;
        }
        g_esp=stack;
    }
    mach_leave();
    printf("Translated original memmove: %llu whole-buffer/EAX cases, lengths0..1024 srcalign0..3 delta-%d..%d PASS; 184 cdecl/nonvolatile/frame checks PASS; %.3fs\n",
        (unsigned long long)cases,distance,distance,seconds()-start);
    return 0;
}
