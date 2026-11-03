#include "win32hle.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
ptrdiff_t g_mem_base;
uint32_t g_cur_func=0x7b3dab,g_esp=0x200,g_ebp=0x300;
extern void thing_exception_diagnostic(uint32_t,uint32_t,uint32_t);
int main(void){
    unsigned char memory[4096]={0},before[4096];g_mem_base=(ptrdiff_t)memory;
    uint32_t parameters[]={0x19930520,0x400,0x600},object[16]={0};
    object[1]=0x800;object[3]=0x80070057;
    memcpy(memory+0x100,parameters,sizeof parameters);
    memcpy(memory+0x400,object,sizeof object);
    strcpy((char*)memory+0x800,"TEST save exception message");
    memcpy(before,memory,sizeof memory);errno=EDOM;
    thing_exception_diagnostic(0xe06d7363,3,0x100);
    assert(errno==EDOM&&!memcmp(before,memory,sizeof memory));
    thing_exception_diagnostic(0xe06d7363,99,0xfffffff0);
    thing_exception_diagnostic(1,0,0);
    assert(errno==EDOM&&!memcmp(before,memory,sizeof memory));
    assert(g_cur_func==0x7b3dab&&g_esp==0x200&&g_ebp==0x300);
    puts("PASS: exception inspection preserves guest state, errno; invalid pointers/counts safe");
}
