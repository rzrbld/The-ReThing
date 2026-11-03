#include "adjacent_batch.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
struct ThingBatch {
 D8State state;
 ThingBatchDraw draw;
 unsigned char *vertices;
 uint32_t *indices;
 size_t vertex_capacity,vertex_used;
 unsigned index_capacity,index_used,vertex_count;
 ThingBatchEmit emit;void *context;
};
ThingBatch *thing_batch_create(size_t bytes,unsigned indices,ThingBatchEmit emit,void *context){
 size_t allocation;
 if(!bytes||indices<3||!emit||__builtin_mul_overflow((size_t)indices,sizeof(uint32_t),&allocation))return NULL;
 ThingBatch *b=calloc(1,sizeof *b);if(!b)return NULL;
 b->vertices=malloc(bytes);b->indices=malloc(allocation);
 if(!b->vertices||!b->indices){free(b->vertices);free(b->indices);free(b);return NULL;}
 b->vertex_capacity=bytes;b->index_capacity=indices;b->emit=emit;b->context=context;return b;
}
int thing_batch_flush(ThingBatch *b){
 if(!b||!b->index_used)return 0;
 ThingBatchDraw d=b->draw;d.state=&b->state;d.vertices=b->vertices;d.vertex_bytes=b->vertex_used;
 d.indices=b->indices;d.index_bytes=(size_t)b->index_used*4;d.index_size=4;
 d.base=0;d.vcount=b->vertex_count;d.count=b->index_used/3;
 int rc=b->emit(b->context,&d);
 b->vertex_used=0;b->vertex_count=b->index_used=0;
 return rc;
}
static int finish_with(ThingBatch *b,int result){return thing_batch_flush(b)?-1:result;}
static uint32_t index_at(const ThingBatchDraw *d,unsigned i){
 if(d->index_size==2){uint16_t x;memcpy(&x,(const unsigned char*)d->indices+(size_t)i*2,2);return x;}
 uint32_t x;memcpy(&x,(const unsigned char*)d->indices+(size_t)i*4,4);return x;
}
/* The supported non-blended renderer consumes world0, view, projection and
 * two texture matrices. Do not copy/compare the other507 unused matrices on
 * every batch. Extend this list before supporting additional transforms. */
static const unsigned matrices[]={256,2,3,16,17};
static void snapshot_state(D8State *dst,const D8State *src){
 memcpy(dst,src,offsetof(D8State,transform));
 for(unsigned i=0;i<5;i++)memcpy(dst->transform[matrices[i]],src->transform[matrices[i]],64);
 memcpy(&dst->material,&src->material,sizeof *dst-offsetof(D8State,material));
}
static int same_state(const D8State *a,const D8State *b){
 if(memcmp(a,b,offsetof(D8State,transform))||memcmp(&a->material,&b->material,sizeof *a-offsetof(D8State,material)))return 0;
 for(unsigned i=0;i<5;i++)if(memcmp(a->transform[matrices[i]],b->transform[matrices[i]],64))return 0;
 return 1;
}
static int compatible(const ThingBatch *b,const ThingBatchDraw *d){
 const ThingBatchDraw *p=&b->draw;
 return p->vertex_identity==d->vertex_identity&&p->target_identity==d->target_identity&&
 p->target_height==d->target_height&&p->default_framebuffer==d->default_framebuffer&&
 p->stride==d->stride&&p->textures[0]==d->textures[0]&&p->textures[1]==d->textures[1]&&
 same_state(&b->state,d->state);
}
int thing_batch_add(ThingBatch *b,const ThingBatchDraw *d){
 if(!b||!d)return -1;
 if(!d->state||!d->vertices||!d->indices||!d->stride)return finish_with(b,-1);
 if(d->primitive!=4||!d->vertex_identity||(d->state->fvf&14)!=2||d->state->rs[27]||d->state->rs[52]||d->state->rs[151]||d->state->rs[167])return finish_with(b,1);
 if((d->index_size!=2&&d->index_size!=4)||d->count>UINT_MAX/3)return finish_with(b,-1);
 unsigned need=d8_fvf_stride(d->state->fvf);
 if(!need||d->stride<need)return finish_with(b,-1);
 unsigned n=d->count*3;if(!n)return 0;
 if((uint64_t)n*d->index_size>d->index_bytes)return finish_with(b,-1);
 if(n>b->index_capacity)return finish_with(b,1);
 unsigned lo=UINT_MAX,hi=0;
 for(unsigned i=0;i<n;i++){
  uint32_t ix=index_at(d,i);
  if(ix>=d->vcount||ix>UINT_MAX-d->base)return finish_with(b,-1);
  if(ix<lo)lo=ix;if(ix>hi)hi=ix;
 }
 uint64_t start=((uint64_t)d->base+lo)*d->stride;
 uint64_t bytes=((uint64_t)hi-lo+1)*d->stride;
 if(start>d->vertex_bytes||bytes>d->vertex_bytes-start)return finish_with(b,-1);
 if(bytes>b->vertex_capacity)return finish_with(b,1);
 unsigned span=hi-lo+1;
 if(b->index_used&&(!compatible(b,d)||bytes>b->vertex_capacity-b->vertex_used||n>b->index_capacity-b->index_used||span>UINT_MAX-b->vertex_count)){
  if(thing_batch_flush(b))return -1;
 }
 if(!b->index_used){b->draw=*d;snapshot_state(&b->state,d->state);}
 memcpy(b->vertices+b->vertex_used,(const unsigned char*)d->vertices+(size_t)start,(size_t)bytes);
 for(unsigned i=0;i<n;i++)b->indices[b->index_used+i]=index_at(d,i)-lo+b->vertex_count;
 b->vertex_used+=(size_t)bytes;b->vertex_count+=span;b->index_used+=n;
 return 0;
}
void thing_batch_destroy(ThingBatch *b){if(b){/* Caller owns flush/error handling. */free(b->vertices);free(b->indices);free(b);}}
