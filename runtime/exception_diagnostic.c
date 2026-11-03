#define _GNU_SOURCE
#include "win32hle.h"
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <execinfo.h>
#include <errno.h>

/* Failure-only inspection. Never dereference an untrusted guest pointer:
 * unsupported/denied self-reads omit details, and exception handling remains
 * fatal exactly as before. No guest memory or registers are modified. */
static int read_guest(uint32_t address,void *out,size_t bytes){
    if(!address||bytes>UINT32_MAX-address)return 0;
    struct iovec local={out,bytes},remote={hle_ptr(address),bytes};
    return process_vm_readv(getpid(),&local,1,&remote,1,0)==(ssize_t)bytes;
}
static void possible_text(uint32_t address,unsigned offset){
    char text[385];size_t n=0;
    for(;n<sizeof(text)-1;n++){
        if(address>UINT32_MAX-n||!read_guest(address+(uint32_t)n,text+n,1))return;
        unsigned char c=text[n];if(!c)break;
        if(c<32&&c!='\n'&&c!='\r'&&c!='\t')return;
        if(c>=127)return;
    }
    text[n]=0;
    if(n>=4)fprintf(stderr,"[guest-exception] object+%u text@%#x: %s\n",offset,address,text);
}
void thing_exception_diagnostic(uint32_t code,uint32_t count,uint32_t address){
    int saved_errno=errno;
    uint32_t parameters[15]={0},words[16]={0};
    unsigned n=count<15?count:15;
    fprintf(stderr,"[guest-exception] code=%#x count=%u params=%#x guest=%#x esp=%#x ebp=%#x\n",
        code,count,address,g_cur_func,g_esp,g_ebp);
    if(n&&read_guest(address,parameters,n*4)){
        fprintf(stderr,"[guest-exception] parameters:");
        for(unsigned i=0;i<n;i++)fprintf(stderr," %#x",parameters[i]);
        fputc('\n',stderr);
        if(code==0xe06d7363u&&n>=3&&parameters[0]==0x19930520u){
            if(read_guest(parameters[1],words,sizeof words)){
                fprintf(stderr,"[guest-exception] object words:");
                for(unsigned i=0;i<16;i++)fprintf(stderr," %#x",words[i]);
                fputc('\n',stderr);
                for(unsigned i=0;i<16;i++)possible_text(words[i],i*4);
            }
        }
    }else fprintf(stderr,"[guest-exception] parameters unavailable\n");
    if(read_guest(g_esp,words,sizeof words)){
        fprintf(stderr,"[guest-exception] stack words:");
        for(unsigned i=0;i<16;i++)fprintf(stderr," %#x",words[i]);
        fputc('\n',stderr);
    }
    void *trace[32];int frames=backtrace(trace,32);backtrace_symbols_fd(trace,frames,2);
    errno=saved_errno;
}
