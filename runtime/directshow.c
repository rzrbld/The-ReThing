/* DirectShow COM boundary for the original PCFMVPlayer.
 * Interface IDs/order/signatures: Wine include/axextend.idl and control.idl.
 * Native MPEG playback is provided by movie_decoder.c. Completion is emitted
 * only after that decoder reports real EOF and drained audio output.
 */
#include "win32hle.h"
#include "directshow.h"
#include "movie_decoder.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define E_POINTER 0x80004003u
#define E_NOINTERFACE 0x80004002u
#define E_INVALIDARG 0x80070057u
#define CANNOT_RENDER 0x80040218u
#define WRONG_STATE 0x80040227u
#define E_FAIL 0x80004005u
#define E_NOTIMPL 0x80004001u
enum {GRAPH,CONTROL,EVENT,WINDOW,POSITION,INTERFACES};
static const uint8_t clsid[16]={0xb3,0xeb,0x36,0xe4,0x4f,0x52,0xce,0x11,0x9f,0x53,0,0x20,0xaf,0x0b,0xa7,0x70};
static const uint8_t unknown[16]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
static const uint8_t dispatch[16]={0,4,2,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
static const uint8_t ids[INTERFACES][16]={
    {0xa9,0x68,0xa8,0x56,0xd4,0x0a,0xce,0x11,0xb0,0x3a,0,0x20,0xaf,0x0b,0xa7,0x70},
    {0xb1,0x68,0xa8,0x56,0xd4,0x0a,0xce,0x11,0xb0,0x3a,0,0x20,0xaf,0x0b,0xa7,0x70},
    {0xc0,0x68,0xa8,0x56,0xd4,0x0a,0xce,0x11,0xb0,0x3a,0,0x20,0xaf,0x0b,0xa7,0x70},
    {0xb4,0x68,0xa8,0x56,0xd4,0x0a,0xce,0x11,0xb0,0x3a,0,0x20,0xaf,0x0b,0xa7,0x70},
    {0xb2,0x68,0xa8,0x56,0xd4,0x0a,0xce,0x11,0xb0,0x3a,0,0x20,0xaf,0x0b,0xa7,0x70}
};
typedef struct Graph {
    struct Graph *next;
    uint32_t references,object[INTERFACES];
    ThingMovie *movie;
    uint32_t state,event,event_arg1,event_arg2,completion_issued;
    uint32_t last_event,last_arg1,last_arg2,last_valid;
    uint32_t owner,style;
    int32_t visible;
    int32_t left,top,width,height;
} Graph;
static Graph *graphs;
static uint32_t tables[INTERFACES];
static const char *const kinds[]={"IGraphBuilder","IMediaControl","IMediaEventEx","IVideoWindow","IMediaPosition"};
static Graph *self(unsigned *kind) {
    for(Graph *g=graphs;g;g=g->next)for(unsigned i=0;i<INTERFACES;i++)if(g->object[i]==A32(0)) {
        if(kind)*kind=i;
        return g;
    }
    hle_fatal("invalid DirectShow interface %#x",A32(0));return NULL;
}
static int interface(const uint8_t *iid) {
    if(!iid)return -1;
    if(!memcmp(iid,unknown,16))return GRAPH;
    for(unsigned i=0;i<INTERFACES;i++)if(!memcmp(iid,ids[i],16))return (int)i;
    if(!memcmp(iid+4,ids[GRAPH]+4,12)) {
        uint32_t value;memcpy(&value,iid,4);
        if(value==0x56a8689fu)return GRAPH; /* inherited IFilterGraph */
        if(value==0x56a868b6u)return EVENT; /* inherited IMediaEvent */
    }
    return -1;
}
static void qi(void) {
    unsigned current;Graph *g=self(&current);
    if(!A32(2))RET(E_POINTER,3);
    MEM32(A32(2))=0;
    const uint8_t *iid=APTR(1);
    int i=interface(iid);
    if(i<0 && current!=GRAPH && iid && !memcmp(iid,dispatch,16))i=(int)current;
    if(i<0)RET(E_NOINTERFACE,3);
    MEM32(A32(2))=g->object[i];g->references++;RET(0,3);
}
static void addref(void) {Graph *g=self(NULL);RET(++g->references,1);}
static void release(void) {
    Graph *g=self(NULL);uint32_t count=--g->references;
    if(!count) {
        Graph **p=&graphs;while(*p && *p!=g)p=&(*p)->next;
        if(*p)*p=g->next;
        for(unsigned i=0;i<INTERFACES;i++)hle_free(hle_ptr(g->object[i]));
        if(g->movie)thing_movie_close(g->movie);
        free(g);
    }
    RET(count,1);
}
static void unsupported(void) {
    (void)self(NULL);
    hle_fatal("DirectShow method not implemented: %s",g_cur_import?g_cur_import:"unknown");
}
static void render_file(void) {
    Graph *g=self(NULL);
    if(!A32(1))RET(E_POINTER,3);
    if(A32(2))RET(E_INVALIDARG,3);
    char path[4096],host[PATH_MAX];size_t used=0;unsigned i;
    for(i=0;i<32768;i++) {
        uint32_t c=MEM16(A32(1)+i*2);
        if(!c)break;
        if(c>=0xd800 && c<=0xdbff) {
            uint32_t lo=MEM16(A32(1)+(i+1)*2);
            if(lo<0xdc00 || lo>0xdfff)RET(E_INVALIDARG,3);
            c=0x10000+((c-0xd800)<<10)+lo-0xdc00;i++;
        } else if(c>=0xdc00 && c<=0xdfff)RET(E_INVALIDARG,3);
        unsigned n=c<0x80?1:c<0x800?2:c<0x10000?3:4;
        if(used+n>=sizeof path)RET(E_INVALIDARG,3);
        if(n==1)path[used++]=(char)c;
        else {
            if(n==2)path[used++]=(char)(0xc0|(c>>6));
            else if(n==3) {path[used++]=(char)(0xe0|(c>>12));path[used++]=(char)(0x80|((c>>6)&63));}
            else {path[used++]=(char)(0xf0|(c>>18));path[used++]=(char)(0x80|((c>>12)&63));path[used++]=(char)(0x80|((c>>6)&63));}
            path[used++]=(char)(0x80|(c&63));
        }
    }
    if(i==32768)RET(E_INVALIDARG,3);
    path[used]=0;
    if(!hle_host_path(path,host,sizeof host))RET(0x80070002u,3);
    ThingMovie *movie=thing_movie_open(host);
    if(!movie) {fprintf(stderr,"[directshow] RenderFile(%s): decoder rejected media\n",path);RET(CANNOT_RENDER,3);}
    if(g->movie)thing_movie_close(g->movie);
    g->movie=movie;g->state=g->event=g->completion_issued=g->last_valid=0;g->visible=0;
    fprintf(stderr,"[directshow] RenderFile(%s): native MPEG opened duration=%.3f\n",path,thing_movie_duration(movie));
    RET(0,3);
}
static void stop(void) {
    Graph *g=self(NULL);if(g->movie)thing_movie_stop(g->movie);
    g->state=0;g->event=g->completion_issued=0;RET(0,1);
}
static void run(void) {
    Graph *g=self(NULL);if(!g->movie)RET(WRONG_STATE,1);
    if(g->state==2)RET(0,1); /* Run while already running is idempotent. */
    if(thing_movie_run(g->movie))RET(E_FAIL,1);
    g->state=2;g->visible=-1;g->event=g->completion_issued=0;RET(0,1);
}
static void pause_movie(void) {
    Graph *g=self(NULL);if(!g->movie)RET(WRONG_STATE,1);
    thing_movie_pause(g->movie);g->state=1;RET(0,1);
}
static void get_state(void) {
    Graph *g=self(NULL);if(!A32(2))RET(E_POINTER,3);
    MEM32(A32(2))=g->state;RET(0,3);
}
static uint64_t milliseconds(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000u+(unsigned)t.tv_nsec/1000000u;
}
static void get_event(void) {
    Graph *g=self(NULL);
    if(!A32(1)||!A32(2)||!A32(3))RET(E_POINTER,5);
    int32_t timeout=(int32_t)A32(4);uint64_t start=milliseconds();
    for(;;) {
        extern void ds_input_pump(void) __attribute__((weak));
        if(ds_input_pump)ds_input_pump();
        if(g->movie && g->state==2 && !g->completion_issued) {
            int state=thing_movie_tick(g->movie);
            if(state) {
                if(state<0)g->state=0;
                g->completion_issued=1;
                g->event=state>0?1u:3u;g->event_arg1=state>0?0u:E_FAIL;g->event_arg2=0;
                fprintf(stderr,"[directshow] %s from native decoder\n",state>0?"EC_COMPLETE (decoded EOF/audio drained)":"EC_ERRORABORT");
            }
        }
        if(g->event) {
            MEM32(A32(1))=g->event;MEM32(A32(2))=g->event_arg1;MEM32(A32(3))=g->event_arg2;
            g->last_event=g->event;g->last_arg1=g->event_arg1;g->last_arg2=g->event_arg2;g->last_valid=1;g->event=0;
            RET(0,5);
        }
        if(timeout==0 || (timeout>0 && milliseconds()-start>=(uint32_t)timeout))break;
        hle_block_begin();usleep(1000);hle_block_end();
    }
    for(unsigned i=1;i<=3;i++)MEM32(A32(i))=0;
    RET(0x80004004u,5); /* empty event queue */
}
static void free_event(void) {
    Graph *g=self(NULL);
    if(!g->last_valid || A32(1)!=g->last_event || A32(2)!=g->last_arg1 || A32(3)!=g->last_arg2)RET(E_INVALIDARG,4);
    g->last_valid=0;RET(0,4); /* emitted events contain no heap-owned payloads */
}
static void get_duration(void) {
    Graph *g=self(NULL);if(!A32(1))RET(E_POINTER,2);if(!g->movie)RET(WRONG_STATE,2);
    double value=thing_movie_duration(g->movie);memcpy(APTR(1),&value,8);RET(0,2);
}
static void get_position(void) {
    Graph *g=self(NULL);if(!A32(1))RET(E_POINTER,2);if(!g->movie)RET(WRONG_STATE,2);
    double value=thing_movie_position(g->movie);memcpy(APTR(1),&value,8);RET(0,2);
}
static void put_position(void) {
    Graph *g=self(NULL);if(!g->movie)RET(WRONG_STATE,3);
    uint64_t bits=(uint64_t)A32(1)|(uint64_t)A32(2)<<32;double value;memcpy(&value,&bits,8);
    if(!isfinite(value)||value<0 || value>thing_movie_duration(g->movie))RET(E_INVALIDARG,3);
    if(thing_movie_seek(g->movie,value))RET(E_FAIL,3);
    g->event=g->completion_issued=0;RET(0,3);
}
static void can_seek(void) {Graph *g=self(NULL);if(!A32(1))RET(E_POINTER,2);MEM32(A32(1))=g->movie?0xffffffffu:0;RET(0,2);}
static void put_owner(void) {Graph *g=self(NULL);g->owner=A32(1);RET(0,2);}
static void get_owner(void) {Graph *g=self(NULL);if(!A32(1))RET(E_POINTER,2);MEM32(A32(1))=g->owner;RET(0,2);}
static void put_style(void) {Graph *g=self(NULL);g->style=A32(1);RET(0,2);}
static void get_style(void) {Graph *g=self(NULL);if(!A32(1))RET(E_POINTER,2);MEM32(A32(1))=g->style;RET(0,2);}
static void put_visible(void) {
    Graph *g=self(NULL);g->visible=A32(1)?-1:0;
    /* PCFMVPlayer hides its video child after real EOS, before releasing or
     * stopping the movie. The native port shares the game's SDL window, so
     * hiding that entire window here would incorrectly hide the next menu.
     * Presentation occurs only on GetEvent ticks, and no frames follow EOS. */
    RET(0,2);
}
static void get_visible(void) {Graph *g=self(NULL);if(!A32(1))RET(E_POINTER,2);MEM32(A32(1))=(uint32_t)g->visible;RET(0,2);}
static void set_window_position(void) {
    Graph *g=self(NULL);
    int32_t x=(int32_t)A32(1),y=(int32_t)A32(2),w=(int32_t)A32(3),h=(int32_t)A32(4);
    if(w<0 || h<0)RET(E_INVALIDARG,5);
    /* The actual game uses (0,0,client_width,client_height) of the shared SDL
     * window. Offscreen/child-window composition is not implemented. */
    if(x || y)RET(E_NOTIMPL,5);
    g->left=x;g->top=y;g->width=w;g->height=h;RET(0,5);
}
static void get_window_position(void) {
    Graph *g=self(NULL);for(unsigned i=1;i<=4;i++)if(!A32(i))RET(E_POINTER,5);
    MEM32(A32(1))=g->left;MEM32(A32(2))=g->top;MEM32(A32(3))=g->width;MEM32(A32(4))=g->height;RET(0,5);
}
static void initialize(void) {
    if(tables[GRAPH])return;
    static win32hle_shim methods[INTERFACES][47];
    static char names[INTERFACES][46][80];
    const unsigned counts[]={18,16,16,46,18};
    for(unsigned k=0;k<INTERFACES;k++) {
        for(unsigned i=0;i<counts[k];i++) {
            snprintf(names[k][i],sizeof names[k][i],"ThingDirectShow.%s.%u",kinds[k],i);
            methods[k][i].name=names[k][i];methods[k][i].fn=unsupported;
        }
        methods[k][0].fn=qi;methods[k][1].fn=addref;methods[k][2].fn=release;
        if(k==GRAPH)methods[k][13].fn=render_file;
        if(k==CONTROL) {methods[k][7].fn=run;methods[k][8].fn=pause_movie;methods[k][9].fn=stop;methods[k][10].fn=get_state;}
        if(k==EVENT) {methods[k][8].fn=get_event;methods[k][12].fn=free_event;}
        if(k==POSITION) {methods[k][7].fn=get_duration;methods[k][8].fn=put_position;methods[k][9].fn=get_position;methods[k][16].fn=can_seek;methods[k][17].fn=can_seek;}
        if(k==WINDOW) {methods[k][9].fn=put_style;methods[k][10].fn=get_style;methods[k][19].fn=put_visible;methods[k][20].fn=get_visible;methods[k][29].fn=put_owner;methods[k][30].fn=get_owner;methods[k][39].fn=set_window_position;methods[k][40].fn=get_window_position;}
        tables[k]=hle_com_vtable(methods[k]);
    }
}
static uint32_t create(const uint8_t *iid,uint32_t *out) {
    if(!out)return E_POINTER;
    *out=0;int kind=interface(iid);
    if(kind<0)return E_NOINTERFACE;
    initialize();
    Graph *g=calloc(1,sizeof *g);
    if(!g)return 0x8007000eu;
    for(unsigned i=0;i<INTERFACES;i++) {
        uint32_t *object=hle_alloc(8);
        if(!object)hle_fatal("DirectShow interface allocation failed");
        object[0]=tables[i];object[1]=i;g->object[i]=hle_va(object);
    }
    g->references=1;g->width=640;g->height=480;g->next=graphs;graphs=g;*out=g->object[kind];return 0;
}
void thing_directshow_register(void) {
    static int registered;
    if(!registered) {registered=1;hle_com_register_class(clsid,create);}
}
