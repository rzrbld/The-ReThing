#define THING_TERRAIN_IDENTITY_DEFINE
#include "terrain_identity.h"
#include <assert.h>
int main(void){uint32_t v[5];ti_cell=11;ti_node=22;ti_target=33;ti_tag(44);ti_render=44;thing_geometry_identity(v);assert(v[0]==44&&v[1]==11&&v[2]==22&&v[3]==33&&!v[4]);ti_tag(44);thing_geometry_identity(v);assert(!v[4]);ti_node=23;ti_tag(44);thing_geometry_identity(v);assert(v[4]==1);ti_present();thing_geometry_identity(v);assert(v[1]==0&&v[2]==0&&!v[4]);for(unsigned i=1;i<=16385;i++)ti_tag(i);thing_geometry_identity(v);assert(v[4]==2);return 0;}
