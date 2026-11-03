#ifndef THING_TERRAIN_IDENTITY_H
#define THING_TERRAIN_IDENTITY_H
#include <stdint.h>
#include <string.h>
extern uint32_t ti_cell,ti_node,ti_target,ti_render;
void ti_tag(uint32_t key);
void ti_present(void);
void thing_geometry_identity(uint32_t out[5]);
#ifdef THING_TERRAIN_IDENTITY_DEFINE
uint32_t ti_cell,ti_node,ti_target,ti_render;
static struct {uint32_t key,cell,node,target,ambiguous;} ti_map[16384];
static unsigned ti_overflow;
static unsigned ti_slot(uint32_t key){unsigned h=(key*2654435761u)>>18;for(unsigned i=0;i<16384;i++,h=(h+1)&16383)if(!ti_map[h].key||ti_map[h].key==key)return h;return 16384;}
void ti_tag(uint32_t key){if(!key)return;unsigned h=ti_slot(key);if(h==16384){ti_overflow++;return;}
 if(!ti_map[h].key){ti_map[h].key=key;ti_map[h].cell=ti_cell;ti_map[h].node=ti_node;ti_map[h].target=ti_target;}
 else if(ti_map[h].cell!=ti_cell||ti_map[h].node!=ti_node||ti_map[h].target!=ti_target)ti_map[h].ambiguous=1;
}
void ti_present(void){memset(ti_map,0,sizeof ti_map);ti_overflow=0;}
void thing_geometry_identity(uint32_t out[5]){memset(out,0,20);out[0]=ti_render;if(ti_overflow){out[4]=2;return;}unsigned h=ti_render?ti_slot(ti_render):16384;
 if(h<16384&&ti_map[h].key){out[1]=ti_map[h].cell;out[2]=ti_map[h].node;out[3]=ti_map[h].target;out[4]=ti_map[h].ambiguous;}}
#endif
#endif
