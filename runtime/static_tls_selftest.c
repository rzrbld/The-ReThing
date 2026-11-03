/* Isolated PE TLS lifecycle test. The runtime under test is static_tls.c;
 * only address allocation, guest callback execution and errors are mocked. */
#include "win32hle.h"
#include "static_tls.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ptrdiff_t g_mem_base;
uint32_t g_fs_base;
static uint32_t next=0x20000,callbacks,reason_count[4],free_count;
void *hle_alloc(uint32_t n) {
    assert(next+n<0x100000);
    void *p=hle_ptr(next);memset(p,0,n);next+=(n+15)&~15u;return p;
}
void hle_free(void *p) {(void)p;free_count++;}
void hle_fatal(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);abort();
}
uint32_t hle_va_bad(const void *p) {hle_fatal("unexpected host pointer %p",p);return 0;}
uint32_t hle_call_guest(uint32_t va,int nargs,const uint32_t *args) {
    assert(nargs==3 && va==args[0]+0x500 && args[1]<4 && !args[2]);
    assert(MEM32(g_fs_base+0x2c));callbacks++;reason_count[args[1]]++;return 0;
}
static void image(uint32_t base,unsigned seed) {
    uint32_t nt=base+0x100,opt=nt+24,tls=base+0x300;
    MEM32(base+0x3c)=0x100;MEM32(nt)=0x4550;MEM16(opt)=0x10b;
    MEM32(opt+56)=0x4000;MEM32(opt+96+9*8)=0x300;MEM32(opt+100+9*8)=24;
    MEM32(tls)=base+0x400;MEM32(tls+4)=base+0x40c;MEM32(tls+8)=base+0x440;
    MEM32(tls+12)=base+0x450;MEM32(tls+16)=20;
    for(unsigned i=0;i<12;i++)MEM8(base+0x400+i)=seed+i;
    MEM32(base+0x450)=base+0x500;MEM32(base+0x454)=0;
}
static uint32_t block(unsigned index) {
    return MEM32(MEM32(g_fs_base+0x2c)+index*4);
}
static void check(uint32_t va,unsigned seed) {
    assert(va);
    for(unsigned i=0;i<12;i++)assert(MEM8(va+i)==seed+i);
    for(unsigned i=12;i<32;i++)assert(MEM8(va+i)==0);
}
static void *worker(void *arg) {
    (void)arg;g_fs_base=0xb000;thing_tls_thread_attach();
    check(block(0),10);assert(reason_count[2]==1);
    MEM8(block(0))=99;
    return (void*)(uintptr_t)block(0);
}
int main(void) {
    g_mem_base=(ptrdiff_t)calloc(1,0x100000);assert(g_mem_base);
    image(0x1000,10);image(0x6000,30);
    thing_tls_image_loaded(0x1000);assert(MEM32(0x1440)==0 && !callbacks);
    g_fs_base=0xa000;thing_tls_thread_attach();uint32_t main_block=block(0);check(main_block,10);
    assert(reason_count[1]==1);
    thing_tls_thread_attach();assert(callbacks==1 && block(0)==main_block);
    pthread_t t;void *worker_block;assert(!pthread_create(&t,NULL,worker,NULL));assert(!pthread_join(t,&worker_block));
    assert((uintptr_t)worker_block!=main_block);
    g_fs_base=0xa000;thing_tls_thread_attach();check(block(0),10);
    thing_tls_image_loaded(0x6000);assert(MEM32(0x6440)==1);check(block(1),30);
    assert(reason_count[1]==2);
    thing_tls_image_unloaded(0x6000);assert(!block(1) && free_count==1);
    image(0x6000,50);thing_tls_image_loaded(0x6000);check(block(1),50);
    assert(reason_count[1]==3);
    puts("PE TLS: static index, initialized template, zero-fill, per-thread isolation, callbacks, load/unload/reload PASS");
    free((void*)g_mem_base);return 0;
}
