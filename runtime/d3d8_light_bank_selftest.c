#include "d3d8_lights.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned seed=17;
static unsigned next(void){seed=seed*1664525u+1013904223u;return seed;}
static void check(const D8LightBank *b){
 float lights[8][26];uint32_t on[8],ids[8];unsigned count=ds_d8_lights_pack(b,lights,on,ids),n=0;
 for(unsigned i=0;i<DS_D8_LOGICAL_LIGHTS;i++)if(b->enabled[i]){
  assert(n<count&&on[n]&&ids[n]==i&&b->active_ids[n]==i);
  assert(!memcmp(lights[n],b->light[i],sizeof lights[n]));n++;
 }
 assert(n==count&&count==b->active_count);
 for(;n<8;n++){assert(!on[n]&&ids[n]==UINT32_MAX);for(unsigned j=0;j<26;j++)assert(lights[n][j]==0);}
}
int main(void){
 D8LightBank b,snapshot;D8LightMask mask;float light[26]={0};uint32_t type=1;memcpy(light,&type,4);light[1]=.75f;
 ds_d8_lights_init(&b);assert(!ds_d8_lights_set(&b,511,light));assert(!ds_d8_lights_enable(&b,511,1));check(&b);
 assert(ds_d8_lights_set(&b,DS_D8_LOGICAL_LIGHTS,light)==-1);
 snapshot=b;ds_d8_light_mask_clear(&mask);mask.enable[511]=1;
 assert(!ds_d8_lights_enable(&b,511,0));assert(!ds_d8_lights_apply_masked(&b,&snapshot,&mask));check(&b);assert(b.active_count==1);
 for(unsigned k=0;k<100000;k++){
  unsigned id=next()%DS_D8_LOGICAL_LIGHTS,enable=(next()>>16)&1;
  D8LightBank before=b;int r=ds_d8_lights_enable(&b,id,enable);
  if(r)assert(!memcmp(&before,&b,sizeof b));
  check(&b);
  if(k%17==0){snapshot=b;ds_d8_light_mask_all(&mask);assert(!ds_d8_lights_apply_masked(&b,&snapshot,&mask));check(&b);}
 }
 printf("PASS: light ID 511, bounded capacity, 100000 enable/disable operations, masked restores and packed order match full-table reference; bank_bytes=%zu\n",sizeof b);
}
