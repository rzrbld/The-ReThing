/* PE static TLS templates are distinct from the TlsAlloc/TlsGetValue slots.
 * The guest accesses this vector through FS:[0x2c]. Templates are copied for
 * each guest thread and lazily refreshed when LoadLibrary changes the set.
 * Dynamic module loads initialize the caller before its DLL entry executes.
 */
#include "win32hle.h"
#include "static_tls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TLS_IMAGES 64u
#define TLS_CALLBACKS 64u
typedef struct {
    uint32_t base,size,zero,index_va,generation;
    uint8_t *data;
    uint32_t callbacks[TLS_CALLBACKS],callback_count;
    int process_attached;
} TLSImage;
static TLSImage images[TLS_IMAGES];
static uint32_t generation;
static _Thread_local struct {
    uint32_t fs,vector,synced_generation,seen[TLS_IMAGES],blocks[TLS_IMAGES];
} thread;

static int in_image(uint32_t base,uint32_t span,uint32_t at,uint32_t bytes) {
    return at>=base && (uint64_t)(at-base)+bytes<=span;
}
void thing_tls_image_loaded(uint32_t base) {
    uint32_t nt=base+MEM32(base+0x3c),opt=nt+24;
    if(MEM32(nt)!=0x4550 || MEM16(opt)!=0x10b)hle_fatal("TLS: invalid PE at %#x",base);
    uint32_t span=MEM32(opt+56),dir=MEM32(opt+96+9*8),dir_size=MEM32(opt+100+9*8);
    if(!dir || !dir_size)return;
    if(dir_size<24 || (uint64_t)dir+24>span)hle_fatal("TLS: malformed directory in %#x",base);
    uint32_t va=base+dir,start=MEM32(va),end=MEM32(va+4),index=MEM32(va+8),callbacks=MEM32(va+12),zero=MEM32(va+16);
    if(end<start || !in_image(base,span,start,end-start) || !in_image(base,span,index,4) ||
       (uint64_t)(end-start)+zero>16u*1024u*1024u)hle_fatal("TLS: invalid template/index in %#x",base);
    unsigned slot;
    for(slot=0;slot<TLS_IMAGES;slot++)if(images[slot].base==base)return;
    for(slot=0;slot<TLS_IMAGES;slot++)if(!images[slot].base)break;
    if(slot==TLS_IMAGES)hle_fatal("TLS image capacity exhausted");
    TLSImage *image=&images[slot];
    memset(image,0,sizeof *image);
    image->base=base;image->size=end-start;image->zero=zero;image->index_va=index;
    image->generation=++generation;
    image->data=malloc(image->size?image->size:1);
    if(!image->data)hle_fatal("TLS template allocation failed");
    if(image->size)memcpy(image->data,hle_ptr(start),image->size);
    if(callbacks) {
        unsigned c;
        for(c=0;c<TLS_CALLBACKS;c++) {
            if(!in_image(base,span,callbacks+c*4,4))hle_fatal("TLS callback array outside image");
            uint32_t fn=MEM32(callbacks+c*4);
            if(!fn)break;
            if(!in_image(base,span,fn,1))hle_fatal("TLS callback outside image: %#x",fn);
            image->callbacks[image->callback_count++]=fn;
        }
        if(c==TLS_CALLBACKS)hle_fatal("TLS callback array exceeds supported bound");
    }
    MEM32(index)=slot;
    fprintf(stderr,"[tls] image=%#x slot=%u template=%u zero=%u callbacks=%u\n",base,slot,image->size,zero,image->callback_count);
    if(g_fs_base)thing_tls_thread_attach();
}
void thing_tls_image_unloaded(uint32_t base) {
    for(unsigned i=0;i<TLS_IMAGES;i++)if(images[i].base==base) {
        free(images[i].data);memset(&images[i],0,sizeof images[i]);
        ++generation;
        if(g_fs_base)thing_tls_thread_attach();
        return;
    }
}
void thing_tls_thread_attach(void) {
    if(!g_fs_base)return;
    if(thread.fs && thread.fs!=g_fs_base)hle_fatal("TLS: guest TIB changed on an initialized native thread");
    if(thread.fs==g_fs_base && thread.vector && thread.synced_generation==generation)return;
    if(!thread.vector) {
        thread.vector=hle_va(hle_alloc(TLS_IMAGES*4));
        if(!thread.vector)hle_fatal("TLS vector allocation failed");
        thread.fs=g_fs_base;
    }
    MEM32(g_fs_base+0x2c)=thread.vector;
    uint32_t reasons[TLS_IMAGES]={0};
    for(unsigned i=0;i<TLS_IMAGES;i++) {
        TLSImage *image=&images[i];
        if(!image->base || thread.seen[i]!=image->generation) {
            if(thread.blocks[i])hle_free(hle_ptr(thread.blocks[i]));
            thread.blocks[i]=0;thread.seen[i]=0;MEM32(thread.vector+i*4)=0;
            if(!image->base)continue;
            uint32_t bytes=image->size+image->zero;
            void *block=hle_alloc(bytes?bytes:1);
            if(!block)hle_fatal("TLS thread block allocation failed");
            if(image->size)memcpy(block,image->data,image->size);
            thread.blocks[i]=hle_va(block);thread.seen[i]=image->generation;
            MEM32(thread.vector+i*4)=thread.blocks[i];
            reasons[i]=image->process_attached?2u:1u;
            image->process_attached=1;
        }
    }
    thread.synced_generation=generation;
    /* All vectors exist before a callback can access another image's TLS.
     * A callback can load another module; nested attach sees the new generation. */
    for(unsigned i=0;i<TLS_IMAGES;i++)if(reasons[i]) {
        TLSImage *image=&images[i];
        for(unsigned c=0;c<image->callback_count;c++) {
            uint32_t args[3]={image->base,reasons[i],0};
            hle_call_guest(image->callbacks[c],3,args);
        }
    }
}
