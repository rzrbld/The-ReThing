/* Exercise the actual PE binder's module-aware game export hook. */
#include "win32hle.h"
#include "pe_loader.h"
#include "pe_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

ptrdiff_t g_mem_base;
static unsigned game_calls,resolver_calls;
uint32_t thing_game_export(const char *module,const char *name,uint32_t ordinal) {
    game_calls++;
    if(!strcasecmp(module,"TheThing.exe") && name && !strcmp(name,"?GameAction@CA@@YAXXZ"))return 0x401000;
    if(!strcasecmp(module,"level1.dll") && !name && ordinal==7)return 0x11001000;
    return 0;
}
static uint32_t system_resolve(const char *name) {
    resolver_calls++;
    if(!strcmp(name,"Sleep"))return 0x7f000010;
    if(!strcmp(name,"dsound.dll#11"))return 0x7f000020;
    assert(!strcmp(name,"?MissingAction@CA@@YAXXZ"));
    return 0;
}
int main(void) {
    g_mem_base=(ptrdiff_t)calloc(1,0x10000);assert(g_mem_base);
    const uint32_t base=0x1000;
    pe_import_descriptor *d=hle_ptr(base+0x100);
    const char *modules[]={"TheThing.exe","level1.dll","KERNEL32.dll","DSOUND.dll","TheThing.exe"};
    const char *names[]={"?GameAction@CA@@YAXXZ",NULL,"Sleep",NULL,"?MissingAction@CA@@YAXXZ"};
    for(unsigned i=0;i<5;i++) {
        d[i].Name=0x400+i*0x40;strcpy(hle_ptr(base+d[i].Name),modules[i]);
        d[i].OriginalFirstThunk=0x600+i*0x10;d[i].FirstThunk=0x700+i*0x10;
        if(names[i]) {
            uint32_t name=0x800+i*0x40;strcpy(hle_ptr(base+name+2),names[i]);
            MEM32(base+d[i].OriginalFirstThunk)=name;
        } else MEM32(base+d[i].OriginalFirstThunk)=PE_ORDINAL_FLAG|(i==1?7:11);
    }
    pe_image image={.base=base,.import_rva=0x100};
    assert(recomp_pe_bind(&image,system_resolve)==1);
    assert(MEM32(base+0x700)==0x401000);
    assert(MEM32(base+0x710)==0x11001000);
    assert(MEM32(base+0x720)==0x7f000010);
    assert(MEM32(base+0x730)==0x7f000020);
    assert(MEM32(base+0x740)==0);
    assert(game_calls==5 && resolver_calls==3);
    puts("PE import binding: game name/ordinal guest VAs, unchanged Win32 name/ordinal shims, unresolved failure PASS");
    free((void*)g_mem_base);return 0;
}
