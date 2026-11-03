#include "fpu_env.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static uint16_t get16(const void *p) {uint16_t v;memcpy(&v,p,2);return v;}
static void put16(void *p,uint16_t v) {memcpy(p,&v,2);}
int main(void) {
#if defined(__x86_64__) || defined(__i386__)
    _Alignas(16) uint8_t host[512],before[512],after[512];
    __asm__ volatile("fxsave %0":"=m"(host));
    const uint16_t words[]={0x027f,0x037f,0x0b7f,0x0f7f};
    unsigned count=0;
    for(int depth=0;depth<=8;depth++)for(unsigned w=0;w<4;w++)for(int cmp=-1;cmp<=2;cmp++) {
        if(!depth && cmp!=1)continue;
        for(unsigned edit=0;edit<8;edit++) {
            double st[8]={0},old_st[8];
            for(int i=0;i<depth;i++)st[i]=i%3==0?0.0:i%3==1?(double)(i+1):-(double)(i+1);
            if(depth)st[0]=cmp<0?-1.0:cmp==0?0.0:cmp==1?1.0:NAN;
            memcpy(old_st,st,sizeof st);
            uint16_t cw=words[w],masked_native,restored_native,status_native;
            uint8_t native[28],model[28];
            __asm__ volatile("fninit\n\tfldcw %0"::"m"(cw):"memory");
            for(int i=depth-1;i>=0;i--)__asm__ volatile("fldl %0"::"m"(st[i]):"memory");
            double zero=0;
            if(depth)__asm__ volatile("fcoml %0"::"m"(zero):"memory");
            __asm__ volatile("fxsave %0":"=m"(before)::"memory");
            __asm__ volatile("fnstenv %0\n\tfnstcw %1":"=m"(native),"=m"(masked_native)::"memory");
            thing_fpu_env_store28(model,st,depth,&cw,cmp);
            assert(get16(model)==get16(native));
            assert((get16(model+4)&0x7f00u)==(get16(native+4)&0x7f00u));
            assert(get16(model+8)==get16(native+8));
            assert(cw==masked_native);
            uint16_t c=(uint16_t)((edit&1?0x100:0)|(edit&2?0x200:0)|(edit&4?0x4000:0));
            put16(model+4,(get16(model+4)&0xbcffu)|c);
            put16(native+4,(get16(native+4)&0xbcffu)|c);
            /* CRT AND operates on a dword, clearing the reserved high half. */
            put16(model+6,0);put16(native+6,0);
            int result=thing_fpu_env_load28(model,st,depth,&cw);
            __asm__ volatile("fldenv %3\n\tfnstcw %0\n\tfnstsw %1\n\tfxsave %2"
                :"=m"(restored_native),"=m"(status_native),"=m"(after):"m"(native):"memory");
            assert(cw==restored_native);
            assert((result&0x4700u)==(status_native&0x4700u));
            assert(((status_native>>11)&7u)==((unsigned)(-depth)&7u));
            assert(!memcmp(st,old_st,sizeof st));
            for(int i=0;i<8;i++)assert(!memcmp(before+32+i*16,after+32+i*16,10));
            count++;
        }
    }
    __asm__ volatile("fxrstor %0"::"m"(host):"memory");
    for(unsigned invalid=0;invalid<6;invalid++) {
        pid_t child=fork();assert(child>=0);
        if(!child) {
            struct rlimit limit={0,0};setrlimit(RLIMIT_CORE,&limit);close(2);
            double st[8]={1.0};uint16_t cw=0x027f;uint8_t env[28];
            thing_fpu_env_store28(env,st,1,&cw,1);
            if(invalid==0)put16(env+8,get16(env+8)^1u);
            else if(invalid==1)put16(env+4,get16(env+4)^0x0800u);
            else if(invalid==2)put16(env,get16(env)^0x0400u);
            else if(invalid==3)env[12]=1;
            else if(invalid==4)st[0]=2.0;
            else thing_fpu_env_store28(env,st,1,&cw,1);
            (void)thing_fpu_env_load28(env,st,1,&cw);_exit(0);
        }
        int status;assert(waitpid(child,&status,0)==child);
        assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
    }
    printf("Checked CRT x87 environment: %u native comparisons of CW, masking, C bits, TOP, tag, register preservation PASS\n",count);
    puts("Six unsupported TOP/tag/CW/pointer/register/nested-store edits correctly abort PASS");
    puts("Coverage excludes x87 sticky exceptions, instruction/data pointers and arbitrary environment/TOP/tag edits (guarded unsupported).");
#else
    puts("Native x87 comparison requires an x86 host");return 77;
#endif
    return 0;
}
