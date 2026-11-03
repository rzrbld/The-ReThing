/* Isolated COM ABI test: real adapter methods, mocked guest allocator/registry. */
#include "win32hle.h"
#include "directshow.h"
#include "movie_decoder.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ptrdiff_t g_mem_base;
uint32_t g_eax,g_esp;
const char *g_cur_import;
static uint32_t next=0x20000,freed;
static recomp_func_t functions[256];
static const char *names[256];
static unsigned methods;
static uint32_t (*factory)(const uint8_t*,uint32_t*);
struct ThingMovie {double position;int playing;};
static ThingMovie *backend;
static unsigned closes,ticks,seeks;
static int tick_result,decoded_eof;
ThingMovie *thing_movie_open(const char *path) {
    if(strstr(path,"bad"))return NULL;
    backend=calloc(1,sizeof *backend);return backend;
}
void thing_movie_close(ThingMovie *movie) {closes++;free(movie);if(backend==movie)backend=NULL;}
int thing_movie_run(ThingMovie *movie) {movie->playing=1;return 0;}
void thing_movie_pause(ThingMovie *movie) {movie->playing=0;}
void thing_movie_stop(ThingMovie *movie) {movie->playing=0;}
int thing_movie_seek(ThingMovie *movie,double time) {seeks++;movie->position=time;decoded_eof=0;tick_result=0;return 0;}
double thing_movie_duration(const ThingMovie *movie) {(void)movie;return 123.0;}
double thing_movie_position(const ThingMovie *movie) {return movie->position;}
int thing_movie_tick(ThingMovie *movie) {(void)movie;ticks++;assert(tick_result!=1||decoded_eof);return tick_result;}
int hle_host_path(const char *guest,char *host,size_t size) {snprintf(host,size,"%s",guest);return 1;}
void hle_block_begin(void) {}
void hle_block_end(void) {}
void *hle_alloc(uint32_t n) {assert(next+n<0x100000);void *p=hle_ptr(next);memset(p,0,n);next+=(n+15)&~15u;return p;}
void hle_free(void *p) {(void)p;freed++;}
void hle_fatal(const char *fmt,...) {va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);abort();}
uint32_t hle_va_bad(const void *p) {hle_fatal("bad pointer %p",p);return 0;}
uint32_t hle_com_vtable(const win32hle_shim *table) {
    unsigned count=0;while(table[count].name)count++;
    uint32_t *out=hle_alloc(count*4);
    for(unsigned i=0;i<count;i++) {
        assert(methods<256);out[i]=0x7f000000+methods*16;
        functions[methods]=table[i].fn;names[methods++]=table[i].name;
    }
    return hle_va(out);
}
void hle_com_register_class(const uint8_t *clsid,uint32_t(*create)(const uint8_t*,uint32_t*)) {
    assert(!factory && *(const uint32_t*)clsid==0xe436ebb3);factory=create;
}
static uint32_t invoke(uint32_t object,unsigned slot,unsigned count,const uint32_t *args) {
    uint32_t start=0x10000;g_esp=start;MEM32(start)=0xdead0000;
    MEM32(start+4)=object;for(unsigned i=1;i<count;i++)MEM32(start+4+i*4)=args[i-1];
    uint32_t va=MEM32(MEM32(object)+slot*4);unsigned n=(va-0x7f000000)/16;
    assert(n<methods);g_cur_import=names[n];functions[n]();
    assert(g_esp==start+4+count*4);return g_eax;
}
int main(void) {
    g_mem_base=(ptrdiff_t)calloc(1,0x100000);assert(g_mem_base);
    thing_directshow_register();thing_directshow_register();assert(factory);
    uint8_t iid[16]={0xa9,0x68,0xa8,0x56,0xd4,0x0a,0xce,0x11,0xb0,0x3a,0,0x20,0xaf,0x0b,0xa7,0x70};
    uint32_t graph=0;assert(factory(iid,&graph)==0 && graph);
    uint32_t *out=hle_alloc(32),iid_va=hle_va(hle_alloc(16)),out_va=hle_va(out);
    const uint8_t ids[]={0xb1,0xc0,0xb4,0xb2};uint32_t objects[4];
    for(unsigned i=0;i<4;i++) {
        iid[0]=ids[i];memcpy(hle_ptr(iid_va),iid,16);uint32_t args[]={iid_va,out_va};
        assert(invoke(graph,0,3,args)==0 && *out);objects[i]=*out;
    }
    assert(invoke(objects[0],7,1,NULL)==0x80040227u);
    assert(invoke(objects[0],8,1,NULL)==0x80040227u);
    assert(invoke(objects[0],9,1,NULL)==0);
    uint32_t event_args[]={out_va,out_va+4,out_va+8,0};
    assert(invoke(objects[1],8,5,event_args)==0x80004004u);
    uint32_t wide=hle_va(hle_alloc(128));const char path[]="C:\\data\\FMV\\test.mpg";
    for(unsigned i=0;i<sizeof path;i++)MEM16(wide+i*2)=(unsigned char)path[i];
    uint32_t render_args[]={wide,0};
    assert(invoke(graph,13,3,render_args)==0 && backend);
    assert(invoke(objects[0],7,1,NULL)==0 && backend->playing);
    uint32_t state_args[]={0,out_va};
    assert(invoke(objects[0],10,3,state_args)==0 && *out==2);
    assert(invoke(objects[1],8,5,event_args)==0x80004004u && !*out && ticks==1);
    /* Duration/clock alone must never generate EC_COMPLETE. */
    backend->position=123.0;
    assert(invoke(objects[1],8,5,event_args)==0x80004004u && !*out);
    decoded_eof=1;tick_result=1;
    assert(invoke(objects[1],8,5,event_args)==0 && out[0]==1 && !out[1] && !out[2]);
    uint32_t visible_out[]={out_va},hide[]={0};
    assert(invoke(objects[2],20,2,visible_out)==0 && *out==0xffffffffu);
    assert(invoke(objects[2],19,2,hide)==0);
    assert(invoke(objects[2],20,2,visible_out)==0 && *out==0);
    unsigned completed_ticks=ticks;
    assert(invoke(objects[0],10,3,state_args)==0 && *out==2);
    assert(invoke(objects[0],7,1,NULL)==0); /* no new transition/no duplicate EOS */
    assert(invoke(objects[1],8,5,event_args)==0x80004004u && ticks==completed_ticks);
    uint32_t free_args[]={1,0,0};
    assert(invoke(objects[1],12,4,free_args)==0);
    assert(invoke(objects[1],12,4,free_args)==0x80070057u);
    double seconds=25.0;uint64_t bits;memcpy(&bits,&seconds,8);
    uint32_t seek_args[]={(uint32_t)bits,(uint32_t)(bits>>32)};
    assert(invoke(objects[3],8,3,seek_args)==0 && seeks==1 && !decoded_eof);
    uint32_t value_args[]={out_va+16};
    assert(invoke(objects[3],9,2,value_args)==0);memcpy(&seconds,out+4,8);assert(seconds==25.0);
    assert(invoke(objects[3],7,2,value_args)==0);memcpy(&seconds,out+4,8);assert(seconds==123.0);
    assert(invoke(objects[0],7,1,NULL)==0);
    assert(invoke(objects[0],8,1,NULL)==0 && !backend->playing);
    assert(invoke(objects[0],10,3,state_args)==0 && *out==1);
    assert(invoke(objects[0],7,1,NULL)==0 && backend->playing);
    tick_result=-1;
    assert(invoke(objects[1],8,5,event_args)==0 && out[0]==3 && out[1]==0x80004005u);
    uint32_t error_args[]={3,0x80004005u,0};assert(invoke(objects[1],12,4,error_args)==0);
    assert(invoke(objects[0],9,1,NULL)==0 && !backend->playing);
    assert(invoke(objects[1],8,5,event_args)==0x80004004u && !*out);
    uint32_t owner_args[]={0x1234};assert(invoke(objects[2],29,2,owner_args)==0);
    uint32_t out_args[]={out_va};assert(invoke(objects[2],30,2,out_args)==0 && *out==0x1234);
    uint32_t rectangle[]={0,0,640,480},rectangle_out[]={out_va,out_va+4,out_va+8,out_va+12};
    assert(invoke(objects[2],39,5,rectangle)==0);
    assert(invoke(objects[2],40,5,rectangle_out)==0 && out[2]==640 && out[3]==480);
    /* Invalid media fails without replacing the previously validated stream. */
    MEM16(wide)='b';MEM16(wide+2)='a';MEM16(wide+4)='d';MEM16(wide+6)=0;
    assert(invoke(graph,13,3,render_args)==0x80040218u && closes==0);
    memset(hle_ptr(iid_va),0xff,16);uint32_t args[]={iid_va,out_va};*out=0xffffffff;
    assert(invoke(graph,0,3,args)==0x80004002u && !*out);
    memset(hle_ptr(iid_va),0,16);MEM8(iid_va+8)=0xc0;MEM8(iid_va+15)=0x46;
    assert(invoke(objects[0],0,3,args)==0 && *out==graph);
    assert(invoke(graph,2,1,NULL)==5);
    for(unsigned i=0;i<4;i++)assert(invoke(objects[i],2,1,NULL)==4-i);
    assert(invoke(graph,2,1,NULL)==0 && freed==5 && closes==1);
    puts("DirectShow: QI/refcounts/stdcall, valid-media gate, real-EOS-only single completion, seek/reset, pause/stop, error-event and cleanup PASS");
    free((void*)g_mem_base);return 0;
}
