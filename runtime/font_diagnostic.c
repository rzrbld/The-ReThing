#define _GNU_SOURCE
#include "win32hle.h"
#include "font_diagnostic.h"
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int enabled=-1;
static unsigned errors,traces;
/* A diagnostic must not turn a damaged guest pointer into another host fault.
 * Linux reads its own process safely; unavailable/denied reads only omit data. */
static ssize_t read_guest(uint32_t address,void *out,size_t length){
    if(!address||length>UINT32_MAX-address)return -1;
    struct iovec local={out,length},remote={hle_ptr(address),length};
    return process_vm_readv(getpid(),&local,1,&remote,1,0);
}
static void text(uint32_t address,char *out,size_t size){
    if(!address){snprintf(out,size,"<null>");return;}
    ssize_t n=read_guest(address,out,size-1);
    if(n<=0){snprintf(out,size,"<unreadable:%#x>",address);return;}
    out[n]=0;for(ssize_t i=0;i<n&&out[i];i++)if((unsigned char)out[i]<32||out[i]==127)out[i]='?';
}
void thing_font_diagnostic(uint32_t site,uint32_t object,uint32_t requested,uint32_t resolved,uint32_t result){
    int saved_errno=errno;
    if(enabled<0){const char *v=getenv("THING_FONT_TRACE");enabled=v&&!strcmp(v,"1");}
    int missing=site==0x768830u;
    if(!missing&&site!=0x76887du)goto done;
    int error=missing||!(result&255u);
    if(!error&&!enabled)goto done;
    if(error){if(errors++>=32)goto done;}else if(traces++>=128)goto done;
    char name[129],path[1025];uint8_t fields[0x89];int loaded=-1;uint32_t glyphs=0,metrics=0;
    text(requested,name,sizeof name);text(resolved,path,sizeof path);
    if(read_guest(object,fields,sizeof fields)==sizeof fields){loaded=fields[0x88];if(loaded){memcpy(&glyphs,fields+0x40,4);memcpy(&metrics,fields+0x50,4);}}
    fprintf(stderr,"[font-%s] site=%#x stage=%s object=%#x requested=\"%s\" resolved=\"%s\" return_al=%u loaded=%d glyphs=%u metrics=%#x\n",error?"error":"trace",site,missing?"resolve-miss":"loader-return",object,name,path,result&255u,loaded,glyphs,metrics);
 done:errno=saved_errno;
}
