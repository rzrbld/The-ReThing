#define _GNU_SOURCE
#include "win32hle.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
ptrdiff_t g_mem_base;
extern int thing_save_slot_absent(uint32_t);
int main(void){
 char temp[]="/tmp/thing-save-slots-XXXXXX",path[512];unsigned char memory[1024]={0};
 assert(mkdtemp(temp));g_mem_base=(ptrdiff_t)memory;hle_set_drive('C',temp);
 snprintf(path,sizeof path,"%s/bin",temp);assert(!mkdir(path,0700));
 strcpy((char*)memory+16,"C:\\bin\\Slot1.sav");errno=EDOM;
 assert(thing_save_slot_absent(16)==1&&errno==EDOM);
 snprintf(path,sizeof path,"%s/bin/sLoT1.SaV",temp);FILE *f=fopen(path,"wb");assert(f);fputs("existing save bytes",f);fclose(f);
 assert(thing_save_slot_absent(16)==0);assert(!unlink(path));
 snprintf(path,sizeof path,"%s/overlay",temp);assert(!mkdir(path,0700));hle_set_overlay(temp,path);
 snprintf(path,sizeof path,"%s/overlay/bin",temp);assert(!mkdir(path,0700));
 snprintf(path,sizeof path,"%s/overlay/bin/Slot1.sav",temp);f=fopen(path,"wb");assert(f);fclose(f);
 assert(thing_save_slot_absent(16)==0);assert(!unlink(path));
 strcpy((char*)memory+16,"C:\\bin\\Slot9.sav");assert(!thing_save_slot_absent(16));
 strcpy((char*)memory+16,"C:\\bin\\Other.sav");assert(!thing_save_slot_absent(16));assert(!thing_save_slot_absent(0));
 snprintf(path,sizeof path,"%s/overlay/bin",temp);rmdir(path);snprintf(path,sizeof path,"%s/overlay",temp);rmdir(path);snprintf(path,sizeof path,"%s/bin",temp);rmdir(path);rmdir(temp);
 puts("PASS: missing slot, existing mixed-case slot, overlay slot, unrelated paths, errno");
}
