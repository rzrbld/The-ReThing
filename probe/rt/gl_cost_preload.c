/* Optional SINGLE-renderer diagnostic. No GL calls are inserted. */
#define _GNU_SOURCE
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
#define V(n,p,a) ID_##n,
#define R(t,n,p,a) ID_##n,
#include "gl_cost_calls.def"
#undef V
#undef R
    ID_buffer_vertex, ID_buffer_index_small, ID_buffer_index_large,
    CALL_COUNT
};
static const char *names[] = {
#define V(n,p,a) #n,
#define R(t,n,p,a) #n,
#include "gl_cost_calls.def"
#undef V
#undef R
    "glBufferData.vertex", "glBufferData.index_le32", "glBufferData.index_gt32_or_zero",
};
#define V(n,p,a) static void (*real_##n) p;
#define R(t,n,p,a) static t (*real_##n) p;
#include "gl_cost_calls.def"
#undef V
#undef R

/* Only the thread invoking begin/Present is measured; worker calls pass through. */
struct Count { uint64_t calls, samples, cpu, wall, max_wall; };
static _Thread_local struct {
    struct Count counts[CALL_COUNT];
    unsigned remaining, frames, window, recursion;
    uint32_t random;
    uint64_t presents;
} state;
static uint64_t auto_frame;
static uint64_t now(clockid_t clock) {
    struct timespec ts;
    if (clock_gettime(clock, &ts)) return 0;
    return (uint64_t)ts.tv_sec * 1000000000u + ts.tv_nsec;
}
__attribute__((constructor)) static void resolve(void) {
    int saved = errno;
#define V(n,p,a) *(void **)(&real_##n) = dlsym(RTLD_NEXT, #n);
#define R(t,n,p,a) V(n,p,a)
#include "gl_cost_calls.def"
#undef V
#undef R
    const char *s = getenv("THING_GLCOST_START_FRAME");
    if (s) auto_frame = strtoull(s, NULL, 10);
    errno = saved;
}
void ds_gl_cost_flush(void) {
    int saved = errno;
    if (!state.remaining && !state.frames) return;
    state.remaining = 0;
    const char *path = getenv("THING_GLCOST_FILE");
    if (!path || !*path) path = "gl-cost.csv";
    FILE *f = fopen(path, "a");
    if (f) {
        if (ftell(f) == 0)
            fputs("window,frames,end_present,call,calls,samples,sampled_cpu_ns,sampled_wall_ns,max_sample_wall_ns\n", f);
        for (unsigned i=0; i<CALL_COUNT; ++i) {
            struct Count *c = &state.counts[i];
            if (c->calls) fprintf(f,"%u,%u,%llu,%s,%llu,%llu,%llu,%llu,%llu\n",
                state.window,state.frames,(unsigned long long)state.presents,names[i],
                (unsigned long long)c->calls,(unsigned long long)c->samples,
                (unsigned long long)c->cpu,(unsigned long long)c->wall,
                (unsigned long long)c->max_wall);
        }
        if (fclose(f)) fprintf(stderr,"[glcost] output write failed\n");
    } else fprintf(stderr,"[glcost] cannot open output: %s\n",path);
    state.frames = 0;
    errno = saved;
}
int ds_gl_cost_begin(unsigned frames) {
    int saved = errno;
    int (*active)(void) = dlsym(RTLD_DEFAULT,"ds_rt_active");
    if ((active && active()) || !frames) { errno = saved; return 0; }
    ds_gl_cost_flush();
    memset(state.counts,0,sizeof(state.counts));
    state.remaining = frames;
    state.frames = 0;
    ++state.window;
    state.random = 0x9e3779b9u ^ state.window;
    fprintf(stderr,"[glcost] sampling 1/16 GL calls for %u SINGLE frames\n",frames);
    errno = saved;
    return 1;
}
void ds_gl_cost_present(void) {
    ++state.presents;
    if (state.remaining) {
        ++state.frames;
        if (--state.remaining == 0) ds_gl_cost_flush();
    }
    if (auto_frame && state.presents == auto_frame) ds_gl_cost_begin(120);
}
struct Stamp { uint64_t cpu, wall; int sampled; };
static struct Stamp enter(unsigned id) {
    struct Stamp s = {0};
    if (state.recursion++ || !state.remaining) return s;
    ++state.counts[id].calls;
    uint32_t r=state.random;
    r ^= r<<13; r ^= r>>17; r ^= r<<5; state.random=r;
    if (r & 15) return s;
    s.sampled=1;
    s.wall=now(CLOCK_MONOTONIC);
    s.cpu=now(CLOCK_THREAD_CPUTIME_ID);
    return s;
}
static void leave(unsigned id, struct Stamp s) {
    --state.recursion;
    if (!s.sampled) return;
    uint64_t cpu=now(CLOCK_THREAD_CPUTIME_ID)-s.cpu;
    uint64_t wall=now(CLOCK_MONOTONIC)-s.wall;
    struct Count *c=&state.counts[id];
    ++c->samples; c->cpu+=cpu; c->wall+=wall;
    if (wall>c->max_wall) c->max_wall=wall;
}
#define BEFORE(n) \
    if (!real_##n) { fputs("[glcost] unresolved " #n "\n",stderr); abort(); } \
    int incoming_errno=errno; struct Stamp stamp=enter(ID_##n); errno=incoming_errno
#define AFTER(n) int result_errno=errno; leave(ID_##n,stamp); errno=result_errno
#define GLCOST_CUSTOM_BUFFER_DATA
#define V(n,p,a) void n p { BEFORE(n); real_##n a; AFTER(n); }
#define R(t,n,p,a) t n p { BEFORE(n); t result=real_##n a; AFTER(n); return result; }
#include "gl_cost_calls.def"
#undef V
#undef R

/* Distinguish upload classes without extra timing calls or aggregate double count. */
void glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage) {
 if(!real_glBufferData){fputs("[glcost] unresolved glBufferData\n",stderr);abort();}
 unsigned id=target==GL_ARRAY_BUFFER?ID_buffer_vertex:
  target==GL_ELEMENT_ARRAY_BUFFER?(size>0&&size<=32?ID_buffer_index_small:ID_buffer_index_large):ID_glBufferData;
 int saved=errno;struct Stamp stamp=enter(id);errno=saved;
 real_glBufferData(target,size,data,usage);
 saved=errno;leave(id,stamp);errno=saved;
}
