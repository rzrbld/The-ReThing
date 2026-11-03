/* Queue range/lifetime invariant used by the range-handoff candidate.
 * No GPU calls: compare emitted indexed vertices against copied source values. */
#include "adjacent_batch.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t rng=0x17942a61;
static uint32_t next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
unsigned d8_fvf_stride(unsigned fvf){assert(fvf==2);return 12;}
static uint32_t expected[200000][3];
static unsigned pending,consumed,emits,draws;
static int emit(void *context,const ThingBatchDraw *d){
 (void)context;assert(d->index_size==4&&d->base==0&&d->vcount&&d->count);
 assert(d->vertex_bytes==(size_t)d->vcount*d->stride);
 unsigned lo=UINT32_MAX,hi=0;
 for(unsigned i=0;i<d->count*3;i++){
  uint32_t ix;memcpy(&ix,(const char*)d->indices+4*i,4);
  assert(ix<d->vcount&&consumed<pending);
  assert(!memcmp((const char*)d->vertices+ix*d->stride,expected[consumed++],12));
  if(ix<lo)lo=ix;if(ix>hi)hi=ix;
 }
 assert(lo==0&&hi==d->vcount-1);emits++;return 0;
}
int main(void){
 D8State state={0};state.fvf=2;
 for(unsigned batch=0;batch<1000;batch++){
  pending=consumed=0;
  ThingBatch *q=thing_batch_create(4096,512,emit,NULL);assert(q);
  for(unsigned draw=0;draw<50;draw++){
   uint32_t vertices[72][3],ix32[24];uint16_t ix16[24];
   unsigned base=next()%8,vc=1+next()%64,n=(1+next()%8)*3,isize=(draw&1)?2:4;
   for(unsigned i=0;i<72;i++)for(unsigned j=0;j<3;j++)vertices[i][j]=next();
   for(unsigned i=0;i<n;i++){
    unsigned ix=(draw%7==0)?vc-1:next()%vc;ix32[i]=ix;ix16[i]=(uint16_t)ix;
    assert(pending<200000);memcpy(expected[pending++],vertices[base+ix],12);
   }
   if(draw%9==0)state.rs[60]=next();
   ThingBatchDraw d={.state=&state,.vertices=vertices,.vertex_bytes=(base+vc)*12,
    .indices=isize==2?(void*)ix16:(void*)ix32,.index_bytes=n*isize,.index_size=isize,
    .primitive=4,.count=n/3,.stride=12,.vcount=vc,.base=base,
    .vertex_identity=1,.target_identity=1,.target_height=480};
   assert(!thing_batch_add(q,&d));draws++;
   memset(vertices,0,sizeof vertices);memset(ix16,255,sizeof ix16);memset(ix32,255,sizeof ix32);
   if(draw%13==0)assert(!thing_batch_flush(q));
  }
  assert(!thing_batch_flush(q));assert(consumed==pending);thing_batch_destroy(q);
 }
 printf("PASS %u source draws, %u emissions: exact indexed vertex order, owned lifetime, min0/max(vcount-1), sparse/degenerate indices,16/32-bit sources,base offsets,state/capacity/explicit flushes\n",draws,emits);
 return 0;
}
