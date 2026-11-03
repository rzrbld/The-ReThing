#ifndef THING_ADJACENT_BATCH_H
#define THING_ADJACENT_BATCH_H
#include "d3d8_renderer.h"
typedef struct {
 const D8State *state;
 const void *vertices,*indices;
 size_t vertex_bytes,index_bytes;
 unsigned primitive,count,stride,vcount,base,index_size;
 uintptr_t vertex_identity,target_identity;
 unsigned target_height;
 GLuint textures[2];
 int default_framebuffer;
} ThingBatchDraw;
typedef int (*ThingBatchEmit)(void *context,const ThingBatchDraw *draw);
typedef struct ThingBatch ThingBatch;
ThingBatch *thing_batch_create(size_t vertex_capacity,unsigned index_capacity,ThingBatchEmit emit,void *context);
/* 0 queued,1 unsupported/too large (caller must draw normally),-1 invalid/error.
 * Unsupported/error paths flush earlier draws first. Geometry is copied now;
 * textures,targets and external GL work still require explicit flush barriers.
 * Intended for GPU vertex/cache-OFF experiments only, opt-in SINGLE renderer only. */
int thing_batch_add(ThingBatch *,const ThingBatchDraw *);
int thing_batch_flush(ThingBatch *);
void thing_batch_destroy(ThingBatch *);
#endif
