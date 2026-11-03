/* A narrowly scoped replacement for the missing-file exception in the
 * original save-slot enumeration try/catch. Existing/corrupt/unreadable saves
 * continue through the original stream reader; never create dummy saves. */
#include "win32hle.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
int thing_save_slot_absent(uint32_t guest_path){
    if(!guest_path)return 0;
    const char *path=hle_ptr(guest_path),*leaf=path;
    for(const char *p=path;*p;p++)if(*p=='/'||*p=='\\')leaf=p+1;
    if(strlen(leaf)!=9||strncasecmp(leaf,"Slot",4)||leaf[4]<'1'||leaf[4]>'8'||strcasecmp(leaf+5,".sav"))return 0;
    int old_errno=errno;char host[4096]={0};struct stat st;
    hle_host_path(path,host,sizeof host);
    int absent=host[0]&&stat(host,&st)<0&&errno==ENOENT;
    if(absent)fprintf(stderr,"[save-slot] empty slot: %s; original empty-slot handler\n",path);
    errno=old_errno;return absent;
}
